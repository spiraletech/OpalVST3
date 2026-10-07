#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

class OpalEngine
{
public:
    struct Parameters
    {
        float frequencyHz = 528.0f;
        float boostDb = 3.0f;
        float opal = 0.35f;
        float mix = 0.50f;
    };

    void prepare (double newSampleRate, int maximumBlockSize, int)
    {
        sampleRate = newSampleRate;

        juce::dsp::ProcessSpec stereoSpec;
        stereoSpec.sampleRate = sampleRate;
        stereoSpec.maximumBlockSize = static_cast<juce::uint32> (maximumBlockSize);
        stereoSpec.numChannels = 2;

        crystalReverb.prepare (stereoSpec);
        crystalReverb.reset();

        resonantBuffer.setSize (1, maximumBlockSize, false, false, true);
        reverbBuffer.setSize (2, maximumBlockSize, false, false, true);

        boostSmoothed.reset (sampleRate, 0.030);
        opalSmoothed.reset (sampleRate, 0.040);
        mixSmoothed.reset (sampleRate, 0.030);

        boostSmoothed.setCurrentAndTargetValue (1.0f);
        opalSmoothed.setCurrentAndTargetValue (0.0f);
        mixSmoothed.setCurrentAndTargetValue (0.50f);

        currentFrequencyHz = 528.0f;
        detectorPhase = 0.0;
        resetDetector();
        updateDetectorBandwidth();
        updateCrystalReverb();
    }

    void reset()
    {
        crystalReverb.reset();
        detectorPhase = 0.0;
        resetDetector();
        lastEnergyDb.store (-100.0f);
    }

    void setParameters (const Parameters& p)
    {
        parameters = p;

        const auto boostTarget =
            juce::Decibels::decibelsToGain (
                juce::jlimit (0.0f, 15.0f, p.boostDb));

        if (p.boostDb <= 0.0001f)
            boostSmoothed.setCurrentAndTargetValue (1.0f);
        else
            boostSmoothed.setTargetValue (boostTarget);

        const auto opalTarget =
            juce::jlimit (0.0f, 1.0f, p.opal);

        if (opalTarget <= 0.0001f)
            opalSmoothed.setCurrentAndTargetValue (0.0f);
        else
            opalSmoothed.setTargetValue (opalTarget);

        const auto mixTarget =
            juce::jlimit (0.0f, 1.0f, p.mix);

        if (mixTarget <= 0.0001f)
            mixSmoothed.setCurrentAndTargetValue (0.0f);
        else
            mixSmoothed.setTargetValue (mixTarget);

        const auto selectedHz =
            juce::jmax (20.0f, p.frequencyHz);

        if (std::abs (selectedHz - currentFrequencyHz) > 0.01f)
        {
            currentFrequencyHz = selectedHz;
            resetDetector();
            updateDetectorBandwidth();
        }
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const auto channels = buffer.getNumChannels();
        const auto samples = buffer.getNumSamples();

        if (channels <= 0 || samples <= 0)
            return;

        jassert (samples <= resonantBuffer.getNumSamples());

        // Neutral-path guarantees for SPAN/null testing.
        if (parameters.mix <= 0.0001f
            || (parameters.boostDb <= 0.0001f
                && parameters.opal <= 0.0001f))
        {
            if (parameters.opal <= 0.0001f)
                crystalReverb.reset();

            lastEnergyDb.store (-100.0f);
            return;
        }

        resonantBuffer.clear();
        reverbBuffer.clear();

        auto* resonant =
            resonantBuffer.getWritePointer (0);

        auto* reverbLeft =
            reverbBuffer.getWritePointer (0);

        auto* reverbRight =
            reverbBuffer.getWritePointer (1);

        double wetEnergy = 0.0;

        // Lock-in detector:
        // demodulate around the selected frequency, low-pass the I/Q components
        // to a 1% total bandwidth, then remodulate at the exact selected Hz.
        //
        // This separates DETECTOR WIDTH from OUTPUT CENTER:
        // the detector listens to a 1% neighborhood, while the generated
        // resonance remains phase-locked to the selected carrier.
        for (int sample = 0; sample < samples; ++sample)
        {
            float mono = 0.0f;

            for (int ch = 0; ch < channels; ++ch)
                mono += buffer.getSample (ch, sample);

            mono /= static_cast<float> (channels);

            const auto carrierCos =
                static_cast<float> (std::cos (detectorPhase));

            const auto carrierSin =
                static_cast<float> (std::sin (detectorPhase));

            double iValue =
                static_cast<double> (mono)
                * static_cast<double> (carrierCos);

            double qValue =
                -static_cast<double> (mono)
                * static_cast<double> (carrierSin);

            // Six cascaded stable one-pole sections.
            // stageCutoff is calibrated so the FINAL lock-in passband is
            // approximately -3 dB at +/-0.5% of the selected center,
            // i.e. 1% total detector bandwidth.
            for (size_t stage = 0; stage < detectorStages; ++stage)
            {
                detectorI[stage] +=
                    detectorAlpha
                    * (iValue - detectorI[stage]);

                detectorQ[stage] +=
                    detectorAlpha
                    * (qValue - detectorQ[stage]);

                iValue = detectorI[stage];
                qValue = detectorQ[stage];
            }

            const auto selectedComponent =
                static_cast<float> (
                    2.0
                    * (iValue * static_cast<double> (carrierCos)
                       - qValue * static_cast<double> (carrierSin)));

            const auto phaseOffset =
                std::atan2 (
                    qValue,
                    iValue);

            const auto selectedAmplitude =
                static_cast<float> (
                    2.0
                    * std::sqrt (
                        iValue * iValue
                        + qValue * qValue));

            const auto boostGain =
                boostSmoothed.getNextValue();

            const auto opal =
                opalSmoothed.getNextValue();

            // BOOST = additional exact selected-frequency resonance.
            // For a stationary tone at the selected center, the reconstructed
            // component is phase-aligned with the source, so +15 dB measures
            // as +15 dB at MIX 100 / OPAL 0 after detector settling.
            const auto boostDelta =
                selectedComponent
                * (boostGain - 1.0f);

            // OPAL's harmonic family is phase-locked to the selected frequency.
            // No broadband distortion path reaches the output.
            const auto theta =
                detectorPhase + phaseOffset;

            const auto nyquist =
                static_cast<float> (sampleRate * 0.49);

            float crystalHarmonics = 0.0f;

            if (opal > 0.0001f)
            {
                if (currentFrequencyHz * 2.0f < nyquist)
                {
                    crystalHarmonics +=
                        static_cast<float> (std::cos (theta * 2.0))
                        * selectedAmplitude
                        * 0.34f;
                }

                if (currentFrequencyHz * 3.0f < nyquist)
                {
                    crystalHarmonics +=
                        static_cast<float> (std::cos (theta * 3.0))
                        * selectedAmplitude
                        * 0.19f;
                }

                if (currentFrequencyHz * 4.0f < nyquist)
                {
                    crystalHarmonics +=
                        static_cast<float> (std::cos (theta * 4.0))
                        * selectedAmplitude
                        * 0.09f;
                }

                if (currentFrequencyHz * 5.0f < nyquist)
                {
                    crystalHarmonics +=
                        static_cast<float> (std::cos (theta * 5.0))
                        * selectedAmplitude
                        * 0.045f;
                }
            }

            const auto crystalResonance =
                selectedComponent
                * (0.22f * opal);

            const auto harmonicField =
                crystalHarmonics
                * opal;

            const auto opalCore =
                crystalResonance
                + harmonicField;

            resonant[sample] =
                boostDelta + opalCore;

            // Only OPAL enters the reverb field.
            reverbLeft[sample] = opalCore;
            reverbRight[sample] = opalCore;

            detectorPhase +=
                juce::MathConstants<double>::twoPi
                * static_cast<double> (currentFrequencyHz)
                / sampleRate;

            if (detectorPhase >= juce::MathConstants<double>::twoPi)
                detectorPhase -= juce::MathConstants<double>::twoPi;
        }

        if (parameters.opal > 0.0001f)
        {
            juce::dsp::AudioBlock<float> block (reverbBuffer);

            auto activeBlock =
                block.getSubBlock (
                    0,
                    static_cast<size_t> (samples));

            juce::dsp::ProcessContextReplacing<float> context (activeBlock);
            crystalReverb.process (context);
        }
        else
        {
            reverbBuffer.clear();
            crystalReverb.reset();
        }

        const auto* reverbLeftOut =
            reverbBuffer.getReadPointer (0);

        const auto* reverbRightOut =
            reverbBuffer.getReadPointer (1);

        // MIX scales only the added circuit. Dry L/R is never rewritten.
        for (int sample = 0; sample < samples; ++sample)
        {
            const auto mix =
                mixSmoothed.getNextValue();

            const auto immediate =
                resonant[sample];

            const auto leftTail =
                parameters.opal > 0.0001f
                    ? reverbLeftOut[sample] * 0.20f
                    : 0.0f;

            const auto rightTail =
                parameters.opal > 0.0001f
                    ? reverbRightOut[sample] * 0.20f
                    : 0.0f;

            for (int ch = 0; ch < channels; ++ch)
            {
                float tail = 0.0f;

                if (channels == 1)
                    tail = 0.5f * (leftTail + rightTail);
                else
                    tail = ch == 0 ? leftTail : rightTail;

                const auto delta =
                    (immediate + tail) * mix;

                buffer.addSample (
                    ch,
                    sample,
                    delta);

                wetEnergy +=
                    static_cast<double> (delta)
                    * static_cast<double> (delta);
            }
        }

        const auto count =
            juce::jmax (1, samples * channels);

        const auto rms =
            std::sqrt (
                wetEnergy
                / static_cast<double> (count));

        lastEnergyDb.store (
            juce::Decibels::gainToDecibels (
                static_cast<float> (rms),
                -100.0f));
    }

    float getLastEnergyDb() const noexcept
    {
        return lastEnergyDb.load();
    }

private:
    static constexpr size_t detectorStages = 6;

    void resetDetector()
    {
        detectorI.fill (0.0);
        detectorQ.fill (0.0);
    }

    void updateDetectorBandwidth()
    {
        // Requested detector bandwidth = 1% of center, total.
        const auto halfBandwidthHz =
            static_cast<double> (currentFrequencyHz)
            * 0.005;

        // For N cascaded one-pole LPFs:
        // |H| = (1 + (f/fc)^2)^(-N/2).
        // Solve for the per-stage fc that yields -3 dB at halfBandwidthHz.
        const auto ratioAtThreeDb =
            std::sqrt (
                std::pow (2.0, 1.0 / static_cast<double> (detectorStages))
                - 1.0);

        const auto stageCutoffHz =
            halfBandwidthHz
            / ratioAtThreeDb;

        detectorAlpha =
            1.0
            - std::exp (
                -juce::MathConstants<double>::twoPi
                * stageCutoffHz
                / sampleRate);
    }

    void updateCrystalReverb()
    {
        juce::dsp::Reverb::Parameters rp;

        rp.roomSize = 0.24f;
        rp.damping = 0.76f;
        rp.wetLevel = 1.0f;
        rp.dryLevel = 0.0f;

        // Stereo bloom is intentionally confined to OPAL's reverb.
        rp.width = 0.34f;

        rp.freezeMode = 0.0f;

        crystalReverb.setParameters (rp);
    }

    double sampleRate = 44100.0;
    float currentFrequencyHz = 528.0f;
    double detectorPhase = 0.0;
    double detectorAlpha = 0.0;

    Parameters parameters;

    std::array<double, detectorStages> detectorI {};
    std::array<double, detectorStages> detectorQ {};

    juce::dsp::Reverb crystalReverb;

    juce::AudioBuffer<float> resonantBuffer;
    juce::AudioBuffer<float> reverbBuffer;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> boostSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> opalSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed;

    std::atomic<float> lastEnergyDb { -100.0f };
};

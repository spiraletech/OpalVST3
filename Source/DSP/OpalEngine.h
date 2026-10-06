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

        selectedBuffer.setSize (1, maximumBlockSize, false, false, true);
        reverbBuffer.setSize (2, maximumBlockSize, false, false, true);

        boostSmoothed.reset (sampleRate, 0.030);
        opalSmoothed.reset (sampleRate, 0.040);
        mixSmoothed.reset (sampleRate, 0.030);

        boostSmoothed.setCurrentAndTargetValue (1.0f);
        opalSmoothed.setCurrentAndTargetValue (0.0f);
        mixSmoothed.setCurrentAndTargetValue (0.50f);

        currentFrequencyHz = 528.0f;

        updateFrequencyNetwork (currentFrequencyHz);
        updateCrystalReverb();
    }

    void reset()
    {
        for (auto& filter : selectedFilters)
            filter.reset();

        for (auto& filter : harmonicFilters)
            filter.reset();

        crystalReverb.reset();
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
            updateFrequencyNetwork (currentFrequencyHz);
        }
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const auto channels = buffer.getNumChannels();
        const auto samples = buffer.getNumSamples();

        if (channels <= 0 || samples <= 0)
            return;

        jassert (samples <= selectedBuffer.getNumSamples());

        // Hard neutral-path invariants:
        // MIX=0 must be bit-identical bypass.
        // BOOST=0 + OPAL=0 must also be bit-identical regardless of MIX.
        if (parameters.mix <= 0.0001f
            || (parameters.boostDb <= 0.0001f
                && parameters.opal <= 0.0001f))
        {
            if (parameters.opal <= 0.0001f)
                crystalReverb.reset();

            lastEnergyDb.store (-100.0f);
            return;
        }

        auto* selected = selectedBuffer.getWritePointer (0);
        selectedBuffer.clear();
        reverbBuffer.clear();

        // The dry audio is NEVER rewritten or collapsed here.
        // Only a mono analysis/extraction copy is made for the selected frequency.
        for (int sample = 0; sample < samples; ++sample)
        {
            float mono = 0.0f;

            for (int ch = 0; ch < channels; ++ch)
                mono += buffer.getSample (ch, sample);

            mono /= static_cast<float> (channels);

            float narrowBand = mono;

            // Four cascaded Q=100 stages keep the selected region at or tighter
            // than the requested 1% nominal bandwidth.
            for (auto& filter : selectedFilters)
                narrowBand = filter.processSample (narrowBand);

            selected[sample] = narrowBand;
        }

        auto* reverbLeft = reverbBuffer.getWritePointer (0);
        auto* reverbRight = reverbBuffer.getWritePointer (1);

        double wetEnergy = 0.0;

        // Build only the delta that OPAL is allowed to add.
        for (int sample = 0; sample < samples; ++sample)
        {
            const auto sourceBand = selected[sample];
            const auto boostGain = boostSmoothed.getNextValue();
            const auto opal = opalSmoothed.getNextValue();

            // BOOST is literal gain above unity for the selected Q=100 band.
            const auto boostDelta =
                sourceBand * (boostGain - 1.0f);

            float tubeHarmonics = 0.0f;

            if (opal > 0.0001f)
            {
                // Smooth symmetrical tube-like transfer. The broadband distortion
                // never reaches the output directly; only tuned harmonic bands do.
                const auto driven =
                    std::tanh (sourceBand * 3.4f);

                const auto excitation =
                    driven - sourceBand;

                for (size_t h = 0; h < harmonicFilters.size(); ++h)
                {
                    if (! harmonicActive[h])
                        continue;

                    const auto harmonic =
                        harmonicFilters[h].processSample (excitation);

                    tubeHarmonics +=
                        harmonic
                        * harmonicWeights[h];
                }
            }

            // Resonance remains tied to the selected frequency only.
            const auto resonance =
                sourceBand
                * (0.18f * opal);

            const auto harmonics =
                tubeHarmonics
                * (0.55f * opal);

            const auto opalCore =
                resonance + harmonics;

            // Feed ONLY the OPAL core to the chamber.
            reverbLeft[sample] = opalCore;
            reverbRight[sample] = opalCore;

            // Reuse selectedBuffer as the mono immediate wet delta.
            selected[sample] =
                boostDelta + opalCore;
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

        // Add the effect delta to the untouched original dry channels.
        // MIX scales only this delta; it never crossfades into a mono dry path.
        for (int sample = 0; sample < samples; ++sample)
        {
            const auto mix = mixSmoothed.getNextValue();
            const auto immediate = selected[sample];

            const auto leftTail =
                parameters.opal > 0.0001f
                    ? reverbLeftOut[sample] * 0.22f
                    : 0.0f;

            const auto rightTail =
                parameters.opal > 0.0001f
                    ? reverbRightOut[sample] * 0.22f
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

                buffer.addSample (ch, sample, delta);

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
    void updateFrequencyNetwork (float selectedHz)
    {
        constexpr float selectedQ = 100.0f;

        for (auto& filter : selectedFilters)
        {
            filter.coefficients =
                juce::dsp::IIR::Coefficients<float>::makeBandPass (
                    sampleRate,
                    static_cast<double> (selectedHz),
                    selectedQ);

            filter.reset();
        }

        const auto safeTop =
            static_cast<float> (sampleRate * 0.46);

        const std::array<float, 3> multiples {
            2.0f, 3.0f, 4.0f
        };

        for (size_t i = 0; i < harmonicFilters.size(); ++i)
        {
            const auto hz =
                selectedHz * multiples[i];

            harmonicActive[i] =
                hz < safeTop;

            if (harmonicActive[i])
            {
                harmonicFilters[i].coefficients =
                    juce::dsp::IIR::Coefficients<float>::makeBandPass (
                        sampleRate,
                        static_cast<double> (hz),
                        32.0f);

                harmonicFilters[i].reset();
            }
        }
    }

    void updateCrystalReverb()
    {
        juce::dsp::Reverb::Parameters rp;

        rp.roomSize = 0.26f;
        rp.damping = 0.72f;
        rp.wetLevel = 1.0f;
        rp.dryLevel = 0.0f;

        // Slight stereo bloom belongs only to OPAL's reverb.
        rp.width = 0.38f;

        rp.freezeMode = 0.0f;

        crystalReverb.setParameters (rp);
    }

    double sampleRate = 44100.0;
    float currentFrequencyHz = 528.0f;

    Parameters parameters;

    std::array<juce::dsp::IIR::Filter<float>, 4> selectedFilters;

    std::array<juce::dsp::IIR::Filter<float>, 3> harmonicFilters;
    std::array<bool, 3> harmonicActive { true, true, true };
    const std::array<float, 3> harmonicWeights {
        0.62f, 0.30f, 0.14f
    };

    juce::dsp::Reverb crystalReverb;

    juce::AudioBuffer<float> selectedBuffer;
    juce::AudioBuffer<float> reverbBuffer;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> boostSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> opalSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed;

    std::atomic<float> lastEnergyDb { -100.0f };
};

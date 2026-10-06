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

        crystalReverb.prepare ({
            sampleRate,
            static_cast<juce::uint32> (maximumBlockSize),
            1
        });
        crystalReverb.reset();

        dryMonoBuffer.setSize (1, maximumBlockSize, false, false, true);
        wetBuffer.setSize (1, maximumBlockSize, false, false, true);
        reverbBuffer.setSize (1, maximumBlockSize, false, false, true);

        boostSmoothed.reset (sampleRate, 0.035);
        opalSmoothed.reset (sampleRate, 0.045);
        mixSmoothed.reset (sampleRate, 0.035);

        boostSmoothed.setCurrentAndTargetValue (1.0f);
        opalSmoothed.setCurrentAndTargetValue (0.35f);
        mixSmoothed.setCurrentAndTargetValue (0.50f);

        detectorEnvelope = 0.0f;
        fundamentalPhase = 0.0;
        currentFrequencyHz = 528.0f;

        updateDetectorFrequency (currentFrequencyHz);
        updateCrystalReverb();
    }

    void reset()
    {
        for (auto& filter : detectorFilters)
            filter.reset();

        crystalReverb.reset();

        detectorEnvelope = 0.0f;
        fundamentalPhase = 0.0;

        lastEnergyDb.store (-100.0f);
    }

    void setParameters (const Parameters& p)
    {
        parameters = p;

        boostSmoothed.setTargetValue (
            juce::Decibels::decibelsToGain (
                juce::jlimit (0.0f, 15.0f, p.boostDb)));

        opalSmoothed.setTargetValue (
            juce::jlimit (0.0f, 1.0f, p.opal));

        mixSmoothed.setTargetValue (
            juce::jlimit (0.0f, 1.0f, p.mix));

        const auto selectedHz = juce::jmax (20.0f, p.frequencyHz);

        if (std::abs (selectedHz - currentFrequencyHz) > 0.01f)
        {
            currentFrequencyHz = selectedHz;
            detectorEnvelope = 0.0f;
            updateDetectorFrequency (currentFrequencyHz);
        }
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const auto channels = buffer.getNumChannels();
        const auto samples = buffer.getNumSamples();

        if (channels <= 0 || samples <= 0)
            return;

        jassert (samples <= dryMonoBuffer.getNumSamples());

        auto* dryMono = dryMonoBuffer.getWritePointer (0);
        auto* wetPath = wetBuffer.getWritePointer (0);
        auto* reverbInput = reverbBuffer.getWritePointer (0);

        wetBuffer.clear();
        reverbBuffer.clear();

        // One mono analogue-style signal path. Stereo hosts receive dual mono.
        for (int sample = 0; sample < samples; ++sample)
        {
            float sum = 0.0f;

            for (int ch = 0; ch < channels; ++ch)
                sum += buffer.getSample (ch, sample);

            dryMono[sample] = sum / static_cast<float> (channels);
        }

        const auto attackCoeff = static_cast<float> (
            1.0 - std::exp (-1.0 / (0.006 * sampleRate)));

        const auto releaseCoeff = static_cast<float> (
            1.0 - std::exp (-1.0 / (0.160 * sampleRate)));

        double wetEnergy = 0.0;

        for (int sample = 0; sample < samples; ++sample)
        {
            const auto dry = dryMono[sample];

            // 1% detector bandwidth: Q = centre / bandwidth = 100.
            // Four cascaded biquads make rejection outside that 1% window steep.
            float detector = dry;

            for (auto& filter : detectorFilters)
                detector = filter.processSample (detector);

            const auto targetEnvelope =
                juce::jlimit (0.0f, 1.0f, std::abs (detector) * 4.5f);

            if (targetEnvelope > detectorEnvelope)
                detectorEnvelope += (targetEnvelope - detectorEnvelope) * attackCoeff;
            else
                detectorEnvelope += (targetEnvelope - detectorEnvelope) * releaseCoeff;

            const auto frequency = currentFrequencyHz;
            const auto boostGain = boostSmoothed.getNextValue();
            const auto opal = opalSmoothed.getNextValue();

            const auto phaseAdvance =
                juce::MathConstants<double>::twoPi
                * static_cast<double> (frequency)
                / sampleRate;

            const auto fundamental =
                static_cast<float> (std::sin (fundamentalPhase));

            const auto harmonic2 =
                static_cast<float> (std::sin (fundamentalPhase * 2.0));

            const auto harmonic3 =
                static_cast<float> (std::sin (fundamentalPhase * 3.0));

            const auto harmonic5 =
                static_cast<float> (std::sin (fundamentalPhase * 5.0));

            const auto harmonic7 =
                static_cast<float> (std::sin (fundamentalPhase * 7.0));

            fundamentalPhase += phaseAdvance;

            if (fundamentalPhase >= juce::MathConstants<double>::twoPi)
                fundamentalPhase -= juce::MathConstants<double>::twoPi;

            // BOOST = exact selected frequency only.
            const auto boostedFundamental =
                fundamental
                * detectorEnvelope
                * (boostGain - 1.0f)
                * 0.115f;

            // OPAL = exact integer harmonics + resonance + restrained crystal chamber.
            float harmonicField = 0.0f;
            const auto nyquist = static_cast<float> (sampleRate * 0.49);

            if (frequency * 2.0f < nyquist)
                harmonicField += harmonic2 * 0.42f;

            if (frequency * 3.0f < nyquist)
                harmonicField += harmonic3 * 0.23f;

            if (frequency * 5.0f < nyquist)
                harmonicField += harmonic5 * 0.105f;

            if (frequency * 7.0f < nyquist)
                harmonicField += harmonic7 * 0.050f;

            const auto resonance =
                fundamental
                * detectorEnvelope
                * 0.090f
                * opal;

            const auto harmonics =
                harmonicField
                * detectorEnvelope
                * 0.082f
                * opal;

            const auto opalField =
                resonance + harmonics;

            reverbInput[sample] = opalField;
            wetPath[sample] = boostedFundamental + opalField;
        }

        {
            juce::dsp::AudioBlock<float> block (reverbBuffer);
            auto activeBlock = block.getSubBlock (0, static_cast<size_t> (samples));
            juce::dsp::ProcessContextReplacing<float> context (activeBlock);
            crystalReverb.process (context);
        }

        const auto* reverbOut = reverbBuffer.getReadPointer (0);
        const auto* immediateWet = wetBuffer.getReadPointer (0);

        for (int sample = 0; sample < samples; ++sample)
        {
            const auto dry = dryMono[sample];
            const auto mix = mixSmoothed.getNextValue();
            const auto opal = parameters.opal;

            const auto crystalTail =
                reverbOut[sample]
                * (0.12f + 0.14f * opal);

            const auto wet =
                immediateWet[sample] + crystalTail;

            const auto output =
                dry + wet * mix;

            for (int ch = 0; ch < channels; ++ch)
                buffer.setSample (ch, sample, output);

            wetEnergy +=
                static_cast<double> (wet)
                * static_cast<double> (wet);
        }

        const auto rms =
            samples > 0
                ? std::sqrt (wetEnergy / static_cast<double> (samples))
                : 0.0;

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
    void updateDetectorFrequency (float selectedHz)
    {
        constexpr float detectorQ = 100.0f; // exactly 1% nominal bandwidth

        for (auto& filter : detectorFilters)
        {
            filter.coefficients =
                juce::dsp::IIR::Coefficients<float>::makeBandPass (
                    sampleRate,
                    static_cast<double> (selectedHz),
                    detectorQ);

            filter.reset();
        }
    }

    void updateCrystalReverb()
    {
        juce::dsp::Reverb::Parameters rp;

        rp.roomSize = 0.29f;
        rp.damping = 0.77f;
        rp.wetLevel = 1.0f;
        rp.dryLevel = 0.0f;
        rp.width = 0.0f;
        rp.freezeMode = 0.0f;

        crystalReverb.setParameters (rp);
    }

    double sampleRate = 44100.0;
    float currentFrequencyHz = 528.0f;

    Parameters parameters;

    std::array<juce::dsp::IIR::Filter<float>, 4> detectorFilters;

    juce::dsp::Reverb crystalReverb;

    juce::AudioBuffer<float> dryMonoBuffer;
    juce::AudioBuffer<float> wetBuffer;
    juce::AudioBuffer<float> reverbBuffer;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> boostSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> opalSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed;

    float detectorEnvelope = 0.0f;
    double fundamentalPhase = 0.0;

    std::atomic<float> lastEnergyDb { -100.0f };
};

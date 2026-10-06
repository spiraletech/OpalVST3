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
        float opal = 0.35f;   // 0..1
        float mix = 0.50f;    // 0..1
    };

    void prepare (double newSampleRate, int maximumBlockSize, int)
    {
        sampleRate = newSampleRate;

        juce::dsp::ProcessSpec monoSpec;
        monoSpec.sampleRate = sampleRate;
        monoSpec.maximumBlockSize = static_cast<juce::uint32> (maximumBlockSize);
        monoSpec.numChannels = 1;

        for (auto& filter : detectorFilters)
        {
            filter.prepare (monoSpec);
            filter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
            filter.setResonance (3.2f);
        }

        crystalReverb.prepare (monoSpec);
        crystalReverb.reset();

        dryMonoBuffer.setSize (1, maximumBlockSize, false, false, true);
        reverbBuffer.setSize (1, maximumBlockSize, false, false, true);

        boostSmoothed.reset (sampleRate, 0.035);
        opalSmoothed.reset (sampleRate, 0.045);
        mixSmoothed.reset (sampleRate, 0.035);
        frequencySmoothed.reset (sampleRate, 0.050);

        boostSmoothed.setCurrentAndTargetValue (1.0f);
        opalSmoothed.setCurrentAndTargetValue (0.35f);
        mixSmoothed.setCurrentAndTargetValue (0.50f);
        frequencySmoothed.setCurrentAndTargetValue (528.0f);

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

        frequencySmoothed.setTargetValue (
            juce::jmax (20.0f, p.frequencyHz));

        if (std::abs (p.frequencyHz - currentFrequencyHz) > 0.01f)
            updateDetectorFrequency (p.frequencyHz);
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const auto channels = buffer.getNumChannels();
        const auto samples = buffer.getNumSamples();

        if (channels <= 0 || samples <= 0)
            return;

        jassert (samples <= dryMonoBuffer.getNumSamples());

        auto* dryMono = dryMonoBuffer.getWritePointer (0);
        auto* reverbInput = reverbBuffer.getWritePointer (0);

        reverbBuffer.clear();

        // OPAL is one mono signal path. Stereo hosts receive dual-mono output.
        for (int sample = 0; sample < samples; ++sample)
        {
            float sum = 0.0f;

            for (int ch = 0; ch < channels; ++ch)
                sum += buffer.getSample (ch, sample);

            dryMono[sample] = sum / static_cast<float> (channels);
        }

        const auto attackCoeff = static_cast<float> (
            1.0 - std::exp (-1.0 / (0.008 * sampleRate)));

        const auto releaseCoeff = static_cast<float> (
            1.0 - std::exp (-1.0 / (0.180 * sampleRate)));

        double wetEnergy = 0.0;

        // Pass 1: detect a very narrow area around the requested frequency,
        // then regenerate the selected fundamental as an exact sine at that Hz.
        for (int sample = 0; sample < samples; ++sample)
        {
            const auto dry = dryMono[sample];

            float detector = dry;

            for (auto& filter : detectorFilters)
                detector = filter.processSample (0, detector);

            const auto targetEnvelope = juce::jmin (1.0f, std::abs (detector) * 5.0f);

            if (targetEnvelope > detectorEnvelope)
                detectorEnvelope += (targetEnvelope - detectorEnvelope) * attackCoeff;
            else
                detectorEnvelope += (targetEnvelope - detectorEnvelope) * releaseCoeff;

            const auto frequency = frequencySmoothed.getNextValue();
            const auto boostGain = boostSmoothed.getNextValue();
            const auto opal = opalSmoothed.getNextValue();

            const auto phaseAdvance = juce::MathConstants<double>::twoPi
                                    * static_cast<double> (frequency)
                                    / sampleRate;

            const auto fundamental = static_cast<float> (std::sin (fundamentalPhase));
            const auto harmonic2 = static_cast<float> (std::sin (fundamentalPhase * 2.0));
            const auto harmonic3 = static_cast<float> (std::sin (fundamentalPhase * 3.0));
            const auto harmonic5 = static_cast<float> (std::sin (fundamentalPhase * 5.0));
            const auto harmonic7 = static_cast<float> (std::sin (fundamentalPhase * 7.0));

            fundamentalPhase += phaseAdvance;

            if (fundamentalPhase >= juce::MathConstants<double>::twoPi)
                fundamentalPhase -= juce::MathConstants<double>::twoPi;

            // BOOST adds only the exact selected fundamental.
            const auto boostedFundamental =
                fundamental
                * detectorEnvelope
                * (boostGain - 1.0f)
                * 0.115f;

            // OPAL adds crystalline resonance and exact integer harmonics.
            // Harmonics above Nyquist are automatically omitted.
            float harmonicField = 0.0f;
            const auto nyquist = static_cast<float> (sampleRate * 0.49);

            if (frequency * 2.0f < nyquist)
                harmonicField += harmonic2 * 0.42f;

            if (frequency * 3.0f < nyquist)
                harmonicField += harmonic3 * 0.24f;

            if (frequency * 5.0f < nyquist)
                harmonicField += harmonic5 * 0.11f;

            if (frequency * 7.0f < nyquist)
                harmonicField += harmonic7 * 0.055f;

            const auto resonance =
                fundamental * detectorEnvelope * 0.095f * opal;

            const auto harmonics =
                harmonicField * detectorEnvelope * 0.085f * opal;

            reverbInput[sample] = resonance + harmonics;

            const auto immediateWet =
                boostedFundamental + resonance + harmonics;

            reverbInput[sample] = immediateWet;
        }

        {
            juce::dsp::AudioBlock<float> block (reverbBuffer);
            auto activeBlock = block.getSubBlock (0, static_cast<size_t> (samples));
            juce::dsp::ProcessContextReplacing<float> context (activeBlock);
            crystalReverb.process (context);
        }

        const auto* reverbOut = reverbBuffer.getReadPointer (0);

        // Pass 2: add a restrained fixed mono crystal chamber.
        for (int sample = 0; sample < samples; ++sample)
        {
            const auto dry = dryMono[sample];
            const auto mix = mixSmoothed.getNextValue();
            const auto opal = parameters.opal;

            // Recreate the immediate exact-frequency field for this sample from
            // the dry-to-wet difference already carried into the reverb input.
            // Reverb itself remains intentionally subtle.
            const auto crystalTail = reverbOut[sample] * (0.18f + 0.16f * opal);

            // The reverb input was the exact generated OPAL field; the reverb
            // output contains that field plus the chamber response.
            const auto wet = crystalTail;

            // BOOST and OPAL are additive processors; MIX controls effect amount
            // while the mono dry signal remains at unity.
            const auto output = dry + wet * mix;

            for (int ch = 0; ch < channels; ++ch)
                buffer.setSample (ch, sample, output);

            wetEnergy += static_cast<double> (wet) * static_cast<double> (wet);
        }

        const auto rms = samples > 0
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
        currentFrequencyHz = juce::jmax (20.0f, selectedHz);

        // Cascaded narrow detector only controls amplitude. It never reaches the
        // output directly; the audible fundamental is regenerated exactly at the
        // selected frequency, eliminating adjacent-frequency bleed and width.
        for (size_t i = 0; i < detectorFilters.size(); ++i)
        {
            auto& filter = detectorFilters[i];
            filter.setCutoffFrequency (currentFrequencyHz);
            filter.setResonance (3.2f + 0.35f * static_cast<float> (i));
            filter.reset();
        }
    }

    void updateCrystalReverb()
    {
        juce::dsp::Reverb::Parameters rp;
        rp.roomSize = 0.33f;
        rp.damping = 0.73f;
        rp.wetLevel = 1.0f;
        rp.dryLevel = 1.0f;
        rp.width = 0.0f;
        rp.freezeMode = 0.0f;

        crystalReverb.setParameters (rp);
    }

    double sampleRate = 44100.0;
    float currentFrequencyHz = 528.0f;
    Parameters parameters;

    std::array<juce::dsp::StateVariableTPTFilter<float>, 4> detectorFilters;

    juce::dsp::Reverb crystalReverb;
    juce::AudioBuffer<float> dryMonoBuffer;
    juce::AudioBuffer<float> reverbBuffer;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> boostSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> opalSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> frequencySmoothed;

    float detectorEnvelope = 0.0f;
    double fundamentalPhase = 0.0;

    std::atomic<float> lastEnergyDb { -100.0f };
};

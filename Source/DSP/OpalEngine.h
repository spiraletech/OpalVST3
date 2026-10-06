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
        float opal = 0.35f;   // 0..1: resonance + octave harmonic + tuned reverb
        float mix = 0.50f;    // 0..1
        bool antiPhase = false;
    };

    void prepare (double newSampleRate, int maximumBlockSize, int)
    {
        sampleRate = newSampleRate;

        juce::dsp::ProcessSpec monoSpec;
        monoSpec.sampleRate = sampleRate;
        monoSpec.maximumBlockSize = static_cast<juce::uint32> (maximumBlockSize);
        monoSpec.numChannels = 1;

        for (auto& filter : targetFilters)
        {
            filter.prepare (monoSpec);
            filter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
            filter.setResonance (1.18f);
        }

        for (auto& filter : octaveFilters)
        {
            filter.prepare (monoSpec);
            filter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
            filter.setResonance (1.06f);
        }

        reverb.prepare (monoSpec);
        reverb.reset();

        monoBuffer.setSize (1, maximumBlockSize, false, false, true);
        reverbBuffer.setSize (1, maximumBlockSize, false, false, true);

        boostSmoothed.reset (sampleRate, 0.030);
        opalSmoothed.reset (sampleRate, 0.040);
        mixSmoothed.reset (sampleRate, 0.030);
        frequencySmoothed.reset (sampleRate, 0.045);

        boostSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (3.0f));
        opalSmoothed.setCurrentAndTargetValue (0.35f);
        mixSmoothed.setCurrentAndTargetValue (0.50f);
        frequencySmoothed.setCurrentAndTargetValue (528.0f);

        safetyGain = 1.0f;
        humPhase = 0.0;
        updateFilterFrequencies (528.0f);
        updateReverb();
    }

    void reset()
    {
        for (auto& filter : targetFilters)
            filter.reset();

        for (auto& filter : octaveFilters)
            filter.reset();

        reverb.reset();
        safetyGain = 1.0f;
        humPhase = 0.0;
        lastGainReductionDb.store (0.0f);
        lastEnergyDb.store (-100.0f);
    }

    void setParameters (const Parameters& p)
    {
        parameters = p;

        boostSmoothed.setTargetValue (
            juce::Decibels::decibelsToGain (juce::jlimit (0.0f, 15.0f, p.boostDb)));

        opalSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, p.opal));
        mixSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, p.mix));
        frequencySmoothed.setTargetValue (juce::jmax (20.0f, p.frequencyHz));

        if (std::abs (p.frequencyHz - currentFrequencyHz) > 0.01f)
            updateFilterFrequencies (p.frequencyHz);
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const auto channels = buffer.getNumChannels();
        const auto samples = buffer.getNumSamples();

        if (channels <= 0 || samples <= 0)
            return;

        jassert (samples <= monoBuffer.getNumSamples());

        monoBuffer.clear();
        reverbBuffer.clear();

        // OPAL is deliberately mono. Stereo inputs are summed to a single analogue-style
        // processing path, and stereo hosts receive the same mono result on both channels.
        auto* mono = monoBuffer.getWritePointer (0);

        for (int sample = 0; sample < samples; ++sample)
        {
            float sum = 0.0f;

            for (int ch = 0; ch < channels; ++ch)
                sum += buffer.getSample (ch, sample);

            mono[sample] = sum / static_cast<float> (channels);
        }

        auto* reverbIn = reverbBuffer.getWritePointer (0);

        // First pass: narrow centre-frequency circuit and one exact octave-up harmonic circuit.
        for (int sample = 0; sample < samples; ++sample)
        {
            const auto dry = mono[sample];
            const auto boost = boostSmoothed.getNextValue();
            const auto opal = opalSmoothed.getNextValue();
            const auto frequency = frequencySmoothed.getNextValue();

            float target = dry;

            for (auto& filter : targetFilters)
                target = filter.processSample (0, target);

            // Analogue-style full-wave excitation creates even harmonic content.
            // The octave circuit then tightly selects only 2x the chosen OPAL frequency.
            float octaveExciter = std::abs (target) - 0.3183f * std::abs (dry);

            for (auto& filter : octaveFilters)
                octaveExciter = filter.processSample (0, octaveExciter);

            const auto boostedDelta = target * (boost - 1.0f);

            const auto phaseAdvance = juce::MathConstants<double>::twoPi
                                    * static_cast<double> (frequency)
                                    / sampleRate;
            const auto pilot = static_cast<float> (std::sin (humPhase))
                             * juce::Decibels::decibelsToGain (-54.0f)
                             * opal;

            humPhase += phaseAdvance;
            if (humPhase >= juce::MathConstants<double>::twoPi)
                humPhase -= juce::MathConstants<double>::twoPi;

            // OPAL feeds the selected resonance, one tuned octave harmonic,
            // and a faint pilot tone into a fixed mono chamber.
            const auto resonance = target * (0.46f + 0.34f * opal);
            const auto octave = octaveExciter * 0.42f;
            reverbIn[sample] = (resonance + octave + pilot) * opal;

            // Store the non-reverb analogue path back into monoBuffer.
            mono[sample] = dry + boostedDelta + (resonance + octave + pilot) * opal * 0.52f;
        }

        {
            juce::dsp::AudioBlock<float> block (reverbBuffer);
            auto activeBlock = block.getSubBlock (0, static_cast<size_t> (samples));
            juce::dsp::ProcessContextReplacing<float> context (activeBlock);
            reverb.process (context);
        }

        const auto* verb = reverbBuffer.getReadPointer (0);

        const auto releaseCoeff = static_cast<float> (
            1.0 - std::exp (-1.0 / (0.180 * sampleRate)));

        constexpr float protectionThreshold = 0.50f; // about -6 dBFS
        float blockMaxReduction = 0.0f;
        double energy = 0.0;

        // Second pass: combine the fixed OPAL chamber, perform zero-lookahead protection,
        // polarity inversion if requested, then crossfade with the mono dry source.
        for (int sample = 0; sample < samples; ++sample)
        {
            float dry = 0.0f;

            for (int ch = 0; ch < channels; ++ch)
                dry += buffer.getSample (ch, sample);

            dry /= static_cast<float> (channels);

            const auto opal = parameters.opal;
            auto processed = mono[sample] + verb[sample] * (0.66f * opal);

            const auto absProcessed = std::abs (processed);
            const auto desiredGain = absProcessed > protectionThreshold
                                   ? protectionThreshold / absProcessed
                                   : 1.0f;

            if (desiredGain < safetyGain)
                safetyGain = desiredGain;
            else
                safetyGain += (1.0f - safetyGain) * releaseCoeff;

            processed *= safetyGain;

            const auto reductionDb = -juce::Decibels::gainToDecibels (safetyGain, -60.0f);
            blockMaxReduction = juce::jmax (blockMaxReduction, reductionDb);

            if (parameters.antiPhase)
                processed = -processed;

            const auto mix = mixSmoothed.getNextValue();
            const auto output = dry * (1.0f - mix) + processed * mix;

            for (int ch = 0; ch < channels; ++ch)
                buffer.setSample (ch, sample, output);

            energy += static_cast<double> (processed) * static_cast<double> (processed);
        }

        lastGainReductionDb.store (blockMaxReduction);

        const auto rms = std::sqrt (energy / static_cast<double> (samples));
        lastEnergyDb.store (
            juce::Decibels::gainToDecibels (static_cast<float> (rms), -100.0f));
    }

    float getLastEnergyDb() const noexcept
    {
        return lastEnergyDb.load();
    }

    float getGainReductionDb() const noexcept
    {
        return lastGainReductionDb.load();
    }

private:
    void updateFilterFrequencies (float selectedHz)
    {
        currentFrequencyHz = juce::jmax (20.0f, selectedHz);
        const auto octaveHz = juce::jmin (
            static_cast<float> (sampleRate * 0.44),
            currentFrequencyHz * 2.0f);

        // Three cascaded band-pass stages make the spiritual centre intentionally tight.
        // This is a narrow resonant circuit, not a broad EQ bell.
        for (size_t i = 0; i < targetFilters.size(); ++i)
        {
            auto& filter = targetFilters[i];
            filter.setCutoffFrequency (currentFrequencyHz);
            filter.setResonance (1.18f + 0.08f * static_cast<float> (i));
            filter.reset();
        }

        for (size_t i = 0; i < octaveFilters.size(); ++i)
        {
            auto& filter = octaveFilters[i];
            filter.setCutoffFrequency (octaveHz);
            filter.setResonance (1.02f + 0.06f * static_cast<float> (i));
            filter.reset();
        }
    }

    void updateReverb()
    {
        juce::dsp::Reverb::Parameters rp;
        rp.roomSize = 0.52f;
        rp.damping = 0.62f;
        rp.wetLevel = 1.0f;
        rp.dryLevel = 0.0f;
        rp.width = 0.0f; // mono chamber
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
    }

    double sampleRate = 44100.0;
    float currentFrequencyHz = 528.0f;
    Parameters parameters;

    std::array<juce::dsp::StateVariableTPTFilter<float>, 3> targetFilters;
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> octaveFilters;

    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> monoBuffer;
    juce::AudioBuffer<float> reverbBuffer;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> boostSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> opalSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> frequencySmoothed;

    float safetyGain = 1.0f;
    double humPhase = 0.0;

    std::atomic<float> lastGainReductionDb { 0.0f };
    std::atomic<float> lastEnergyDb { -100.0f };
};

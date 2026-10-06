#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

class OpalEngine
{
public:
    static constexpr int harmonicCount = 5;

    struct Parameters
    {
        float frequencyHz = 528.0f;
        float boostDb = 3.0f;
        float harmonics = 0.35f; // 0..1
        float space = 0.25f;     // 0..1
        float width = 1.0f;      // 0..2
        float field = 0.125f;    // 0..1
        float mix = 0.50f;       // 0..1
    };

    void prepare (double newSampleRate, int maximumBlockSize, int channels)
    {
        sampleRate = newSampleRate;
        preparedChannels = juce::jlimit (1, 2, channels);

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32> (maximumBlockSize);
        spec.numChannels = static_cast<juce::uint32> (preparedChannels);

        for (auto& filter : harmonicFilters)
        {
            filter.prepare (spec);
            filter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
            filter.setResonance (1.35f);
        }

        reverb.prepare (spec);
        reverb.reset();

        originalTargetBuffer.setSize (preparedChannels, maximumBlockSize, false, false, true);
        controlledTargetBuffer.setSize (preparedChannels, maximumBlockSize, false, false, true);
        harmonicBuffer.setSize (preparedChannels, maximumBlockSize, false, false, true);
        fieldBuffer.setSize (preparedChannels, maximumBlockSize, false, false, true);
        reverbBuffer.setSize (preparedChannels, maximumBlockSize, false, false, true);
        widthScratch.resize (static_cast<size_t> (maximumBlockSize), 1.0f);

        boostSmoothed.reset (sampleRate, 0.030);
        harmonicSmoothed.reset (sampleRate, 0.030);
        mixSmoothed.reset (sampleRate, 0.030);
        widthSmoothed.reset (sampleRate, 0.035);
        fieldSmoothed.reset (sampleRate, 0.050);
        frequencySmoothed.reset (sampleRate, 0.040);

        boostSmoothed.setCurrentAndTargetValue (0.0f);
        harmonicSmoothed.setCurrentAndTargetValue (0.35f);
        mixSmoothed.setCurrentAndTargetValue (0.50f);
        widthSmoothed.setCurrentAndTargetValue (1.0f);
        fieldSmoothed.setCurrentAndTargetValue (0.0f);
        frequencySmoothed.setCurrentAndTargetValue (528.0f);

        humPhase = 0.0;
        updateFilterFrequencies (528.0f);
    }

    void reset()
    {
        for (auto& filter : harmonicFilters)
            filter.reset();

        reverb.reset();
        humPhase = 0.0;
        lastEnergyDb.store (-100.0f);
    }

    void setParameters (const Parameters& p)
    {
        parameters = p;

        const auto boostLinear = juce::Decibels::decibelsToGain (juce::jlimit (0.0f, 15.0f, p.boostDb));
        boostSmoothed.setTargetValue (juce::jmax (0.0f, boostLinear - 1.0f));
        harmonicSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, p.harmonics));
        mixSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, p.mix));
        widthSmoothed.setTargetValue (juce::jlimit (0.0f, 2.0f, p.width));
        frequencySmoothed.setTargetValue (juce::jmax (20.0f, p.frequencyHz));

        const auto fieldAmount = juce::jlimit (0.0f, 1.0f, p.field);
        const auto fieldGain = fieldAmount <= 0.0001f
                             ? 0.0f
                             : juce::Decibels::decibelsToGain (juce::jmap (fieldAmount, 0.0f, 1.0f, -60.0f, -28.0f));
        fieldSmoothed.setTargetValue (fieldGain);

        if (std::abs (p.frequencyHz - currentFrequencyHz) > 0.01f)
            updateFilterFrequencies (p.frequencyHz);

        juce::dsp::Reverb::Parameters rp;
        const auto space = juce::jlimit (0.0f, 1.0f, p.space);
        rp.roomSize = 0.24f + (0.66f * space);
        rp.damping = 0.52f;
        rp.wetLevel = 1.0f;
        rp.dryLevel = 0.0f;
        rp.width = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const auto channels = juce::jmin (preparedChannels, buffer.getNumChannels());
        const auto samples = buffer.getNumSamples();

        if (channels <= 0 || samples <= 0)
            return;

        jassert (samples <= originalTargetBuffer.getNumSamples());
        jassert (samples <= static_cast<int> (widthScratch.size()));

        originalTargetBuffer.clear();
        controlledTargetBuffer.clear();
        harmonicBuffer.clear();
        fieldBuffer.clear();
        reverbBuffer.clear();

        // Extract the selected centre and its already-present harmonic family.
        for (int sample = 0; sample < samples; ++sample)
        {
            for (int ch = 0; ch < channels; ++ch)
            {
                const auto input = buffer.getSample (ch, sample);

                if (harmonicActive[0])
                {
                    const auto target = harmonicFilters[0].processSample (ch, input);
                    originalTargetBuffer.setSample (ch, sample, target);
                }

                float harmonicFamily = 0.0f;

                for (int h = 1; h < harmonicCount; ++h)
                {
                    if (! harmonicActive[static_cast<size_t> (h)])
                        continue;

                    const auto band = harmonicFilters[static_cast<size_t> (h)].processSample (ch, input);
                    const auto harmonicNumber = static_cast<float> (h + 1);
                    harmonicFamily += band * (0.72f / harmonicNumber);
                }

                harmonicBuffer.setSample (ch, sample, harmonicFamily);
            }
        }

        for (int ch = 0; ch < channels; ++ch)
            controlledTargetBuffer.copyFrom (ch, 0, originalTargetBuffer, ch, 0, samples);

        for (int sample = 0; sample < samples; ++sample)
            widthScratch[static_cast<size_t> (sample)] = widthSmoothed.getNextValue();

        // WIDTH now controls the actual selected band, not only the added wet layer.
        // At 0%, the selected frequency becomes mono/centred. At 100%, its original
        // stereo image is preserved. Above 100%, existing side information expands.
        if (channels == 2)
        {
            auto* left = controlledTargetBuffer.getWritePointer (0);
            auto* right = controlledTargetBuffer.getWritePointer (1);

            for (int sample = 0; sample < samples; ++sample)
            {
                const auto width = widthScratch[static_cast<size_t> (sample)];
                const auto mid = 0.5f * (left[sample] + right[sample]);
                const auto side = 0.5f * (left[sample] - right[sample]) * width;

                left[sample] = mid + side;
                right[sample] = mid - side;
            }
        }

        // Build OPAL's enhancement field from the controlled target band,
        // source-derived harmonics, and the optional low-level FIELD tone.
        for (int sample = 0; sample < samples; ++sample)
        {
            const auto boost = boostSmoothed.getNextValue();
            const auto harmonicAmount = harmonicSmoothed.getNextValue();
            const auto fieldGain = fieldSmoothed.getNextValue();
            const auto frequencyHz = frequencySmoothed.getNextValue();

            const auto phaseAdvance = juce::MathConstants<double>::twoPi
                                    * static_cast<double> (frequencyHz)
                                    / sampleRate;

            const auto leftTone = static_cast<float> (std::sin (humPhase)
                                + 0.18 * std::sin (humPhase * 2.0));
            const auto rightPhase = humPhase + 0.16;
            const auto rightTone = static_cast<float> (std::sin (rightPhase)
                                 + 0.18 * std::sin (rightPhase * 2.0));

            for (int ch = 0; ch < channels; ++ch)
            {
                const auto target = controlledTargetBuffer.getSample (ch, sample);
                const auto harmonics = harmonicBuffer.getSample (ch, sample);
                const auto tone = channels == 1 ? leftTone
                                                : (ch == 0 ? leftTone : rightTone);

                const auto field = target * boost
                                 + harmonics * harmonicAmount
                                 + tone * fieldGain;

                fieldBuffer.setSample (ch, sample, field);
            }

            humPhase += phaseAdvance;

            if (humPhase >= juce::MathConstants<double>::twoPi)
                humPhase -= juce::MathConstants<double>::twoPi;
        }

        for (int ch = 0; ch < channels; ++ch)
            reverbBuffer.copyFrom (ch, 0, fieldBuffer, ch, 0, samples);

        {
            juce::dsp::AudioBlock<float> block (reverbBuffer);
            auto activeBlock = block.getSubBlock (0, static_cast<size_t> (samples));
            juce::dsp::ProcessContextReplacing<float> context (activeBlock);
            reverb.process (context);
        }

        const auto space = juce::jlimit (0.0f, 1.0f, parameters.space);

        for (int ch = 0; ch < channels; ++ch)
        {
            auto* direct = fieldBuffer.getWritePointer (ch);
            const auto* reverberant = reverbBuffer.getReadPointer (ch);

            for (int sample = 0; sample < samples; ++sample)
                direct[sample] = direct[sample] * (1.0f - 0.52f * space)
                               + reverberant[sample] * (0.76f * space);
        }

        // Apply the same WIDTH law to the whole resonance field. Reverb cannot
        // secretly reintroduce stereo spread when WIDTH is at zero.
        if (channels == 2)
        {
            auto* left = fieldBuffer.getWritePointer (0);
            auto* right = fieldBuffer.getWritePointer (1);

            for (int sample = 0; sample < samples; ++sample)
            {
                const auto width = widthScratch[static_cast<size_t> (sample)];
                const auto mid = 0.5f * (left[sample] + right[sample]);
                const auto side = 0.5f * (left[sample] - right[sample]) * width;

                left[sample] = mid + side;
                right[sample] = mid - side;
            }
        }

        double energy = 0.0;
        int energySamples = 0;

        for (int sample = 0; sample < samples; ++sample)
        {
            const auto mix = mixSmoothed.getNextValue();

            for (int ch = 0; ch < channels; ++ch)
            {
                const auto originalTarget = originalTargetBuffer.getSample (ch, sample);
                const auto controlledTarget = controlledTargetBuffer.getSample (ch, sample);
                const auto widthReplacement = controlledTarget - originalTarget;
                const auto wet = fieldBuffer.getSample (ch, sample);

                // Frequency-selective replacement makes WIDTH meaningful while
                // leaving the rest of the dry signal untouched.
                buffer.addSample (ch, sample, (widthReplacement + wet) * mix);

                const auto measured = widthReplacement + wet;
                energy += static_cast<double> (measured) * static_cast<double> (measured);
                ++energySamples;
            }
        }

        const auto rms = energySamples > 0
                       ? std::sqrt (energy / static_cast<double> (energySamples))
                       : 0.0;

        lastEnergyDb.store (juce::Decibels::gainToDecibels (static_cast<float> (rms), -100.0f));
    }

    float getLastEnergyDb() const noexcept
    {
        return lastEnergyDb.load();
    }

private:
    void updateFilterFrequencies (float selectedHz)
    {
        currentFrequencyHz = juce::jmax (20.0f, selectedHz);
        const auto safeTop = static_cast<float> (sampleRate * 0.45);

        for (int h = 0; h < harmonicCount; ++h)
        {
            const auto harmonicHz = currentFrequencyHz * static_cast<float> (h + 1);
            const auto active = harmonicHz < safeTop;

            harmonicActive[static_cast<size_t> (h)] = active;

            if (active)
            {
                auto& filter = harmonicFilters[static_cast<size_t> (h)];
                filter.setCutoffFrequency (harmonicHz);

                const auto resonance = juce::jmax (0.82f, 1.35f - (0.12f * static_cast<float> (h)));
                filter.setResonance (resonance);
                filter.reset();
            }
        }
    }

    double sampleRate = 44100.0;
    int preparedChannels = 2;
    float currentFrequencyHz = 528.0f;
    Parameters parameters;

    std::array<juce::dsp::StateVariableTPTFilter<float>, harmonicCount> harmonicFilters;
    std::array<bool, harmonicCount> harmonicActive { true, true, true, true, true };

    juce::dsp::Reverb reverb;

    juce::AudioBuffer<float> originalTargetBuffer;
    juce::AudioBuffer<float> controlledTargetBuffer;
    juce::AudioBuffer<float> harmonicBuffer;
    juce::AudioBuffer<float> fieldBuffer;
    juce::AudioBuffer<float> reverbBuffer;
    std::vector<float> widthScratch;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> boostSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> harmonicSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> widthSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> fieldSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> frequencySmoothed;

    double humPhase = 0.0;
    std::atomic<float> lastEnergyDb { -100.0f };
};

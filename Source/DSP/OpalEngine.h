#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

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
            filter.setResonance (7.5f);
        }

        reverb.prepare (spec);
        reverb.reset();

        fieldBuffer.setSize (preparedChannels, maximumBlockSize, false, false, true);
        reverbBuffer.setSize (preparedChannels, maximumBlockSize, false, false, true);

        boostSmoothed.reset (sampleRate, 0.030);
        harmonicSmoothed.reset (sampleRate, 0.030);
        mixSmoothed.reset (sampleRate, 0.030);
        widthSmoothed.reset (sampleRate, 0.030);

        boostSmoothed.setCurrentAndTargetValue (0.0f);
        harmonicSmoothed.setCurrentAndTargetValue (0.35f);
        mixSmoothed.setCurrentAndTargetValue (0.50f);
        widthSmoothed.setCurrentAndTargetValue (1.0f);

        updateFilterFrequencies (528.0f);
    }

    void reset()
    {
        for (auto& filter : harmonicFilters)
            filter.reset();

        reverb.reset();
        lastEnergyDb.store (-100.0f);
    }

    void setParameters (const Parameters& p)
    {
        parameters = p;

        const auto boostLinear = juce::Decibels::decibelsToGain (juce::jlimit (0.0f, 12.0f, p.boostDb));
        boostSmoothed.setTargetValue (juce::jmax (0.0f, boostLinear - 1.0f));
        harmonicSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, p.harmonics));
        mixSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, p.mix));
        widthSmoothed.setTargetValue (juce::jlimit (0.0f, 2.0f, p.width));

        if (std::abs (p.frequencyHz - currentFrequencyHz) > 0.01f)
            updateFilterFrequencies (p.frequencyHz);

        juce::dsp::Reverb::Parameters rp;
        const auto space = juce::jlimit (0.0f, 1.0f, p.space);
        rp.roomSize = 0.28f + (0.62f * space);
        rp.damping = 0.48f;
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

        jassert (samples <= fieldBuffer.getNumSamples());

        fieldBuffer.clear();
        reverbBuffer.clear();

        // Input-reactive law:
        // We never synthesize a free-running tone here. Each contribution comes
        // from content already present near the selected center or its harmonics.
        for (int sample = 0; sample < samples; ++sample)
        {
            const auto boost = boostSmoothed.getNextValue();
            const auto harmonicAmount = harmonicSmoothed.getNextValue();

            for (int ch = 0; ch < channels; ++ch)
            {
                const auto input = buffer.getSample (ch, sample);
                float field = 0.0f;

                for (int h = 0; h < harmonicCount; ++h)
                {
                    if (! harmonicActive[static_cast<size_t> (h)])
                        continue;

                    const auto band = harmonicFilters[static_cast<size_t> (h)].processSample (ch, input);

                    if (h == 0)
                    {
                        field += band * boost;
                    }
                    else
                    {
                        const auto harmonicNumber = static_cast<float> (h + 1);
                        const auto weight = harmonicAmount * (0.72f / harmonicNumber);
                        field += band * weight;
                    }
                }

                fieldBuffer.setSample (ch, sample, field);
            }
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

        // Blend the direct resonance field with its tuned/reverberant tail.
        for (int ch = 0; ch < channels; ++ch)
        {
            auto* direct = fieldBuffer.getWritePointer (ch);
            const auto* reverberant = reverbBuffer.getReadPointer (ch);

            for (int sample = 0; sample < samples; ++sample)
                direct[sample] = direct[sample] * (1.0f - 0.55f * space)
                               + reverberant[sample] * (0.80f * space);
        }

        // Widen only the processed frequency field. The dry input is untouched.
        if (channels == 2)
        {
            auto* left = fieldBuffer.getWritePointer (0);
            auto* right = fieldBuffer.getWritePointer (1);

            for (int sample = 0; sample < samples; ++sample)
            {
                const auto width = widthSmoothed.getNextValue();
                const auto mid = 0.5f * (left[sample] + right[sample]);
                const auto side = 0.5f * (left[sample] - right[sample]) * width;

                left[sample] = mid + side;
                right[sample] = mid - side;
            }
        }
        else
        {
            widthSmoothed.skip (samples);
        }

        double energy = 0.0;
        int energySamples = 0;

        for (int ch = 0; ch < channels; ++ch)
        {
            const auto* field = fieldBuffer.getReadPointer (ch);
            auto* output = buffer.getWritePointer (ch);

            for (int sample = 0; sample < samples; ++sample)
            {
                const auto mix = mixSmoothed.getNextValue();
                const auto wet = field[sample];

                output[sample] += wet * mix;
                energy += static_cast<double> (wet) * static_cast<double> (wet);
                ++energySamples;
            }
        }

        if (channels > 1)
        {
            // The mix smoother was advanced once per channel above. Keep its
            // endpoint stable for the next block rather than doubling slew time.
            mixSmoothed.setCurrentAndTargetValue (parameters.mix);
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

                // Slightly broader upper harmonics keep the result musical and
                // reduce the chance of a whistle-like narrow resonance.
                const auto q = juce::jmax (3.5f, 7.5f - (0.65f * static_cast<float> (h)));
                filter.setResonance (q);
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
    juce::AudioBuffer<float> fieldBuffer;
    juce::AudioBuffer<float> reverbBuffer;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> boostSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> harmonicSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> widthSmoothed;

    std::atomic<float> lastEnergyDb { -100.0f };
};

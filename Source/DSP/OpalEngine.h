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
        boostDeltaBuffer.setSize (2, maximumBlockSize, false, false, true);
        reverbBuffer.setSize (2, maximumBlockSize, false, false, true);

        opalSmoothed.reset (sampleRate, 0.040);
        mixSmoothed.reset (sampleRate, 0.030);

        opalSmoothed.setCurrentAndTargetValue (0.0f);
        mixSmoothed.setCurrentAndTargetValue (0.50f);

        currentFrequencyHz = 528.0f;
        currentBoostDb = 0.0f;

        updateFrequencyNetwork (currentFrequencyHz);
        updateBoostFilters (currentFrequencyHz, currentBoostDb);
        updateCrystalReverb();
    }

    void reset()
    {
        for (auto& filter : selectedFilters)
            filter.reset();

        for (auto& filter : harmonicFilters)
            filter.reset();

        for (auto& filter : boostFilters)
            filter.reset();

        crystalReverb.reset();
        lastEnergyDb.store (-100.0f);
    }

    void setParameters (const Parameters& p)
    {
        parameters = p;

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

        const auto boostDb =
            juce::jlimit (0.0f, 15.0f, p.boostDb);

        const auto frequencyChanged =
            std::abs (selectedHz - currentFrequencyHz) > 0.01f;

        const auto boostChanged =
            std::abs (boostDb - currentBoostDb) > 0.0001f;

        if (frequencyChanged)
        {
            currentFrequencyHz = selectedHz;
            updateFrequencyNetwork (currentFrequencyHz);
        }

        if (frequencyChanged || boostChanged)
        {
            currentBoostDb = boostDb;
            updateBoostFilters (currentFrequencyHz, currentBoostDb);
        }
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const auto channels = buffer.getNumChannels();
        const auto samples = buffer.getNumSamples();

        if (channels <= 0 || samples <= 0)
            return;

        jassert (samples <= selectedBuffer.getNumSamples());
        jassert (samples <= boostDeltaBuffer.getNumSamples());

        // Hard neutral-path invariants.
        if (parameters.mix <= 0.0001f
            || (parameters.boostDb <= 0.0001f
                && parameters.opal <= 0.0001f))
        {
            if (parameters.opal <= 0.0001f)
                crystalReverb.reset();

            lastEnergyDb.store (-100.0f);
            return;
        }

        selectedBuffer.clear();
        boostDeltaBuffer.clear();
        reverbBuffer.clear();

        auto* selected =
            selectedBuffer.getWritePointer (0);

        // BOOST: a true per-channel Q=100 peaking EQ.
        // The original buffer remains untouched until the final delta-add stage.
        for (int ch = 0; ch < channels; ++ch)
        {
            auto* delta =
                boostDeltaBuffer.getWritePointer (ch);

            for (int sample = 0; sample < samples; ++sample)
            {
                const auto dry =
                    buffer.getSample (ch, sample);

                const auto boosted =
                    currentBoostDb > 0.0001f
                        ? boostFilters[static_cast<size_t> (ch)].processSample (dry)
                        : dry;

                delta[sample] =
                    boosted - dry;
            }
        }

        // OPAL analysis/extraction uses a mono copy only.
        // It never replaces or collapses the dry stereo source.
        for (int sample = 0; sample < samples; ++sample)
        {
            float mono = 0.0f;

            for (int ch = 0; ch < channels; ++ch)
                mono += buffer.getSample (ch, sample);

            mono /= static_cast<float> (channels);

            float narrowBand = mono;

            for (auto& filter : selectedFilters)
                narrowBand = filter.processSample (narrowBand);

            selected[sample] = narrowBand;
        }

        auto* reverbLeft =
            reverbBuffer.getWritePointer (0);

        auto* reverbRight =
            reverbBuffer.getWritePointer (1);

        // Build OPAL only from the selected frequency.
        for (int sample = 0; sample < samples; ++sample)
        {
            const auto sourceBand =
                selected[sample];

            const auto opal =
                opalSmoothed.getNextValue();

            float tubeHarmonics = 0.0f;

            if (opal > 0.0001f)
            {
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
                        harmonic * harmonicWeights[h];
                }
            }

            const auto resonance =
                sourceBand * (0.18f * opal);

            const auto harmonics =
                tubeHarmonics * (0.55f * opal);

            const auto opalCore =
                resonance + harmonics;

            selected[sample] = opalCore;

            // Only OPAL feeds the stereo chamber.
            reverbLeft[sample] = opalCore;
            reverbRight[sample] = opalCore;
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

        double wetEnergy = 0.0;

        // MIX scales only the processed delta.
        for (int sample = 0; sample < samples; ++sample)
        {
            const auto mix =
                mixSmoothed.getNextValue();

            const auto opalCore =
                selected[sample];

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
                const auto boostDelta =
                    boostDeltaBuffer.getSample (ch, sample);

                float tail = 0.0f;

                if (channels == 1)
                    tail = 0.5f * (leftTail + rightTail);
                else
                    tail = ch == 0 ? leftTail : rightTail;

                const auto delta =
                    (boostDelta + opalCore + tail) * mix;

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
    void updateBoostFilters (float selectedHz, float boostDb)
    {
        constexpr float boostQ = 100.0f;

        const auto gain =
            juce::Decibels::decibelsToGain (
                juce::jlimit (0.0f, 15.0f, boostDb));

        auto coeffs =
            juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                sampleRate,
                static_cast<double> (selectedHz),
                boostQ,
                gain);

        for (auto& filter : boostFilters)
        {
            filter.coefficients = coeffs;
            filter.reset();
        }
    }

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
        rp.width = 0.38f;
        rp.freezeMode = 0.0f;

        crystalReverb.setParameters (rp);
    }

    double sampleRate = 44100.0;
    float currentFrequencyHz = 528.0f;
    float currentBoostDb = 0.0f;

    Parameters parameters;

    std::array<juce::dsp::IIR::Filter<float>, 2> boostFilters;
    std::array<juce::dsp::IIR::Filter<float>, 4> selectedFilters;

    std::array<juce::dsp::IIR::Filter<float>, 3> harmonicFilters;
    std::array<bool, 3> harmonicActive { true, true, true };

    const std::array<float, 3> harmonicWeights {
        0.62f, 0.30f, 0.14f
    };

    juce::dsp::Reverb crystalReverb;

    juce::AudioBuffer<float> selectedBuffer;
    juce::AudioBuffer<float> boostDeltaBuffer;
    juce::AudioBuffer<float> reverbBuffer;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> opalSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed;

    std::atomic<float> lastEnergyDb { -100.0f };
};

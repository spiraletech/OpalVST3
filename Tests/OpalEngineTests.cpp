#include <JuceHeader.h>
#include "../Source/DSP/OpalEngine.h"
#include <cmath>
#include <iostream>

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 256;

    bool nearlyEqual (float a, float b, float tolerance)
    {
        return std::abs (a - b) <= tolerance;
    }

    bool testBypassWhenControlsOff()
    {
        OpalEngine engine;
        engine.prepare (sampleRate, blockSize, 2);

        OpalEngine::Parameters p;
        p.frequencyHz = 528.0f;
        p.boostDb = 0.0f;
        p.opal = 0.0f;
        p.mix = 1.0f;
        engine.setParameters (p);

        juce::AudioBuffer<float> buffer (2, blockSize);

        for (int n = 0; n < blockSize; ++n)
        {
            buffer.setSample (0, n, 0.17f * std::sin (0.013f * static_cast<float> (n)));
            buffer.setSample (1, n, 0.11f * std::cos (0.021f * static_cast<float> (n)));
        }

        juce::AudioBuffer<float> original;
        original.makeCopyOf (buffer);

        engine.process (buffer);

        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < blockSize; ++n)
                if (buffer.getSample (ch, n) != original.getSample (ch, n))
                    return false;

        return true;
    }

    bool testMixZeroIsBypass()
    {
        OpalEngine engine;
        engine.prepare (sampleRate, blockSize, 2);

        OpalEngine::Parameters p;
        p.frequencyHz = 528.0f;
        p.boostDb = 15.0f;
        p.opal = 1.0f;
        p.mix = 0.0f;
        engine.setParameters (p);

        juce::AudioBuffer<float> buffer (2, blockSize);

        for (int n = 0; n < blockSize; ++n)
        {
            buffer.setSample (0, n, 0.14f * std::sin (0.017f * static_cast<float> (n)));
            buffer.setSample (1, n, 0.08f * std::cos (0.027f * static_cast<float> (n)));
        }

        juce::AudioBuffer<float> original;
        original.makeCopyOf (buffer);

        engine.process (buffer);

        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < blockSize; ++n)
                if (buffer.getSample (ch, n) != original.getSample (ch, n))
                    return false;

        return true;
    }

    float measureBoostGainDb (float selectedFrequencyHz,
                              float inputFrequencyHz,
                              float boostDb)
    {
        OpalEngine engine;
        engine.prepare (sampleRate, blockSize, 2);

        OpalEngine::Parameters p;
        p.frequencyHz = selectedFrequencyHz;
        p.boostDb = boostDb;
        p.opal = 0.0f;
        p.mix = 1.0f;
        engine.setParameters (p);

        constexpr float amplitude = 0.05f;
        constexpr int totalSamples = static_cast<int> (sampleRate * 4.0);
        constexpr int ignoreSamples = static_cast<int> (sampleRate * 1.0);

        double phase = 0.0;
        const auto phaseStep =
            juce::MathConstants<double>::twoPi
            * static_cast<double> (inputFrequencyHz)
            / sampleRate;

        double inputEnergy = 0.0;
        double outputEnergy = 0.0;
        int measuredSamples = 0;

        juce::AudioBuffer<float> buffer (2, blockSize);

        for (int base = 0; base < totalSamples; base += blockSize)
        {
            const auto count =
                juce::jmin (blockSize, totalSamples - base);

            buffer.clear();

            for (int n = 0; n < count; ++n)
            {
                const auto value =
                    amplitude
                    * static_cast<float> (std::sin (phase));

                phase += phaseStep;

                if (phase >= juce::MathConstants<double>::twoPi)
                    phase -= juce::MathConstants<double>::twoPi;

                buffer.setSample (0, n, value);
                buffer.setSample (1, n, value);
            }

            if (count < blockSize)
                buffer.clear (count, blockSize - count);

            juce::AudioBuffer<float> input;
            input.makeCopyOf (buffer);

            engine.process (buffer);

            for (int n = 0; n < count; ++n)
            {
                const auto absoluteSample = base + n;

                if (absoluteSample < ignoreSamples)
                    continue;

                const auto in = input.getSample (0, n);
                const auto out = buffer.getSample (0, n);

                inputEnergy += static_cast<double> (in) * static_cast<double> (in);
                outputEnergy += static_cast<double> (out) * static_cast<double> (out);
                ++measuredSamples;
            }
        }

        const auto inputRms =
            std::sqrt (inputEnergy / static_cast<double> (measuredSamples));

        const auto outputRms =
            std::sqrt (outputEnergy / static_cast<double> (measuredSamples));

        return 20.0f
             * std::log10 (
                   static_cast<float> (outputRms / inputRms));
    }
}

int main()
{
    if (! testBypassWhenControlsOff())
    {
        std::cerr << "FAIL: BOOST=0 OPAL=0 was not bit-identical bypass.\n";
        return 1;
    }

    if (! testMixZeroIsBypass())
    {
        std::cerr << "FAIL: MIX=0 was not bit-identical bypass.\n";
        return 2;
    }

    const auto measured15 =
        measureBoostGainDb (528.0f, 528.0f, 15.0f);

    std::cout
        << "Measured 528 Hz gain at +15 dB setting: "
        << measured15
        << " dB\n";

    if (! nearlyEqual (measured15, 15.0f, 0.20f))
    {
        std::cerr
            << "FAIL: +15 dB calibration outside +/-0.20 dB tolerance.\n";
        return 3;
    }

    const auto measured6 =
        measureBoostGainDb (432.0f, 432.0f, 6.0f);

    std::cout
        << "Measured 432 Hz gain at +6 dB setting: "
        << measured6
        << " dB\n";

    if (! nearlyEqual (measured6, 6.0f, 0.20f))
    {
        std::cerr
            << "FAIL: +6 dB calibration outside +/-0.20 dB tolerance.\n";
        return 4;
    }

    const auto offCenterGain =
        measureBoostGainDb (444.0f, 444.0f * 1.03f, 15.0f);

    std::cout
        << "Measured gain 3% away from 444 Hz at +15 dB setting: "
        << offCenterGain
        << " dB\n";

    if (offCenterGain > 0.50f)
    {
        std::cerr
            << "FAIL: detector is too wide; 3%-off tone changed by more than 0.50 dB.\n";
        return 5;
    }

    std::cout << "PASS: OPAL DSP calibration tests.\n";
    return 0;
}

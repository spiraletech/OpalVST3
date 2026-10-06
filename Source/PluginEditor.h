#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "OpalFrequencyData.h"
#include "UI/OpalLookAndFeel.h"

class OpalAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit OpalAudioProcessorEditor (OpalAudioProcessor&);
    ~OpalAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class OpalStone final : public juce::Component
    {
    public:
        void setEnergyDb (float db)
        {
            energy = juce::jlimit (0.0f, 1.0f, juce::jmap (db, -66.0f, -10.0f, 0.0f, 1.0f));
        }

        void setOpalAmount (float amount)
        {
            activation = juce::jlimit (0.0f, 1.0f, amount);
        }

        void setPhase (float p)
        {
            phase = p;
            repaint();
        }

        void paint (juce::Graphics&) override;

    private:
        float phase = 0.0f;
        float energy = 0.0f;
        float activation = 0.0f;
    };

    void timerCallback() override;
    void configureKnob (juce::Slider&, juce::Label&, const juce::String&);
    void updateFrequencyInfo();

    OpalAudioProcessor& processor;
    OpalLookAndFeel lookAndFeel;

    juce::Slider frequencyKnob;
    juce::Slider boostKnob;
    juce::Slider opalKnob;
    juce::Slider mixKnob;

    juce::Label frequencyLabel;
    juce::Label boostLabel;
    juce::Label opalLabel;
    juce::Label mixLabel;
    juce::Label titleLabel;
    juce::Label frequencyInfoLabel;

    OpalStone opalStone;

    std::vector<std::unique_ptr<juce::TextButton>> frequencyButtons;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<SliderAttachment> frequencyAttachment;
    std::unique_ptr<SliderAttachment> boostAttachment;
    std::unique_ptr<SliderAttachment> opalAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;

    float animationPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpalAudioProcessorEditor)
};

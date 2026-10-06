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
            energy = juce::jlimit (0.0f, 1.0f, juce::jmap (db, -60.0f, -6.0f, 0.0f, 1.0f));
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
    };

    class EnergyMeter final : public juce::Component
    {
    public:
        void setLevelDb (float db)
        {
            levelDb = juce::jlimit (-60.0f, 0.0f, db);
            repaint();
        }

        void paint (juce::Graphics&) override;

    private:
        float levelDb = -60.0f;
    };

    void timerCallback() override;
    void configureKnob (juce::Slider& slider, juce::Label& label, const juce::String& name);
    void updateFrequencyInfo();

    OpalAudioProcessor& processor;
    OpalLookAndFeel lookAndFeel;

    juce::Slider frequencyKnob;
    juce::Slider boostKnob;
    juce::Slider harmonicsKnob;
    juce::Slider spaceKnob;
    juce::Slider widthKnob;
    juce::Slider fieldKnob;
    juce::Slider mixKnob;

    juce::Label frequencyLabel;
    juce::Label boostLabel;
    juce::Label harmonicsLabel;
    juce::Label spaceLabel;
    juce::Label widthLabel;
    juce::Label fieldLabel;
    juce::Label mixLabel;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label frequencyInfoLabel;
    juce::Label meterLabel;

    OpalStone opalStone;
    EnergyMeter energyMeter;

    std::vector<std::unique_ptr<juce::TextButton>> frequencyButtons;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> frequencyAttachment;
    std::unique_ptr<SliderAttachment> boostAttachment;
    std::unique_ptr<SliderAttachment> harmonicsAttachment;
    std::unique_ptr<SliderAttachment> spaceAttachment;
    std::unique_ptr<SliderAttachment> widthAttachment;
    std::unique_ptr<SliderAttachment> fieldAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;

    float animationPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpalAudioProcessorEditor)
};

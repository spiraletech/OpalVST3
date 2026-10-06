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

    class GRMeter final : public juce::Component
    {
    public:
        void setReductionDb (float db)
        {
            targetReduction = juce::jlimit (0.0f, 24.0f, db);
        }

        void tick()
        {
            displayedReduction += (targetReduction - displayedReduction) * 0.24f;
            repaint();
        }

        void paint (juce::Graphics&) override;

    private:
        float targetReduction = 0.0f;
        float displayedReduction = 0.0f;
    };

    void timerCallback() override;
    void configureKnob (juce::Slider& slider, juce::Label& label, const juce::String& name);
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
    juce::Label subtitleLabel;
    juce::Label frequencyInfoLabel;
    juce::Label grLabel;

    juce::TextButton antiPhaseButton;

    OpalStone opalStone;
    GRMeter grMeter;

    std::vector<std::unique_ptr<juce::TextButton>> frequencyButtons;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> frequencyAttachment;
    std::unique_ptr<SliderAttachment> boostAttachment;
    std::unique_ptr<SliderAttachment> opalAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<ButtonAttachment> antiPhaseAttachment;

    float animationPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpalAudioProcessorEditor)
};

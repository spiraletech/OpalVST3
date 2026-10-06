#pragma once

#include <JuceHeader.h>

class OpalLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    OpalLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffe9edf2));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x00101014));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf01a1b20));
        setColour (juce::TooltipWindow::textColourId, juce::Colour (0xfff2f3f6));
        setColour (juce::TooltipWindow::outlineColourId, juce::Colour (0x303f4550));
    }

    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                              static_cast<float> (y),
                                              static_cast<float> (width),
                                              static_cast<float> (height)).reduced (8.0f);

        const auto diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
        bounds = bounds.withSizeKeepingCentre (diameter, diameter);

        const auto centre = bounds.getCentre();
        const auto radius = diameter * 0.5f;
        const auto angle = rotaryStartAngle
                         + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        juce::ColourGradient ringGradient (
            juce::Colour (0xff72e9ff), bounds.getX(), bounds.getY(),
            juce::Colour (0xffff8ddd), bounds.getRight(), bounds.getBottom(), false);
        ringGradient.addColour (0.48, juce::Colour (0xff8a7dff));
        ringGradient.addColour (0.75, juce::Colour (0xffa6ffc9));

        g.setGradientFill (ringGradient);
        g.fillEllipse (bounds);

        auto inner = bounds.reduced (3.0f);
        juce::ColourGradient bodyGradient (
            juce::Colour (0xff343740), inner.getX(), inner.getY(),
            juce::Colour (0xff111216), inner.getRight(), inner.getBottom(), false);
        bodyGradient.addColour (0.45, juce::Colour (0xff202229));
        g.setGradientFill (bodyGradient);
        g.fillEllipse (inner);

        g.setColour (juce::Colour (0x50101010));
        g.drawEllipse (inner.reduced (1.0f), 1.0f);

        juce::Path pointer;
        const auto pointerLength = radius * 0.54f;
        const auto pointerThickness = juce::jmax (1.5f, radius * 0.055f);
        pointer.addRoundedRectangle (-pointerThickness * 0.5f,
                                     -radius * 0.63f,
                                     pointerThickness,
                                     pointerLength,
                                     pointerThickness * 0.5f);
        pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));

        g.setColour (juce::Colour (0xfff6f8fb));
        g.fillPath (pointer);

        g.setColour (juce::Colour (0x35ffffff));
        g.fillEllipse (inner.withSizeKeepingCentre (inner.getWidth() * 0.56f,
                                                    inner.getHeight() * 0.56f)
                            .translated (-inner.getWidth() * 0.08f,
                                         -inner.getHeight() * 0.10f));
    }

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour&,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        const auto selected = button.getToggleState();

        auto base = juce::Colour (0xff17191e);

        if (selected)
            base = juce::Colour (0xff282b34);
        else if (shouldDrawButtonAsHighlighted)
            base = juce::Colour (0xff20232a);

        if (shouldDrawButtonAsDown)
            base = base.brighter (0.08f);

        g.setColour (base);
        g.fillRoundedRectangle (bounds, 7.0f);

        if (selected)
        {
            juce::ColourGradient edge (
                juce::Colour (0xff75eaff), bounds.getX(), bounds.getCentreY(),
                juce::Colour (0xffff8fdc), bounds.getRight(), bounds.getCentreY(), false);
            edge.addColour (0.5, juce::Colour (0xff9586ff));
            g.setGradientFill (edge);
            g.drawRoundedRectangle (bounds.reduced (0.5f), 7.0f, 1.6f);
        }
        else
        {
            g.setColour (juce::Colour (0x304d5562));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 7.0f, 1.0f);
        }
    }

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool,
                         bool) override
    {
        g.setColour (button.getToggleState()
                         ? juce::Colour (0xfff6f8ff)
                         : juce::Colour (0xffb9bec8));

        auto font = getTextButtonFont (button, button.getHeight());
        font.setHeight (10.5f);
        font.setBold (button.getToggleState());
        g.setFont (font);

        g.drawFittedText (button.getButtonText(),
                          button.getLocalBounds().reduced (3),
                          juce::Justification::centred,
                          2,
                          0.88f);
    }
};

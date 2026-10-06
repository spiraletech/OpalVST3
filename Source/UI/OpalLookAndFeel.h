#pragma once

#include <JuceHeader.h>

class OpalLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    OpalLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffe8ebf0));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff090a0d));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff3a404a));
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf01a1b20));
        setColour (juce::TooltipWindow::textColourId, juce::Colour (0xfff2f3f6));
        setColour (juce::TooltipWindow::outlineColourId, juce::Colour (0x603f4550));
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
                                              static_cast<float> (height)).reduced (9.0f);

        const auto diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
        bounds = bounds.withSizeKeepingCentre (diameter, diameter);

        const auto centre = bounds.getCentre();
        const auto radius = diameter * 0.5f;
        const auto angle = rotaryStartAngle
                         + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        juce::ColourGradient outerMetal (
            juce::Colour (0xff626973), bounds.getX(), bounds.getY(),
            juce::Colour (0xff15171c), bounds.getRight(), bounds.getBottom(), false);
        outerMetal.addColour (0.33, juce::Colour (0xff383d46));
        outerMetal.addColour (0.72, juce::Colour (0xff22262d));
        g.setGradientFill (outerMetal);
        g.fillEllipse (bounds);

        auto arcBounds = bounds.reduced (2.0f);
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y,
                           arcBounds.getWidth() * 0.5f,
                           arcBounds.getHeight() * 0.5f,
                           0.0f,
                           rotaryStartAngle,
                           angle,
                           true);

        juce::ColourGradient arcGradient (
            juce::Colour (0xff69e9ff), arcBounds.getX(), arcBounds.getCentreY(),
            juce::Colour (0xffff94d8), arcBounds.getRight(), arcBounds.getCentreY(), false);
        arcGradient.addColour (0.48, juce::Colour (0xff8f84ff));
        arcGradient.addColour (0.76, juce::Colour (0xffa6ffd0));
        g.setGradientFill (arcGradient);
        g.strokePath (arc, juce::PathStrokeType (3.0f,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        auto inner = bounds.reduced (7.0f);
        juce::ColourGradient bodyGradient (
            juce::Colour (0xff4a4f58), inner.getX(), inner.getY(),
            juce::Colour (0xff13151a), inner.getRight(), inner.getBottom(), false);
        bodyGradient.addColour (0.40, juce::Colour (0xff292d34));
        g.setGradientFill (bodyGradient);
        g.fillEllipse (inner);

        g.setColour (juce::Colour (0x70101012));
        g.drawEllipse (inner.reduced (0.6f), 1.0f);

        for (int tick = 0; tick <= 10; ++tick)
        {
            const auto t = static_cast<float> (tick) / 10.0f;
            const auto a = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
            const auto outerR = radius * 0.86f;
            const auto innerR = radius * 0.79f;
            const auto x1 = centre.x + std::sin (a) * innerR;
            const auto y1 = centre.y - std::cos (a) * innerR;
            const auto x2 = centre.x + std::sin (a) * outerR;
            const auto y2 = centre.y - std::cos (a) * outerR;

            g.setColour (juce::Colour (0x706e7580));
            g.drawLine (x1, y1, x2, y2, tick == 0 || tick == 10 ? 1.2f : 0.8f);
        }

        juce::Path pointer;
        const auto pointerLength = radius * 0.48f;
        const auto pointerThickness = juce::jmax (1.5f, radius * 0.05f);
        pointer.addRoundedRectangle (-pointerThickness * 0.5f,
                                     -radius * 0.58f,
                                     pointerThickness,
                                     pointerLength,
                                     pointerThickness * 0.5f);
        pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));

        g.setColour (juce::Colour (0xfff4f6f8));
        g.fillPath (pointer);

        g.setColour (juce::Colour (0x26ffffff));
        g.fillEllipse (inner.withSizeKeepingCentre (inner.getWidth() * 0.52f,
                                                    inner.getHeight() * 0.52f)
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

        juce::ColourGradient baseGradient (
            selected ? juce::Colour (0xff30343d) : juce::Colour (0xff1d2026),
            bounds.getX(), bounds.getY(),
            selected ? juce::Colour (0xff15171c) : juce::Colour (0xff101216),
            bounds.getRight(), bounds.getBottom(), false);

        if (shouldDrawButtonAsHighlighted)
            baseGradient.addColour (0.45, juce::Colour (0xff292d35));

        g.setGradientFill (baseGradient);
        g.fillRoundedRectangle (bounds, 5.0f);

        if (shouldDrawButtonAsDown)
        {
            g.setColour (juce::Colour (0x30000000));
            g.fillRoundedRectangle (bounds.reduced (1.5f), 4.0f);
        }

        if (selected)
        {
            juce::ColourGradient edge (
                juce::Colour (0xff70eaff), bounds.getX(), bounds.getCentreY(),
                juce::Colour (0xffff92d8), bounds.getRight(), bounds.getCentreY(), false);
            edge.addColour (0.5, juce::Colour (0xff9588ff));
            g.setGradientFill (edge);
            g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.5f);
        }
        else
        {
            g.setColour (juce::Colour (0x5048505d));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);
        }
    }

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool,
                         bool) override
    {
        g.setColour (button.getToggleState()
                         ? juce::Colour (0xfff7f9ff)
                         : juce::Colour (0xffb8bdc7));

        auto font = getTextButtonFont (button, button.getHeight());
        font.setHeight (9.8f);
        font.setBold (button.getToggleState());
        g.setFont (font);

        g.drawFittedText (button.getButtonText(),
                          button.getLocalBounds().reduced (3),
                          juce::Justification::centred,
                          2,
                          0.86f);
    }
};

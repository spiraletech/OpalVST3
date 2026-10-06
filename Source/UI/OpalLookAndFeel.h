#pragma once

#include <JuceHeader.h>
#include <cmath>

class OpalLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    OpalLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffecece8));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff0a0c0f));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff4d535b));

        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf016181c));
        setColour (juce::TooltipWindow::textColourId, juce::Colour (0xfff0f1ef));
        setColour (juce::TooltipWindow::outlineColourId, juce::Colour (0x70525860));
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override
    {
        return juce::Font ("Segoe UI", 9.6f, juce::Font::plain);
    }

    juce::Font getLabelFont (juce::Label& label) override
    {
        return label.getFont();
    }

    void drawRotarySlider (juce::Graphics& g,
                           int x,
                           int y,
                           int width,
                           int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider&) override
    {
        auto bounds =
            juce::Rectangle<float> (
                static_cast<float> (x),
                static_cast<float> (y),
                static_cast<float> (width),
                static_cast<float> (height))
            .reduced (10.0f);

        const auto diameter =
            juce::jmin (bounds.getWidth(), bounds.getHeight());

        bounds =
            bounds.withSizeKeepingCentre (
                diameter,
                diameter);

        const auto centre = bounds.getCentre();
        const auto radius = diameter * 0.5f;

        const auto angle =
            rotaryStartAngle
            + sliderPosProportional
            * (rotaryEndAngle - rotaryStartAngle);

        auto shadowBounds = bounds.translated (0.0f, 3.0f);

        g.setColour (juce::Colour (0x70000000));
        g.fillEllipse (shadowBounds);

        juce::ColourGradient outerMetal (
            juce::Colour (0xff8a8f94),
            bounds.getX(),
            bounds.getY(),
            juce::Colour (0xff1b1e22),
            bounds.getRight(),
            bounds.getBottom(),
            false);

        outerMetal.addColour (0.22, juce::Colour (0xff555a61));
        outerMetal.addColour (0.52, juce::Colour (0xff2b2f34));
        outerMetal.addColour (0.78, juce::Colour (0xff43474d));

        g.setGradientFill (outerMetal);
        g.fillEllipse (bounds);

        auto ringBounds = bounds.reduced (2.4f);

        juce::Path ring;
        ring.addCentredArc (
            centre.x,
            centre.y,
            ringBounds.getWidth() * 0.5f,
            ringBounds.getHeight() * 0.5f,
            0.0f,
            rotaryStartAngle,
            angle,
            true);

        juce::ColourGradient prism (
            juce::Colour (0xff70eef3),
            ringBounds.getX(),
            ringBounds.getCentreY(),
            juce::Colour (0xffef93cf),
            ringBounds.getRight(),
            ringBounds.getCentreY(),
            false);

        prism.addColour (0.34, juce::Colour (0xff9a91ff));
        prism.addColour (0.66, juce::Colour (0xff91f0c9));

        g.setGradientFill (prism);

        g.strokePath (
            ring,
            juce::PathStrokeType (
                2.8f,
                juce::PathStrokeType::curved,
                juce::PathStrokeType::rounded));

        auto inner = bounds.reduced (8.0f);

        juce::ColourGradient knobBody (
            juce::Colour (0xff575c62),
            inner.getX(),
            inner.getY(),
            juce::Colour (0xff111317),
            inner.getRight(),
            inner.getBottom(),
            false);

        knobBody.addColour (0.36, juce::Colour (0xff34383e));
        knobBody.addColour (0.72, juce::Colour (0xff20242a));

        g.setGradientFill (knobBody);
        g.fillEllipse (inner);

        g.setColour (juce::Colour (0x70101316));
        g.drawEllipse (inner.reduced (0.8f), 1.0f);

        for (int tick = 0; tick <= 10; ++tick)
        {
            const auto t =
                static_cast<float> (tick) / 10.0f;

            const auto a =
                rotaryStartAngle
                + t * (rotaryEndAngle - rotaryStartAngle);

            const auto r1 = radius * 0.79f;
            const auto r2 = radius * 0.86f;

            const auto x1 =
                centre.x + std::sin (a) * r1;

            const auto y1 =
                centre.y - std::cos (a) * r1;

            const auto x2 =
                centre.x + std::sin (a) * r2;

            const auto y2 =
                centre.y - std::cos (a) * r2;

            g.setColour (juce::Colour (0x706f757c));

            g.drawLine (
                x1,
                y1,
                x2,
                y2,
                tick == 0 || tick == 10 ? 1.1f : 0.75f);
        }

        juce::Path pointer;

        const auto pointerLength = radius * 0.46f;
        const auto pointerThickness = juce::jmax (1.5f, radius * 0.048f);

        pointer.addRoundedRectangle (
            -pointerThickness * 0.5f,
            -radius * 0.57f,
            pointerThickness,
            pointerLength,
            pointerThickness * 0.5f);

        pointer.applyTransform (
            juce::AffineTransform::rotation (angle)
                .translated (centre.x, centre.y));

        g.setColour (juce::Colour (0xfff3f4f2));
        g.fillPath (pointer);

        g.setColour (juce::Colour (0x28ffffff));

        g.fillEllipse (
            inner.withSizeKeepingCentre (
                inner.getWidth() * 0.52f,
                inner.getHeight() * 0.52f)
            .translated (
                -inner.getWidth() * 0.08f,
                -inner.getHeight() * 0.10f));
    }

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour&,
                               bool highlighted,
                               bool down) override
    {
        auto bounds =
            button.getLocalBounds()
                .toFloat()
                .reduced (1.0f);

        const auto selected =
            button.getToggleState();

        juce::Colour top =
            selected
                ? juce::Colour (0xff3a3e44)
                : juce::Colour (0xff23262b);

        juce::Colour bottom =
            selected
                ? juce::Colour (0xff171a1e)
                : juce::Colour (0xff111317);

        if (highlighted)
        {
            top = top.brighter (0.08f);
            bottom = bottom.brighter (0.05f);
        }

        juce::ColourGradient tile (
            top,
            bounds.getX(),
            bounds.getY(),
            bottom,
            bounds.getRight(),
            bounds.getBottom(),
            false);

        tile.addColour (
            0.46,
            selected
                ? juce::Colour (0xff292d32)
                : juce::Colour (0xff1a1d21));

        g.setGradientFill (tile);
        g.fillRoundedRectangle (bounds, 5.0f);

        if (down)
        {
            g.setColour (juce::Colour (0x22000000));
            g.fillRoundedRectangle (bounds.reduced (1.5f), 4.0f);
        }

        if (selected)
        {
            juce::ColourGradient edge (
                juce::Colour (0xff73edf2),
                bounds.getX(),
                bounds.getCentreY(),
                juce::Colour (0xffee96d1),
                bounds.getRight(),
                bounds.getCentreY(),
                false);

            edge.addColour (0.36, juce::Colour (0xff9b90ff));
            edge.addColour (0.68, juce::Colour (0xff92efc9));

            g.setGradientFill (edge);

            g.drawRoundedRectangle (
                bounds.reduced (0.5f),
                5.0f,
                1.4f);
        }
        else
        {
            g.setColour (juce::Colour (0x60515962));

            g.drawRoundedRectangle (
                bounds.reduced (0.5f),
                5.0f,
                0.9f);
        }
    }

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool,
                         bool) override
    {
        g.setColour (
            button.getToggleState()
                ? juce::Colour (0xfff5f5f1)
                : juce::Colour (0xffbec1c2));

        auto font =
            juce::Font (
                "Segoe UI",
                9.0f,
                button.getToggleState()
                    ? juce::Font::bold
                    : juce::Font::plain);

        g.setFont (font);

        g.drawFittedText (
            button.getButtonText(),
            button.getLocalBounds().reduced (3),
            juce::Justification::centred,
            2,
            0.86f);
    }
};

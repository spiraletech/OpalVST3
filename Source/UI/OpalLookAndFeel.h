#pragma once

#include <JuceHeader.h>
#include <cmath>

class OpalLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    OpalLookAndFeel()
    {
        setColour (
            juce::Slider::textBoxTextColourId,
            juce::Colour (0xfff2f3f1));

        setColour (
            juce::Slider::textBoxBackgroundColourId,
            juce::Colour (0xff07090c));

        setColour (
            juce::Slider::textBoxOutlineColourId,
            juce::Colour (0xff3f4650));

        setColour (
            juce::Slider::textBoxHighlightColourId,
            juce::Colour (0x505fe4e8));

        setColour (
            juce::TooltipWindow::backgroundColourId,
            juce::Colour (0xf014171b));

        setColour (
            juce::TooltipWindow::textColourId,
            juce::Colour (0xfff0f1ef));

        setColour (
            juce::TooltipWindow::outlineColourId,
            juce::Colour (0x70515861));
    }

    juce::Font getTextButtonFont (
        juce::TextButton&,
        int) override
    {
        return juce::Font (
            "Bahnschrift",
            9.7f,
            juce::Font::plain);
    }

    juce::Font getLabelFont (
        juce::Label& label) override
    {
        return label.getFont();
    }

    void drawRotarySlider (
        juce::Graphics& g,
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
            .reduced (11.0f);

        const auto diameter =
            juce::jmin (
                bounds.getWidth(),
                bounds.getHeight());

        bounds =
            bounds.withSizeKeepingCentre (
                diameter,
                diameter);

        const auto centre =
            bounds.getCentre();

        const auto radius =
            diameter * 0.5f;

        const auto angle =
            rotaryStartAngle
            + sliderPosProportional
            * (rotaryEndAngle - rotaryStartAngle);

        // Recess behind the knob.
        auto recess =
            bounds.expanded (5.0f)
                  .translated (0.0f, 1.5f);

        juce::ColourGradient recessGradient (
            juce::Colour (0xff030406),
            recess.getX(),
            recess.getY(),
            juce::Colour (0xff1a1f25),
            recess.getRight(),
            recess.getBottom(),
            false);

        g.setGradientFill (recessGradient);
        g.fillEllipse (recess);

        g.setColour (juce::Colour (0x80384049));
        g.drawEllipse (recess, 1.0f);

        // Cast shadow.
        g.setColour (juce::Colour (0x80000000));

        g.fillEllipse (
            bounds.translated (0.0f, 4.0f));

        // Outer machined metal ring.
        juce::ColourGradient outerMetal (
            juce::Colour (0xffa4a9ae),
            bounds.getX(),
            bounds.getY(),
            juce::Colour (0xff171b20),
            bounds.getRight(),
            bounds.getBottom(),
            false);

        outerMetal.addColour (
            0.20,
            juce::Colour (0xff666d74));

        outerMetal.addColour (
            0.46,
            juce::Colour (0xff343a40));

        outerMetal.addColour (
            0.76,
            juce::Colour (0xff252a30));

        g.setGradientFill (outerMetal);
        g.fillEllipse (bounds);

        auto prismBounds =
            bounds.reduced (2.3f);

        juce::Path activeArc;

        activeArc.addCentredArc (
            centre.x,
            centre.y,
            prismBounds.getWidth() * 0.5f,
            prismBounds.getHeight() * 0.5f,
            0.0f,
            rotaryStartAngle,
            angle,
            true);

        juce::ColourGradient prism (
            juce::Colour (0xff75eef1),
            prismBounds.getX(),
            prismBounds.getCentreY(),
            juce::Colour (0xfff1a0d3),
            prismBounds.getRight(),
            prismBounds.getCentreY(),
            false);

        prism.addColour (
            0.34,
            juce::Colour (0xffa49aff));

        prism.addColour (
            0.67,
            juce::Colour (0xff98efce));

        g.setGradientFill (prism);

        g.strokePath (
            activeArc,
            juce::PathStrokeType (
                2.6f,
                juce::PathStrokeType::curved,
                juce::PathStrokeType::rounded));

        auto inner =
            bounds.reduced (8.0f);

        juce::ColourGradient knobBody (
            juce::Colour (0xff656b72),
            inner.getX(),
            inner.getY(),
            juce::Colour (0xff111419),
            inner.getRight(),
            inner.getBottom(),
            false);

        knobBody.addColour (
            0.28,
            juce::Colour (0xff454b52));

        knobBody.addColour (
            0.58,
            juce::Colour (0xff272d33));

        knobBody.addColour (
            0.82,
            juce::Colour (0xff171b20));

        g.setGradientFill (knobBody);
        g.fillEllipse (inner);

        g.setColour (juce::Colour (0x80101317));

        g.drawEllipse (
            inner.reduced (0.8f),
            1.0f);

        // 21 ticks gives precise visual calibration without clutter.
        for (int tick = 0; tick <= 20; ++tick)
        {
            const auto t =
                static_cast<float> (tick) / 20.0f;

            const auto a =
                rotaryStartAngle
                + t
                * (rotaryEndAngle - rotaryStartAngle);

            const auto major =
                tick % 5 == 0;

            const auto r1 =
                radius
                * (major ? 0.775f : 0.795f);

            const auto r2 =
                radius * 0.865f;

            const auto x1 =
                centre.x
                + std::sin (a) * r1;

            const auto y1 =
                centre.y
                - std::cos (a) * r1;

            const auto x2 =
                centre.x
                + std::sin (a) * r2;

            const auto y2 =
                centre.y
                - std::cos (a) * r2;

            g.setColour (
                major
                    ? juce::Colour (0x907b8289)
                    : juce::Colour (0x50697077));

            g.drawLine (
                x1,
                y1,
                x2,
                y2,
                major ? 1.0f : 0.65f);
        }

        // Precision pointer.
        juce::Path pointer;

        const auto pointerLength =
            radius * 0.48f;

        const auto pointerThickness =
            juce::jmax (
                1.5f,
                radius * 0.044f);

        pointer.addRoundedRectangle (
            -pointerThickness * 0.5f,
            -radius * 0.58f,
            pointerThickness,
            pointerLength,
            pointerThickness * 0.5f);

        pointer.applyTransform (
            juce::AffineTransform::rotation (angle)
                .translated (
                    centre.x,
                    centre.y));

        g.setColour (
            juce::Colour (0xfff6f7f5));

        g.fillPath (pointer);

        // Convex face reflection.
        juce::ColourGradient gloss (
            juce::Colour (0x38ffffff),
            inner.getX() + inner.getWidth() * 0.28f,
            inner.getY() + inner.getHeight() * 0.18f,
            juce::Colour (0x00ffffff),
            inner.getCentreX(),
            inner.getCentreY(),
            true);

        g.setGradientFill (gloss);

        g.fillEllipse (
            inner.withSizeKeepingCentre (
                inner.getWidth() * 0.56f,
                inner.getHeight() * 0.42f)
            .translated (
                -inner.getWidth() * 0.09f,
                -inner.getHeight() * 0.14f));
    }

    void drawButtonBackground (
        juce::Graphics& g,
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

        juce::ColourGradient tile (
            selected
                ? juce::Colour (0xff373d44)
                : juce::Colour (0xff1b2025),
            bounds.getX(),
            bounds.getY(),
            selected
                ? juce::Colour (0xff12161b)
                : juce::Colour (0xff090c10),
            bounds.getRight(),
            bounds.getBottom(),
            false);

        tile.addColour (
            0.45,
            selected
                ? juce::Colour (0xff252a30)
                : juce::Colour (0xff12161a));

        g.setGradientFill (tile);
        g.fillRoundedRectangle (bounds, 5.0f);

        if (highlighted)
        {
            g.setColour (
                juce::Colour (0x18ffffff));

            g.fillRoundedRectangle (
                bounds.reduced (1.0f),
                4.0f);
        }

        if (down)
        {
            g.setColour (
                juce::Colour (0x30000000));

            g.fillRoundedRectangle (
                bounds.reduced (1.5f),
                4.0f);
        }

        if (selected)
        {
            juce::ColourGradient edge (
                juce::Colour (0xff72eef1),
                bounds.getX(),
                bounds.getCentreY(),
                juce::Colour (0xfff09dd2),
                bounds.getRight(),
                bounds.getCentreY(),
                false);

            edge.addColour (
                0.34,
                juce::Colour (0xffa39aff));

            edge.addColour (
                0.67,
                juce::Colour (0xff98efcd));

            g.setGradientFill (edge);

            g.drawRoundedRectangle (
                bounds.reduced (0.5f),
                5.0f,
                1.35f);
        }
        else
        {
            g.setColour (
                juce::Colour (0x60444b54));

            g.drawRoundedRectangle (
                bounds.reduced (0.5f),
                5.0f,
                0.8f);
        }
    }

    void drawButtonText (
        juce::Graphics& g,
        juce::TextButton& button,
        bool,
        bool) override
    {
        g.setColour (
            button.getToggleState()
                ? juce::Colour (0xfff6f6f3)
                : juce::Colour (0xffc7c9c7));

        g.setFont (
            juce::Font (
                "Bahnschrift",
                9.2f,
                button.getToggleState()
                    ? juce::Font::bold
                    : juce::Font::plain));

        g.drawFittedText (
            button.getButtonText(),
            button.getLocalBounds()
                  .reduced (3),
            juce::Justification::centred,
            2,
            0.88f);
    }
};

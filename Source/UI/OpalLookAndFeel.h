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
            juce::Colour (0xfff0f1ef));

        setColour (
            juce::Slider::textBoxBackgroundColourId,
            juce::Colour (0xff080b0e));

        setColour (
            juce::Slider::textBoxOutlineColourId,
            juce::Colour (0xff424950));

        setColour (
            juce::Slider::textBoxHighlightColourId,
            juce::Colour (0x404fdde2));

        setColour (
            juce::TooltipWindow::backgroundColourId,
            juce::Colour (0xf0121518));

        setColour (
            juce::TooltipWindow::textColourId,
            juce::Colour (0xfff0f1ef));

        setColour (
            juce::TooltipWindow::outlineColourId,
            juce::Colour (0x7061686f));
    }

    juce::Font getTextButtonFont (
        juce::TextButton&,
        int) override
    {
        return juce::Font (
            "Arial",
            9.2f,
            juce::Font::bold);
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
            .reduced (7.0f);

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

        // Dark socket behind the physical knob.
        auto socket =
            bounds.expanded (4.0f);

        juce::ColourGradient socketGradient (
            juce::Colour (0xff050709),
            socket.getX(),
            socket.getY(),
            juce::Colour (0xff1c2126),
            socket.getRight(),
            socket.getBottom(),
            false);

        g.setGradientFill (socketGradient);
        g.fillEllipse (socket);

        g.setColour (
            juce::Colour (0x804b535a));

        g.drawEllipse (
            socket,
            0.9f);

        // Small contact shadow.
        g.setColour (
            juce::Colour (0x70000000));

        g.fillEllipse (
            bounds.translated (0.0f, 3.0f));

        // Machined silver outer ring.
        juce::ColourGradient metal (
            juce::Colour (0xffb7bbbe),
            bounds.getX(),
            bounds.getY(),
            juce::Colour (0xff33393e),
            bounds.getRight(),
            bounds.getBottom(),
            false);

        metal.addColour (
            0.22,
            juce::Colour (0xff777d82));

        metal.addColour (
            0.54,
            juce::Colour (0xff50565b));

        metal.addColour (
            0.82,
            juce::Colour (0xff3a4045));

        g.setGradientFill (metal);
        g.fillEllipse (bounds);

        auto face =
            bounds.reduced (7.0f);

        juce::ColourGradient faceGradient (
            juce::Colour (0xff3a4046),
            face.getX(),
            face.getY(),
            juce::Colour (0xff101419),
            face.getRight(),
            face.getBottom(),
            false);

        faceGradient.addColour (
            0.46,
            juce::Colour (0xff242a2f));

        g.setGradientFill (faceGradient);
        g.fillEllipse (face);

        g.setColour (
            juce::Colour (0x70383e44));

        g.drawEllipse (
            face,
            0.8f);

        // Restrained calibration marks.
        for (int tick = 0; tick <= 10; ++tick)
        {
            const auto t =
                static_cast<float> (tick) / 10.0f;

            const auto a =
                rotaryStartAngle
                + t * (rotaryEndAngle - rotaryStartAngle);

            const auto major =
                tick == 0
                || tick == 5
                || tick == 10;

            const auto r1 =
                radius * (major ? 0.76f : 0.79f);

            const auto r2 =
                radius * 0.87f;

            const auto p1 =
                juce::Point<float> (
                    centre.x + std::sin (a) * r1,
                    centre.y - std::cos (a) * r1);

            const auto p2 =
                juce::Point<float> (
                    centre.x + std::sin (a) * r2,
                    centre.y - std::cos (a) * r2);

            g.setColour (
                major
                    ? juce::Colour (0x907f858a)
                    : juce::Colour (0x50676e74));

            g.drawLine (
                p1.x,
                p1.y,
                p2.x,
                p2.y,
                major ? 1.0f : 0.7f);
        }

        // Very small prismatic reflection, not a decorative rainbow ring.
        juce::Path prismArc;

        prismArc.addCentredArc (
            centre.x,
            centre.y,
            bounds.getWidth() * 0.44f,
            bounds.getHeight() * 0.44f,
            0.0f,
            -2.45f,
            -1.55f,
            true);

        juce::ColourGradient prism (
            juce::Colour (0x907beff1),
            bounds.getX(),
            bounds.getY(),
            juce::Colour (0x80d6a0ef),
            bounds.getCentreX(),
            bounds.getY(),
            false);

        g.setGradientFill (prism);

        g.strokePath (
            prismArc,
            juce::PathStrokeType (
                1.3f,
                juce::PathStrokeType::curved,
                juce::PathStrokeType::rounded));

        // White precision pointer.
        const auto pointerLength =
            radius * 0.46f;

        const auto pointerThickness =
            juce::jmax (
                1.4f,
                radius * 0.045f);

        juce::Path pointer;

        pointer.addRoundedRectangle (
            -pointerThickness * 0.5f,
            -radius * 0.57f,
            pointerThickness,
            pointerLength,
            pointerThickness * 0.45f);

        pointer.applyTransform (
            juce::AffineTransform::rotation (angle)
                .translated (
                    centre.x,
                    centre.y));

        g.setColour (
            juce::Colour (0xfff4f5f2));

        g.fillPath (pointer);

        // Liquid-metal reflection on face.
        juce::ColourGradient gloss (
            juce::Colour (0x34ffffff),
            face.getX() + face.getWidth() * 0.28f,
            face.getY() + face.getHeight() * 0.16f,
            juce::Colour (0x00ffffff),
            face.getCentreX(),
            face.getCentreY(),
            true);

        g.setGradientFill (gloss);

        g.fillEllipse (
            face.withSizeKeepingCentre (
                face.getWidth() * 0.52f,
                face.getHeight() * 0.38f)
            .translated (
                -face.getWidth() * 0.08f,
                -face.getHeight() * 0.13f));
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
                ? juce::Colour (0xff3c4349)
                : juce::Colour (0xff20252a),
            bounds.getX(),
            bounds.getY(),
            selected
                ? juce::Colour (0xff101419)
                : juce::Colour (0xff0b0e12),
            bounds.getRight(),
            bounds.getBottom(),
            false);

        tile.addColour (
            0.50,
            selected
                ? juce::Colour (0xff252b30)
                : juce::Colour (0xff151a1f));

        g.setGradientFill (tile);
        g.fillRoundedRectangle (bounds, 4.0f);

        if (highlighted)
        {
            g.setColour (
                juce::Colour (0x14ffffff));

            g.fillRoundedRectangle (
                bounds.reduced (1.0f),
                3.0f);
        }

        if (down)
        {
            g.setColour (
                juce::Colour (0x30000000));

            g.fillRoundedRectangle (
                bounds.reduced (1.5f),
                3.0f);
        }

        if (selected)
        {
            juce::ColourGradient edge (
                juce::Colour (0xff83edf0),
                bounds.getX(),
                bounds.getCentreY(),
                juce::Colour (0xffc9a6ec),
                bounds.getRight(),
                bounds.getCentreY(),
                false);

            g.setGradientFill (edge);

            g.drawRoundedRectangle (
                bounds.reduced (0.5f),
                4.0f,
                1.15f);
        }
        else
        {
            g.setColour (
                juce::Colour (0x6050575e));

            g.drawRoundedRectangle (
                bounds.reduced (0.5f),
                4.0f,
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
                ? juce::Colour (0xfff4f5f2)
                : juce::Colour (0xffc9ccc9));

        g.setFont (
            juce::Font (
                "Arial",
                8.9f,
                juce::Font::bold));

        g.drawFittedText (
            button.getButtonText(),
            button.getLocalBounds()
                  .reduced (3),
            juce::Justification::centred,
            2,
            0.88f);
    }
};

#include "PluginEditor.h"
#include <cmath>

namespace
{
    juce::Colour opalColour (float hue,
                             float saturation = 0.62f,
                             float brightness = 1.0f,
                             float alpha = 1.0f)
    {
        hue = hue - std::floor (hue);
        return juce::Colour::fromHSV (hue, saturation, brightness, alpha);
    }

    juce::Font uiFont (float size, bool bold = false)
    {
        return juce::Font (
            "Arial",
            size,
            bold ? juce::Font::bold : juce::Font::plain);
    }
}

OpalAudioProcessorEditor::OpalAudioProcessorEditor (OpalAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    setSize (760, 460);

    titleLabel.setText ("OPAL", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (
        juce::Label::textColourId,
        juce::Colour (0xfff1f2f0));
    titleLabel.setFont (uiFont (18.0f, true));
    addAndMakeVisible (titleLabel);

    frequencyInfoLabel.setJustificationType (juce::Justification::centred);
    frequencyInfoLabel.setColour (
        juce::Label::textColourId,
        juce::Colour (0xffeef0ed));
    frequencyInfoLabel.setFont (uiFont (13.0f, true));
    addAndMakeVisible (frequencyInfoLabel);

    configureKnob (frequencyKnob, frequencyLabel, "FREQUENCY");
    configureKnob (boostKnob, boostLabel, "BOOST");
    configureKnob (opalKnob, opalLabel, "OPAL");
    configureKnob (mixKnob, mixLabel, "MIX");

    frequencyKnob.setRange (
        0.0,
        static_cast<double> (OpalFrequencyData::entries.size() - 1),
        1.0);

    frequencyKnob.textFromValueFunction = [] (double value)
    {
        const auto index =
            OpalFrequencyData::clampIndex (
                juce::roundToInt (value));

        return juce::String (
            static_cast<int> (
                OpalFrequencyData::entries[
                    static_cast<size_t> (index)].hz))
            + " Hz";
    };

    boostKnob.setTextValueSuffix (" dB");
    opalKnob.setTextValueSuffix (" %");
    mixKnob.setTextValueSuffix (" %");

    frequencyKnob.setDoubleClickReturnValue (true, 9.0);
    boostKnob.setDoubleClickReturnValue (true, 3.0);
    opalKnob.setDoubleClickReturnValue (true, 35.0);
    mixKnob.setDoubleClickReturnValue (true, 50.0);

    addAndMakeVisible (opalStone);

    auto& state = processor.getValueTreeState();

    frequencyAttachment =
        std::make_unique<SliderAttachment> (
            state,
            "frequency",
            frequencyKnob);

    boostAttachment =
        std::make_unique<SliderAttachment> (
            state,
            "boost",
            boostKnob);

    opalAttachment =
        std::make_unique<SliderAttachment> (
            state,
            "opal",
            opalKnob);

    mixAttachment =
        std::make_unique<SliderAttachment> (
            state,
            "mix",
            mixKnob);

    frequencyButtons.reserve (
        OpalFrequencyData::entries.size());

    for (int i = 0;
         i < static_cast<int> (OpalFrequencyData::entries.size());
         ++i)
    {
        const auto& entry =
            OpalFrequencyData::entries[
                static_cast<size_t> (i)];

        auto button =
            std::make_unique<juce::TextButton>();

        button->setClickingTogglesState (false);

        button->setButtonText (
            juce::String (static_cast<int> (entry.hz))
            + "\n"
            + entry.label);

        button->setTooltip (
            juce::String (static_cast<int> (entry.hz))
            + " Hz — "
            + entry.label);

        button->onClick = [this, i]
        {
            if (auto* parameter =
                    processor.getValueTreeState()
                        .getParameter ("frequency"))
            {
                parameter->beginChangeGesture();

                parameter->setValueNotifyingHost (
                    parameter->convertTo0to1 (
                        static_cast<float> (i)));

                parameter->endChangeGesture();
            }
        };

        addAndMakeVisible (*button);
        frequencyButtons.push_back (std::move (button));
    }

    updateFrequencyInfo();
    startTimerHz (30);
}

OpalAudioProcessorEditor::~OpalAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void OpalAudioProcessorEditor::configureKnob (juce::Slider& slider,
                                               juce::Label& label,
                                               const juce::String& name)
{
    slider.setSliderStyle (
        juce::Slider::RotaryHorizontalVerticalDrag);

    slider.setRotaryParameters (
        juce::MathConstants<float>::pi * 1.17f,
        juce::MathConstants<float>::pi * 2.83f,
        true);

    slider.setTextBoxStyle (
        juce::Slider::TextBoxBelow,
        false,
        82,
        19);

    slider.setMouseDragSensitivity (900);
    slider.setVelocityBasedMode (false);
    slider.setScrollWheelEnabled (true);
    slider.setNumDecimalPlacesToDisplay (1);

    addAndMakeVisible (slider);

    label.setText (
        name,
        juce::dontSendNotification);

    label.setJustificationType (
        juce::Justification::centred);

    label.setColour (
        juce::Label::textColourId,
        juce::Colour (0xffe0e2df));

    label.setFont (
        uiFont (10.5f, true));

    addAndMakeVisible (label);
}

void OpalAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Flat commercial console body with liquid-metal surface treatment.
    juce::ColourGradient background (
        juce::Colour (0xff6d7378),
        0.0f,
        0.0f,
        juce::Colour (0xff1a1e22),
        0.0f,
        static_cast<float> (getHeight()),
        false);

    background.addColour (
        0.16,
        juce::Colour (0xff50565c));

    background.addColour (
        0.52,
        juce::Colour (0xff30353a));

    background.addColour (
        0.84,
        juce::Colour (0xff22272c));

    g.setGradientFill (background);
    g.fillAll();

    auto face =
        getLocalBounds()
            .toFloat()
            .reduced (8.0f);

    g.setColour (
        juce::Colour (0x70373d42));

    g.drawRoundedRectangle (
        face,
        9.0f,
        1.0f);

    // Fine satin-metal lines, intentionally subtle.
    for (int y = 11; y < getHeight() - 11; y += 5)
    {
        g.setColour (
            juce::Colour::fromRGBA (
                255,
                255,
                255,
                static_cast<juce::uint8> (
                    (y % 15 == 0) ? 8 : 3)));

        g.drawHorizontalLine (
            y,
            12.0f,
            static_cast<float> (getWidth() - 12));
    }

    auto drawInset =
        [&g] (juce::Rectangle<float> r,
              float corner = 8.0f)
        {
            juce::ColourGradient well (
                juce::Colour (0xff12171b),
                r.getX(),
                r.getY(),
                juce::Colour (0xff080b0e),
                r.getRight(),
                r.getBottom(),
                false);

            well.addColour (
                0.48,
                juce::Colour (0xff0d1115));

            g.setGradientFill (well);
            g.fillRoundedRectangle (r, corner);

            g.setColour (
                juce::Colour (0x905e656b));

            g.drawRoundedRectangle (
                r,
                corner,
                1.0f);

            g.setColour (
                juce::Colour (0x60000000));

            g.drawLine (
                r.getX() + 8.0f,
                r.getY() + 1.0f,
                r.getRight() - 8.0f,
                r.getY() + 1.0f,
                1.5f);
        };

    // Top-left frequency control and top-right readout each have their own bay.
    drawInset ({
        22.0f,
        24.0f,
        182.0f,
        202.0f
    }, 10.0f);

    drawInset ({
        556.0f,
        62.0f,
        174.0f,
        110.0f
    }, 10.0f);

    // Selector strip.
    drawInset ({
        22.0f,
        246.0f,
        static_cast<float> (getWidth() - 44),
        76.0f
    }, 9.0f);

    // Three isolated lower control bays.
    const std::array<juce::Rectangle<float>, 3> controlBays {{
        { 25.0f, 342.0f, 218.0f, 102.0f },
        { 271.0f, 342.0f, 218.0f, 102.0f },
        { 517.0f, 342.0f, 218.0f, 102.0f }
    }};

    for (const auto& bay : controlBays)
        drawInset (bay, 10.0f);

    // Minimal OPAL plaque, fully inside the central top region.
    auto plaque =
        titleLabel.getBounds()
            .toFloat()
            .expanded (11.0f, 3.0f);

    juce::ColourGradient plaqueMetal (
        juce::Colour (0xff7a8186),
        plaque.getX(),
        plaque.getY(),
        juce::Colour (0xff272c31),
        plaque.getRight(),
        plaque.getBottom(),
        false);

    plaqueMetal.addColour (
        0.50,
        juce::Colour (0xff4b5156));

    g.setGradientFill (plaqueMetal);
    g.fillRoundedRectangle (plaque, 5.0f);

    g.setColour (
        juce::Colour (0x906b7278));

    g.drawRoundedRectangle (
        plaque,
        5.0f,
        1.0f);
}

void OpalAudioProcessorEditor::OpalStone::paint (juce::Graphics& g)
{
    auto available =
        getLocalBounds()
            .toFloat()
            .reduced (24.0f);

    const auto size =
        juce::jmin (
            available.getWidth(),
            available.getHeight());

    auto stoneBounds =
        available.withSizeKeepingCentre (
            size,
            size);

    juce::Path stone;
    stone.addEllipse (stoneBounds);

    // Physical depth: wide floating shadow + tight contact shadow.
    juce::DropShadow (
        juce::Colour (0xb8000000),
        28,
        { 0, 13 })
        .drawForPath (g, stone);

    juce::DropShadow (
        juce::Colour (0x90000000),
        10,
        { 0, 5 })
        .drawForPath (g, stone);

    auto bezel =
        stoneBounds.expanded (12.0f);

    juce::ColourGradient bezelMetal (
        juce::Colour (0xffc5c9cc),
        bezel.getX(),
        bezel.getY(),
        juce::Colour (0xff2b3035),
        bezel.getRight(),
        bezel.getBottom(),
        false);

    bezelMetal.addColour (
        0.20,
        juce::Colour (0xff81888e));

    bezelMetal.addColour (
        0.52,
        juce::Colour (0xff4c5359));

    bezelMetal.addColour (
        0.80,
        juce::Colour (0xff31373d));

    g.setGradientFill (bezelMetal);
    g.fillEllipse (bezel);

    g.setColour (
        juce::Colour (0xff080a0c));

    g.fillEllipse (
        stoneBounds.expanded (2.0f));

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (stone);

        const auto cx =
            stoneBounds.getCentreX();

        const auto cy =
            stoneBounds.getCentreY();

        // Thick translucent opal body.
        juce::ColourGradient body (
            juce::Colour (0xfff8fbf7),
            cx - size * 0.28f,
            cy - size * 0.35f,
            juce::Colour (0xff26333b),
            cx + size * 0.36f,
            cy + size * 0.42f,
            true);

        body.addColour (
            0.18,
            juce::Colour (0xffd7ece2));

        body.addColour (
            0.40,
            juce::Colour (0xff91a8ad));

        body.addColour (
            0.66,
            juce::Colour (0xff596872));

        body.addColour (
            0.90,
            juce::Colour (0xff263039));

        g.setGradientFill (body);
        g.fillRect (stoneBounds);

        const auto activity =
            activation
            * (0.30f + 0.70f * energy);

        // Moving refractive volumes only: no drawn line-art caustics.
        const std::array<float, 8> hues {
            0.48f,
            0.53f,
            0.58f,
            0.66f,
            0.76f,
            0.88f,
            0.95f,
            0.38f
        };

        for (int i = 0; i < static_cast<int> (hues.size()); ++i)
        {
            const auto fi =
                static_cast<float> (i);

            const auto t =
                phase * (0.24f + fi * 0.035f);

            const auto px =
                cx
                + std::sin (t + fi * 1.31f)
                * size
                * (0.07f + fi * 0.009f);

            const auto py =
                cy
                + std::cos (t * 0.76f + fi * 1.07f)
                * size
                * (0.08f + fi * 0.008f);

            const auto rx =
                size * (0.16f + fi * 0.012f);

            const auto ry =
                rx * (0.72f + 0.06f * std::sin (t + fi));

            auto core =
                opalColour (
                    hues[static_cast<size_t> (i)]
                    + phase * 0.0035f,
                    0.52f,
                    1.0f,
                    0.055f
                    + activity * 0.23f);

            juce::ColourGradient volume (
                core,
                px,
                py,
                core.withAlpha (0.0f),
                px + rx,
                py,
                true);

            volume.addColour (
                0.30,
                core.withAlpha (
                    core.getFloatAlpha() * 0.85f));

            g.setGradientFill (volume);

            g.fillEllipse (
                px - rx,
                py - ry,
                rx * 2.0f,
                ry * 2.0f);
        }

        if (activation > 0.001f)
        {
            // Aqua liquid lens drifting under the glass.
            const auto lensRadius =
                size * (0.18f + 0.10f * activity);

            const auto lensX =
                cx
                + std::sin (phase * 0.51f)
                * size * 0.11f;

            const auto lensY =
                cy
                + std::cos (phase * 0.43f)
                * size * 0.085f;

            const auto aqua =
                juce::Colour::fromFloatRGBA (
                    0.14f,
                    0.93f,
                    1.0f,
                    0.12f + activity * 0.28f);

            juce::ColourGradient liquidLens (
                aqua,
                lensX,
                lensY,
                aqua.withAlpha (0.0f),
                lensX + lensRadius,
                lensY,
                true);

            g.setGradientFill (liquidLens);

            g.fillEllipse (
                lensX - lensRadius,
                lensY - lensRadius,
                lensRadius * 2.0f,
                lensRadius * 2.0f);
        }

        // Large curved specular reflection creates the glass dome.
        juce::ColourGradient topGloss (
            juce::Colour (0x84ffffff),
            stoneBounds.getX() + size * 0.22f,
            stoneBounds.getY() + size * 0.12f,
            juce::Colour (0x00ffffff),
            stoneBounds.getX() + size * 0.62f,
            stoneBounds.getY() + size * 0.52f,
            true);

        g.setGradientFill (topGloss);

        g.fillEllipse (
            stoneBounds.getX() + size * 0.10f,
            stoneBounds.getY() + size * 0.06f,
            size * 0.72f,
            size * 0.42f);

        // Lower optical density.
        juce::ColourGradient bottomShade (
            juce::Colour (0x00000000),
            cx,
            cy,
            juce::Colour (0x76000000),
            cx,
            stoneBounds.getBottom(),
            false);

        g.setGradientFill (bottomShade);

        g.fillEllipse (
            stoneBounds.getX(),
            stoneBounds.getY() + size * 0.44f,
            size,
            size * 0.60f);
    }

    g.setColour (
        juce::Colour (0xc0ffffff));

    g.drawEllipse (
        stoneBounds.reduced (0.6f),
        1.2f);

    // Small high-intensity specular point sells the 3D pop.
    g.setColour (
        juce::Colour (0x98ffffff));

    g.fillEllipse (
        stoneBounds.getX() + size * 0.24f,
        stoneBounds.getY() + size * 0.15f,
        size * 0.085f,
        size * 0.045f);
}

void OpalAudioProcessorEditor::timerCallback()
{
    const auto opalAmount =
        processor.getValueTreeState()
            .getRawParameterValue ("opal")
            ->load()
        * 0.01f;

    if (opalAmount > 0.001f)
    {
        animationPhase +=
            0.0055f
            + 0.019f * opalAmount;

        if (animationPhase
            > juce::MathConstants<float>::twoPi * 12.0f)
        {
            animationPhase = 0.0f;
        }
    }

    opalStone.setEnergyDb (
        processor.getResonanceEnergyDb());

    opalStone.setOpalAmount (
        opalAmount);

    opalStone.setPhase (
        animationPhase);

    const auto selectedIndex =
        OpalFrequencyData::clampIndex (
            juce::roundToInt (
                processor.getValueTreeState()
                    .getRawParameterValue ("frequency")
                    ->load()));

    for (int i = 0;
         i < static_cast<int> (frequencyButtons.size());
         ++i)
    {
        frequencyButtons[
            static_cast<size_t> (i)]
            ->setToggleState (
                i == selectedIndex,
                juce::dontSendNotification);
    }

    updateFrequencyInfo();
}

void OpalAudioProcessorEditor::updateFrequencyInfo()
{
    const auto index =
        OpalFrequencyData::clampIndex (
            juce::roundToInt (
                processor.getValueTreeState()
                    .getRawParameterValue ("frequency")
                    ->load()));

    const auto& entry =
        OpalFrequencyData::entries[
            static_cast<size_t> (index)];

    frequencyInfoLabel.setText (
        juce::String (
            static_cast<int> (entry.hz))
        + " Hz\n"
        + juce::String (entry.label)
              .toUpperCase(),
        juce::dontSendNotification);
}

void OpalAudioProcessorEditor::resized()
{
    const auto centreX =
        getWidth() / 2;

    // Frequency bay.
    frequencyLabel.setBounds (
        54,
        36,
        118,
        20);

    frequencyKnob.setBounds (
        42,
        58,
        142,
        154);

    // Raised centre opal.
    opalStone.setBounds (
        centreX - 112,
        17,
        224,
        224);

    titleLabel.setBounds (
        centreX - 58,
        214,
        116,
        24);

    // Readout bay.
    frequencyInfoLabel.setBounds (
        571,
        84,
        144,
        66);

    constexpr int columns = 9;
    constexpr int gap = 4;

    const int gridX = 29;
    const int gridY = 254;
    const int gridWidth =
        getWidth() - 58;

    const int cellWidth =
        (gridWidth
         - (columns - 1) * gap)
        / columns;

    const int cellHeight = 27;

    for (int i = 0;
         i < static_cast<int> (frequencyButtons.size());
         ++i)
    {
        const auto row = i / columns;
        const auto col = i % columns;

        frequencyButtons[
            static_cast<size_t> (i)]
            ->setBounds (
                gridX
                + col * (cellWidth + gap),
                gridY
                + row * (cellHeight + gap),
                cellWidth,
                cellHeight);
    }

    // Every lower control has its own non-overlapping bay.
    const std::array<int, 3> bayX {
        25, 271, 517
    };

    std::array<juce::Slider*, 3> knobs {
        &boostKnob,
        &opalKnob,
        &mixKnob
    };

    std::array<juce::Label*, 3> labels {
        &boostLabel,
        &opalLabel,
        &mixLabel
    };

    for (int i = 0; i < 3; ++i)
    {
        labels[static_cast<size_t> (i)]
            ->setBounds (
                bayX[static_cast<size_t> (i)] + 51,
                349,
                116,
                20);

        knobs[static_cast<size_t> (i)]
            ->setBounds (
                bayX[static_cast<size_t> (i)] + 39,
                369,
                140,
                70);
    }
}

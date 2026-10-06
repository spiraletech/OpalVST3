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
        return juce::Font ("Bahnschrift",
                           size,
                           bold ? juce::Font::bold : juce::Font::plain);
    }
}

OpalAudioProcessorEditor::OpalAudioProcessorEditor (OpalAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    setSize (780, 500);

    titleLabel.setText ("OPAL", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xfff3f3f1));
    titleLabel.setFont (uiFont (18.5f, true));
    addAndMakeVisible (titleLabel);

    frequencyInfoLabel.setJustificationType (juce::Justification::centred);
    frequencyInfoLabel.setColour (juce::Label::textColourId, juce::Colour (0xfff0f1ef));
    frequencyInfoLabel.setFont (uiFont (13.5f, true));
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

        frequencyButtons.push_back (
            std::move (button));
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
        juce::MathConstants<float>::pi * 1.16f,
        juce::MathConstants<float>::pi * 2.84f,
        true);

    slider.setTextBoxStyle (
        juce::Slider::TextBoxBelow,
        false,
        82,
        20);

    slider.setMouseDragSensitivity (760);
    slider.setVelocityBasedMode (false);
    slider.setScrollWheelEnabled (true);
    slider.setNumDecimalPlacesToDisplay (1);

    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (
        juce::Label::textColourId,
        juce::Colour (0xffd5d7d5));
    label.setFont (uiFont (10.2f, true));

    addAndMakeVisible (label);
}

void OpalAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Liquid black-metal body: clean enough for commercial hardware,
    // with depth coming from material rather than decorative clutter.
    juce::ColourGradient background (
        juce::Colour (0xff20242a),
        0.0f,
        0.0f,
        juce::Colour (0xff05070a),
        0.0f,
        static_cast<float> (getHeight()),
        false);

    background.addColour (
        0.32,
        juce::Colour (0xff11151a));

    background.addColour (
        0.68,
        juce::Colour (0xff0a0d11));

    g.setGradientFill (background);
    g.fillAll();

    auto chassis =
        getLocalBounds()
            .toFloat()
            .reduced (10.0f);

    juce::ColourGradient bodyMetal (
        juce::Colour (0xff3a4048),
        chassis.getX(),
        chassis.getY(),
        juce::Colour (0xff0b0e12),
        chassis.getRight(),
        chassis.getBottom(),
        false);

    bodyMetal.addColour (
        0.18,
        juce::Colour (0xff252a31));

    bodyMetal.addColour (
        0.49,
        juce::Colour (0xff15191f));

    bodyMetal.addColour (
        0.76,
        juce::Colour (0xff242931));

    g.setGradientFill (bodyMetal);
    g.fillRoundedRectangle (chassis, 18.0f);

    // Satin micro-grain.
    for (int y = 17; y < getHeight() - 17; y += 5)
    {
        const auto alpha =
            static_cast<juce::uint8> (
                (y % 15 == 0) ? 7 : 3);

        g.setColour (
            juce::Colour::fromRGBA (
                255,
                255,
                255,
                alpha));

        g.drawHorizontalLine (
            y,
            18.0f,
            static_cast<float> (getWidth() - 18));
    }

    g.setColour (juce::Colour (0x704f5661));

    g.drawRoundedRectangle (
        chassis,
        18.0f,
        1.0f);

    // Top instrument well.
    auto upperWell =
        juce::Rectangle<float> (
            24.0f,
            24.0f,
            static_cast<float> (getWidth() - 48),
            226.0f);

    juce::ColourGradient upperRecess (
        juce::Colour (0xff080a0d),
        upperWell.getX(),
        upperWell.getY(),
        juce::Colour (0xff151a20),
        upperWell.getRight(),
        upperWell.getBottom(),
        false);

    upperRecess.addColour (
        0.44,
        juce::Colour (0xff0d1116));

    g.setGradientFill (upperRecess);
    g.fillRoundedRectangle (upperWell, 14.0f);

    // Inner shadow on top / subtle liquid-metal highlight on bottom.
    g.setColour (juce::Colour (0x90000000));

    g.drawLine (
        upperWell.getX() + 14.0f,
        upperWell.getY() + 1.0f,
        upperWell.getRight() - 14.0f,
        upperWell.getY() + 1.0f,
        2.0f);

    juce::ColourGradient upperEdge (
        juce::Colour (0x5076eff3),
        upperWell.getX(),
        upperWell.getCentreY(),
        juce::Colour (0x40ef9ad6),
        upperWell.getRight(),
        upperWell.getCentreY(),
        false);

    upperEdge.addColour (
        0.46,
        juce::Colour (0x489f96ff));

    upperEdge.addColour (
        0.72,
        juce::Colour (0x4093efc8));

    g.setGradientFill (upperEdge);

    g.drawRoundedRectangle (
        upperWell.reduced (0.5f),
        14.0f,
        1.0f);

    // Frequency readout well.
    auto readout =
        juce::Rectangle<float> (
            566.0f,
            82.0f,
            146.0f,
            82.0f);

    juce::ColourGradient displayGlass (
        juce::Colour (0xff1c2228),
        readout.getX(),
        readout.getY(),
        juce::Colour (0xff07090c),
        readout.getRight(),
        readout.getBottom(),
        false);

    displayGlass.addColour (
        0.52,
        juce::Colour (0xff0f1317));

    g.setGradientFill (displayGlass);
    g.fillRoundedRectangle (readout, 9.0f);

    g.setColour (juce::Colour (0x802f3740));

    g.drawRoundedRectangle (
        readout,
        9.0f,
        1.0f);

    // Deep selector rail.
    auto selectorRail =
        juce::Rectangle<float> (
            24.0f,
            271.0f,
            static_cast<float> (getWidth() - 48),
            80.0f);

    g.setColour (juce::Colour (0xff090c10));
    g.fillRoundedRectangle (selectorRail, 11.0f);

    g.setColour (juce::Colour (0x80434a53));

    g.drawRoundedRectangle (
        selectorRail,
        11.0f,
        1.0f);

    // Lower hardware deck.
    auto controlDeck =
        juce::Rectangle<float> (
            24.0f,
            368.0f,
            static_cast<float> (getWidth() - 48),
            112.0f);

    juce::ColourGradient deckMetal (
        juce::Colour (0xff242a31),
        controlDeck.getX(),
        controlDeck.getY(),
        juce::Colour (0xff0a0d11),
        controlDeck.getRight(),
        controlDeck.getBottom(),
        false);

    deckMetal.addColour (
        0.50,
        juce::Colour (0xff14191e));

    g.setGradientFill (deckMetal);
    g.fillRoundedRectangle (controlDeck, 12.0f);

    g.setColour (juce::Colour (0x7048505a));

    g.drawRoundedRectangle (
        controlDeck,
        12.0f,
        1.0f);

    // Recessed label wells: bold type gets its own physical space.
    const std::array<juce::Rectangle<int>, 4> labelRects {
        frequencyLabel.getBounds(),
        boostLabel.getBounds(),
        opalLabel.getBounds(),
        mixLabel.getBounds()
    };

    for (const auto& r : labelRects)
    {
        auto well =
            r.toFloat()
                .expanded (8.0f, 3.0f);

        juce::ColourGradient labelWell (
            juce::Colour (0xff05070a),
            well.getX(),
            well.getY(),
            juce::Colour (0xff171b20),
            well.getRight(),
            well.getBottom(),
            false);

        g.setGradientFill (labelWell);
        g.fillRoundedRectangle (well, 5.0f);

        g.setColour (juce::Colour (0x703a414a));

        g.drawRoundedRectangle (
            well,
            5.0f,
            0.8f);
    }

    // OPAL nameplate sits under the stone, inside the plugin body.
    auto nameplate =
        titleLabel.getBounds()
            .toFloat()
            .expanded (13.0f, 4.0f);

    juce::ColourGradient plate (
        juce::Colour (0xff3d434a),
        nameplate.getX(),
        nameplate.getY(),
        juce::Colour (0xff101318),
        nameplate.getRight(),
        nameplate.getBottom(),
        false);

    plate.addColour (
        0.50,
        juce::Colour (0xff22272d));

    g.setGradientFill (plate);
    g.fillRoundedRectangle (nameplate, 6.0f);

    g.setColour (juce::Colour (0x80606872));

    g.drawRoundedRectangle (
        nameplate,
        6.0f,
        1.0f);
}

void OpalAudioProcessorEditor::OpalStone::paint (juce::Graphics& g)
{
    // Extra room around the dome allows the stone to cast a real-looking shadow.
    auto outer =
        getLocalBounds()
            .toFloat()
            .reduced (22.0f);

    const auto size =
        juce::jmin (
            outer.getWidth(),
            outer.getHeight());

    auto bounds =
        outer.withSizeKeepingCentre (
            size,
            size);

    juce::Path stone;
    stone.addEllipse (bounds);

    // Deep ambient shadow and tighter contact shadow create the pop-out effect.
    juce::DropShadow (
        juce::Colour (0xb8000000),
        24,
        { 0, 11 })
        .drawForPath (g, stone);

    juce::DropShadow (
        juce::Colour (0x90000000),
        9,
        { 0, 4 })
        .drawForPath (g, stone);

    auto outerBezel =
        bounds.expanded (13.0f);

    juce::ColourGradient bezelOuter (
        juce::Colour (0xffb8bdc1),
        outerBezel.getX(),
        outerBezel.getY(),
        juce::Colour (0xff1a1e23),
        outerBezel.getRight(),
        outerBezel.getBottom(),
        false);

    bezelOuter.addColour (
        0.18,
        juce::Colour (0xff737a81));

    bezelOuter.addColour (
        0.48,
        juce::Colour (0xff393f46));

    bezelOuter.addColour (
        0.78,
        juce::Colour (0xff22272c));

    g.setGradientFill (bezelOuter);
    g.fillEllipse (outerBezel);

    auto innerBezel =
        bounds.expanded (6.0f);

    juce::ColourGradient bezelInner (
        juce::Colour (0xff22272d),
        innerBezel.getX(),
        innerBezel.getY(),
        juce::Colour (0xff757d84),
        innerBezel.getRight(),
        innerBezel.getBottom(),
        false);

    bezelInner.addColour (
        0.52,
        juce::Colour (0xff343a40));

    g.setGradientFill (bezelInner);
    g.fillEllipse (innerBezel);

    g.setColour (juce::Colour (0xff05070a));
    g.fillEllipse (bounds.expanded (1.8f));

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (stone);

        const auto cx = bounds.getCentreX();
        const auto cy = bounds.getCentreY();

        // Deep domed body.
        juce::ColourGradient base (
            juce::Colour (0xfff6faf5),
            cx - size * 0.27f,
            cy - size * 0.34f,
            juce::Colour (0xff26313a),
            cx + size * 0.34f,
            cy + size * 0.40f,
            true);

        base.addColour (
            0.18,
            juce::Colour (0xffd1eadf));

        base.addColour (
            0.42,
            juce::Colour (0xff819ba3));

        base.addColour (
            0.69,
            juce::Colour (0xff52606a));

        base.addColour (
            0.91,
            juce::Colour (0xff202831));

        g.setGradientFill (base);
        g.fillRect (bounds);

        const auto activeGlow =
            activation
            * (0.34f + 0.66f * energy);

        // Slow refractive volumes underneath the "glass".
        const std::array<float, 7> hues {
            0.48f,
            0.54f,
            0.61f,
            0.76f,
            0.89f,
            0.96f,
            0.38f
        };

        for (int i = 0; i < static_cast<int> (hues.size()); ++i)
        {
            const auto t =
                phase * (0.30f + 0.055f * static_cast<float> (i));

            const auto px =
                cx
                + std::sin (t + i * 1.27f)
                * size
                * (0.08f + 0.013f * i);

            const auto py =
                cy
                + std::cos (t * 0.78f + i * 1.11f)
                * size
                * (0.09f + 0.010f * i);

            const auto radius =
                size
                * (0.19f + 0.020f * i);

            auto core =
                opalColour (
                    hues[static_cast<size_t> (i)]
                    + phase * 0.005f,
                    0.57f,
                    1.0f,
                    0.07f
                    + activeGlow * 0.31f);

            juce::ColourGradient volume (
                core,
                px,
                py,
                core.withAlpha (0.0f),
                px + radius,
                py,
                true);

            volume.addColour (
                0.30,
                core.withAlpha (
                    core.getFloatAlpha() * 0.82f));

            g.setGradientFill (volume);

            g.fillEllipse (
                px - radius,
                py - radius,
                radius * 2.0f,
                radius * 2.0f);
        }

        if (activation > 0.001f)
        {
            // 3D liquid caustics: layered curves at different apparent depths.
            for (int layer = 0; layer < 10; ++layer)
            {
                juce::Path caustic;

                const auto depth =
                    static_cast<float> (layer) / 9.0f;

                const auto baseY =
                    bounds.getY()
                    + size
                    * (0.10f + depth * 0.80f);

                const auto amplitude =
                    size
                    * (0.009f
                       + 0.022f * activeGlow
                       + depth * 0.004f);

                const auto speed =
                    phase
                    * (0.42f
                       + 0.037f * static_cast<float> (layer));

                for (int step = 0; step <= 46; ++step)
                {
                    const auto norm =
                        static_cast<float> (step) / 46.0f;

                    const auto x =
                        bounds.getX()
                        + norm * size;

                    const auto y =
                        baseY
                        + std::sin (
                              norm
                              * juce::MathConstants<float>::twoPi
                              * (1.65f + depth * 0.40f)
                              + speed
                              + static_cast<float> (layer) * 0.47f)
                              * amplitude
                        + std::sin (
                              norm
                              * juce::MathConstants<float>::twoPi
                              * 3.35f
                              - speed * 0.61f)
                              * amplitude
                              * 0.33f;

                    if (step == 0)
                        caustic.startNewSubPath (x, y);
                    else
                        caustic.lineTo (x, y);
                }

                g.setColour (
                    opalColour (
                        0.48f
                        + depth * 0.10f
                        + phase * 0.0025f,
                        0.52f,
                        1.0f,
                        0.045f
                        + activeGlow
                        * (0.12f + depth * 0.11f)));

                g.strokePath (
                    caustic,
                    juce::PathStrokeType (
                        0.7f
                        + activeGlow
                        * (0.9f + depth * 0.8f),
                        juce::PathStrokeType::curved,
                        juce::PathStrokeType::rounded));
            }

            // Moving lens pool.
            const auto poolRadius =
                size
                * (0.18f + 0.12f * activeGlow);

            const auto poolX =
                cx
                + std::sin (phase * 0.59f)
                * size
                * 0.11f;

            const auto poolY =
                cy
                + std::cos (phase * 0.49f)
                * size
                * 0.085f;

            auto aqua =
                juce::Colour::fromFloatRGBA (
                    0.18f,
                    0.92f,
                    1.0f,
                    0.13f
                    + activeGlow * 0.28f);

            juce::ColourGradient pool (
                aqua,
                poolX,
                poolY,
                aqua.withAlpha (0.0f),
                poolX + poolRadius,
                poolY,
                true);

            g.setGradientFill (pool);

            g.fillEllipse (
                poolX - poolRadius,
                poolY - poolRadius,
                poolRadius * 2.0f,
                poolRadius * 2.0f);

            // Concentric depth ripples.
            for (int ripple = 0; ripple < 4; ++ripple)
            {
                const auto r =
                    size
                    * (0.18f + 0.075f * static_cast<float> (ripple))
                    + std::sin (
                          phase * 0.72f
                          + static_cast<float> (ripple))
                          * size
                          * 0.011f;

                g.setColour (
                    juce::Colour::fromFloatRGBA (
                        0.70f,
                        1.0f,
                        1.0f,
                        0.04f
                        + activeGlow * 0.10f));

                g.drawEllipse (
                    juce::Rectangle<float> (r, r)
                        .withCentre ({
                            cx
                            + std::sin (
                                  phase * 0.41f
                                  + static_cast<float> (ripple))
                                  * size
                                  * 0.025f,
                            cy
                            + std::cos (
                                  phase * 0.37f
                                  + static_cast<float> (ripple))
                                  * size
                                  * 0.022f
                        }),
                    0.7f + activeGlow * 0.8f);
            }
        }

        // Large convex-lens reflection.
        juce::ColourGradient gloss (
            juce::Colour (0x7affffff),
            bounds.getX() + size * 0.26f,
            bounds.getY() + size * 0.17f,
            juce::Colour (0x00ffffff),
            bounds.getX() + size * 0.62f,
            bounds.getY() + size * 0.55f,
            true);

        g.setGradientFill (gloss);

        g.fillEllipse (
            bounds.getX() + size * 0.16f,
            bounds.getY() + size * 0.10f,
            size * 0.60f,
            size * 0.34f);

        // Lower internal shadow makes the dome read as thick material.
        juce::ColourGradient lowerShade (
            juce::Colour (0x00000000),
            cx,
            cy,
            juce::Colour (0x78000000),
            cx,
            bounds.getBottom(),
            false);

        g.setGradientFill (lowerShade);

        g.fillEllipse (
            bounds.getX(),
            bounds.getY() + size * 0.42f,
            size,
            size * 0.64f);
    }

    // Crisp lens rim and bright top-left specular edge.
    g.setColour (juce::Colour (0xb0ffffff));

    g.drawEllipse (
        bounds.reduced (0.7f),
        1.1f);

    juce::Path highlightArc;

    highlightArc.addCentredArc (
        bounds.getCentreX(),
        bounds.getCentreY(),
        bounds.getWidth() * 0.47f,
        bounds.getHeight() * 0.47f,
        0.0f,
        -2.45f,
        -0.65f,
        true);

    g.setColour (juce::Colour (0x90ffffff));

    g.strokePath (
        highlightArc,
        juce::PathStrokeType (
            1.7f,
            juce::PathStrokeType::curved,
            juce::PathStrokeType::rounded));
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
            0.007f
            + 0.026f * opalAmount;

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

    frequencyKnob.setBounds (
        52,
        67,
        150,
        140);

    frequencyLabel.setBounds (
        72,
        209,
        110,
        18);

    // Larger component, smaller stone inside: shadow can extend around the dome.
    opalStone.setBounds (
        centreX - 117,
        29,
        234,
        234);

    titleLabel.setBounds (
        centreX - 60,
        232,
        120,
        23);

    frequencyInfoLabel.setBounds (
        571,
        88,
        136,
        70);

    constexpr int columns = 9;
    constexpr int cellGap = 4;

    const int gridX = 30;
    const int gridY = 281;
    const int gridWidth =
        getWidth() - 60;

    const int cellWidth =
        (gridWidth
         - (columns - 1) * cellGap)
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
                + col * (cellWidth + cellGap),
                gridY
                + row * (cellHeight + cellGap),
                cellWidth,
                cellHeight);
    }

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

    const int knobY = 379;
    const int knobWidth = 148;
    const int spacing = 54;

    const int totalWidth =
        static_cast<int> (knobs.size())
        * knobWidth
        + (static_cast<int> (knobs.size()) - 1)
        * spacing;

    const int startX =
        (getWidth() - totalWidth) / 2;

    for (int i = 0;
         i < static_cast<int> (knobs.size());
         ++i)
    {
        const auto x =
            startX
            + i * (knobWidth + spacing);

        knobs[
            static_cast<size_t> (i)]
            ->setBounds (
                x,
                knobY,
                knobWidth,
                89);

        labels[
            static_cast<size_t> (i)]
            ->setBounds (
                x + 17,
                470,
                knobWidth - 34,
                18);
    }
}

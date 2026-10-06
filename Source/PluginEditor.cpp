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
        return juce::Font ("Segoe UI",
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
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xfff2f1ed));
    titleLabel.setFont (uiFont (17.0f, true));
    addAndMakeVisible (titleLabel);

    frequencyInfoLabel.setJustificationType (juce::Justification::centred);
    frequencyInfoLabel.setColour (juce::Label::textColourId, juce::Colour (0xffecebe7));
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
        const auto index = OpalFrequencyData::clampIndex (juce::roundToInt (value));
        return juce::String (
            static_cast<int> (OpalFrequencyData::entries[static_cast<size_t> (index)].hz))
            + " Hz";
    };

    boostKnob.setTextValueSuffix (" dB");
    opalKnob.setTextValueSuffix (" %");
    mixKnob.setTextValueSuffix (" %");

    addAndMakeVisible (opalStone);

    auto& state = processor.getValueTreeState();

    frequencyAttachment = std::make_unique<SliderAttachment> (state, "frequency", frequencyKnob);
    boostAttachment = std::make_unique<SliderAttachment> (state, "boost", boostKnob);
    opalAttachment = std::make_unique<SliderAttachment> (state, "opal", opalKnob);
    mixAttachment = std::make_unique<SliderAttachment> (state, "mix", mixKnob);

    frequencyButtons.reserve (OpalFrequencyData::entries.size());

    for (int i = 0; i < static_cast<int> (OpalFrequencyData::entries.size()); ++i)
    {
        const auto& entry = OpalFrequencyData::entries[static_cast<size_t> (i)];
        auto button = std::make_unique<juce::TextButton>();

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
            if (auto* parameter = processor.getValueTreeState().getParameter ("frequency"))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (
                    parameter->convertTo0to1 (static_cast<float> (i)));
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
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (
        juce::MathConstants<float>::pi * 1.18f,
        juce::MathConstants<float>::pi * 2.82f,
        true);

    slider.setTextBoxStyle (
        juce::Slider::TextBoxBelow,
        false,
        78,
        18);

    slider.setNumDecimalPlacesToDisplay (1);
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colour (0xffa5a7a4));
    label.setFont (uiFont (9.2f, true));
    addAndMakeVisible (label);
}

void OpalAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (
        juce::Colour (0xff25272a), 0.0f, 0.0f,
        juce::Colour (0xff08090b), 0.0f, static_cast<float> (getHeight()), false);

    background.addColour (0.28, juce::Colour (0xff17191c));
    background.addColour (0.72, juce::Colour (0xff0c0e11));

    g.setGradientFill (background);
    g.fillAll();

    auto chassis = getLocalBounds().toFloat().reduced (9.0f);

    juce::ColourGradient metal (
        juce::Colour (0xff393b3f), chassis.getX(), chassis.getY(),
        juce::Colour (0xff101216), chassis.getRight(), chassis.getBottom(), false);

    metal.addColour (0.18, juce::Colour (0xff292b2f));
    metal.addColour (0.48, juce::Colour (0xff17191d));
    metal.addColour (0.78, juce::Colour (0xff222428));

    g.setGradientFill (metal);
    g.fillRoundedRectangle (chassis, 13.0f);

    for (int y = 15; y < getHeight() - 15; y += 4)
    {
        const auto alpha = static_cast<juce::uint8> ((y % 12 == 0) ? 9 : 4);
        g.setColour (juce::Colour::fromRGBA (255, 255, 255, alpha));
        g.drawHorizontalLine (
            y,
            15.0f,
            static_cast<float> (getWidth() - 15));
    }

    g.setColour (juce::Colour (0x805c6066));
    g.drawRoundedRectangle (chassis, 13.0f, 1.0f);

    auto upperBay = juce::Rectangle<float> (
        20.0f,
        22.0f,
        static_cast<float> (getWidth() - 40),
        238.0f);

    g.setColour (juce::Colour (0xff0b0d10));
    g.fillRoundedRectangle (upperBay, 10.0f);

    juce::ColourGradient upperEdge (
        juce::Colour (0x806ae7ee), upperBay.getX(), upperBay.getCentreY(),
        juce::Colour (0x60ee8fcf), upperBay.getRight(), upperBay.getCentreY(), false);

    upperEdge.addColour (0.48, juce::Colour (0x709b8cff));
    upperEdge.addColour (0.74, juce::Colour (0x608ff6c8));

    g.setGradientFill (upperEdge);
    g.drawRoundedRectangle (upperBay, 10.0f, 1.0f);

    auto readout = juce::Rectangle<float> (565.0f, 79.0f, 150.0f, 88.0f);

    juce::ColourGradient readoutMetal (
        juce::Colour (0xff31343a), readout.getX(), readout.getY(),
        juce::Colour (0xff121419), readout.getRight(), readout.getBottom(), false);

    readoutMetal.addColour (0.48, juce::Colour (0xff202329));

    g.setGradientFill (readoutMetal);
    g.fillRoundedRectangle (readout, 7.0f);

    g.setColour (juce::Colour (0x80616770));
    g.drawRoundedRectangle (readout, 7.0f, 1.0f);

    auto selectorRail = juce::Rectangle<float> (
        20.0f,
        276.0f,
        static_cast<float> (getWidth() - 40),
        76.0f);

    g.setColour (juce::Colour (0xff101216));
    g.fillRoundedRectangle (selectorRail, 8.0f);

    g.setColour (juce::Colour (0x70545b64));
    g.drawRoundedRectangle (selectorRail, 8.0f, 1.0f);

    auto lowerDeck = juce::Rectangle<float> (
        20.0f,
        372.0f,
        static_cast<float> (getWidth() - 40),
        112.0f);

    juce::ColourGradient lowerMetal (
        juce::Colour (0xff26292d), lowerDeck.getX(), lowerDeck.getY(),
        juce::Colour (0xff0d0f12), lowerDeck.getRight(), lowerDeck.getBottom(), false);

    lowerMetal.addColour (0.52, juce::Colour (0xff171a1e));

    g.setGradientFill (lowerMetal);
    g.fillRoundedRectangle (lowerDeck, 9.0f);

    g.setColour (juce::Colour (0x70575e66));
    g.drawRoundedRectangle (lowerDeck, 9.0f, 1.0f);

    auto nameplate = titleLabel.getBounds().toFloat().expanded (13.0f, 3.0f);

    juce::ColourGradient plate (
        juce::Colour (0xff4a4d52), nameplate.getX(), nameplate.getY(),
        juce::Colour (0xff15171b), nameplate.getRight(), nameplate.getBottom(), false);

    plate.addColour (0.48, juce::Colour (0xff2a2d32));

    g.setGradientFill (plate);
    g.fillRoundedRectangle (nameplate, 5.0f);

    g.setColour (juce::Colour (0x80727880));
    g.drawRoundedRectangle (nameplate, 5.0f, 1.0f);

    auto drawScrew = [&g] (float x, float y)
    {
        juce::ColourGradient screw (
            juce::Colour (0xff7d8186), x - 4.0f, y - 4.0f,
            juce::Colour (0xff25282c), x + 4.0f, y + 4.0f, false);

        g.setGradientFill (screw);
        g.fillEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f);

        g.setColour (juce::Colour (0xff111316));
        g.drawLine (x - 2.2f, y + 1.0f, x + 2.2f, y - 1.0f, 1.0f);
    };

    drawScrew (24.0f, 24.0f);
    drawScrew (756.0f, 24.0f);
    drawScrew (24.0f, 476.0f);
    drawScrew (756.0f, 476.0f);
}

void OpalAudioProcessorEditor::OpalStone::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (9.0f);
    const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());

    bounds = bounds.withSizeKeepingCentre (size, size);

    juce::Path stone;
    stone.addEllipse (bounds);

    juce::DropShadow (
        juce::Colour (0xb0000000),
        18,
        { 0, 8 }).drawForPath (g, stone);

    auto bezel = bounds.expanded (8.0f);

    juce::ColourGradient bezelGradient (
        juce::Colour (0xff93979a), bezel.getX(), bezel.getY(),
        juce::Colour (0xff181b20), bezel.getRight(), bezel.getBottom(), false);

    bezelGradient.addColour (0.28, juce::Colour (0xff555a60));
    bezelGradient.addColour (0.72, juce::Colour (0xff282c31));

    g.setGradientFill (bezelGradient);
    g.fillEllipse (bezel);

    g.setColour (juce::Colour (0xff05070a));
    g.fillEllipse (bounds.expanded (2.5f));

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (stone);

        const auto cx = bounds.getCentreX();
        const auto cy = bounds.getCentreY();

        juce::ColourGradient pearl (
            juce::Colour (0xfff0f5ef), cx - size * 0.27f, cy - size * 0.35f,
            juce::Colour (0xff4c5962), cx + size * 0.34f, cy + size * 0.39f, true);

        pearl.addColour (0.20, juce::Colour (0xffc7e5d8));
        pearl.addColour (0.46, juce::Colour (0xff81949c));
        pearl.addColour (0.75, juce::Colour (0xff505861));
        pearl.addColour (0.95, juce::Colour (0xff24292e));

        g.setGradientFill (pearl);
        g.fillRect (bounds);

        const auto activeGlow = activation * (0.30f + 0.70f * energy);

        const std::array<float, 6> hues {
            0.49f, 0.56f, 0.76f, 0.91f, 0.38f, 0.64f
        };

        for (int i = 0; i < static_cast<int> (hues.size()); ++i)
        {
            const auto t = phase * (0.38f + 0.07f * static_cast<float> (i));

            const auto px =
                cx
                + std::sin (t + i * 1.43f)
                * size
                * (0.10f + 0.014f * i);

            const auto py =
                cy
                + std::cos (t * 0.83f + i * 1.03f)
                * size
                * (0.10f + 0.011f * i);

            const auto radius =
                size * (0.22f + 0.018f * i);

            auto core = opalColour (
                hues[static_cast<size_t> (i)] + phase * 0.007f,
                0.56f,
                1.0f,
                0.08f + activeGlow * 0.28f);

            juce::ColourGradient fire (
                core,
                px,
                py,
                core.withAlpha (0.0f),
                px + radius,
                py,
                true);

            fire.addColour (
                0.34,
                core.withAlpha (core.getFloatAlpha() * 0.78f));

            g.setGradientFill (fire);

            g.fillEllipse (
                px - radius,
                py - radius,
                radius * 2.0f,
                radius * 2.0f);
        }

        if (activation > 0.001f)
        {
            for (int band = 0; band < 8; ++band)
            {
                juce::Path wave;

                const auto baseY =
                    bounds.getY()
                    + size * (0.13f + 0.095f * static_cast<float> (band));

                const auto amplitude =
                    size * (0.012f + 0.026f * activeGlow);

                const auto speed =
                    phase * (0.46f + 0.052f * static_cast<float> (band));

                for (int step = 0; step <= 38; ++step)
                {
                    const auto norm = static_cast<float> (step) / 38.0f;
                    const auto x = bounds.getX() + norm * size;

                    const auto y =
                        baseY
                        + std::sin (
                              norm
                              * juce::MathConstants<float>::twoPi
                              * 1.85f
                              + speed
                              + static_cast<float> (band) * 0.51f)
                              * amplitude
                        + std::sin (
                              norm
                              * juce::MathConstants<float>::twoPi
                              * 3.1f
                              - speed * 0.58f)
                              * amplitude
                              * 0.36f;

                    if (step == 0)
                        wave.startNewSubPath (x, y);
                    else
                        wave.lineTo (x, y);
                }

                g.setColour (
                    opalColour (
                        0.48f
                        + 0.024f * static_cast<float> (band)
                        + phase * 0.003f,
                        0.65f,
                        1.0f,
                        0.06f + 0.25f * activeGlow));

                g.strokePath (
                    wave,
                    juce::PathStrokeType (
                        0.9f + 1.6f * activeGlow,
                        juce::PathStrokeType::curved,
                        juce::PathStrokeType::rounded));
            }

            const auto poolRadius =
                size * (0.20f + 0.10f * activeGlow);

            const auto poolX =
                cx + std::sin (phase * 0.67f) * size * 0.12f;

            const auto poolY =
                cy + std::cos (phase * 0.52f) * size * 0.09f;

            auto aqua =
                juce::Colour::fromFloatRGBA (
                    0.23f,
                    0.95f,
                    1.0f,
                    0.15f + 0.26f * activeGlow);

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
        }

        g.setColour (juce::Colour (0x42ffffff));

        g.fillEllipse (
            bounds.withSizeKeepingCentre (
                size * 0.70f,
                size * 0.22f)
            .translated (
                -size * 0.10f,
                -size * 0.29f));
    }

    g.setColour (juce::Colour (0xa0ffffff));
    g.drawEllipse (bounds.reduced (0.8f), 1.0f);

    g.setColour (juce::Colour (0x30ffffff));
    g.drawEllipse (bounds.reduced (5.0f), 0.8f);
}

void OpalAudioProcessorEditor::timerCallback()
{
    const auto opalAmount =
        processor.getValueTreeState().getRawParameterValue ("opal")->load()
        * 0.01f;

    if (opalAmount > 0.001f)
    {
        animationPhase += 0.009f + 0.032f * opalAmount;

        if (animationPhase > juce::MathConstants<float>::twoPi * 12.0f)
            animationPhase = 0.0f;
    }

    opalStone.setEnergyDb (processor.getResonanceEnergyDb());
    opalStone.setOpalAmount (opalAmount);
    opalStone.setPhase (animationPhase);

    const auto selectedIndex =
        OpalFrequencyData::clampIndex (
            juce::roundToInt (
                processor.getValueTreeState()
                    .getRawParameterValue ("frequency")
                    ->load()));

    for (int i = 0; i < static_cast<int> (frequencyButtons.size()); ++i)
    {
        frequencyButtons[static_cast<size_t> (i)]
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
        OpalFrequencyData::entries[static_cast<size_t> (index)];

    frequencyInfoLabel.setText (
        juce::String (static_cast<int> (entry.hz))
        + " Hz\n"
        + juce::String (entry.label).toUpperCase(),
        juce::dontSendNotification);
}

void OpalAudioProcessorEditor::resized()
{
    const auto centreX = getWidth() / 2;

    frequencyKnob.setBounds (55, 72, 145, 135);
    frequencyLabel.setBounds (67, 204, 120, 17);

    opalStone.setBounds (
        centreX - 94,
        48,
        188,
        188);

    titleLabel.setBounds (
        centreX - 58,
        231,
        116,
        22);

    frequencyInfoLabel.setBounds (
        570,
        89,
        140,
        68);

    constexpr int columns = 9;
    constexpr int cellGap = 4;

    const int gridX = 28;
    const int gridY = 286;
    const int gridWidth = getWidth() - 56;

    const int cellWidth =
        (gridWidth - (columns - 1) * cellGap)
        / columns;

    const int cellHeight = 26;

    for (int i = 0; i < static_cast<int> (frequencyButtons.size()); ++i)
    {
        const auto row = i / columns;
        const auto col = i % columns;

        frequencyButtons[static_cast<size_t> (i)]
            ->setBounds (
                gridX + col * (cellWidth + cellGap),
                gridY + row * (cellHeight + cellGap),
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

    const int knobY = 383;
    const int knobWidth = 145;
    const int spacing = 56;

    const int totalWidth =
        static_cast<int> (knobs.size()) * knobWidth
        + (static_cast<int> (knobs.size()) - 1) * spacing;

    const int startX =
        (getWidth() - totalWidth) / 2;

    for (int i = 0; i < static_cast<int> (knobs.size()); ++i)
    {
        const auto x =
            startX + i * (knobWidth + spacing);

        knobs[static_cast<size_t> (i)]
            ->setBounds (
                x,
                knobY,
                knobWidth,
                88);

        labels[static_cast<size_t> (i)]
            ->setBounds (
                x + 10,
                468,
                knobWidth - 20,
                17);
    }
}

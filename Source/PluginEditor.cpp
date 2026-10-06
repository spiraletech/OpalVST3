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
}

OpalAudioProcessorEditor::OpalAudioProcessorEditor (OpalAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    setSize (780, 520);

    titleLabel.setText ("OPAL", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xfff1eee7));
    titleLabel.setFont (juce::Font (18.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("MONO RESONANCE INSTRUMENT • ZERO LATENCY", juce::dontSendNotification);
    subtitleLabel.setJustificationType (juce::Justification::centredRight);
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8d928f));
    subtitleLabel.setFont (juce::Font (8.8f, juce::Font::plain));
    addAndMakeVisible (subtitleLabel);

    frequencyInfoLabel.setJustificationType (juce::Justification::centred);
    frequencyInfoLabel.setColour (juce::Label::textColourId, juce::Colour (0xffece9e1));
    frequencyInfoLabel.setFont (juce::Font (12.5f, juce::Font::bold));
    addAndMakeVisible (frequencyInfoLabel);

    grLabel.setText ("GAIN REDUCTION", juce::dontSendNotification);
    grLabel.setJustificationType (juce::Justification::centred);
    grLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8b8f8d));
    grLabel.setFont (juce::Font (8.5f, juce::Font::bold));
    addAndMakeVisible (grLabel);

    configureKnob (frequencyKnob, frequencyLabel, "FREQUENCY");
    configureKnob (boostKnob, boostLabel, "BOOST");
    configureKnob (opalKnob, opalLabel, "OPAL");
    configureKnob (mixKnob, mixLabel, "MIX");

    frequencyKnob.setRange (0.0,
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

    antiPhaseButton.setName ("AntiPhase");
    antiPhaseButton.setButtonText ("Ø");
    antiPhaseButton.setClickingTogglesState (true);
    antiPhaseButton.setTooltip ("Invert OPAL processed polarity");
    addAndMakeVisible (antiPhaseButton);

    addAndMakeVisible (opalStone);
    addAndMakeVisible (grMeter);

    auto& state = processor.getValueTreeState();

    frequencyAttachment = std::make_unique<SliderAttachment> (state, "frequency", frequencyKnob);
    boostAttachment = std::make_unique<SliderAttachment> (state, "boost", boostKnob);
    opalAttachment = std::make_unique<SliderAttachment> (state, "opal", opalKnob);
    mixAttachment = std::make_unique<SliderAttachment> (state, "mix", mixKnob);
    antiPhaseAttachment = std::make_unique<ButtonAttachment> (state, "antiPhase", antiPhaseButton);

    frequencyButtons.reserve (OpalFrequencyData::entries.size());

    for (int i = 0; i < static_cast<int> (OpalFrequencyData::entries.size()); ++i)
    {
        const auto& entry = OpalFrequencyData::entries[static_cast<size_t> (i)];
        auto button = std::make_unique<juce::TextButton>();

        button->setClickingTogglesState (false);
        button->setButtonText (
            juce::String (static_cast<int> (entry.hz)) + "\n" + entry.label);
        button->setTooltip (
            juce::String (static_cast<int> (entry.hz)) + " Hz — " + entry.label);

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
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.18f,
                                juce::MathConstants<float>::pi * 2.82f,
                                true);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
    slider.setNumDecimalPlacesToDisplay (1);
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colour (0xff999d98));
    label.setFont (juce::Font (9.0f, juce::Font::bold));
    addAndMakeVisible (label);
}

void OpalAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (
        juce::Colour (0xff24221f), 0.0f, 0.0f,
        juce::Colour (0xff080908), 0.0f, static_cast<float> (getHeight()), false);
    background.addColour (0.24, juce::Colour (0xff181815));
    background.addColour (0.68, juce::Colour (0xff0d0f0e));
    g.setGradientFill (background);
    g.fillAll();

    auto panel = getLocalBounds().toFloat().reduced (8.0f);

    juce::ColourGradient chassis (
        juce::Colour (0xff34322e), panel.getX(), panel.getY(),
        juce::Colour (0xff101210), panel.getRight(), panel.getBottom(), false);
    chassis.addColour (0.23, juce::Colour (0xff252521));
    chassis.addColour (0.70, juce::Colour (0xff151714));
    g.setGradientFill (chassis);
    g.fillRoundedRectangle (panel, 12.0f);

    // Fine brushed-metal grain.
    for (int y = 14; y < getHeight() - 14; y += 3)
    {
        const auto alpha = static_cast<juce::uint8> ((y % 9 == 0) ? 11 : 5);
        g.setColour (juce::Colour::fromRGBA (255, 252, 242, alpha));
        g.drawHorizontalLine (y, 15.0f, static_cast<float> (getWidth() - 15));
    }

    g.setColour (juce::Colour (0x804a4943));
    g.drawRoundedRectangle (panel, 12.0f, 1.2f);

    auto upperBay = juce::Rectangle<float> (
        18.0f, 48.0f, static_cast<float> (getWidth() - 36), 214.0f);
    g.setColour (juce::Colour (0xff0a0c0b));
    g.fillRoundedRectangle (upperBay, 8.0f);
    g.setColour (juce::Colour (0x80615f57));
    g.drawRoundedRectangle (upperBay, 8.0f, 1.0f);

    auto selectorRail = juce::Rectangle<float> (
        18.0f, 286.0f, static_cast<float> (getWidth() - 36), 84.0f);
    g.setColour (juce::Colour (0xff111310));
    g.fillRoundedRectangle (selectorRail, 7.0f);
    g.setColour (juce::Colour (0x60534f48));
    g.drawRoundedRectangle (selectorRail, 7.0f, 1.0f);

    auto lowerDeck = juce::Rectangle<float> (
        18.0f, 386.0f, static_cast<float> (getWidth() - 36), 118.0f);
    juce::ColourGradient lowerMetal (
        juce::Colour (0xff20211d), lowerDeck.getX(), lowerDeck.getY(),
        juce::Colour (0xff0a0c0a), lowerDeck.getRight(), lowerDeck.getBottom(), false);
    g.setGradientFill (lowerMetal);
    g.fillRoundedRectangle (lowerDeck, 8.0f);
    g.setColour (juce::Colour (0x70524f48));
    g.drawRoundedRectangle (lowerDeck, 8.0f, 1.0f);

    auto drawScrew = [&g] (float x, float y)
    {
        juce::ColourGradient screw (
            juce::Colour (0xff7c7970), x - 4.0f, y - 4.0f,
            juce::Colour (0xff242621), x + 4.0f, y + 4.0f, false);
        g.setGradientFill (screw);
        g.fillEllipse (x - 5.0f, y - 5.0f, 10.0f, 10.0f);
        g.setColour (juce::Colour (0xff10110f));
        g.drawLine (x - 2.7f, y + 1.4f, x + 2.7f, y - 1.4f, 1.1f);
    };

    drawScrew (22.0f, 22.0f);
    drawScrew (758.0f, 22.0f);
    drawScrew (22.0f, 498.0f);
    drawScrew (758.0f, 498.0f);

    g.setColour (juce::Colour (0xff9e9b91));
    g.setFont (juce::Font (8.3f, juce::Font::bold));
    g.drawText ("SPIRALTECH • FUTURE ANALOGUE", 30, 15, 220, 18,
                juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff74776f));
    g.setFont (juce::Font (8.0f, juce::Font::bold));
    g.drawText ("RESONANCE FREQUENCY", 28, 270, 170, 16,
                juce::Justification::centredLeft);

    auto nameplate = titleLabel.getBounds().toFloat().expanded (12.0f, 3.0f);
    juce::ColourGradient plate (
        juce::Colour (0xff535049), nameplate.getX(), nameplate.getY(),
        juce::Colour (0xff181a17), nameplate.getRight(), nameplate.getBottom(), false);
    plate.addColour (0.48, juce::Colour (0xff2d2e29));
    g.setGradientFill (plate);
    g.fillRoundedRectangle (nameplate, 4.0f);
    g.setColour (juce::Colour (0x90827e73));
    g.drawRoundedRectangle (nameplate, 4.0f, 1.0f);

    g.setColour (juce::Colour (0xff666961));
    g.setFont (juce::Font (7.8f, juce::Font::plain));
    g.drawText ("OPAL = TUNED REVERB • HARMONIC INFORMATION • RESONANCE",
                230, 371, 520, 14, juce::Justification::centredRight);
}

void OpalAudioProcessorEditor::OpalStone::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (9.0f);
    const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    bounds = bounds.withSizeKeepingCentre (size, size);

    juce::Path stone;
    stone.addEllipse (bounds);

    juce::DropShadow (juce::Colour (0xb0000000), 18, { 0, 8 }).drawForPath (g, stone);

    auto bezel = bounds.expanded (8.0f);
    juce::ColourGradient bezelGradient (
        juce::Colour (0xff8b887e), bezel.getX(), bezel.getY(),
        juce::Colour (0xff151713), bezel.getRight(), bezel.getBottom(), false);
    bezelGradient.addColour (0.30, juce::Colour (0xff484941));
    bezelGradient.addColour (0.74, juce::Colour (0xff252822));
    g.setGradientFill (bezelGradient);
    g.fillEllipse (bezel);

    g.setColour (juce::Colour (0xff050706));
    g.fillEllipse (bounds.expanded (2.5f));

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (stone);

        const auto cx = bounds.getCentreX();
        const auto cy = bounds.getCentreY();

        juce::ColourGradient pearl (
            juce::Colour (0xffeff5ea), cx - size * 0.26f, cy - size * 0.34f,
            juce::Colour (0xff46545a), cx + size * 0.34f, cy + size * 0.38f, true);
        pearl.addColour (0.22, juce::Colour (0xffbfded1));
        pearl.addColour (0.50, juce::Colour (0xff748b92));
        pearl.addColour (0.78, juce::Colour (0xff454d56));
        pearl.addColour (0.95, juce::Colour (0xff22272a));
        g.setGradientFill (pearl);
        g.fillRect (bounds);

        const auto activeGlow = activation * (0.38f + 0.62f * energy);

        // Opal fire remains subtle at rest and blooms when the OPAL circuit is active.
        const std::array<float, 6> hueOffsets { 0.49f, 0.56f, 0.76f, 0.91f, 0.38f, 0.64f };

        for (int i = 0; i < static_cast<int> (hueOffsets.size()); ++i)
        {
            const auto t = phase * (0.42f + 0.08f * static_cast<float> (i));
            const auto px = cx + std::sin (t + i * 1.41f) * size * (0.11f + 0.014f * i);
            const auto py = cy + std::cos (t * 0.81f + i * 1.07f) * size * (0.11f + 0.012f * i);
            const auto radius = size * (0.23f + 0.018f * i);

            auto core = opalColour (
                hueOffsets[static_cast<size_t> (i)] + phase * 0.008f,
                0.58f,
                1.0f,
                0.10f + activeGlow * 0.30f);

            juce::ColourGradient fire (
                core, px, py, core.withAlpha (0.0f), px + radius, py, true);
            fire.addColour (0.34, core.withAlpha (core.getFloatAlpha() * 0.78f));
            g.setGradientFill (fire);
            g.fillEllipse (px - radius, py - radius, radius * 2.0f, radius * 2.0f);
        }

        if (activation > 0.001f)
        {
            // Aqua-liquid caustics: only animate while OPAL is engaged.
            for (int band = 0; band < 8; ++band)
            {
                juce::Path wave;
                const auto baseY = bounds.getY()
                                 + size * (0.13f + 0.095f * static_cast<float> (band));
                const auto amplitude = size * (0.013f + 0.026f * activeGlow);
                const auto speed = phase * (0.48f + 0.055f * static_cast<float> (band));

                for (int step = 0; step <= 36; ++step)
                {
                    const auto norm = static_cast<float> (step) / 36.0f;
                    const auto x = bounds.getX() + norm * size;
                    const auto y = baseY
                                 + std::sin (
                                       norm * juce::MathConstants<float>::twoPi * 1.9f
                                       + speed
                                       + static_cast<float> (band) * 0.52f)
                                       * amplitude
                                 + std::sin (
                                       norm * juce::MathConstants<float>::twoPi * 3.2f
                                       - speed * 0.62f)
                                       * amplitude * 0.38f;

                    if (step == 0)
                        wave.startNewSubPath (x, y);
                    else
                        wave.lineTo (x, y);
                }

                g.setColour (opalColour (
                    0.48f + 0.025f * static_cast<float> (band) + phase * 0.003f,
                    0.66f,
                    1.0f,
                    0.08f + 0.26f * activeGlow));

                g.strokePath (
                    wave,
                    juce::PathStrokeType (
                        0.9f + 1.7f * activeGlow,
                        juce::PathStrokeType::curved,
                        juce::PathStrokeType::rounded));
            }

            // Moving aqua refraction pool.
            const auto poolRadius = size * (0.22f + 0.11f * activeGlow);
            const auto poolX = cx + std::sin (phase * 0.67f) * size * 0.13f;
            const auto poolY = cy + std::cos (phase * 0.53f) * size * 0.10f;

            auto aqua = juce::Colour::fromFloatRGBA (
                0.25f, 0.98f, 1.0f, 0.18f + 0.26f * activeGlow);

            juce::ColourGradient pool (
                aqua, poolX, poolY,
                aqua.withAlpha (0.0f), poolX + poolRadius, poolY, true);
            g.setGradientFill (pool);
            g.fillEllipse (
                poolX - poolRadius,
                poolY - poolRadius,
                poolRadius * 2.0f,
                poolRadius * 2.0f);

            for (int ripple = 0; ripple < 3; ++ripple)
            {
                const auto r = size * (0.22f + 0.09f * static_cast<float> (ripple))
                             + std::sin (phase * 0.8f + ripple) * size * 0.015f;

                g.setColour (juce::Colour::fromFloatRGBA (
                    0.68f, 1.0f, 1.0f, 0.07f + 0.13f * activeGlow));

                g.drawEllipse (
                    juce::Rectangle<float> (r, r)
                        .withCentre ({
                            cx + std::sin (phase * 0.45f + ripple) * size * 0.04f,
                            cy + std::cos (phase * 0.38f + ripple) * size * 0.035f
                        }),
                    0.8f + activeGlow);
            }
        }

        g.setColour (juce::Colour (0x40ffffff));
        g.fillEllipse (
            bounds.withSizeKeepingCentre (size * 0.70f, size * 0.23f)
                  .translated (-size * 0.10f, -size * 0.29f));
    }

    g.setColour (juce::Colour (0xa0ffffff));
    g.drawEllipse (bounds.reduced (0.8f), 1.0f);
    g.setColour (juce::Colour (0x30ffffff));
    g.drawEllipse (bounds.reduced (5.0f), 0.8f);
}

void OpalAudioProcessorEditor::GRMeter::paint (juce::Graphics& g)
{
    auto outer = getLocalBounds().toFloat().reduced (2.0f);

    juce::ColourGradient frame (
        juce::Colour (0xff747168), outer.getX(), outer.getY(),
        juce::Colour (0xff171914), outer.getRight(), outer.getBottom(), false);
    frame.addColour (0.40, juce::Colour (0xff383a33));
    g.setGradientFill (frame);
    g.fillRoundedRectangle (outer, 6.0f);

    auto window = outer.reduced (6.0f);
    juce::ColourGradient paper (
        juce::Colour (0xffdad2b8), window.getX(), window.getY(),
        juce::Colour (0xff9e967e), window.getRight(), window.getBottom(), false);
    paper.addColour (0.45, juce::Colour (0xffc7c0a8));
    g.setGradientFill (paper);
    g.fillRoundedRectangle (window, 3.0f);

    g.setColour (juce::Colour (0xff27271f));
    g.drawRoundedRectangle (window, 3.0f, 1.0f);

    const auto centre = juce::Point<float> (
        window.getCentreX(),
        window.getBottom() + window.getHeight() * 0.26f);

    const auto radius = window.getWidth() * 0.43f;
    constexpr float startAngle = -1.05f;
    constexpr float endAngle = 1.05f;

    const std::array<float, 7> values { 24.0f, 20.0f, 15.0f, 10.0f, 6.0f, 3.0f, 0.0f };

    g.setFont (juce::Font (7.2f, juce::Font::bold));

    for (const auto value : values)
    {
        const auto norm = 1.0f - value / 24.0f;
        const auto angle = startAngle + norm * (endAngle - startAngle);

        const auto innerR = radius * 0.78f;
        const auto outerR = radius * 0.92f;

        const auto x1 = centre.x + std::sin (angle) * innerR;
        const auto y1 = centre.y - std::cos (angle) * innerR;
        const auto x2 = centre.x + std::sin (angle) * outerR;
        const auto y2 = centre.y - std::cos (angle) * outerR;

        g.setColour (juce::Colour (0xff38372e));
        g.drawLine (x1, y1, x2, y2, 1.0f);

        const auto textR = radius * 0.64f;
        const auto tx = centre.x + std::sin (angle) * textR;
        const auto ty = centre.y - std::cos (angle) * textR;

        g.drawText (
            juce::String (static_cast<int> (value)),
            juce::Rectangle<int> (
                static_cast<int> (tx - 10.0f),
                static_cast<int> (ty - 6.0f),
                20,
                12),
            juce::Justification::centred);
    }

    const auto reduction = juce::jlimit (0.0f, 24.0f, displayedReduction);
    const auto norm = 1.0f - reduction / 24.0f;
    const auto needleAngle = startAngle + norm * (endAngle - startAngle);

    const auto needleEnd = juce::Point<float> (
        centre.x + std::sin (needleAngle) * radius * 0.83f,
        centre.y - std::cos (needleAngle) * radius * 0.83f);

    g.setColour (juce::Colour (0xff8b1712));
    g.drawLine (centre.x, centre.y, needleEnd.x, needleEnd.y, 1.5f);

    g.setColour (juce::Colour (0xff24251f));
    g.fillEllipse (centre.x - 5.0f, centre.y - 5.0f, 10.0f, 10.0f);

    g.setColour (juce::Colour (0xff5b1815));
    g.setFont (juce::Font (7.6f, juce::Font::bold));
    g.drawText (
        "GR dB",
        static_cast<int> (window.getX()),
        static_cast<int> (window.getY() + 4.0f),
        static_cast<int> (window.getWidth()),
        12,
        juce::Justification::centred);
}

void OpalAudioProcessorEditor::timerCallback()
{
    const auto opalAmount =
        processor.getValueTreeState().getRawParameterValue ("opal")->load() * 0.01f;

    // The aqua motion wakes only while the OPAL circuit is engaged.
    if (opalAmount > 0.001f)
    {
        animationPhase += 0.010f + 0.035f * opalAmount;

        if (animationPhase > juce::MathConstants<float>::twoPi * 12.0f)
            animationPhase = 0.0f;
    }

    const auto db = processor.getResonanceEnergyDb();

    opalStone.setEnergyDb (db);
    opalStone.setOpalAmount (opalAmount);
    opalStone.setPhase (animationPhase);

    grMeter.setReductionDb (processor.getGainReductionDb());
    grMeter.tick();

    const auto selectedIndex = OpalFrequencyData::clampIndex (
        juce::roundToInt (
            processor.getValueTreeState().getRawParameterValue ("frequency")->load()));

    for (int i = 0; i < static_cast<int> (frequencyButtons.size()); ++i)
        frequencyButtons[static_cast<size_t> (i)]->setToggleState (
            i == selectedIndex, juce::dontSendNotification);

    updateFrequencyInfo();
}

void OpalAudioProcessorEditor::updateFrequencyInfo()
{
    const auto index = OpalFrequencyData::clampIndex (
        juce::roundToInt (
            processor.getValueTreeState().getRawParameterValue ("frequency")->load()));

    const auto& entry = OpalFrequencyData::entries[static_cast<size_t> (index)];

    frequencyInfoLabel.setText (
        juce::String (static_cast<int> (entry.hz))
            + " Hz  •  "
            + entry.label,
        juce::dontSendNotification);
}

void OpalAudioProcessorEditor::resized()
{
    const auto centreX = getWidth() / 2;

    subtitleLabel.setBounds (445, 14, 300, 18);

    frequencyKnob.setBounds (62, 82, 128, 122);
    frequencyLabel.setBounds (72, 201, 108, 16);

    opalStone.setBounds (centreX - 93, 60, 186, 186);
    frequencyInfoLabel.setBounds (centreX - 130, 235, 260, 21);
    titleLabel.setBounds (centreX - 58, 264, 116, 22);

    grMeter.setBounds (566, 83, 166, 112);
    grLabel.setBounds (582, 195, 134, 17);
    antiPhaseButton.setBounds (624, 218, 50, 36);

    constexpr int columns = 9;
    constexpr int cellGap = 4;
    const int gridX = 27;
    const int gridY = 304;
    const int gridWidth = getWidth() - 54;
    const int cellWidth = (gridWidth - (columns - 1) * cellGap) / columns;
    const int cellHeight = 27;

    for (int i = 0; i < static_cast<int> (frequencyButtons.size()); ++i)
    {
        const auto row = i / columns;
        const auto col = i % columns;

        frequencyButtons[static_cast<size_t> (i)]->setBounds (
            gridX + col * (cellWidth + cellGap),
            gridY + row * (cellHeight + cellGap),
            cellWidth,
            cellHeight);
    }

    std::array<juce::Slider*, 3> knobs {
        &boostKnob, &opalKnob, &mixKnob
    };

    std::array<juce::Label*, 3> labels {
        &boostLabel, &opalLabel, &mixLabel
    };

    const int knobY = 397;
    const int knobWidth = 150;
    const int spacing = 38;
    const int totalWidth = static_cast<int> (knobs.size()) * knobWidth
                         + (static_cast<int> (knobs.size()) - 1) * spacing;
    const int startX = (getWidth() - totalWidth) / 2;

    for (int i = 0; i < static_cast<int> (knobs.size()); ++i)
    {
        const auto x = startX + i * (knobWidth + spacing);
        knobs[static_cast<size_t> (i)]->setBounds (x, knobY, knobWidth, 90);
        labels[static_cast<size_t> (i)]->setBounds (x + 8, 485, knobWidth - 16, 16);
    }
}

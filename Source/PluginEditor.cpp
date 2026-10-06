#include "PluginEditor.h"
#include <cmath>

namespace
{
    juce::Colour opalColour (float hue, float saturation = 0.62f, float brightness = 1.0f, float alpha = 1.0f)
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
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xfff3f5f8));
    titleLabel.setFont (juce::Font (18.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("HARMONIC RESONANCE FIELD", juce::dontSendNotification);
    subtitleLabel.setJustificationType (juce::Justification::centredRight);
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff7f8793));
    subtitleLabel.setFont (juce::Font (9.5f, juce::Font::plain));
    addAndMakeVisible (subtitleLabel);

    frequencyInfoLabel.setJustificationType (juce::Justification::centred);
    frequencyInfoLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe9ecf4));
    frequencyInfoLabel.setFont (juce::Font (12.5f, juce::Font::bold));
    addAndMakeVisible (frequencyInfoLabel);

    meterLabel.setText ("FIELD LVL", juce::dontSendNotification);
    meterLabel.setJustificationType (juce::Justification::centred);
    meterLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8c929d));
    meterLabel.setFont (juce::Font (8.8f, juce::Font::bold));
    addAndMakeVisible (meterLabel);

    configureKnob (frequencyKnob, frequencyLabel, "FREQ");
    configureKnob (boostKnob, boostLabel, "BOOST");
    configureKnob (harmonicsKnob, harmonicsLabel, "HARMONICS");
    configureKnob (spaceKnob, spaceLabel, "SPACE");
    configureKnob (widthKnob, widthLabel, "WIDTH");
    configureKnob (fieldKnob, fieldLabel, "FIELD");
    configureKnob (mixKnob, mixLabel, "MIX");

    frequencyKnob.setRange (0.0, static_cast<double> (OpalFrequencyData::entries.size() - 1), 1.0);
    frequencyKnob.textFromValueFunction = [] (double value)
    {
        const auto index = OpalFrequencyData::clampIndex (juce::roundToInt (value));
        return juce::String (static_cast<int> (OpalFrequencyData::entries[static_cast<size_t> (index)].hz)) + " Hz";
    };

    boostKnob.setTextValueSuffix (" dB");
    harmonicsKnob.setTextValueSuffix (" %");
    spaceKnob.setTextValueSuffix (" %");
    widthKnob.setTextValueSuffix (" %");
    fieldKnob.setTextValueSuffix (" %");
    mixKnob.setTextValueSuffix (" %");

    addAndMakeVisible (opalStone);
    addAndMakeVisible (energyMeter);

    auto& state = processor.getValueTreeState();

    frequencyAttachment = std::make_unique<SliderAttachment> (state, "frequency", frequencyKnob);
    boostAttachment = std::make_unique<SliderAttachment> (state, "boost", boostKnob);
    harmonicsAttachment = std::make_unique<SliderAttachment> (state, "harmonics", harmonicsKnob);
    spaceAttachment = std::make_unique<SliderAttachment> (state, "space", spaceKnob);
    widthAttachment = std::make_unique<SliderAttachment> (state, "width", widthKnob);
    fieldAttachment = std::make_unique<SliderAttachment> (state, "field", fieldKnob);
    mixAttachment = std::make_unique<SliderAttachment> (state, "mix", mixKnob);

    frequencyButtons.reserve (OpalFrequencyData::entries.size());

    for (int i = 0; i < static_cast<int> (OpalFrequencyData::entries.size()); ++i)
    {
        const auto& entry = OpalFrequencyData::entries[static_cast<size_t> (i)];
        auto button = std::make_unique<juce::TextButton>();

        button->setClickingTogglesState (false);
        button->setButtonText (juce::String (static_cast<int> (entry.hz)) + "\n" + entry.label);
        button->setTooltip (juce::String (static_cast<int> (entry.hz)) + " Hz — " + entry.label);

        button->onClick = [this, i]
        {
            if (auto* parameter = processor.getValueTreeState().getParameter ("frequency"))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (i)));
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
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 68, 17);
    slider.setNumDecimalPlacesToDisplay (1);
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colour (0xff8d939e));
    label.setFont (juce::Font (9.0f, juce::Font::bold));
    addAndMakeVisible (label);
}

void OpalAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (
        juce::Colour (0xff17191e), 0.0f, 0.0f,
        juce::Colour (0xff090a0d), 0.0f, static_cast<float> (getHeight()), false);
    background.addColour (0.42, juce::Colour (0xff101217));
    g.setGradientFill (background);
    g.fillAll();

    auto panel = getLocalBounds().toFloat().reduced (8.0f);

    juce::ColourGradient chassis (
        juce::Colour (0xff24272e), panel.getX(), panel.getY(),
        juce::Colour (0xff0c0e12), panel.getRight(), panel.getBottom(), false);
    chassis.addColour (0.24, juce::Colour (0xff191c22));
    chassis.addColour (0.72, juce::Colour (0xff111318));
    g.setGradientFill (chassis);
    g.fillRoundedRectangle (panel, 14.0f);

    for (int x = 16; x < getWidth() - 16; x += 7)
    {
        const auto alpha = (x % 14 == 0) ? 0x09 : 0x05;
        g.setColour (juce::Colour (static_cast<juce::uint8> (alpha), 255, 255, 255));
        g.drawVerticalLine (x, 14.0f, static_cast<float> (getHeight() - 14));
    }

    g.setColour (juce::Colour (0x502f343d));
    g.drawRoundedRectangle (panel, 14.0f, 1.2f);

    auto resonanceArea = juce::Rectangle<float> (18.0f, 48.0f,
                                                 static_cast<float> (getWidth() - 36), 218.0f);
    g.setColour (juce::Colour (0xff0d0f13));
    g.fillRoundedRectangle (resonanceArea, 10.0f);
    g.setColour (juce::Colour (0x60414955));
    g.drawRoundedRectangle (resonanceArea, 10.0f, 1.0f);

    auto rail = juce::Rectangle<float> (18.0f, 276.0f,
                                        static_cast<float> (getWidth() - 36), 108.0f);
    g.setColour (juce::Colour (0xff111319));
    g.fillRoundedRectangle (rail, 8.0f);
    g.setColour (juce::Colour (0x4a3b414d));
    g.drawRoundedRectangle (rail, 8.0f, 1.0f);

    auto controlDeck = juce::Rectangle<float> (18.0f, 392.0f,
                                               static_cast<float> (getWidth() - 36), 112.0f);
    g.setColour (juce::Colour (0xff0c0e12));
    g.fillRoundedRectangle (controlDeck, 9.0f);
    g.setColour (juce::Colour (0x503f4652));
    g.drawRoundedRectangle (controlDeck, 9.0f, 1.0f);

    auto drawScrew = [&g] (float x, float y)
    {
        g.setColour (juce::Colour (0xff363b45));
        g.fillEllipse (x - 5.0f, y - 5.0f, 10.0f, 10.0f);
        g.setColour (juce::Colour (0xff0a0b0e));
        g.drawEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f, 1.0f);
        g.drawLine (x - 2.5f, y, x + 2.5f, y, 1.0f);
    };

    drawScrew (22.0f, 22.0f);
    drawScrew (758.0f, 22.0f);
    drawScrew (22.0f, 498.0f);
    drawScrew (758.0f, 498.0f);

    g.setColour (juce::Colour (0xff757d89));
    g.setFont (juce::Font (8.2f, juce::Font::bold));
    g.drawText ("SPIRALTECH AUDIO", 30, 15, 160, 18, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff676e79));
    g.setFont (juce::Font (8.3f, juce::Font::bold));
    g.drawText ("SELECT RESONANCE", 28, 280, 180, 18, juce::Justification::centredLeft);

    auto nameplate = titleLabel.getBounds().toFloat().expanded (10.0f, 3.0f);
    juce::ColourGradient nameplateGradient (
        juce::Colour (0xff30343d), nameplate.getX(), nameplate.getY(),
        juce::Colour (0xff13151a), nameplate.getRight(), nameplate.getBottom(), false);
    g.setGradientFill (nameplateGradient);
    g.fillRoundedRectangle (nameplate, 4.0f);
    g.setColour (juce::Colour (0x70545b66));
    g.drawRoundedRectangle (nameplate, 4.0f, 1.0f);
}

void OpalAudioProcessorEditor::OpalStone::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (10.0f);
    const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    bounds = bounds.withSizeKeepingCentre (size, size);

    juce::Path stone;
    stone.addEllipse (bounds);

    juce::DropShadow (juce::Colour (0xa0000000), 18, { 0, 8 }).drawForPath (g, stone);

    auto bezel = bounds.expanded (8.0f);
    juce::ColourGradient bezelGradient (
        juce::Colour (0xff777e88), bezel.getX(), bezel.getY(),
        juce::Colour (0xff17191f), bezel.getRight(), bezel.getBottom(), false);
    bezelGradient.addColour (0.28, juce::Colour (0xff373c46));
    bezelGradient.addColour (0.70, juce::Colour (0xff20242b));
    g.setGradientFill (bezelGradient);
    g.fillEllipse (bezel);

    g.setColour (juce::Colour (0xff07080b));
    g.fillEllipse (bounds.expanded (2.5f));

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (stone);

        const auto cx = bounds.getCentreX();
        const auto cy = bounds.getCentreY();
        const auto pulse = 0.72f + 0.28f * energy;

        juce::ColourGradient pearl (
            juce::Colour (0xffedf4ee), cx - size * 0.26f, cy - size * 0.34f,
            juce::Colour (0xff586572), cx + size * 0.32f, cy + size * 0.38f, true);
        pearl.addColour (0.22, juce::Colour (0xffc8e7dc));
        pearl.addColour (0.50, juce::Colour (0xff8098a0));
        pearl.addColour (0.78, juce::Colour (0xff4d5665));
        pearl.addColour (0.94, juce::Colour (0xff252932));
        g.setGradientFill (pearl);
        g.fillRect (bounds);

        const std::array<float, 6> hueOffsets { 0.49f, 0.76f, 0.91f, 0.38f, 0.64f, 0.03f };

        for (int i = 0; i < static_cast<int> (hueOffsets.size()); ++i)
        {
            const auto t = phase * (0.42f + 0.08f * static_cast<float> (i));
            const auto px = cx + std::sin (t + i * 1.41f) * size * (0.12f + 0.015f * i);
            const auto py = cy + std::cos (t * 0.81f + i * 1.07f) * size * (0.12f + 0.012f * i);
            const auto radius = size * (0.24f + 0.018f * i);

            auto core = opalColour (hueOffsets[static_cast<size_t> (i)] + phase * 0.009f,
                                    0.64f,
                                    1.0f,
                                    (0.18f + energy * 0.17f) * pulse);
            juce::ColourGradient fire (core, px, py, core.withAlpha (0.0f), px + radius, py, true);
            fire.addColour (0.36, core.withAlpha (core.getFloatAlpha() * 0.78f));
            g.setGradientFill (fire);
            g.fillEllipse (px - radius, py - radius, radius * 2.0f, radius * 2.0f);
        }

        // Slow moving water-caustic ribbons. Their amplitude and brightness follow
        // the FIELD energy so the opal looks fluid rather than like a static orb.
        for (int band = 0; band < 6; ++band)
        {
            juce::Path wave;
            const auto baseY = bounds.getY() + size * (0.18f + 0.115f * static_cast<float> (band));
            const auto amplitude = size * (0.018f + 0.016f * energy);
            const auto speed = phase * (0.34f + 0.045f * static_cast<float> (band));

            for (int step = 0; step <= 28; ++step)
            {
                const auto norm = static_cast<float> (step) / 28.0f;
                const auto x = bounds.getX() + norm * size;
                const auto y = baseY
                             + std::sin (norm * juce::MathConstants<float>::twoPi * 1.7f
                                      + speed
                                      + static_cast<float> (band) * 0.65f) * amplitude;

                if (step == 0)
                    wave.startNewSubPath (x, y);
                else
                    wave.lineTo (x, y);
            }

            g.setColour (opalColour (0.50f + 0.07f * static_cast<float> (band) + phase * 0.004f,
                                     0.42f,
                                     1.0f,
                                     0.10f + 0.11f * energy));
            g.strokePath (wave, juce::PathStrokeType (1.1f + 1.1f * energy,
                                                      juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }

        const auto rippleSize = size * (0.46f + 0.03f * std::sin (phase * 0.7f));
        g.setColour (juce::Colour::fromFloatRGBA (0.92f, 1.0f, 1.0f, 0.08f + 0.09f * energy));
        g.drawEllipse (juce::Rectangle<float> (rippleSize, rippleSize)
                           .withCentre ({ cx + std::sin (phase * 0.6f) * size * 0.05f,
                                          cy + std::cos (phase * 0.5f) * size * 0.04f }),
                       1.0f);

        g.setColour (juce::Colour (0x40ffffff));
        g.fillEllipse (bounds.withSizeKeepingCentre (size * 0.70f, size * 0.24f)
                             .translated (-size * 0.10f, -size * 0.29f));
    }

    g.setColour (juce::Colour (0x98ffffff));
    g.drawEllipse (bounds.reduced (0.8f), 1.0f);
    g.setColour (juce::Colour (0x28ffffff));
    g.drawEllipse (bounds.reduced (5.0f), 0.8f);
}

void OpalAudioProcessorEditor::EnergyMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (5.0f);
    g.setColour (juce::Colour (0xff050609));
    g.fillRoundedRectangle (bounds, 4.0f);

    const auto norm = juce::jlimit (0.0f, 1.0f, juce::jmap (levelDb, -60.0f, 0.0f, 0.0f, 1.0f));
    auto fill = bounds.withTop (bounds.getBottom() - bounds.getHeight() * norm).reduced (3.0f);

    juce::ColourGradient meterGradient (
        juce::Colour (0xff62e8ff), fill.getCentreX(), fill.getBottom(),
        juce::Colour (0xffff98d5), fill.getCentreX(), fill.getY(), false);
    meterGradient.addColour (0.58, juce::Colour (0xff9c8cff));
    g.setGradientFill (meterGradient);
    g.fillRoundedRectangle (fill, 2.2f);

    g.setColour (juce::Colour (0x7049505c));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    g.setColour (juce::Colour (0x666f7783));
    for (int i = 1; i < 7; ++i)
    {
        const auto y = bounds.getY() + bounds.getHeight() * static_cast<float> (i) / 7.0f;
        g.drawHorizontalLine (juce::roundToInt (y), bounds.getX() + 3.0f, bounds.getRight() - 3.0f);
    }
}

void OpalAudioProcessorEditor::timerCallback()
{
    animationPhase += 0.022f;

    if (animationPhase > juce::MathConstants<float>::twoPi * 12.0f)
        animationPhase = 0.0f;

    const auto db = processor.getResonanceEnergyDb();
    opalStone.setEnergyDb (db);
    opalStone.setPhase (animationPhase);
    energyMeter.setLevelDb (db);

    const auto selectedIndex = OpalFrequencyData::clampIndex (
        juce::roundToInt (processor.getValueTreeState().getRawParameterValue ("frequency")->load()));

    for (int i = 0; i < static_cast<int> (frequencyButtons.size()); ++i)
        frequencyButtons[static_cast<size_t> (i)]->setToggleState (i == selectedIndex,
                                                                  juce::dontSendNotification);

    updateFrequencyInfo();
}

void OpalAudioProcessorEditor::updateFrequencyInfo()
{
    const auto index = OpalFrequencyData::clampIndex (
        juce::roundToInt (processor.getValueTreeState().getRawParameterValue ("frequency")->load()));

    const auto& entry = OpalFrequencyData::entries[static_cast<size_t> (index)];

    frequencyInfoLabel.setText (
        juce::String (static_cast<int> (entry.hz)) + " Hz  •  " + entry.label,
        juce::dontSendNotification);
}

void OpalAudioProcessorEditor::resized()
{
    const auto centreX = getWidth() / 2;

    subtitleLabel.setBounds (500, 14, 245, 18);

    frequencyKnob.setBounds (58, 82, 124, 120);
    frequencyLabel.setBounds (72, 199, 96, 16);

    opalStone.setBounds (centreX - 91, 62, 182, 182);
    frequencyInfoLabel.setBounds (centreX - 125, 235, 250, 21);
    titleLabel.setBounds (centreX - 58, 264, 116, 22);

    energyMeter.setBounds (694, 80, 32, 140);
    meterLabel.setBounds (668, 222, 84, 17);

    constexpr int columns = 9;
    constexpr int cellGap = 4;
    const int gridX = 27;
    const int gridY = 303;
    const int gridWidth = getWidth() - 54;
    const int cellWidth = (gridWidth - (columns - 1) * cellGap) / columns;
    const int cellHeight = 34;

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

    std::array<juce::Slider*, 6> knobs {
        &boostKnob, &harmonicsKnob, &spaceKnob, &widthKnob, &fieldKnob, &mixKnob
    };

    std::array<juce::Label*, 6> labels {
        &boostLabel, &harmonicsLabel, &spaceLabel, &widthLabel, &fieldLabel, &mixLabel
    };

    const int knobY = 398;
    const int knobWidth = 100;
    const int spacing = 17;
    const int totalWidth = static_cast<int> (knobs.size()) * knobWidth
                         + (static_cast<int> (knobs.size()) - 1) * spacing;
    const int startX = (getWidth() - totalWidth) / 2;

    for (int i = 0; i < static_cast<int> (knobs.size()); ++i)
    {
        const auto x = startX + i * (knobWidth + spacing);
        knobs[static_cast<size_t> (i)]->setBounds (x, knobY, knobWidth, 88);
        labels[static_cast<size_t> (i)]->setBounds (x + 4, 484, knobWidth - 8, 16);
    }
}

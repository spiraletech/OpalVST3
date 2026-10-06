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
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xfff4f6fa));
    titleLabel.setFont (juce::Font (22.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("HARMONIC RESONANCE FIELD", juce::dontSendNotification);
    subtitleLabel.setJustificationType (juce::Justification::centredRight);
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff7f8793));
    subtitleLabel.setFont (juce::Font (10.5f, juce::Font::plain));
    addAndMakeVisible (subtitleLabel);

    frequencyInfoLabel.setJustificationType (juce::Justification::centred);
    frequencyInfoLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe9ecf4));
    frequencyInfoLabel.setFont (juce::Font (13.0f, juce::Font::bold));
    addAndMakeVisible (frequencyInfoLabel);

    meterLabel.setText ("FIELD", juce::dontSendNotification);
    meterLabel.setJustificationType (juce::Justification::centred);
    meterLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8c929d));
    meterLabel.setFont (juce::Font (9.5f, juce::Font::bold));
    addAndMakeVisible (meterLabel);

    configureKnob (frequencyKnob, frequencyLabel, "FREQ");
    configureKnob (boostKnob, boostLabel, "BOOST");
    configureKnob (harmonicsKnob, harmonicsLabel, "HARMONICS");
    configureKnob (spaceKnob, spaceLabel, "SPACE");
    configureKnob (widthKnob, widthLabel, "WIDTH");
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
    mixKnob.setTextValueSuffix (" %");

    addAndMakeVisible (opalStone);
    addAndMakeVisible (energyMeter);

    auto& state = processor.getValueTreeState();

    frequencyAttachment = std::make_unique<SliderAttachment> (state, "frequency", frequencyKnob);
    boostAttachment = std::make_unique<SliderAttachment> (state, "boost", boostKnob);
    harmonicsAttachment = std::make_unique<SliderAttachment> (state, "harmonics", harmonicsKnob);
    spaceAttachment = std::make_unique<SliderAttachment> (state, "space", spaceKnob);
    widthAttachment = std::make_unique<SliderAttachment> (state, "width", widthKnob);
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
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
    slider.setNumDecimalPlacesToDisplay (1);
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colour (0xff8d939e));
    label.setFont (juce::Font (9.5f, juce::Font::bold));
    addAndMakeVisible (label);
}

void OpalAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (
        juce::Colour (0xff111217), 0.0f, 0.0f,
        juce::Colour (0xff08090c), 0.0f, static_cast<float> (getHeight()), false);
    background.addColour (0.52, juce::Colour (0xff0d0e13));
    g.setGradientFill (background);
    g.fillAll();

    auto panel = getLocalBounds().toFloat().reduced (8.0f);
    g.setColour (juce::Colour (0x401f222a));
    g.fillRoundedRectangle (panel, 18.0f);

    juce::ColourGradient edge (
        juce::Colour (0x6077eaff), panel.getX(), panel.getY(),
        juce::Colour (0x50ff94da), panel.getRight(), panel.getBottom(), false);
    edge.addColour (0.52, juce::Colour (0x459483ff));
    g.setGradientFill (edge);
    g.drawRoundedRectangle (panel, 18.0f, 1.0f);

    g.setColour (juce::Colour (0x18ffffff));
    g.drawLine (20.0f, 42.0f, static_cast<float> (getWidth() - 20), 42.0f, 1.0f);

    auto resonanceArea = juce::Rectangle<float> (18.0f, 52.0f,
                                                 static_cast<float> (getWidth() - 36), 212.0f);
    g.setColour (juce::Colour (0x151a1d25));
    g.fillRoundedRectangle (resonanceArea, 14.0f);
    g.setColour (juce::Colour (0x2539414d));
    g.drawRoundedRectangle (resonanceArea, 14.0f, 1.0f);

    g.setColour (juce::Colour (0xff676e79));
    g.setFont (juce::Font (8.8f, juce::Font::bold));
    g.drawText ("SELECT RESONANCE", 26, 272, 180, 18, juce::Justification::centredLeft);
}

void OpalAudioProcessorEditor::OpalStone::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (10.0f);
    const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    bounds = bounds.withSizeKeepingCentre (size, size);

    juce::Path stone;
    stone.addEllipse (bounds);

    juce::DropShadow (juce::Colour (0x90000000), 18, { 0, 7 }).drawForPath (g, stone);

    auto bezel = bounds.expanded (7.0f);
    juce::ColourGradient bezelGradient (
        juce::Colour (0xff555b66), bezel.getX(), bezel.getY(),
        juce::Colour (0xff17191f), bezel.getRight(), bezel.getBottom(), false);
    bezelGradient.addColour (0.35, juce::Colour (0xff2b2f37));
    g.setGradientFill (bezelGradient);
    g.fillEllipse (bezel);

    g.setColour (juce::Colour (0xff090a0d));
    g.fillEllipse (bounds.expanded (2.0f));

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (stone);

        const auto pulse = 0.65f + (0.35f * energy);
        const auto cx = bounds.getCentreX();
        const auto cy = bounds.getCentreY();

        juce::ColourGradient pearl (
            juce::Colour (0xfff2f4eb), cx - size * 0.25f, cy - size * 0.32f,
            juce::Colour (0xff66727c), cx + size * 0.30f, cy + size * 0.34f, true);
        pearl.addColour (0.28, juce::Colour (0xffb8d8d2));
        pearl.addColour (0.62, juce::Colour (0xff7e8494));
        pearl.addColour (0.86, juce::Colour (0xff262a33));
        g.setGradientFill (pearl);
        g.fillRect (bounds);

        const std::array<float, 5> hueOffsets { 0.50f, 0.78f, 0.91f, 0.38f, 0.66f };

        for (int i = 0; i < static_cast<int> (hueOffsets.size()); ++i)
        {
            const auto t = phase * (0.55f + 0.12f * static_cast<float> (i));
            const auto px = cx + std::sin (t + i * 1.71f) * size * (0.12f + 0.018f * i);
            const auto py = cy + std::cos (t * 0.83f + i * 1.19f) * size * (0.13f + 0.014f * i);
            const auto radius = size * (0.25f + 0.025f * i);

            auto core = opalColour (hueOffsets[static_cast<size_t> (i)] + phase * 0.012f,
                                    0.68f,
                                    1.0f,
                                    (0.23f + energy * 0.18f) * pulse);
            auto transparent = core.withAlpha (0.0f);

            juce::ColourGradient fire (core, px, py, transparent, px + radius, py, true);
            fire.addColour (0.38, core.withAlpha (core.getFloatAlpha() * 0.72f));
            g.setGradientFill (fire);
            g.fillEllipse (px - radius, py - radius, radius * 2.0f, radius * 2.0f);
        }

        g.setColour (juce::Colour (0x38ffffff));
        g.fillEllipse (bounds.withSizeKeepingCentre (size * 0.72f, size * 0.30f)
                             .translated (-size * 0.10f, -size * 0.27f));
    }

    g.setColour (juce::Colour (0x80ffffff));
    g.drawEllipse (bounds.reduced (0.7f), 1.0f);

    g.setColour (juce::Colour (0x30ffffff));
    g.drawEllipse (bounds.reduced (4.0f), 0.7f);
}

void OpalAudioProcessorEditor::EnergyMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (5.0f);
    g.setColour (juce::Colour (0xff08090c));
    g.fillRoundedRectangle (bounds, 5.0f);

    const auto norm = juce::jlimit (0.0f, 1.0f, juce::jmap (levelDb, -60.0f, 0.0f, 0.0f, 1.0f));
    auto fill = bounds.withTop (bounds.getBottom() - bounds.getHeight() * norm).reduced (3.0f);

    juce::ColourGradient meterGradient (
        juce::Colour (0xff5fe9ff), fill.getCentreX(), fill.getBottom(),
        juce::Colour (0xffff9bd6), fill.getCentreX(), fill.getY(), false);
    meterGradient.addColour (0.58, juce::Colour (0xff9c8cff));
    g.setGradientFill (meterGradient);
    g.fillRoundedRectangle (fill, 2.5f);

    g.setColour (juce::Colour (0x405f6774));
    g.drawRoundedRectangle (bounds, 5.0f, 1.0f);

    g.setColour (juce::Colour (0x55747b87));
    for (int i = 1; i < 6; ++i)
    {
        const auto y = bounds.getY() + bounds.getHeight() * static_cast<float> (i) / 6.0f;
        g.drawHorizontalLine (juce::roundToInt (y), bounds.getX() + 3.0f, bounds.getRight() - 3.0f);
    }
}

void OpalAudioProcessorEditor::timerCallback()
{
    animationPhase += 0.035f;
    if (animationPhase > juce::MathConstants<float>::twoPi * 8.0f)
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
    titleLabel.setBounds (24, 10, 180, 28);
    subtitleLabel.setBounds (470, 12, 282, 24);

    const auto centreX = getWidth() / 2;

    frequencyKnob.setBounds (54, 78, 130, 128);
    frequencyLabel.setBounds (70, 202, 98, 16);

    opalStone.setBounds (centreX - 94, 59, 188, 188);
    frequencyInfoLabel.setBounds (centreX - 120, 235, 240, 24);

    energyMeter.setBounds (690, 77, 34, 142);
    meterLabel.setBounds (672, 221, 70, 18);

    constexpr int columns = 9;
    constexpr int cellGap = 4;
    const int gridX = 24;
    const int gridY = 292;
    const int gridWidth = getWidth() - 48;
    const int cellWidth = (gridWidth - (columns - 1) * cellGap) / columns;
    const int cellHeight = 36;

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

    std::array<juce::Slider*, 5> knobs {
        &boostKnob, &harmonicsKnob, &spaceKnob, &widthKnob, &mixKnob
    };

    std::array<juce::Label*, 5> labels {
        &boostLabel, &harmonicsLabel, &spaceLabel, &widthLabel, &mixLabel
    };

    const int knobY = 389;
    const int knobWidth = 116;
    const int spacing = 20;
    const int totalWidth = static_cast<int> (knobs.size()) * knobWidth
                         + (static_cast<int> (knobs.size()) - 1) * spacing;
    const int startX = (getWidth() - totalWidth) / 2;

    for (int i = 0; i < static_cast<int> (knobs.size()); ++i)
    {
        const auto x = startX + i * (knobWidth + spacing);
        knobs[static_cast<size_t> (i)]->setBounds (x, knobY, knobWidth, 103);
        labels[static_cast<size_t> (i)]->setBounds (x + 4, 488, knobWidth - 8, 16);
    }
}

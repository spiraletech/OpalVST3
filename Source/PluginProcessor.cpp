#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "OpalFrequencyData.h"

OpalAudioProcessor::OpalAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "OPAL_STATE", createParameterLayout())
{
}

void OpalAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    setLatencySamples (0);
}

void OpalAudioProcessor::releaseResources()
{
    engine.reset();
}

void OpalAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalInputChannels = getTotalNumInputChannels();
    const auto totalOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalInputChannels; channel < totalOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    const auto frequencyIndex = juce::roundToInt (*parameters.getRawParameterValue ("frequency"));

    OpalEngine::Parameters p;
    p.frequencyHz = OpalFrequencyData::frequencyForIndex (frequencyIndex);
    p.boostDb = *parameters.getRawParameterValue ("boost");
    p.harmonics = *parameters.getRawParameterValue ("harmonics") * 0.01f;
    p.space = *parameters.getRawParameterValue ("space") * 0.01f;
    p.width = *parameters.getRawParameterValue ("width") * 0.01f;
    p.mix = *parameters.getRawParameterValue ("mix") * 0.01f;

    engine.setParameters (p);
    engine.process (buffer);
}

bool OpalAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& input = layouts.getMainInputChannelSet();
    const auto& output = layouts.getMainOutputChannelSet();

    if (input != output)
        return false;

    return output == juce::AudioChannelSet::mono()
        || output == juce::AudioChannelSet::stereo();
}

void OpalAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void OpalAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout OpalAudioProcessor::createParameterLayout()
{
    using APF = juce::AudioParameterFloat;
    using APC = juce::AudioParameterChoice;
    using PID = juce::ParameterID;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<APC> (
        PID { "frequency", 1 },
        "Frequency",
        OpalFrequencyData::makeChoiceStrings(),
        8));

    layout.add (std::make_unique<APF> (
        PID { "boost", 1 },
        "Boost",
        juce::NormalisableRange<float> { 0.0f, 12.0f, 0.01f },
        3.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<APF> (
        PID { "harmonics", 1 },
        "Harmonics",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f },
        35.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<APF> (
        PID { "space", 1 },
        "Space",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f },
        25.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<APF> (
        PID { "width", 1 },
        "Width",
        juce::NormalisableRange<float> { 0.0f, 200.0f, 0.1f },
        100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<APF> (
        PID { "mix", 1 },
        "Mix",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f },
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    return layout;
}

juce::AudioProcessorEditor* OpalAudioProcessor::createEditor()
{
    return new OpalAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OpalAudioProcessor();
}

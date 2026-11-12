#include "PluginProcessor.h"
#include "PluginEditor.h"

T8CamProcessor::T8CamProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      params (*this, nullptr, "T8CamState", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout T8CamProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { ParamIDs::mirror, 1 }, "Specchio", true));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { ParamIDs::mono,   1 }, "Bianco e nero", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::pulse,  1 }, "Pulse",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    return layout;
}

bool T8CamProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

void T8CamProcessor::prepareToPlay (double sampleRate, int)
{
    envelope = 0.0f;
    // Rilascio di circa 150 ms, valutato una volta per blocco.
    releaseCoeff = (float) std::exp (-1.0 / (0.15 * sampleRate));
}

void T8CamProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int n = buffer.getNumSamples();

    if (n == 0)
        return;

    float peak = 0.0f;

    for (int ch = 0; ch < getTotalNumInputChannels(); ++ch)
        peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, n));

    // Inviluppo: attacco immediato, rilascio esponenziale.
    const float blockRelease = std::pow (releaseCoeff, (float) n);
    envelope = peak > envelope ? peak : envelope * blockRelease;
    level.store (envelope, std::memory_order_relaxed);
}

juce::String T8CamProcessor::getCameraId() const
{
    return params.state.getProperty ("cameraId").toString();
}

void T8CamProcessor::setCameraId (const juce::String& id)
{
    params.state.setProperty ("cameraId", id, nullptr);
}

void T8CamProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = params.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void T8CamProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (params.state.getType()))
            params.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* T8CamProcessor::createEditor()
{
    return new T8CamEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new T8CamProcessor();
}

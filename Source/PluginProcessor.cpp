#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include "VideoEffects.h"

T8CamProcessor::T8CamProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      params (*this, nullptr, "T8CamState", createLayout())
{
    presets = std::make_unique<PresetManager> (*this);
}

T8CamProcessor::~T8CamProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout T8CamProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { ParamIDs::mirror, 1 }, "Specchio", true));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::effect, 2 }, "Effetto",
                                                        VideoEffects::getNames(), 0));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::amount, 2 }, String::fromUTF8 ("Intensità"),
                                                        NormalisableRange<float> (0.0f, 1.0f), 1.0f));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::pulse,  1 }, "Pulse",
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
    // Rilascio di circa 150 ms.
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

//==============================================================================
juce::String T8CamProcessor::getCameraId() const          { return params.state.getProperty ("cameraId").toString(); }
void T8CamProcessor::setCameraId (const juce::String& id) { params.state.setProperty ("cameraId", id, nullptr); }
bool T8CamProcessor::isUiHidden() const                   { return params.state.getProperty ("uiHidden", false); }
void T8CamProcessor::setUiHidden (bool h)                 { params.state.setProperty ("uiHidden", h, nullptr); }

void T8CamProcessor::replaceMedia (const juce::ValueTree& newMedia)
{
    jassert (juce::MessageManager::existsAndIsCurrentThread());

    editMedia ([&] (juce::ValueTree& m)
    {
        m.removeAllChildren (nullptr);

        if (newMedia.isValid())
            for (const auto& child : newMedia)
                m.appendChild (child.createCopy(), nullptr);
    });

    mediaCache.purge (media);
}

void T8CamProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = params.copyState();
    state.setProperty ("presetName", presets->getCurrentName(), nullptr);

    {
        const juce::ScopedLock sl (mediaLock);
        state.appendChild (media.createCopy(), nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void T8CamProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (params.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    auto savedMedia = state.getChildWithName (MediaIDs::MEDIA);
    state.removeChild (savedMedia, nullptr);
    const auto presetName = state.getProperty ("presetName").toString();

    params.replaceState (state);

    // I media (e chi li ascolta) vivono sul message thread.
    juce::MessageManager::callAsync ([this, savedMedia = savedMedia.createCopy(), presetName,
                                      weak = juce::WeakReference<T8CamProcessor> (this)]
    {
        if (weak == nullptr)
            return;

        replaceMedia (savedMedia);
        presets->setCurrentName (presetName);
    });
}

juce::AudioProcessorEditor* T8CamProcessor::createEditor()
{
    return new T8CamEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new T8CamProcessor();
}

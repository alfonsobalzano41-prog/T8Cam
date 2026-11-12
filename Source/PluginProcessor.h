#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

namespace ParamIDs
{
    inline constexpr auto mirror = "mirror";
    inline constexpr auto mono   = "mono";
    inline constexpr auto pulse  = "pulse";
}

/** Il plugin lascia passare l'audio intatto: misura solo il livello per gli effetti video. */
class T8CamProcessor final : public juce::AudioProcessor
{
public:
    T8CamProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** Livello audio corrente (0..1 circa), letto dall'interfaccia grafica. */
    float getLevel() const noexcept { return level.load (std::memory_order_relaxed); }

    /** Webcam scelta, salvata nel progetto del DAW. */
    juce::String getCameraId() const;
    void setCameraId (const juce::String& id);

    juce::AudioProcessorValueTreeState params;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float> level { 0.0f };
    float envelope = 0.0f;
    float releaseCoeff = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (T8CamProcessor)
};

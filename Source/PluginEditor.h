#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "CameraView.h"

class T8CamEditor final : public juce::AudioProcessorEditor
{
public:
    explicit T8CamEditor (T8CamProcessor&);
    ~T8CamEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void refreshDeviceList();

    T8CamProcessor& processor;

    CameraView view;

    juce::ComboBox cameraBox;
    juce::TextButton refreshButton { "Aggiorna" };
    juce::ToggleButton mirrorButton { "Specchio" };
    juce::ToggleButton monoButton { "B/N" };
    juce::Label pulseLabel { {}, "Pulse" };
    juce::Slider pulseSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    ButtonAttachment mirrorAttachment, monoAttachment;
    SliderAttachment pulseAttachment;

    juce::Array<CameraSource::DeviceInfo> devices;

    static constexpr int toolbarHeight = 36;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (T8CamEditor)
};

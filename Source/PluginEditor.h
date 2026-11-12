#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "CameraView.h"
#include "MediaPanel.h"
#include "UpdateChecker.h"

/** Avviso "nuova versione disponibile", in basso sopra il video. */
class UpdateBanner final : public juce::Component
{
public:
    UpdateBanner();
    void show (const juce::String& version, const juce::String& url);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Label label;
    juce::TextButton downloadButton { "Scarica" }, closeButton { "X" };
    juce::String downloadUrl;
};

class T8CamEditor final : public juce::AudioProcessorEditor,
                          private juce::ChangeListener,
                          private juce::Timer
{
public:
    explicit T8CamEditor (T8CamProcessor&);
    ~T8CamEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void refreshDeviceList();
    void refreshPresetList();
    void savePreset();
    void showMenu();
    void openMediaWindow();
    void setUiHidden (bool);
    void checkForUpdates (bool manual);
    void showError (const juce::String&);

    void changeListenerCallback (juce::ChangeBroadcaster*) override { refreshPresetList(); }
    void timerCallback() override;

    T8CamProcessor& processor;
    juce::Value selectedMediaId;

    CameraView view;
    UpdateBanner updateBanner;
    std::unique_ptr<MediaWindow> mediaWindow;
    UpdateChecker updateChecker;

    // Riga 1
    juce::ComboBox presetBox;
    juce::TextButton saveButton { "Salva" }, menuButton { "Menu" }, mediaButton { "Media" }, hideButton { "Nascondi UI" };
    // Riga 2
    juce::ComboBox cameraBox, effectBox;
    juce::TextButton refreshButton { "Aggiorna" };
    juce::Label amountLabel, pulseLabel { {}, "Pulse" };
    juce::Slider amountSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Slider pulseSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::ToggleButton mirrorButton { "Specchio" };

    // Visibile solo in modalità pulita, quando il mouse è sopra il plugin.
    juce::TextButton showUiButton { "Mostra UI" };

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<ButtonAttachment> mirrorAttachment;
    std::unique_ptr<SliderAttachment> amountAttachment, pulseAttachment;
    std::unique_ptr<ComboAttachment> effectAttachment;

    juce::Array<CameraSource::DeviceInfo> devices;
    juce::Array<juce::File> userPresetFiles;
    bool uiHidden = false;

    static constexpr int rowHeight = 32;
    static constexpr int toolbarHeight = rowHeight * 2 + 8;
    static constexpr int userPresetIdBase = 1000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (T8CamEditor)
};

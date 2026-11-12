#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CameraSource.h"
#include "PluginProcessor.h"

/** Mostra la webcam con gli effetti, oppure un messaggio se la webcam non è disponibile. */
class CameraView final : public juce::Component,
                         private juce::Timer
{
public:
    explicit CameraView (T8CamProcessor&);
    ~CameraView() override;

    /** Apre (o riapre) la webcam salvata nel processore, dopo aver controllato i permessi. */
    void start();
    void stop();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void handleFrame (const juce::Image&);   // thread della camera
    void showMessage (const juce::String& text, bool showSettings, bool showApp);
    void openWithPermission();

    static juce::File findStandaloneApp();

    T8CamProcessor& processor;
    std::unique_ptr<CameraSource> source;

    juce::CriticalSection frameLock;
    juce::Image latestFrame;
    std::atomic<bool> hasNewFrame { false };
    juce::Image currentFrame;      // usato solo dal message thread

    std::atomic<float>* mirrorParam = nullptr;
    std::atomic<float>* monoParam = nullptr;
    std::atomic<float>* pulseParam = nullptr;

    float displayLevel = 0.0f;
    juce::String message;

    juce::TextButton settingsButton { "Apri Impostazioni privacy" };
    juce::TextButton appButton { "Apri l'app T8Cam" };

    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CameraView)
};

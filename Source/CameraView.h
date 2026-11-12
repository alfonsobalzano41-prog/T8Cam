#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CameraSource.h"
#include "PluginProcessor.h"

/** Mostra la webcam con effetti e media sovrapposti.
    I media si spostano trascinandoli, si ridimensionano con la rotella (o il pinch del trackpad).
    Si possono trascinare file PNG/JPG/GIF direttamente qui sopra.
*/
class CameraView final : public juce::Component,
                         public juce::FileDragAndDropTarget,
                         private juce::Timer,
                         private juce::ValueTree::Listener,
                         private juce::Value::Listener
{
public:
    CameraView (T8CamProcessor&, juce::Value& selectedMediaId);
    ~CameraView() override;

    /** Apre (o riapre) la webcam salvata nel processore, dopo aver controllato i permessi. */
    void start();
    void stop();

    /** In modalità pulita non mostriamo il riquadro di selezione. */
    void setShowSelection (bool);

    std::function<void()> onDoubleClickBackground;
    std::function<void (const juce::String& error)> onMediaError;

    /** Aggiunge un file immagine come media, al punto indicato (coordinate della vista). */
    void addMediaFile (const juce::File&, juce::Point<float> where);

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scaleFactor) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;
    void handleFrame (const juce::Image&);   // thread della camera
    void showMessage (const juce::String& text, bool showSettings, bool showApp);
    void openWithPermission();

    juce::ValueTree findItemAt (juce::Point<float>);
    juce::ValueTree getSelectedItem();
    void scaleItem (juce::ValueTree item, float factor);

    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override       { mediaChanged(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { mediaChanged(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override        { repaint(); }
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { repaint(); }
    void valueChanged (juce::Value&) override                                    { repaint(); }
    void mediaChanged();

    static juce::File findStandaloneApp();

    T8CamProcessor& processor;
    juce::ValueTree media;
    juce::Value selectedId;

    std::unique_ptr<CameraSource> source;

    juce::CriticalSection frameLock;
    juce::Image latestFrame;
    std::atomic<bool> hasNewFrame { false };
    juce::Image currentFrame;      // usato solo dal message thread

    std::atomic<float>* mirrorParam = nullptr;
    std::atomic<float>* effectParam = nullptr;
    std::atomic<float>* amountParam = nullptr;
    std::atomic<float>* pulseParam = nullptr;

    float displayLevel = 0.0f;
    juce::String message;
    bool showSelection = true;
    bool animatedMedia = false;

    juce::ValueTree dragItem;
    juce::Point<float> dragOffset;

    juce::TextButton settingsButton { "Apri Impostazioni privacy" };
    juce::TextButton appButton { "Apri l'app T8Cam" };

    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CameraView)
};

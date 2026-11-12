#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

/** Pannello della finestra "Media": elenco degli elementi e loro proprietà. */
class MediaPanel final : public juce::Component,
                         private juce::ListBoxModel,
                         private juce::ValueTree::Listener,
                         private juce::Value::Listener
{
public:
    MediaPanel (T8CamProcessor&, juce::Value& selectedMediaId);
    ~MediaPanel() override;

    /** Chiamato quando l'utente sceglie un file da aggiungere. */
    std::function<void (const juce::File&)> onAddFile;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;

    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override        { refreshList(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override  { refreshList(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override         { refreshList(); }
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeRedirected (juce::ValueTree&) override                          { refreshList(); }
    void valueChanged (juce::Value&) override                                     { syncSelectionFromValue(); }

    juce::ValueTree selectedItem() const;
    void refreshList();
    void syncSelectionFromValue();
    void refreshControls();
    void setProp (const juce::Identifier&, const juce::var&);
    void moveSelected (int delta);
    void chooseFile();
    void chooseColour();

    T8CamProcessor& processor;
    juce::ValueTree media;
    juce::Value selectedId;
    bool updating = false;

    juce::TextButton addTextButton { "+ Testo" };
    juce::TextButton addImageButton { "+ Immagine / GIF" };
    juce::ListBox list { "Media", this };
    juce::TextButton upButton, downButton, duplicateButton, deleteButton;

    juce::Label textLabel { {}, "Testo" };
    juce::TextEditor textEditor;
    juce::ComboBox fontBox;
    juce::TextButton colourButton { "Colore" };
    juce::ToggleButton boldToggle { "Grassetto" }, shadowToggle { "Ombra" };

    juce::Label sizeLabel { {}, "Dimensione" }, opacityLabel, rotationLabel { {}, "Rotazione" };
    juce::Slider sizeSlider, opacitySlider, rotationSlider;
    juce::ToggleButton visibleToggle { "Visibile" };
    juce::TextButton centreButton { "Centra" };
    juce::Label hint;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::StringArray fontNames;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MediaPanel)
};

/** Finestra separata che contiene il MediaPanel. Resta sopra il DAW. */
class MediaWindow final : public juce::DocumentWindow
{
public:
    MediaWindow (T8CamProcessor&, juce::Value& selectedMediaId);
    void closeButtonPressed() override { setVisible (false); }

    MediaPanel& getPanel() { return *panel; }

private:
    MediaPanel* panel = nullptr;
};

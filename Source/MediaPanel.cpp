#include "MediaPanel.h"

using namespace MediaIDs;

namespace
{
    const juce::Colour panelBg (0xff1e1e1e);
    const juce::Colour accent (0xff4fc3f7);

    /** Selettore colore che avvisa con una lambda (sicuro anche se il pannello sparisce). */
    struct ColourPicker final : public juce::ColourSelector,
                                private juce::ChangeListener
    {
        explicit ColourPicker (std::function<void (juce::Colour)> cb)
            : juce::ColourSelector (juce::ColourSelector::showColourAtTop | juce::ColourSelector::showSliders
                                    | juce::ColourSelector::showColourspace | juce::ColourSelector::showAlphaChannel),
              onChange (std::move (cb))
        {
            addChangeListener (this);
            setSize (300, 320);
        }

        ~ColourPicker() override { removeChangeListener (this); }

        void changeListenerCallback (juce::ChangeBroadcaster*) override
        {
            if (onChange)
                onChange (getCurrentColour());
        }

        std::function<void (juce::Colour)> onChange;
    };
}

MediaPanel::MediaPanel (T8CamProcessor& p, juce::Value& selectedMediaId)
    : processor (p), media (p.getMedia())
{
    selectedId.referTo (selectedMediaId);

    upButton.setButtonText ("Su");
    downButton.setButtonText (juce::String::fromUTF8 ("Giù"));
    duplicateButton.setButtonText ("Duplica");
    deleteButton.setButtonText ("Elimina");
    opacityLabel.setText (juce::String::fromUTF8 ("Opacità"), juce::dontSendNotification);
    hint.setText (juce::String::fromUTF8 ("Trascina gli elementi sul video per spostarli.\n"
                                          "Rotella o pinch sul trackpad = dimensione.\n"
                                          "Puoi trascinare PNG, JPG e GIF direttamente sul video."),
                  juce::dontSendNotification);
    hint.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));
    hint.setFont (juce::FontOptions (12.0f));
    hint.setJustificationType (juce::Justification::topLeft);

    for (auto* c : std::initializer_list<juce::Component*> {
             &addTextButton, &addImageButton, &list, &upButton, &downButton, &duplicateButton, &deleteButton,
             &textLabel, &textEditor, &fontBox, &colourButton, &boldToggle, &shadowToggle,
             &sizeLabel, &opacityLabel, &rotationLabel, &sizeSlider, &opacitySlider, &rotationSlider,
             &visibleToggle, &centreButton, &hint })
        addAndMakeVisible (c);

    list.setRowHeight (26);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff141414));
    list.setOutlineThickness (1);

    textEditor.setMultiLine (true, true);
    textEditor.setReturnKeyStartsNewLine (true);
    textEditor.onTextChange = [this] { setProp (text, textEditor.getText()); };

    fontNames = juce::Font::findAllTypefaceNames();
    fontBox.addItemList (fontNames, 1);
    fontBox.onChange = [this]
    {
        if (fontBox.getSelectedId() > 0)
            setProp (font, fontBox.getText());
    };

    for (auto* s : { &sizeSlider, &opacitySlider, &rotationSlider })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
    }

    sizeSlider.setRange (0.01, 1.5, 0.001);
    sizeSlider.setSkewFactorFromMidPoint (0.25);
    opacitySlider.setRange (0.0, 1.0, 0.01);
    rotationSlider.setRange (-180.0, 180.0, 1.0);
    rotationSlider.setDoubleClickReturnValue (true, 0.0);

    sizeSlider.onValueChange     = [this] { setProp (size, (float) sizeSlider.getValue()); };
    opacitySlider.onValueChange  = [this] { setProp (opacity, (float) opacitySlider.getValue()); };
    rotationSlider.onValueChange = [this] { setProp (rotation, (float) rotationSlider.getValue()); };
    boldToggle.onClick    = [this] { setProp (bold, boldToggle.getToggleState()); };
    shadowToggle.onClick  = [this] { setProp (shadow, shadowToggle.getToggleState()); };
    visibleToggle.onClick = [this] { setProp (visible, visibleToggle.getToggleState()); };
    centreButton.onClick  = [this] { setProp (x, 0.5f); setProp (y, 0.5f); };
    colourButton.onClick  = [this] { chooseColour(); };

    addTextButton.onClick = [this]
    {
        auto item = Media::createText ("Scrivi qui");
        processor.editMedia ([&] (juce::ValueTree& m) { m.appendChild (item, nullptr); });
        selectedId = item[id].toString();
        textEditor.grabKeyboardFocus();
        textEditor.selectAll();
    };

    addImageButton.onClick = [this] { chooseFile(); };
    upButton.onClick   = [this] { moveSelected (+1); };
    downButton.onClick = [this] { moveSelected (-1); };

    duplicateButton.onClick = [this]
    {
        auto item = selectedItem();
        if (! item.isValid()) return;

        auto copy = item.createCopy();
        copy.setProperty (id, juce::Uuid().toString(), nullptr);
        copy.setProperty (x, (float) item[x] + 0.04f, nullptr);
        copy.setProperty (y, (float) item[y] + 0.04f, nullptr);
        processor.editMedia ([&] (juce::ValueTree& m) { m.appendChild (copy, nullptr); });
        selectedId = copy[id].toString();
    };

    deleteButton.onClick = [this]
    {
        auto item = selectedItem();
        if (! item.isValid()) return;

        processor.editMedia ([&] (juce::ValueTree& m) { m.removeChild (item, nullptr); });
        selectedId = juce::String();
    };

    media.addListener (this);
    selectedId.addListener (this);

    refreshList();
    setSize (360, 640);
}

MediaPanel::~MediaPanel()
{
    media.removeListener (this);
    selectedId.removeListener (this);
}

//==============================================================================
juce::ValueTree MediaPanel::selectedItem() const
{
    const auto sel = selectedId.toString();
    return sel.isEmpty() ? juce::ValueTree() : media.getChildWithProperty (id, sel);
}

void MediaPanel::setProp (const juce::Identifier& prop, const juce::var& value)
{
    if (updating)
        return;

    auto item = selectedItem();

    if (item.isValid())
        processor.editMedia ([&] (juce::ValueTree&) { item.setProperty (prop, value, nullptr); });
}

void MediaPanel::moveSelected (int delta)
{
    auto item = selectedItem();
    if (! item.isValid()) return;

    const int from = media.indexOf (item);
    const int to = juce::jlimit (0, media.getNumChildren() - 1, from + delta);

    if (from != to)
        processor.editMedia ([&] (juce::ValueTree& m) { m.moveChild (from, to, nullptr); });
}

void MediaPanel::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Scegli immagini o GIF", juce::File(), Media::getSupportedWildcard());

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectMultipleItems,
                          [safe = juce::Component::SafePointer<MediaPanel> (this)] (const juce::FileChooser& fc)
                          {
                              if (safe == nullptr || ! safe->onAddFile)
                                  return;

                              for (auto& f : fc.getResults())
                                  safe->onAddFile (f);
                          });
}

void MediaPanel::chooseColour()
{
    auto item = selectedItem();
    if (! item.isValid()) return;

    auto picker = std::make_unique<ColourPicker> ([safe = juce::Component::SafePointer<MediaPanel> (this)] (juce::Colour c)
    {
        if (safe != nullptr)
            safe->setProp (colour, c.toString());
    });

    picker->setCurrentColour (juce::Colour::fromString (item[colour].toString()), juce::dontSendNotification);
    juce::CallOutBox::launchAsynchronously (std::move (picker), colourButton.getScreenBounds(), nullptr);
}

//==============================================================================
void MediaPanel::refreshList()
{
    list.updateContent();
    syncSelectionFromValue();
    list.repaint();
}

void MediaPanel::syncSelectionFromValue()
{
    const auto item = selectedItem();
    const int index = item.isValid() ? media.getNumChildren() - 1 - media.indexOf (item) : -1;

    updating = true;

    if (index >= 0)
        list.selectRow (index, true, true);
    else
        list.deselectAllRows();

    updating = false;
    refreshControls();
}

void MediaPanel::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& prop)
{
    list.repaint();

    // Aggiorna i controlli solo se la modifica viene da fuori (es. trascinamento sul video).
    if (tree == selectedItem() && ! updating && prop != text)
        refreshControls();
}

void MediaPanel::refreshControls()
{
    const auto item = selectedItem();
    const bool has = item.isValid();
    const bool isText = has && Media::isText (item);

    updating = true;

    for (auto* c : std::initializer_list<juce::Component*> { &sizeSlider, &opacitySlider, &rotationSlider,
                                                             &visibleToggle, &centreButton, &upButton, &downButton,
                                                             &duplicateButton, &deleteButton })
        c->setEnabled (has);

    for (auto* c : std::initializer_list<juce::Component*> { &textEditor, &fontBox, &colourButton, &boldToggle, &shadowToggle })
        c->setEnabled (isText);

    if (has)
    {
        sizeSlider.setValue ((double) item[size], juce::dontSendNotification);
        opacitySlider.setValue ((double) item[opacity], juce::dontSendNotification);
        rotationSlider.setValue ((double) item[rotation], juce::dontSendNotification);
        visibleToggle.setToggleState ((bool) item[visible], juce::dontSendNotification);
    }

    if (isText)
    {
        if (textEditor.getText() != item[text].toString())
            textEditor.setText (item[text].toString(), false);

        const int fontIndex = fontNames.indexOf (item[font].toString());
        fontBox.setSelectedId (fontIndex >= 0 ? fontIndex + 1 : 0, juce::dontSendNotification);
        if (fontIndex < 0)
        {
            const auto f = item[font].toString();
            fontBox.setText (f.startsWith ("<") ? juce::String ("Carattere predefinito") : f, juce::dontSendNotification);
        }

        boldToggle.setToggleState ((bool) item[bold], juce::dontSendNotification);
        shadowToggle.setToggleState ((bool) item[shadow], juce::dontSendNotification);
        colourButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromString (item[colour].toString()).withAlpha (1.0f));
        colourButton.setColour (juce::TextButton::textColourOffId,
                                juce::Colour::fromString (item[colour].toString()).getPerceivedBrightness() > 0.5f
                                    ? juce::Colours::black : juce::Colours::white);
    }
    else
    {
        textEditor.setText ({}, false);
        fontBox.setSelectedId (0, juce::dontSendNotification);
        colourButton.removeColour (juce::TextButton::buttonColourId);
        colourButton.removeColour (juce::TextButton::textColourOffId);
    }

    updating = false;
}

//==============================================================================
// L'elenco mostra in alto l'elemento che sta sopra (ultimo figlio).
int MediaPanel::getNumRows() { return media.getNumChildren(); }

void MediaPanel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    const auto item = media.getChild (media.getNumChildren() - 1 - row);
    if (! item.isValid()) return;

    if (selected)
        g.fillAll (accent.withAlpha (0.35f));

    const bool vis = item[visible];
    g.setColour (juce::Colours::white.withAlpha (vis ? 0.9f : 0.4f));
    g.setFont (juce::FontOptions (14.0f));
    g.drawText (Media::getDisplayName (item) + (vis ? "" : "  (nascosto)"),
                8, 0, width - 16, height, juce::Justification::centredLeft, true);
}

void MediaPanel::selectedRowsChanged (int lastRowSelected)
{
    if (updating)
        return;

    const auto item = media.getChild (media.getNumChildren() - 1 - lastRowSelected);
    selectedId = item.isValid() ? item[id].toString() : juce::String();
}

void MediaPanel::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    selectedRowsChanged (row);
}

//==============================================================================
void MediaPanel::paint (juce::Graphics& g)
{
    g.fillAll (panelBg);

    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawHorizontalLine (textLabel.getY() - 8, 10.0f, (float) getWidth() - 10.0f);
}

void MediaPanel::resized()
{
    auto area = getLocalBounds().reduced (10);
    constexpr int rowH = 28, gap = 6;

    auto row = area.removeFromTop (rowH);
    addTextButton.setBounds (row.removeFromLeft (row.getWidth() / 2 - 3));
    row.removeFromLeft (6);
    addImageButton.setBounds (row);
    area.removeFromTop (gap);

    list.setBounds (area.removeFromTop (150));
    area.removeFromTop (gap);

    row = area.removeFromTop (rowH);
    const int bw = (row.getWidth() - 3 * 4) / 4;
    for (auto* b : { &upButton, &downButton, &duplicateButton, &deleteButton })
    {
        b->setBounds (row.removeFromLeft (bw));
        row.removeFromLeft (4);
    }

    area.removeFromTop (16);

    textLabel.setBounds (area.removeFromTop (20));
    textEditor.setBounds (area.removeFromTop (56));
    area.removeFromTop (gap);
    fontBox.setBounds (area.removeFromTop (rowH));
    area.removeFromTop (gap);

    row = area.removeFromTop (rowH);
    colourButton.setBounds (row.removeFromLeft (90));
    row.removeFromLeft (8);
    boldToggle.setBounds (row.removeFromLeft (110));
    shadowToggle.setBounds (row);
    area.removeFromTop (gap * 2);

    auto sliderRow = [&] (juce::Label& l, juce::Slider& s)
    {
        auto r = area.removeFromTop (rowH);
        l.setBounds (r.removeFromLeft (84));
        s.setBounds (r);
        area.removeFromTop (gap);
    };

    sliderRow (sizeLabel, sizeSlider);
    sliderRow (opacityLabel, opacitySlider);
    sliderRow (rotationLabel, rotationSlider);

    row = area.removeFromTop (rowH);
    visibleToggle.setBounds (row.removeFromLeft (110));
    centreButton.setBounds (row.removeFromLeft (90));
    area.removeFromTop (gap * 2);

    hint.setBounds (area);
}

//==============================================================================
MediaWindow::MediaWindow (T8CamProcessor& p, juce::Value& selectedMediaId)
    : juce::DocumentWindow ("T8Cam - Media", juce::Colour (0xff1e1e1e), juce::DocumentWindow::closeButton)
{
    panel = new MediaPanel (p, selectedMediaId);
    setUsingNativeTitleBar (true);
    setContentOwned (panel, true);
    setResizable (true, false);
    setResizeLimits (320, 560, 800, 1400);
    setAlwaysOnTop (true);
}

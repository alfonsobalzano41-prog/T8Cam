#include "PluginEditor.h"
#include "CameraPermission.h"
#include "Presets.h"
#include "VideoEffects.h"

namespace
{
    const juce::Colour toolbarBg (0xff1a1a1a);
    const juce::Colour accent (0xff4fc3f7);
}

//==============================================================================
UpdateBanner::UpdateBanner()
{
    addAndMakeVisible (label);
    addAndMakeVisible (downloadButton);
    addAndMakeVisible (closeButton);

    label.setColour (juce::Label::textColourId, juce::Colours::white);
    downloadButton.onClick = [this] { juce::URL (downloadUrl).launchInDefaultBrowser(); };
    closeButton.onClick = [this] { setVisible (false); };
}

void UpdateBanner::show (const juce::String& version, const juce::String& url)
{
    label.setText (juce::String::fromUTF8 ("È disponibile T8Cam ") + version
                       + " (tu hai la " + UpdateChecker::getCurrentVersion() + ")",
                   juce::dontSendNotification);
    downloadUrl = url;
    setVisible (true);
}

void UpdateBanner::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xe0202830));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    g.setColour (accent);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
}

void UpdateBanner::resized()
{
    auto r = getLocalBounds().reduced (6, 4);
    closeButton.setBounds (r.removeFromRight (28));
    r.removeFromRight (4);
    downloadButton.setBounds (r.removeFromRight (80));
    label.setBounds (r);
}

//==============================================================================
T8CamEditor::T8CamEditor (T8CamProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      view (p, selectedMediaId)
{
    addAndMakeVisible (view);
    addChildComponent (updateBanner);
    addChildComponent (showUiButton);

    for (auto* c : std::initializer_list<juce::Component*> {
             &presetBox, &saveButton, &menuButton, &mediaButton, &hideButton,
             &cameraBox, &refreshButton, &effectBox, &amountLabel, &amountSlider, &mirrorButton, &pulseLabel, &pulseSlider })
        addAndMakeVisible (c);

    // --- Preset
    presetBox.setTextWhenNothingSelected ("Preset");
    presetBox.onChange = [this]
    {
        const int idVal = presetBox.getSelectedId();

        if (idVal <= 0)
            return;

        if (idVal >= userPresetIdBase)
        {
            const auto file = userPresetFiles[idVal - userPresetIdBase];
            if (! processor.getPresets().loadUser (file))
                showError ("Non riesco a leggere il preset " + file.getFileNameWithoutExtension() + ".");
        }
        else
        {
            processor.getPresets().loadFactory (idVal - 1);
        }
    };

    saveButton.onClick = [this] { savePreset(); };
    menuButton.onClick = [this] { showMenu(); };
    mediaButton.onClick = [this] { openMediaWindow(); };
    hideButton.onClick = [this] { setUiHidden (true); };
    showUiButton.onClick = [this] { setUiHidden (false); };

    // --- Webcam
    cameraBox.setTextWhenNothingSelected ("Webcam");
    cameraBox.setTextWhenNoChoicesAvailable ("Nessuna webcam");
    cameraBox.onChange = [this]
    {
        const int index = cameraBox.getSelectedItemIndex();

        if (juce::isPositiveAndBelow (index, devices.size())
             && devices[index].id != processor.getCameraId())
        {
            processor.setCameraId (devices[index].id);
            view.start();
        }
    };

    refreshButton.onClick = [this]
    {
        refreshDeviceList();
        view.start();
    };

    // --- Effetti
    effectBox.addItemList (VideoEffects::getNames(), 1);
    amountLabel.setText (juce::String::fromUTF8 ("Intensità"), juce::dontSendNotification);
    amountSlider.setTooltip (juce::String::fromUTF8 ("Quanto è forte l'effetto"));
    pulseSlider.setTooltip ("L'immagine pulsa a tempo con l'audio della traccia");

    effectAttachment = std::make_unique<ComboAttachment>  (p.params, ParamIDs::effect, effectBox);
    amountAttachment = std::make_unique<SliderAttachment> (p.params, ParamIDs::amount, amountSlider);
    pulseAttachment  = std::make_unique<SliderAttachment> (p.params, ParamIDs::pulse,  pulseSlider);
    mirrorAttachment = std::make_unique<ButtonAttachment> (p.params, ParamIDs::mirror, mirrorButton);

    // --- Video
    view.onDoubleClickBackground = [this] { setUiHidden (! uiHidden); };
    view.onMediaError = [this] (const juce::String& e) { showError (e); };

    processor.getPresets().addChangeListener (this);

    setResizable (true, true);
    setResizeLimits (480, 270 + toolbarHeight, 3840, 2160);
    setSize (800, 450 + toolbarHeight);

    refreshPresetList();
    refreshDeviceList();
    setUiHidden (processor.isUiHidden());
    view.start();

    startTimerHz (10);
    checkForUpdates (false);
}

T8CamEditor::~T8CamEditor()
{
    processor.getPresets().removeChangeListener (this);
    mediaWindow.reset();
    view.stop();
}

//==============================================================================
void T8CamEditor::refreshDeviceList()
{
    devices.clear();
    cameraBox.clear (juce::dontSendNotification);

    // Se il DAW non può chiedere il permesso, non tocchiamo nemmeno l'elenco delle camere.
    const auto status = CameraPermission::getStatus();

    if (status == CameraPermission::Status::hostCannotAsk || status == CameraPermission::Status::unsupported)
        return;

    devices = CameraSource::getAvailableDevices();

    for (int i = 0; i < devices.size(); ++i)
        cameraBox.addItem (devices[i].name, i + 1);

    const auto current = processor.getCameraId();

    for (int i = 0; i < devices.size(); ++i)
        if (devices[i].id == current)
            cameraBox.setSelectedItemIndex (i, juce::dontSendNotification);

    if (cameraBox.getSelectedItemIndex() < 0 && ! devices.isEmpty())
        cameraBox.setSelectedItemIndex (0, juce::dontSendNotification);
}

void T8CamEditor::refreshPresetList()
{
    auto& presets = processor.getPresets();
    presetBox.clear (juce::dontSendNotification);

    presetBox.addSectionHeading ("Preset T8Cam");
    for (int i = 0; i < presets.getNumFactory(); ++i)
        presetBox.addItem (presets.getFactoryName (i), i + 1);

    userPresetFiles = presets.getUserPresets();

    if (! userPresetFiles.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("I miei preset");

        for (int i = 0; i < userPresetFiles.size(); ++i)
            presetBox.addItem (userPresetFiles[i].getFileNameWithoutExtension(), userPresetIdBase + i);
    }

    // Mostra il preset attuale (prima i tuoi, che hanno la precedenza sui nomi uguali).
    const auto current = presets.getCurrentName();

    for (int i = 0; i < userPresetFiles.size(); ++i)
        if (userPresetFiles[i].getFileNameWithoutExtension() == current)
            return presetBox.setSelectedId (userPresetIdBase + i, juce::dontSendNotification);

    for (int i = 0; i < presets.getNumFactory(); ++i)
        if (presets.getFactoryName (i) == current)
            return presetBox.setSelectedId (i + 1, juce::dontSendNotification);

    presetBox.setText (current, juce::dontSendNotification);
}

void T8CamEditor::savePreset()
{
    auto* window = new juce::AlertWindow ("Salva preset",
                                          juce::String::fromUTF8 ("Il preset salva effetto, intensità, specchio, pulse e tutti i media."),
                                          juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor ("name", processor.getPresets().getCurrentName(), "Nome:");
    window->addButton ("Salva", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Annulla", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    window->enterModalState (true, juce::ModalCallbackFunction::create (
        [safe = juce::Component::SafePointer<T8CamEditor> (this), window] (int result)
        {
            const auto name = window->getTextEditorContents ("name").trim();

            if (safe == nullptr || result != 1 || name.isEmpty())
                return;

            if (safe->processor.getPresets().saveUser (name) == juce::File())
                safe->showError ("Non riesco a salvare il preset.");
        }), true);
}

void T8CamEditor::showMenu()
{
    auto& presets = processor.getPresets();
    const auto currentUserFile = presets.findUser (presets.getCurrentName());

    juce::PopupMenu menu;
    menu.addItem (1, "Elimina il preset \"" + presets.getCurrentName() + "\"", currentUserFile != juce::File());
    menu.addItem (2, "Apri la cartella dei preset");
    menu.addSeparator();
    menu.addItem (3, "Controlla aggiornamenti");
    menu.addItem (4, "Pagina download di T8Cam");
    menu.addSeparator();
    menu.addItem (5, "T8Cam " + UpdateChecker::getCurrentVersion(), false, false);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (menuButton),
        [safe = juce::Component::SafePointer<T8CamEditor> (this), currentUserFile] (int choice)
        {
            if (safe == nullptr)
                return;

            switch (choice)
            {
                case 1:
                    safe->processor.getPresets().deleteUser (currentUserFile);
                    break;

                case 2:
                {
                    auto folder = PresetManager::getUserFolder();
                    folder.createDirectory();
                    folder.startAsProcess();
                    break;
                }

                case 3: safe->checkForUpdates (true); break;
                case 4: juce::URL (UpdateChecker::getReleasesPage()).launchInDefaultBrowser(); break;
                default: break;
            }
        });
}

void T8CamEditor::openMediaWindow()
{
    if (mediaWindow == nullptr)
    {
        mediaWindow = std::make_unique<MediaWindow> (processor, selectedMediaId);
        mediaWindow->getPanel().onAddFile = [this] (const juce::File& f)
        {
            view.addMediaFile (f, view.getLocalBounds().getCentre().toFloat());
        };

        const auto screen = getScreenBounds();
        mediaWindow->setBounds (screen.getRight() + 12, screen.getY(), 360, 640);

        // Se finisce fuori dallo schermo, mettila accanto al bordo sinistro dell'editor.
        if (auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (screen))
            if (! display->userArea.contains (mediaWindow->getBounds()))
                mediaWindow->setTopLeftPosition (juce::jmax (display->userArea.getX(), screen.getX() - 372), screen.getY());
    }

    mediaWindow->setVisible (true);
    mediaWindow->toFront (true);
}

void T8CamEditor::setUiHidden (bool hidden)
{
    uiHidden = hidden;
    processor.setUiHidden (hidden);

    for (auto* c : std::initializer_list<juce::Component*> {
             &presetBox, &saveButton, &menuButton, &mediaButton, &hideButton,
             &cameraBox, &refreshButton, &effectBox, &amountLabel, &amountSlider, &mirrorButton, &pulseLabel, &pulseSlider })
        c->setVisible (! hidden);

    if (hidden)
        updateBanner.setVisible (false);

    showUiButton.setVisible (false);
    view.setShowSelection (! hidden);
    resized();
}

void T8CamEditor::checkForUpdates (bool manual)
{
    updateChecker.check (manual, [safe = juce::Component::SafePointer<T8CamEditor> (this), manual] (const UpdateChecker::Result& r)
    {
        if (safe == nullptr)
            return;

        using S = UpdateChecker::Result::Status;

        if (r.status == S::updateAvailable)
        {
            if (! safe->uiHidden)
                safe->updateBanner.show (r.latestVersion, r.downloadUrl);
        }
        else if (manual)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Aggiornamenti",
                r.status == S::upToDate
                    ? juce::String::fromUTF8 ("Hai già l'ultima versione (") + UpdateChecker::getCurrentVersion() + ")."
                    : juce::String ("Non riesco a controllare gli aggiornamenti. Sei connesso a internet?"));
        }
    });
}

void T8CamEditor::showError (const juce::String& text)
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "T8Cam", text);
}

void T8CamEditor::timerCallback()
{
    // In modalità pulita il pulsante "Mostra UI" compare solo col mouse sopra il plugin.
    if (uiHidden)
    {
        const bool over = isMouseOver (true);

        if (over != showUiButton.isVisible())
            showUiButton.setVisible (over);
    }
}

//==============================================================================
void T8CamEditor::paint (juce::Graphics& g)
{
    g.fillAll (toolbarBg);
}

void T8CamEditor::resized()
{
    auto area = getLocalBounds();

    showUiButton.setBounds (getWidth() - 100, 8, 92, 26);

    if (! uiHidden)
    {
        auto bar = area.removeFromTop (toolbarHeight).reduced (6, 4);

        auto row1 = bar.removeFromTop (rowHeight).reduced (0, 2);
        hideButton.setBounds (row1.removeFromRight (100));
        row1.removeFromRight (4);
        mediaButton.setBounds (row1.removeFromRight (70));
        row1.removeFromRight (12);
        presetBox.setBounds (row1.removeFromLeft (juce::jmin (240, row1.getWidth() / 2)));
        row1.removeFromLeft (4);
        saveButton.setBounds (row1.removeFromLeft (60));
        row1.removeFromLeft (4);
        menuButton.setBounds (row1.removeFromLeft (60));

        bar.removeFromTop (4);
        auto row2 = bar.removeFromTop (rowHeight).reduced (0, 2);
        cameraBox.setBounds (row2.removeFromLeft (juce::jmin (180, row2.getWidth() / 5)));
        row2.removeFromLeft (4);
        refreshButton.setBounds (row2.removeFromLeft (70));
        row2.removeFromLeft (10);
        effectBox.setBounds (row2.removeFromLeft (juce::jmin (170, row2.getWidth() / 4)));
        row2.removeFromLeft (6);
        amountLabel.setBounds (row2.removeFromLeft (64));
        const int sliderW = juce::jmax (40, (row2.getWidth() - 90 - 46) / 2);
        amountSlider.setBounds (row2.removeFromLeft (sliderW));
        row2.removeFromLeft (6);
        mirrorButton.setBounds (row2.removeFromLeft (90));
        pulseLabel.setBounds (row2.removeFromLeft (46));
        pulseSlider.setBounds (row2);
    }

    view.setBounds (area);

    const int bannerW = juce::jmin (520, area.getWidth() - 20);
    updateBanner.setBounds (area.getCentreX() - bannerW / 2, area.getBottom() - 46, bannerW, 36);
    updateBanner.toFront (false);
    showUiButton.toFront (false);
}

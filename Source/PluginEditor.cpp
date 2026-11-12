#include "PluginEditor.h"
#include "CameraPermission.h"

T8CamEditor::T8CamEditor (T8CamProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      view (p),
      mirrorAttachment (p.params, ParamIDs::mirror, mirrorButton),
      monoAttachment (p.params, ParamIDs::mono, monoButton),
      pulseAttachment (p.params, ParamIDs::pulse, pulseSlider)
{
    addAndMakeVisible (view);

    for (auto* c : std::initializer_list<juce::Component*> { &cameraBox, &refreshButton, &mirrorButton,
                                                             &monoButton, &pulseLabel, &pulseSlider })
        addAndMakeVisible (c);

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

    pulseSlider.setTooltip ("L'immagine pulsa a tempo con l'audio della traccia");

    setResizable (true, true);
    setResizeLimits (320, 240 + toolbarHeight, 3840, 2160);
    setSize (640, 360 + toolbarHeight);

    refreshDeviceList();
    view.start();
}

T8CamEditor::~T8CamEditor()
{
    view.stop();
}

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

void T8CamEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1a));
}

void T8CamEditor::resized()
{
    auto area = getLocalBounds();
    auto bar = area.removeFromTop (toolbarHeight).reduced (6, 4);

    cameraBox.setBounds (bar.removeFromLeft (juce::jmin (220, bar.getWidth() / 3)));
    bar.removeFromLeft (4);
    refreshButton.setBounds (bar.removeFromLeft (70));
    bar.removeFromLeft (8);
    mirrorButton.setBounds (bar.removeFromLeft (90));
    monoButton.setBounds (bar.removeFromLeft (60));
    pulseLabel.setBounds (bar.removeFromLeft (46));
    pulseSlider.setBounds (bar);

    view.setBounds (area);
}

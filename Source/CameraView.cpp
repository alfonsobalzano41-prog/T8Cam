#include "CameraView.h"
#include "CameraPermission.h"

CameraView::CameraView (T8CamProcessor& p)
    : processor (p)
{
    mirrorParam = processor.params.getRawParameterValue (ParamIDs::mirror);
    monoParam   = processor.params.getRawParameterValue (ParamIDs::mono);
    pulseParam  = processor.params.getRawParameterValue (ParamIDs::pulse);

    settingsButton.onClick = [] { CameraPermission::openSystemSettings(); };

    appButton.onClick = [this]
    {
        const auto app = findStandaloneApp();

        if (app.exists())
            app.startAsProcess();
        else
            showMessage ("Non trovo T8Cam.app.\nCopiala nella cartella Applicazioni e riprova.", false, true);
    };

    addChildComponent (settingsButton);
    addChildComponent (appButton);

    startTimerHz (30);
}

CameraView::~CameraView()
{
    *alive = false;
    stop();
}

//==============================================================================
void CameraView::start()
{
    stop();

    switch (CameraPermission::getStatus())
    {
        case CameraPermission::Status::granted:
            openWithPermission();
            break;

        case CameraPermission::Status::notDetermined:
            showMessage ("In attesa del permesso per la webcam...", false, false);
            CameraPermission::request ([this, flag = alive] (bool granted)
            {
                if (! *flag)
                    return;

                if (granted)
                    openWithPermission();
                else
                    showMessage ("Accesso alla webcam negato.\nAbilitalo in Impostazioni di Sistema > Privacy e sicurezza > Fotocamera.",
                                 true, false);
            });
            break;

        case CameraPermission::Status::denied:
            showMessage (CameraPermission::getHostName()
                           + " non ha il permesso di usare la webcam.\n"
                             "Abilitalo in Impostazioni di Sistema > Privacy e sicurezza > Fotocamera,\n"
                             "poi riavvia " + CameraPermission::getHostName() + ".",
                         true, false);
            break;

        case CameraPermission::Status::hostCannotAsk:
            showMessage (CameraPermission::getHostName()
                           + " non permette ai plugin di usare la webcam:\n"
                             "macOS lo chiuderebbe se ci provassimo, quindi non la apriamo.\n\n"
                             "Usa l'app T8Cam accanto al DAW.",
                         false, true);
            break;

        case CameraPermission::Status::unsupported:
            showMessage (juce::String::fromUTF8 ("La webcam non è supportata su questo sistema."), false, false);
            break;
    }
}

void CameraView::openWithPermission()
{
    source = CameraSource::create();

    if (source == nullptr)
    {
        showMessage (juce::String::fromUTF8 ("La webcam non è supportata su questo sistema."), false, false);
        return;
    }

    source->onFrame = [this] (const juce::Image& img) { handleFrame (img); };

    const auto error = source->open (processor.getCameraId());

    if (error.isNotEmpty())
    {
        source.reset();
        showMessage (error, false, false);
        return;
    }

    showMessage ("Avvio della webcam...", false, false);
}

void CameraView::stop()
{
    if (source != nullptr)
    {
        source->close();    // dopo close() non arrivano più fotogrammi
        source.reset();
    }

    const juce::ScopedLock sl (frameLock);
    latestFrame = {};
    currentFrame = {};
    hasNewFrame = false;
}

void CameraView::showMessage (const juce::String& text, bool showSettings, bool showApp)
{
    message = text;
    settingsButton.setVisible (showSettings);

   #if JUCE_MAC
    appButton.setVisible (showApp && ! juce::JUCEApplicationBase::isStandaloneApp());
   #else
    juce::ignoreUnused (showApp);
    appButton.setVisible (false);
   #endif

    resized();
    repaint();
}

juce::File CameraView::findStandaloneApp()
{
    for (auto dir : { juce::File ("/Applications"),
                      juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Applications") })
    {
        auto app = dir.getChildFile ("T8Cam.app");

        if (app.exists())
            return app;
    }

    return {};
}

//==============================================================================
void CameraView::handleFrame (const juce::Image& img)
{
    // Thread della camera: qui facciamo il lavoro pesante sui pixel.
    juce::Image frame = img;

    if (monoParam->load() > 0.5f)
    {
        frame = img.createCopy();
        juce::Image::BitmapData data (frame, juce::Image::BitmapData::readWrite);

        for (int y = 0; y < data.height; ++y)
        {
            auto* px = data.getLinePointer (y);

            for (int x = 0; x < data.width; ++x, px += data.pixelStride)
            {
                // Layout ARGB di JUCE in memoria (little-endian): B, G, R, A
                const auto lum = (uint8_t) ((px[2] * 77 + px[1] * 150 + px[0] * 29) >> 8);
                px[0] = px[1] = px[2] = lum;
            }
        }
    }

    const juce::ScopedLock sl (frameLock);
    latestFrame = frame;
    hasNewFrame = true;
}

void CameraView::timerCallback()
{
    bool needsRepaint = false;

    if (hasNewFrame.exchange (false))
    {
        const juce::ScopedLock sl (frameLock);
        currentFrame = latestFrame;
        needsRepaint = true;

        if (message.isNotEmpty())
            showMessage ({}, false, false);
    }

    // Livello audio: salita rapida, discesa morbida.
    const float target = juce::jlimit (0.0f, 1.0f, processor.getLevel() * 1.5f);
    displayLevel = juce::jmax (target, displayLevel * 0.85f);

    if (pulseParam->load() > 0.0f && displayLevel > 0.001f)
        needsRepaint = true;

    if (needsRepaint)
        repaint();
}

void CameraView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    auto bounds = getLocalBounds().toFloat();

    if (currentFrame.isValid())
    {
        const auto pulse = pulseParam->load() * displayLevel;
        const auto iw = (float) currentFrame.getWidth();
        const auto ih = (float) currentFrame.getHeight();

        // Riempie la finestra ritagliando i bordi, come una facecam.
        const auto scale = juce::jmax (bounds.getWidth() / iw, bounds.getHeight() / ih) * (1.0f + 0.25f * pulse);
        const auto cx = bounds.getCentreX();
        const auto cy = bounds.getCentreY();

        auto t = juce::AffineTransform::translation (-iw * 0.5f, -ih * 0.5f)
                     .scaled (mirrorParam->load() > 0.5f ? -scale : scale, scale)
                     .translated (cx, cy);

        g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
        g.drawImageTransformed (currentFrame, t, false);

        if (pulse > 0.0f)
            g.fillAll (juce::Colours::white.withAlpha (0.2f * pulse));
    }

    if (message.isNotEmpty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (juce::FontOptions (15.0f));

        auto textArea = getLocalBounds().reduced (24);
        if (settingsButton.isVisible() || appButton.isVisible())
            textArea.removeFromBottom (60);

        g.drawFittedText (message, textArea, juce::Justification::centred, 8);
    }
}

void CameraView::resized()
{
    auto area = getLocalBounds().reduced (24);
    auto row = area.removeFromBottom (36).withSizeKeepingCentre (220, 32);

    settingsButton.setBounds (row);
    appButton.setBounds (row);
}

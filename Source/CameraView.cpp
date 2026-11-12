#include "CameraView.h"
#include "CameraPermission.h"
#include "VideoEffects.h"

using namespace MediaIDs;

CameraView::CameraView (T8CamProcessor& p, juce::Value& selectedMediaId)
    : processor (p), media (p.getMedia())
{
    mirrorParam = processor.params.getRawParameterValue (ParamIDs::mirror);
    effectParam = processor.params.getRawParameterValue (ParamIDs::effect);
    amountParam = processor.params.getRawParameterValue (ParamIDs::amount);
    pulseParam  = processor.params.getRawParameterValue (ParamIDs::pulse);

    selectedId.referTo (selectedMediaId);
    selectedId.addListener (this);
    media.addListener (this);

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

    mediaChanged();
    startTimerHz (30);
}

CameraView::~CameraView()
{
    *alive = false;
    media.removeListener (this);
    selectedId.removeListener (this);
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

void CameraView::setShowSelection (bool shouldShow)
{
    showSelection = shouldShow;
    repaint();
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
    const int effect = juce::roundToInt (effectParam->load());

    if (effect > 0)
    {
        frame = img.createCopy();
        const float level = juce::jlimit (0.0f, 1.0f, processor.getLevel() * 1.5f);
        VideoEffects::apply (frame, effect, amountParam->load(), level, juce::Time::getMillisecondCounter());
    }

    const juce::ScopedLock sl (frameLock);
    latestFrame = frame;
    hasNewFrame = true;
}

void CameraView::timerCallback()
{
    bool needsRepaint = animatedMedia;

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

void CameraView::mediaChanged()
{
    processor.getMediaCache().purge (media);
    animatedMedia = MediaRenderer::hasAnimation (media, processor.getMediaCache());

    if (getSelectedItem() == juce::ValueTree() && selectedId.toString().isNotEmpty())
        selectedId = juce::String();

    repaint();
}

//==============================================================================
void CameraView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    const auto bounds = getLocalBounds().toFloat();

    if (currentFrame.isValid())
    {
        const auto pulse = pulseParam->load() * displayLevel;
        const auto iw = (float) currentFrame.getWidth();
        const auto ih = (float) currentFrame.getHeight();

        // Riempie la finestra ritagliando i bordi, come una facecam.
        const auto scale = juce::jmax (bounds.getWidth() / iw, bounds.getHeight() / ih) * (1.0f + 0.25f * pulse);

        auto t = juce::AffineTransform::translation (-iw * 0.5f, -ih * 0.5f)
                     .scaled (mirrorParam->load() > 0.5f ? -scale : scale, scale)
                     .translated (bounds.getCentreX(), bounds.getCentreY());

        g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
        g.drawImageTransformed (currentFrame, t, false);

        if (pulse > 0.0f)
            g.fillAll (juce::Colours::white.withAlpha (0.2f * pulse));
    }

    // Media sopra il video
    auto& cache = processor.getMediaCache();
    const auto now = juce::Time::getMillisecondCounter();

    for (const auto& item : media)
        MediaRenderer::draw (g, item, bounds, cache, now);

    if (showSelection)
    {
        if (auto sel = getSelectedItem(); sel.isValid())
        {
            const auto geo = MediaRenderer::getGeometry (sel, bounds, cache);

            if (geo.valid)
            {
                juce::Path dashed;
                const float dashes[] = { 6.0f, 4.0f };
                juce::PathStrokeType (1.5f).createDashedStroke (dashed, geo.outline(), dashes, 2);
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.strokePath (geo.outline(), juce::PathStrokeType (3.0f));
                g.setColour (juce::Colour (0xff4fc3f7));
                g.fillPath (dashed);
            }
        }
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

//==============================================================================
juce::ValueTree CameraView::findItemAt (juce::Point<float> p)
{
    const auto bounds = getLocalBounds().toFloat();

    for (int i = media.getNumChildren(); --i >= 0;)   // dall'alto verso il basso
    {
        auto item = media.getChild (i);

        if ((bool) item[visible] && MediaRenderer::getGeometry (item, bounds, processor.getMediaCache()).contains (p))
            return item;
    }

    return {};
}

juce::ValueTree CameraView::getSelectedItem()
{
    const auto sel = selectedId.toString();
    return sel.isEmpty() ? juce::ValueTree() : media.getChildWithProperty (id, sel);
}

void CameraView::scaleItem (juce::ValueTree item, float factor)
{
    processor.editMedia ([&] (juce::ValueTree&)
    {
        item.setProperty (size, juce::jlimit (0.01f, 3.0f, (float) item[size] * factor), nullptr);
    });
}

void CameraView::mouseDown (const juce::MouseEvent& e)
{
    dragItem = findItemAt (e.position);

    if (dragItem.isValid())
    {
        selectedId = dragItem[id].toString();
        const auto b = getLocalBounds().toFloat();
        dragOffset = e.position - juce::Point<float> ((float) dragItem[x] * b.getWidth(), (float) dragItem[y] * b.getHeight());
    }
    else
    {
        selectedId = juce::String();
    }
}

void CameraView::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragItem.isValid())
        return;

    const auto b = getLocalBounds().toFloat();
    const auto pos = e.position - dragOffset;

    processor.editMedia ([&] (juce::ValueTree&)
    {
        dragItem.setProperty (x, juce::jlimit (-0.2f, 1.2f, pos.x / b.getWidth()), nullptr);
        dragItem.setProperty (y, juce::jlimit (-0.2f, 1.2f, pos.y / b.getHeight()), nullptr);
    });
}

void CameraView::mouseUp (const juce::MouseEvent&)
{
    dragItem = {};
}

void CameraView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! findItemAt (e.position).isValid() && onDoubleClickBackground)
        onDoubleClickBackground();
}

void CameraView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    auto item = findItemAt (e.position);

    if (! item.isValid())
        item = getSelectedItem();

    if (item.isValid())
        scaleItem (item, 1.0f + juce::jlimit (-0.5f, 0.5f, wheel.deltaY * (wheel.isReversed ? -1.0f : 1.0f)));
}

void CameraView::mouseMagnify (const juce::MouseEvent& e, float scaleFactor)
{
    auto item = findItemAt (e.position);

    if (! item.isValid())
        item = getSelectedItem();

    if (item.isValid())
        scaleItem (item, scaleFactor);
}

//==============================================================================
bool CameraView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (Media::isSupportedFile (juce::File (f)))
            return true;

    return false;
}

void CameraView::filesDropped (const juce::StringArray& files, int dropX, int dropY)
{
    for (auto& f : files)
        addMediaFile (juce::File (f), { (float) dropX, (float) dropY });
}

void CameraView::addMediaFile (const juce::File& file, juce::Point<float> where)
{
    if (! Media::isSupportedFile (file))
        return;

    if (file.getSize() > Media::maxFileBytes)
    {
        if (onMediaError)
            onMediaError (file.getFileName() + juce::String::fromUTF8 (" è troppo grande (massimo 30 MB)."));
        return;
    }

    juce::MemoryBlock bytes;

    if (! file.loadFileAsData (bytes))
        return;

    auto item = Media::createImage (file.getFileName(), bytes);

    if (processor.getMediaCache().get (item) == nullptr)
    {
        if (onMediaError)
            onMediaError ("Non riesco a leggere " + file.getFileName() + ".");
        return;
    }

    const auto b = getLocalBounds().toFloat();

    if (! b.isEmpty() && b.contains (where))
    {
        item.setProperty (x, where.x / b.getWidth(), nullptr);
        item.setProperty (y, where.y / b.getHeight(), nullptr);
    }

    processor.editMedia ([&] (juce::ValueTree& m) { m.appendChild (item, nullptr); });
    selectedId = item[id].toString();
}

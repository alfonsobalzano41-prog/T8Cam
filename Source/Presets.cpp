#include "Presets.h"
#include "PluginProcessor.h"

namespace
{
    const juce::Identifier presetTag  { "T8CamPreset" };
    const juce::Identifier paramTag   { "PARAM" };

    juce::ValueTree makeParam (const juce::String& id, float value)
    {
        juce::ValueTree p (paramTag);
        p.setProperty ("id", id, nullptr);
        p.setProperty ("value", value, nullptr);
        return p;
    }

    /** Preset di fabbrica: effetto (indice del menu), intensità, pulse, eventuali media. */
    juce::ValueTree makePreset (int effect, float amount, float pulse, juce::ValueTree media = {})
    {
        juce::ValueTree t (presetTag);
        t.appendChild (makeParam (ParamIDs::effect, (float) effect), nullptr);
        t.appendChild (makeParam (ParamIDs::amount, amount), nullptr);
        t.appendChild (makeParam (ParamIDs::pulse,  pulse), nullptr);

        if (media.isValid())
            t.appendChild (media, nullptr);

        return t;
    }

    juce::ValueTree textBadge (const juce::String& text, float x, float y, float size, juce::Colour colour)
    {
        juce::ValueTree media (MediaIDs::MEDIA);
        auto item = Media::createText (text);
        item.setProperty (MediaIDs::x, x, nullptr);
        item.setProperty (MediaIDs::y, y, nullptr);
        item.setProperty (MediaIDs::size, size, nullptr);
        item.setProperty (MediaIDs::colour, colour.toString(), nullptr);
        media.appendChild (item, nullptr);
        return media;
    }

    juce::String safeFileName (const juce::String& name)
    {
        return juce::File::createLegalFileName (name.trim()).substring (0, 80);
    }
}

PresetManager::PresetManager (T8CamProcessor& p)
    : processor (p)
{
    buildFactory();
}

void PresetManager::buildFactory()
{
    // Indici effetti: vedi VideoEffects::getNames()
    enum { none, mono, negative, posterize, thermal, nightVision,
           glitch, sketch, neon, comic, duotone, kaleidoscope };

    const auto u = [] (const char* s) { return juce::String::fromUTF8 (s); };

    const Factory list[] = {
        { "Pulito",               makePreset (none,         1.0f, 0.0f) },
        { "Noir",                 makePreset (mono,         1.0f, 0.0f) },
        { "Glitch club",          makePreset (glitch,       0.4f, 0.6f) },
        { "Termico",              makePreset (thermal,      1.0f, 0.0f) },
        { "Visione notturna",     makePreset (nightVision,  0.6f, 0.0f) },
        { "Fumetto",              makePreset (comic,        0.6f, 0.0f) },
        { "Schizzo a matita",     makePreset (sketch,       1.0f, 0.0f) },
        { "Neon pulse",           makePreset (neon,         1.0f, 0.5f) },
        { "Pop art",              makePreset (posterize,    0.8f, 0.0f) },
        { "Duotone tramonto",     makePreset (duotone,      1.0f, 0.0f) },
        { "Negativo",             makePreset (negative,     1.0f, 0.0f) },
        { "Caleidoscopio",        makePreset (kaleidoscope, 1.0f, 0.3f) },
        { u ("● LIVE"),           makePreset (none,         1.0f, 0.0f,
                                              textBadge (u ("● LIVE"), 0.13f, 0.09f, 0.08f, juce::Colour (0xffff3030))) },
        { "ON AIR",               makePreset (none,         1.0f, 0.0f,
                                              textBadge ("ON AIR", 0.85f, 0.09f, 0.08f, juce::Colours::white)) },
    };

    for (auto& f : list)
        factory.add (f);
}

int PresetManager::getNumFactory() const                   { return factory.size(); }
juce::String PresetManager::getFactoryName (int i) const   { return factory[i].name; }

void PresetManager::loadFactory (int index)
{
    if (! juce::isPositiveAndBelow (index, factory.size()))
        return;

    applyTree (factory.getReference (index).tree);
    setCurrentName (factory.getReference (index).name);
}

//==============================================================================
juce::File PresetManager::getUserFolder()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);

   #if JUCE_MAC
    base = base.getChildFile ("Application Support");
   #endif

    return base.getChildFile ("T8Cam").getChildFile ("Presets");
}

juce::Array<juce::File> PresetManager::getUserPresets() const
{
    auto files = getUserFolder().findChildFiles (juce::File::findFiles, false, "*" + extension);

    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0;
    });

    return files;
}

juce::File PresetManager::findUser (const juce::String& name) const
{
    auto f = getUserFolder().getChildFile (safeFileName (name) + extension);
    return f.existsAsFile() ? f : juce::File();
}

bool PresetManager::loadUser (const juce::File& file)
{
    auto xml = juce::parseXML (file);

    if (xml == nullptr || ! xml->hasTagName (presetTag.toString()))
        return false;

    applyTree (juce::ValueTree::fromXml (*xml));
    setCurrentName (file.getFileNameWithoutExtension());
    return true;
}

juce::File PresetManager::saveUser (const juce::String& name)
{
    const auto clean = safeFileName (name);

    if (clean.isEmpty())
        return {};

    auto folder = getUserFolder();

    if (! folder.createDirectory())
        return {};

    auto file = folder.getChildFile (clean + extension);
    auto xml = createTreeFromCurrent().createXml();

    if (xml == nullptr || ! xml->writeTo (file))
        return {};

    setCurrentName (clean);
    return file;
}

bool PresetManager::deleteUser (const juce::File& file)
{
    const bool ok = file.deleteFile();

    if (ok && file.getFileNameWithoutExtension() == currentName)
        setCurrentName ({});

    sendChangeMessage();
    return ok;
}

void PresetManager::setCurrentName (const juce::String& n)
{
    currentName = n;
    sendChangeMessage();
}

//==============================================================================
juce::ValueTree PresetManager::createTreeFromCurrent() const
{
    juce::ValueTree t (presetTag);
    const auto state = processor.params.copyState();

    for (const auto& child : state)
        if (child.hasType (paramTag))
            t.appendChild (child.createCopy(), nullptr);

    // I preset dell'utente salvano sempre i media (anche vuoti).
    t.appendChild (processor.getMedia().createCopy(), nullptr);
    return t;
}

void PresetManager::applyTree (const juce::ValueTree& t)
{
    for (const auto& child : t)
    {
        if (! child.hasType (paramTag))
            continue;

        if (auto* param = processor.params.getParameter (child.getProperty ("id").toString()))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 ((float) child.getProperty ("value")));
            param->endChangeGesture();
        }
    }

    const auto media = t.getChildWithName (MediaIDs::MEDIA);

    if (media.isValid())
    {
        // Nuovi id, così la cache non confonde elementi di preset diversi.
        auto copy = media.createCopy();
        for (auto item : copy)
            item.setProperty (MediaIDs::id, juce::Uuid().toString(), nullptr);

        processor.replaceMedia (copy);
    }
}

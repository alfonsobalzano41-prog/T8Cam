#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "GifDecoder.h"
#include <map>

/** Media sovrapposti al video (scritte, PNG/JPG, GIF animate).

    Ogni elemento è un nodo ValueTree "ITEM" dentro "MEDIA". L'ordine dei figli è l'ordine
    di disegno (l'ultimo sta sopra). Posizioni e dimensioni sono relative alla finestra
    video, così restano corrette a qualsiasi grandezza.
*/
namespace MediaIDs
{
    #define T8_ID(name) inline const juce::Identifier name { #name };
    T8_ID (MEDIA) T8_ID (ITEM)
    T8_ID (id) T8_ID (kind) T8_ID (name)
    T8_ID (x) T8_ID (y)            // centro, 0..1 rispetto alla finestra video
    T8_ID (size)                   // testo: altezza carattere / altezza video; immagine: larghezza / larghezza video
    T8_ID (opacity) T8_ID (rotation) T8_ID (visible)
    T8_ID (text) T8_ID (colour) T8_ID (font) T8_ID (bold) T8_ID (shadow)
    T8_ID (data)                   // file immagine codificato in base64
    #undef T8_ID

    inline const juce::String kindText  = "text";
    inline const juce::String kindImage = "image";
}

namespace Media
{
    juce::ValueTree createText (const juce::String& text);
    juce::ValueTree createImage (const juce::String& fileName, const juce::MemoryBlock& fileData);

    bool isText (const juce::ValueTree& item);
    juce::String getDisplayName (const juce::ValueTree& item);

    /** Formati accettati per i file trascinati o scelti. */
    juce::String getSupportedWildcard();
    bool isSupportedFile (const juce::File& f);
    inline constexpr juce::int64 maxFileBytes = 30 * 1024 * 1024;
}

/** Tiene in memoria le immagini già decodificate, per id elemento. Solo message thread. */
class MediaCache
{
public:
    const DecodedMedia* get (const juce::ValueTree& item);
    void purge (const juce::ValueTree& mediaRoot);

private:
    std::map<juce::String, std::unique_ptr<DecodedMedia>> entries;
};

/** Posizione di un elemento nella finestra video. */
struct MediaGeometry
{
    juce::Rectangle<float> local;      // rettangolo centrato su (0, 0), in pixel
    juce::AffineTransform toView;      // da coordinate locali a coordinate della finestra video
    bool valid = false;

    bool contains (juce::Point<float> viewPoint) const;
    juce::Path outline() const;
};

namespace MediaRenderer
{
    MediaGeometry getGeometry (const juce::ValueTree& item, juce::Rectangle<float> view, MediaCache&);
    void draw (juce::Graphics&, const juce::ValueTree& item, juce::Rectangle<float> view, MediaCache&, juce::uint32 timeMs);
    bool hasAnimation (const juce::ValueTree& mediaRoot, MediaCache&);
}

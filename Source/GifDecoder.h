#pragma once

#include <juce_graphics/juce_graphics.h>

/** Un'immagine o un'animazione pronta da disegnare. */
struct DecodedMedia
{
    juce::Array<juce::Image> frames;
    juce::Array<int> delaysMs;      // durata di ogni fotogramma
    int totalMs = 0;

    bool isValid() const noexcept    { return ! frames.isEmpty(); }
    bool isAnimated() const noexcept { return frames.size() > 1 && totalMs > 0; }

    /** Il fotogramma da mostrare all'istante indicato (in millisecondi). */
    const juce::Image& frameAt (juce::uint32 timeMs) const;

    /** Decodifica PNG / JPEG / GIF (anche animate). */
    static DecodedMedia decode (const juce::MemoryBlock& data);
};

/** Decodificatore GIF completo (animazione, trasparenza, interlacciamento, disposal). */
namespace GifDecoder
{
    bool isGif (const void* data, size_t size);
    DecodedMedia decode (const void* data, size_t size);
}

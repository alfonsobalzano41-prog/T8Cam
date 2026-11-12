#pragma once

#include <juce_graphics/juce_graphics.h>

namespace VideoEffects
{
    /** Nomi degli effetti, nell'ordine del menu a tendina. */
    const juce::StringArray& getNames();

    /** Applica l'effetto in place.
        @param amount  intensità 0..1
        @param level   livello audio 0..1 (alcuni effetti reagiscono al suono)
        @param timeMs  tempo corrente, per effetti animati (grana, sfarfallio...)
    */
    void apply (juce::Image& image, int effectIndex, float amount, float level, juce::uint32 timeMs);
}

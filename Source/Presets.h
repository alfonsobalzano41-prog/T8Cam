#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class T8CamProcessor;

/** Preset di fabbrica (dentro il plugin) e preset dell'utente (file .t8cam).

    Un preset contiene i parametri (effetto, intensità, specchio, pulse) e, se presente,
    l'elenco dei media. I preset senza media (quasi tutti quelli di fabbrica) cambiano
    solo il look e lasciano le tue scritte e immagini dove sono.
*/
class PresetManager final : public juce::ChangeBroadcaster
{
public:
    explicit PresetManager (T8CamProcessor&);

    int getNumFactory() const;
    juce::String getFactoryName (int index) const;
    void loadFactory (int index);

    juce::Array<juce::File> getUserPresets() const;
    bool loadUser (const juce::File&);
    juce::File saveUser (const juce::String& name);   // restituisce il file creato, oppure {} se fallisce
    bool deleteUser (const juce::File&);
    juce::File findUser (const juce::String& name) const;

    static juce::File getUserFolder();
    static inline const juce::String extension = ".t8cam";

    juce::String getCurrentName() const { return currentName; }
    void setCurrentName (const juce::String&);

private:
    struct Factory
    {
        juce::String name;
        juce::ValueTree tree;
    };

    void buildFactory();
    juce::ValueTree createTreeFromCurrent() const;
    void applyTree (const juce::ValueTree&);

    T8CamProcessor& processor;
    juce::Array<Factory> factory;
    juce::String currentName;
};

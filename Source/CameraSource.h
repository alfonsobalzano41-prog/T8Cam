#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>

/** Una sorgente video (webcam) che consegna fotogrammi come juce::Image.

    - macOS:   cattura diretta con AVFoundation (CameraSource_mac.mm), flusso video a pieno frame rate.
    - Windows: usa juce::CameraDevice (DirectShow).
    - Linux:   non supportato (create() restituisce nullptr).

    Il callback onFrame viene chiamato su un thread della camera, NON sul message thread.
*/
class CameraSource
{
public:
    struct DeviceInfo
    {
        juce::String id;    // identificativo stabile, salvato nel progetto del DAW
        juce::String name;  // nome da mostrare
    };

    virtual ~CameraSource() = default;

    /** Apre la camera indicata. Restituisce un messaggio d'errore, vuoto se è andato tutto bene. */
    virtual juce::String open (const juce::String& deviceId) = 0;
    virtual void close() = 0;

    std::function<void (const juce::Image&)> onFrame;

    static juce::Array<DeviceInfo> getAvailableDevices();
    static std::unique_ptr<CameraSource> create();
};

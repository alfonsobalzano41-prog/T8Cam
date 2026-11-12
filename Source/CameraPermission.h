#pragma once

#include <juce_core/juce_core.h>
#include <functional>

/** Controllo del permesso webcam.

    Su macOS il permesso viene chiesto a nome dell'app che ospita il plugin (il DAW).
    Se il DAW non dichiara NSCameraUsageDescription nel suo Info.plist, macOS chiude
    il DAW appena un plugin prova ad aprire la camera. Per questo controlliamo PRIMA
    di aprire qualunque cosa, e in quel caso non tocchiamo la camera.
*/
namespace CameraPermission
{
    enum class Status
    {
        granted,
        notDetermined,      // macOS chiederà all'utente
        denied,             // l'utente ha detto no (o è bloccato da policy)
        hostCannotAsk,      // il DAW non dichiara l'uso della camera: aprirla lo farebbe chiudere
        unsupported         // piattaforma senza supporto camera
    };

    Status getStatus();

    /** Chiede il permesso (solo se lo stato è notDetermined). Il callback arriva sul message thread. */
    void request (std::function<void (bool granted)> callback);

    /** Nome dell'app che ci ospita, es. "Logic Pro" o "FL Studio". */
    juce::String getHostName();

    /** Apre le impostazioni di sistema su Privacy > Fotocamera. */
    void openSystemSettings();
}

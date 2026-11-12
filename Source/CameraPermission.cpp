#include "CameraPermission.h"

#include <juce_events/juce_events.h>

// L'implementazione macOS sta in CameraPermission_mac.mm.
#if ! JUCE_MAC

namespace CameraPermission
{
    Status getStatus()
    {
       #if T8CAM_HAS_CAMERA
        return Status::granted;   // Windows: nessun prompt per le app desktop classiche
       #else
        return Status::unsupported;
       #endif
    }

    void request (std::function<void (bool)> callback)
    {
        const bool ok = getStatus() == Status::granted;
        juce::MessageManager::callAsync ([callback, ok] { if (callback) callback (ok); });
    }

    juce::String getHostName()
    {
        return juce::File::getSpecialLocation (juce::File::hostApplicationPath).getFileNameWithoutExtension();
    }

    void openSystemSettings()
    {
       #if JUCE_WINDOWS
        juce::URL ("ms-settings:privacy-webcam").launchInDefaultBrowser();
       #endif
    }
}

#endif

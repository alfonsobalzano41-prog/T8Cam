#include "CameraSource.h"

// macOS: implementazione in CameraPermission_mac.mm (AVFoundation).
#if ! JUCE_MAC

#if T8CAM_HAS_CAMERA
 #include <juce_video/juce_video.h>

/** Windows: juce::CameraDevice (DirectShow) consegna i fotogrammi in continuo via Listener. */
class JuceCameraSource final : public CameraSource,
                               private juce::CameraDevice::Listener
{
public:
    ~JuceCameraSource() override { close(); }

    juce::String open (const juce::String& deviceId) override
    {
        close();

        const auto names = juce::CameraDevice::getAvailableDevices();

        if (names.isEmpty())
            return "Nessuna webcam trovata.";

        const int index = juce::jmax (0, names.indexOf (deviceId));
        device.reset (juce::CameraDevice::openDevice (index, 128, 64, 1280, 720, true));

        if (device == nullptr)
            return juce::String::fromUTF8 ("Impossibile aprire la webcam (forse è usata da un'altra app).");

        device->addListener (this);
        return {};
    }

    void close() override
    {
        if (device != nullptr)
        {
            device->removeListener (this);
            device.reset();
        }
    }

private:
    void imageReceived (const juce::Image& image) override
    {
        if (onFrame != nullptr)
            onFrame (image.convertedToFormat (juce::Image::ARGB));
    }

    std::unique_ptr<juce::CameraDevice> device;
};

juce::Array<CameraSource::DeviceInfo> CameraSource::getAvailableDevices()
{
    juce::Array<DeviceInfo> result;

    for (auto& name : juce::CameraDevice::getAvailableDevices())
        result.add ({ name, name });

    return result;
}

std::unique_ptr<CameraSource> CameraSource::create()
{
    return std::make_unique<JuceCameraSource>();
}

#else // nessun supporto camera (Linux)

juce::Array<CameraSource::DeviceInfo> CameraSource::getAvailableDevices() { return {}; }
std::unique_ptr<CameraSource> CameraSource::create() { return nullptr; }

#endif
#endif

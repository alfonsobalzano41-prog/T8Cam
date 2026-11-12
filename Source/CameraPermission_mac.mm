#include "CameraPermission.h"
#include "CameraSource.h"

#include <juce_events/juce_events.h>

#import <AVFoundation/AVFoundation.h>
#import <AppKit/AppKit.h>

//==============================================================================
namespace CameraPermission
{
    static bool hostDeclaresCameraUsage()
    {
        // Se questa chiave manca, una richiesta di accesso alla camera fa terminare il DAW (TCC).
        return [[NSBundle mainBundle] objectForInfoDictionaryKey: @"NSCameraUsageDescription"] != nil;
    }

    Status getStatus()
    {
        switch ([AVCaptureDevice authorizationStatusForMediaType: AVMediaTypeVideo])
        {
            case AVAuthorizationStatusAuthorized:
                return Status::granted;

            case AVAuthorizationStatusDenied:
            case AVAuthorizationStatusRestricted:
                return Status::denied;

            case AVAuthorizationStatusNotDetermined:
            default:
                return hostDeclaresCameraUsage() ? Status::notDetermined : Status::hostCannotAsk;
        }
    }

    void request (std::function<void (bool)> callback)
    {
        if (getStatus() != Status::notDetermined)
        {
            const bool ok = getStatus() == Status::granted;
            juce::MessageManager::callAsync ([callback, ok] { if (callback) callback (ok); });
            return;
        }

        auto cb = std::make_shared<std::function<void (bool)>> (std::move (callback));

        [AVCaptureDevice requestAccessForMediaType: AVMediaTypeVideo
                                 completionHandler: ^(BOOL granted)
        {
            const bool ok = granted == YES;
            juce::MessageManager::callAsync ([cb, ok] { if (*cb) (*cb) (ok); });
        }];
    }

    juce::String getHostName()
    {
        if (NSString* name = [[NSBundle mainBundle] objectForInfoDictionaryKey: @"CFBundleName"])
            return juce::String::fromUTF8 ([name UTF8String]);

        return juce::File::getSpecialLocation (juce::File::hostApplicationPath).getFileNameWithoutExtension();
    }

    void openSystemSettings()
    {
        [[NSWorkspace sharedWorkspace] openURL:
            [NSURL URLWithString: @"x-apple.systempreferences:com.apple.preference.security?Privacy_Camera"]];
    }
}

//==============================================================================
// Cattura video con AVFoundation: AVCaptureVideoDataOutput consegna ogni fotogramma.

class MacCameraSource;

@interface T8CamFrameDelegate : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
{
@public
    MacCameraSource* owner;
}
@end

class MacCameraSource final : public CameraSource
{
public:
    MacCameraSource()
    {
        delegate = [[T8CamFrameDelegate alloc] init];
        delegate->owner = this;
        queue = dispatch_queue_create ("com.t8.t8cam.capture", DISPATCH_QUEUE_SERIAL);
    }

    ~MacCameraSource() override
    {
        close();
        [delegate release];
        dispatch_release (queue);
    }

    juce::String open (const juce::String& deviceId) override
    {
        close();

        AVCaptureDevice* device = nil;

        if (deviceId.isNotEmpty())
            device = [AVCaptureDevice deviceWithUniqueID: [NSString stringWithUTF8String: deviceId.toRawUTF8()]];

        if (device == nil)
            device = [AVCaptureDevice defaultDeviceWithMediaType: AVMediaTypeVideo];

        if (device == nil)
            return "Nessuna webcam trovata.";

        NSError* error = nil;
        AVCaptureDeviceInput* input = [AVCaptureDeviceInput deviceInputWithDevice: device error: &error];

        if (input == nil)
            return "Impossibile aprire la webcam: "
                     + juce::String::fromUTF8 ([[error localizedDescription] UTF8String]);

        session = [[AVCaptureSession alloc] init];
        [session beginConfiguration];

        if ([session canSetSessionPreset: AVCaptureSessionPreset1280x720])
            session.sessionPreset = AVCaptureSessionPreset1280x720;
        else
            session.sessionPreset = AVCaptureSessionPresetHigh;

        if (! [session canAddInput: input])
        {
            [session commitConfiguration];
            close();
            return juce::String::fromUTF8 ("La webcam è occupata o non compatibile.");
        }

        [session addInput: input];

        AVCaptureVideoDataOutput* output = [[[AVCaptureVideoDataOutput alloc] init] autorelease];
        output.videoSettings = @{ (id) kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA) };
        output.alwaysDiscardsLateVideoFrames = YES;
        [output setSampleBufferDelegate: delegate queue: queue];

        if (! [session canAddOutput: output])
        {
            [session commitConfiguration];
            close();
            return "Impossibile leggere il video dalla webcam.";
        }

        [session addOutput: output];
        [session commitConfiguration];
        [session startRunning];

        return {};
    }

    void close() override
    {
        if (session != nil)
        {
            [session stopRunning];

            for (AVCaptureOutput* o in session.outputs)
                if ([o isKindOfClass: [AVCaptureVideoDataOutput class]])
                    [(AVCaptureVideoDataOutput*) o setSampleBufferDelegate: nil queue: nil];

            [session release];
            session = nil;
        }

        // Aspetta che eventuali fotogrammi in volo siano finiti prima di proseguire.
        dispatch_sync (queue, ^{});
    }

    void handleSampleBuffer (CMSampleBufferRef sampleBuffer)
    {
        CVImageBufferRef pixelBuffer = CMSampleBufferGetImageBuffer (sampleBuffer);

        if (pixelBuffer == nullptr || onFrame == nullptr)
            return;

        CVPixelBufferLockBaseAddress (pixelBuffer, kCVPixelBufferLock_ReadOnly);

        const int w = (int) CVPixelBufferGetWidth (pixelBuffer);
        const int h = (int) CVPixelBufferGetHeight (pixelBuffer);
        const size_t srcStride = CVPixelBufferGetBytesPerRow (pixelBuffer);
        auto* src = static_cast<const uint8_t*> (CVPixelBufferGetBaseAddress (pixelBuffer));

        if (src != nullptr && w > 0 && h > 0)
        {
            // Il formato BGRA di CoreVideo coincide con il layout ARGB di JUCE su little-endian.
            juce::Image copy (juce::Image::ARGB, w, h, false);
            {
                juce::Image::BitmapData dst (copy, juce::Image::BitmapData::writeOnly);

                for (int y = 0; y < h; ++y)
                    std::memcpy (dst.getLinePointer (y), src + (size_t) y * srcStride, (size_t) w * 4);
            }

            CVPixelBufferUnlockBaseAddress (pixelBuffer, kCVPixelBufferLock_ReadOnly);
            onFrame (copy);
            return;
        }

        CVPixelBufferUnlockBaseAddress (pixelBuffer, kCVPixelBufferLock_ReadOnly);
    }

private:
    AVCaptureSession* session = nil;
    T8CamFrameDelegate* delegate = nil;
    dispatch_queue_t queue = nullptr;
};

@implementation T8CamFrameDelegate
- (void) captureOutput: (AVCaptureOutput*) output
 didOutputSampleBuffer: (CMSampleBufferRef) sampleBuffer
        fromConnection: (AVCaptureConnection*) connection
{
    juce::ignoreUnused (output, connection);

    if (owner != nullptr)
        owner->handleSampleBuffer (sampleBuffer);
}
@end

//==============================================================================
static NSArray<AVCaptureDevice*>* findVideoDevices()
{
    NSMutableArray<AVCaptureDeviceType>* types = [NSMutableArray arrayWithObject: AVCaptureDeviceTypeBuiltInWideAngleCamera];

    if (@available (macOS 14.0, *))
    {
        [types addObject: AVCaptureDeviceTypeExternal];
        [types addObject: AVCaptureDeviceTypeContinuityCamera];
    }
    else
    {
        JUCE_BEGIN_IGNORE_DEPRECATION_WARNINGS
        [types addObject: AVCaptureDeviceTypeExternalUnknown];
        JUCE_END_IGNORE_DEPRECATION_WARNINGS
    }

    auto* discovery = [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes: types
                                                                             mediaType: AVMediaTypeVideo
                                                                              position: AVCaptureDevicePositionUnspecified];
    return discovery.devices;
}

juce::Array<CameraSource::DeviceInfo> CameraSource::getAvailableDevices()
{
    juce::Array<DeviceInfo> result;

    for (AVCaptureDevice* d in findVideoDevices())
        result.add ({ juce::String::fromUTF8 ([d.uniqueID UTF8String]),
                      juce::String::fromUTF8 ([d.localizedName UTF8String]) });

    return result;
}

std::unique_ptr<CameraSource> CameraSource::create()
{
    return std::make_unique<MacCameraSource>();
}

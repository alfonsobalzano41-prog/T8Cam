#pragma once

#include <juce_events/juce_events.h>
#include <functional>

/** Controlla su GitHub se esiste una versione più nuova di T8Cam.

    Legge l'ultima "Release" del repository. Il controllo automatico avviene al massimo
    una volta ogni 12 ore; quello manuale (dal menu) sempre.
*/
class UpdateChecker final : private juce::Thread
{
public:
    struct Result
    {
        enum class Status { upToDate, updateAvailable, failed } status = Status::failed;
        juce::String latestVersion;
        juce::String downloadUrl;   // zip per questo sistema, oppure la pagina della release
        juce::String pageUrl;
    };

    UpdateChecker();
    ~UpdateChecker() override;

    /** Il callback arriva sul message thread. */
    void check (bool manual, std::function<void (const Result&)> callback);

    static juce::String getCurrentVersion();
    static juce::String getReleasesPage();

    /** true se a è una versione più nuova di b (formato x.y.z, la "v" iniziale è ignorata). */
    static bool isNewer (const juce::String& a, const juce::String& b);

private:
    void run() override;
    static juce::File getStampFile();

    std::function<void (const Result&)> pendingCallback;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
};

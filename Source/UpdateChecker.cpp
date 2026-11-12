#include "UpdateChecker.h"
#include "Presets.h"

namespace
{
    const char* const repo = "alfonsobalzano41-prog/T8Cam";
    constexpr juce::int64 autoCheckIntervalMs = 12 * 60 * 60 * 1000;
}

UpdateChecker::UpdateChecker() : juce::Thread ("T8Cam update check") {}

UpdateChecker::~UpdateChecker()
{
    *alive = false;
    stopThread (10000);
}

juce::String UpdateChecker::getCurrentVersion()  { return JucePlugin_VersionString; }
juce::String UpdateChecker::getReleasesPage()    { return juce::String ("https://github.com/") + repo + "/releases/latest"; }

juce::File UpdateChecker::getStampFile()
{
    return PresetManager::getUserFolder().getParentDirectory().getChildFile ("last-update-check");
}

bool UpdateChecker::isNewer (const juce::String& a, const juce::String& b)
{
    auto parts = [] (const juce::String& v)
    {
        juce::StringArray s;
        s.addTokens (v.trimCharactersAtStart ("vV").upToFirstOccurrenceOf ("-", false, false), ".", {});
        juce::Array<int> nums;
        for (auto& t : s) nums.add (t.getIntValue());
        while (nums.size() < 3) nums.add (0);
        return nums;
    };

    const auto pa = parts (a), pb = parts (b);

    for (int i = 0; i < juce::jmax (pa.size(), pb.size()); ++i)
    {
        const int x = pa[i], y = pb[i];
        if (x != y) return x > y;
    }

    return false;
}

void UpdateChecker::check (bool manual, std::function<void (const Result&)> callback)
{
    if (isThreadRunning())
        return;

    if (! manual)
    {
        const auto stamp = getStampFile();
        const auto last = stamp.loadFileAsString().getLargeIntValue();

        if (juce::Time::currentTimeMillis() - last < autoCheckIntervalMs)
            return;
    }

    pendingCallback = std::move (callback);
    startThread (juce::Thread::Priority::low);
}

void UpdateChecker::run()
{
    Result result;

    const auto apiUrl = juce::URL (juce::String ("https://api.github.com/repos/") + repo + "/releases/latest");
    int statusCode = 0;
    auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                       .withExtraHeaders ("User-Agent: T8Cam\r\nAccept: application/vnd.github+json")
                       .withConnectionTimeoutMs (8000)
                       .withStatusCode (&statusCode);

    auto stream = apiUrl.createInputStream (options);

    if (statusCode == 404)
    {
        // Nessuna release pubblicata: niente da aggiornare.
        result.status = Result::Status::upToDate;
    }
    else if (stream != nullptr)
    {
        const auto json = juce::JSON::parse (stream->readEntireStreamAsString());
        const auto tag = json.getProperty ("tag_name", {}).toString();

        if (tag.isNotEmpty())
        {
            result.latestVersion = tag.trimCharactersAtStart ("vV");
            result.pageUrl = json.getProperty ("html_url", getReleasesPage()).toString();
            result.downloadUrl = result.pageUrl;

           #if JUCE_MAC
            const juce::String wanted = "macOS";
           #else
            const juce::String wanted = "Windows";
           #endif

            if (auto* assets = json.getProperty ("assets", {}).getArray())
                for (auto& asset : *assets)
                    if (asset.getProperty ("name", {}).toString().containsIgnoreCase (wanted))
                        result.downloadUrl = asset.getProperty ("browser_download_url", result.pageUrl).toString();

            result.status = isNewer (result.latestVersion, getCurrentVersion())
                                ? Result::Status::updateAvailable
                                : Result::Status::upToDate;

            getStampFile().getParentDirectory().createDirectory();
            getStampFile().replaceWithText (juce::String (juce::Time::currentTimeMillis()));
        }
    }

    juce::MessageManager::callAsync ([flag = alive, cb = pendingCallback, result]
    {
        if (*flag && cb)
            cb (result);
    });
}

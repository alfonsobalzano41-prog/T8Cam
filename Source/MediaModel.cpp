#include "MediaModel.h"

using namespace MediaIDs;

//==============================================================================
namespace Media
{
    static juce::ValueTree createBase (const juce::String& kindName)
    {
        juce::ValueTree item (ITEM);
        item.setProperty (id, juce::Uuid().toString(), nullptr);
        item.setProperty (kind, kindName, nullptr);
        item.setProperty (x, 0.5f, nullptr);
        item.setProperty (y, 0.5f, nullptr);
        item.setProperty (opacity, 1.0f, nullptr);
        item.setProperty (rotation, 0.0f, nullptr);
        item.setProperty (visible, true, nullptr);
        return item;
    }

    juce::ValueTree createText (const juce::String& t)
    {
        auto item = createBase (kindText);
        item.setProperty (text, t, nullptr);
        item.setProperty (size, 0.1f, nullptr);
        item.setProperty (colour, juce::Colours::white.toString(), nullptr);
        item.setProperty (font, juce::Font::getDefaultSansSerifFontName(), nullptr);
        item.setProperty (bold, true, nullptr);
        item.setProperty (shadow, true, nullptr);
        return item;
    }

    juce::ValueTree createImage (const juce::String& fileName, const juce::MemoryBlock& fileData)
    {
        auto item = createBase (kindImage);
        item.setProperty (name, fileName, nullptr);
        item.setProperty (size, 0.3f, nullptr);
        item.setProperty (data, fileData.toBase64Encoding(), nullptr);
        return item;
    }

    bool isText (const juce::ValueTree& item)
    {
        return item[kind].toString() == kindText;
    }

    juce::String getDisplayName (const juce::ValueTree& item)
    {
        if (isText (item))
        {
            auto t = item[text].toString().replaceCharacters ("\r\n", "  ").trim();
            if (t.isEmpty()) t = "(testo vuoto)";
            return juce::String::fromUTF8 ("Testo: ") + (t.length() > 24 ? t.substring (0, 24) + "..." : t);
        }

        return item[name].toString();
    }

    juce::String getSupportedWildcard()  { return "*.png;*.gif;*.jpg;*.jpeg"; }

    bool isSupportedFile (const juce::File& f)
    {
        return f.hasFileExtension ("png;gif;jpg;jpeg");
    }
}

//==============================================================================
const DecodedMedia* MediaCache::get (const juce::ValueTree& item)
{
    if (Media::isText (item))
        return nullptr;

    const auto key = item[id].toString();
    auto it = entries.find (key);

    if (it == entries.end())
    {
        juce::MemoryBlock bytes;
        bytes.fromBase64Encoding (item[data].toString());
        auto decoded = std::make_unique<DecodedMedia> (DecodedMedia::decode (bytes));
        it = entries.emplace (key, std::move (decoded)).first;
    }

    return it->second->isValid() ? it->second.get() : nullptr;
}

void MediaCache::purge (const juce::ValueTree& mediaRoot)
{
    for (auto it = entries.begin(); it != entries.end();)
    {
        bool found = false;

        for (const auto& child : mediaRoot)
            if (child[id].toString() == it->first) { found = true; break; }

        it = found ? std::next (it) : entries.erase (it);
    }
}

//==============================================================================
bool MediaGeometry::contains (juce::Point<float> p) const
{
    return valid && local.contains (p.transformedBy (toView.inverted()));
}

juce::Path MediaGeometry::outline() const
{
    juce::Path path;
    path.addRectangle (local);
    path.applyTransform (toView);
    return path;
}

namespace MediaRenderer
{
    static juce::Font makeFont (const juce::ValueTree& item, float heightPx)
    {
        auto typeface = item[font].toString();
        if (typeface.isEmpty())
            typeface = juce::Font::getDefaultSansSerifFontName();

        return juce::Font (juce::FontOptions (typeface, juce::jmax (1.0f, heightPx),
                                              (bool) item[bold] ? juce::Font::bold : juce::Font::plain));
    }

    static void layoutText (juce::GlyphArrangement& ga, const juce::ValueTree& item, juce::Rectangle<float> view)
    {
        const auto f = makeFont (item, (float) item[size] * view.getHeight());
        ga.addJustifiedText (f, item[text].toString(), 0.0f, 0.0f, 100000.0f, juce::Justification::centred);
    }

    static juce::AffineTransform placement (const juce::ValueTree& item, juce::Rectangle<float> view)
    {
        return juce::AffineTransform::rotation (juce::degreesToRadians ((float) item[rotation]))
                   .translated (view.getX() + (float) item[x] * view.getWidth(),
                                view.getY() + (float) item[y] * view.getHeight());
    }

    MediaGeometry getGeometry (const juce::ValueTree& item, juce::Rectangle<float> view, MediaCache& cache)
    {
        MediaGeometry geo;

        if (Media::isText (item))
        {
            juce::GlyphArrangement ga;
            layoutText (ga, item, view);
            auto bb = ga.getBoundingBox (0, -1, true);

            if (bb.isEmpty())
                bb = { 0.0f, 0.0f, 40.0f, (float) item[size] * view.getHeight() };

            geo.local = bb.withCentre ({}).expanded (6.0f, 2.0f);
        }
        else if (auto* decoded = cache.get (item))
        {
            const auto& img = decoded->frames.getReference (0);
            const float w = (float) item[size] * view.getWidth();
            const float h = w * (float) img.getHeight() / (float) juce::jmax (1, img.getWidth());
            geo.local = juce::Rectangle<float> (w, h).withCentre ({});
        }
        else
        {
            return geo;
        }

        geo.toView = placement (item, view);
        geo.valid = true;
        return geo;
    }

    void draw (juce::Graphics& g, const juce::ValueTree& item, juce::Rectangle<float> view,
               MediaCache& cache, juce::uint32 timeMs)
    {
        if (! (bool) item[visible])
            return;

        const float alpha = juce::jlimit (0.0f, 1.0f, (float) item[opacity]);
        const auto place = placement (item, view);

        if (Media::isText (item))
        {
            juce::GlyphArrangement ga;
            layoutText (ga, item, view);
            const auto bb = ga.getBoundingBox (0, -1, true);
            const auto centred = juce::AffineTransform::translation (-bb.getCentreX(), -bb.getCentreY());

            if ((bool) item[shadow])
            {
                const float off = juce::jmax (1.0f, (float) item[size] * view.getHeight() * 0.05f);
                g.setColour (juce::Colours::black.withAlpha (0.6f * alpha));
                ga.draw (g, centred.translated (off, off).followedBy (place));
            }

            g.setColour (juce::Colour::fromString (item[colour].toString()).withMultipliedAlpha (alpha));
            ga.draw (g, centred.followedBy (place));
            return;
        }

        if (auto* decoded = cache.get (item))
        {
            const auto& img = decoded->frameAt (timeMs);
            const float scale = (float) item[size] * view.getWidth() / (float) juce::jmax (1, img.getWidth());

            const juce::Graphics::ScopedSaveState save (g);
            g.setOpacity (alpha);
            g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
            g.drawImageTransformed (img,
                                    juce::AffineTransform::translation (-img.getWidth() * 0.5f, -img.getHeight() * 0.5f)
                                        .scaled (scale)
                                        .followedBy (place));
        }
    }

    bool hasAnimation (const juce::ValueTree& mediaRoot, MediaCache& cache)
    {
        for (const auto& item : mediaRoot)
            if ((bool) item[visible])
                if (auto* d = cache.get (item))
                    if (d->isAnimated())
                        return true;

        return false;
    }
}

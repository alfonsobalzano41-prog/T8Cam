#include "VideoEffects.h"

// Layout dei pixel juce::Image::ARGB in memoria (little-endian): B, G, R, A.
// I fotogrammi della webcam sono opachi (A = 255).

namespace VideoEffects
{
namespace
{
    enum Effect
    {
        none, mono, negative, posterize, thermal, nightVision,
        glitch, sketch, neon, comic, duotone, kaleidoscope
    };

    using u8 = juce::uint8;

    inline u8 clamp8 (int v) noexcept         { return (u8) (v < 0 ? 0 : (v > 255 ? 255 : v)); }
    inline int lumaOf (const u8* p) noexcept  { return (p[2] * 77 + p[1] * 150 + p[0] * 29) >> 8; }

    inline void mixInto (u8* p, int r, int g, int b, int amount256) noexcept
    {
        p[2] = (u8) (p[2] + (((r - p[2]) * amount256) >> 8));
        p[1] = (u8) (p[1] + (((g - p[1]) * amount256) >> 8));
        p[0] = (u8) (p[0] + (((b - p[0]) * amount256) >> 8));
    }

    /** Generatore pseudo-casuale veloce (xorshift). */
    struct FastRandom
    {
        juce::uint32 s;
        explicit FastRandom (juce::uint32 seed) : s (seed | 1u) {}
        juce::uint32 next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
        int range (int maxExclusive) noexcept { return maxExclusive > 0 ? (int) (next() % (juce::uint32) maxExclusive) : 0; }
    };

    template <typename Fn>
    void forEachPixel (juce::Image::BitmapData& bd, Fn&& fn)
    {
        for (int y = 0; y < bd.height; ++y)
        {
            auto* p = bd.getLinePointer (y);

            for (int x = 0; x < bd.width; ++x, p += bd.pixelStride)
                fn (p, x, y);
        }
    }

    /** Fattore di vignettatura 0..256 per (x, y). */
    struct Vignette
    {
        float cx, cy, invR2, strength;

        Vignette (int w, int h, float s)
            : cx (w * 0.5f), cy (h * 0.5f), invR2 (1.0f / (cx * cx + cy * cy)), strength (s) {}

        int at (int x, int y) const noexcept
        {
            const float dx = x - cx, dy = y - cy;
            const float d = (dx * dx + dy * dy) * invR2;    // 0 al centro, 1 negli angoli
            return (int) (256.0f * juce::jlimit (0.0f, 1.0f, 1.0f - strength * d * d * 1.6f));
        }
    };

    /** Mappa di luminanza e modulo del gradiente (Sobel). */
    struct EdgeMap
    {
        int w, h;
        juce::HeapBlock<u8> luma, edges;

        explicit EdgeMap (juce::Image::BitmapData& bd)
            : w (bd.width), h (bd.height)
        {
            luma.malloc ((size_t) w * (size_t) h);
            edges.calloc ((size_t) w * (size_t) h);

            forEachPixel (bd, [this] (u8* p, int x, int y) { luma[(size_t) y * (size_t) w + (size_t) x] = (u8) lumaOf (p); });

            for (int y = 1; y < h - 1; ++y)
            {
                const u8* a = luma + (size_t) (y - 1) * (size_t) w;
                const u8* b = luma + (size_t) y * (size_t) w;
                const u8* c = luma + (size_t) (y + 1) * (size_t) w;
                u8* out = edges + (size_t) y * (size_t) w;

                for (int x = 1; x < w - 1; ++x)
                {
                    const int gx = (a[x + 1] + 2 * b[x + 1] + c[x + 1]) - (a[x - 1] + 2 * b[x - 1] + c[x - 1]);
                    const int gy = (c[x - 1] + 2 * c[x] + c[x + 1]) - (a[x - 1] + 2 * a[x] + a[x + 1]);
                    out[x] = clamp8 ((std::abs (gx) + std::abs (gy)) >> 1);
                }
            }
        }

        u8 edge (int x, int y) const noexcept { return edges[(size_t) y * (size_t) w + (size_t) x]; }
    };

    /** Copia di una riga, per effetti che leggono pixel vicini sulla stessa riga. */
    struct RowCopy
    {
        juce::HeapBlock<u8> data;
        explicit RowCopy (int width) { data.malloc ((size_t) width * 4); }
        void load (const u8* src, int width) { std::memcpy (data, src, (size_t) width * 4); }
        const u8* at (int x, int width) const noexcept { return data + (size_t) juce::jlimit (0, width - 1, x) * 4; }
    };

    //==========================================================================
    void applyThermalPalette (u8* p, int v, int a256)
    {
        // Nero -> blu -> viola -> rosso -> arancione -> giallo -> bianco
        static const int stops[][3] = { { 0, 0, 0 }, { 20, 0, 140 }, { 130, 0, 170 }, { 230, 30, 60 },
                                        { 255, 130, 0 }, { 255, 230, 40 }, { 255, 255, 255 } };
        const int scaled = v * 6;
        const int i = juce::jmin (5, scaled >> 8);
        const int t = scaled - (i << 8);
        const auto* s0 = stops[i];
        const auto* s1 = stops[i + 1];

        mixInto (p, s0[0] + (((s1[0] - s0[0]) * t) >> 8),
                    s0[1] + (((s1[1] - s0[1]) * t) >> 8),
                    s0[2] + (((s1[2] - s0[2]) * t) >> 8), a256);
    }
}

//==============================================================================
const juce::StringArray& getNames()
{
    static const juce::StringArray names {
        "Nessuno", "Bianco e nero", "Negativo", "Pop art", "Termico", "Visione notturna",
        "Glitch", "Schizzo", "Neon", "Fumetto", "Duotone", "Caleidoscopio"
    };
    return names;
}

void apply (juce::Image& image, int effect, float amount, float level, juce::uint32 timeMs)
{
    if (effect <= none || ! image.isValid())
        return;

    amount = juce::jlimit (0.0f, 1.0f, amount);
    level  = juce::jlimit (0.0f, 1.0f, level);
    const int a256 = (int) (amount * 256.0f);

    juce::Image::BitmapData bd (image, juce::Image::BitmapData::readWrite);
    const int w = bd.width, h = bd.height;
    FastRandom rng (timeMs * 2654435761u);

    switch (effect)
    {
        case mono:
            forEachPixel (bd, [&] (u8* p, int, int) { const int l = lumaOf (p); mixInto (p, l, l, l, a256); });
            break;

        case negative:
            forEachPixel (bd, [&] (u8* p, int, int) { mixInto (p, 255 - p[2], 255 - p[1], 255 - p[0], a256); });
            break;

        case posterize:
        {
            const int levels = juce::jlimit (2, 8, (int) (8 - amount * 6));
            const int step = 255 / (levels - 1);

            forEachPixel (bd, [&] (u8* p, int, int)
            {
                const int l = lumaOf (p);

                for (int c = 0; c < 3; ++c)
                {
                    // Saturazione spinta, poi quantizzazione.
                    const int v = clamp8 (l + (p[c] - l) * 2);
                    p[c] = (u8) (((v + step / 2) / step) * step);
                }
            });
            break;
        }

        case thermal:
            forEachPixel (bd, [&] (u8* p, int, int) { applyThermalPalette (p, lumaOf (p), a256); });
            break;

        case nightVision:
        {
            Vignette vig (w, h, 0.9f);
            const int noise = 10 + (int) (amount * 40);

            forEachPixel (bd, [&] (u8* p, int x, int y)
            {
                int l = clamp8 ((lumaOf (p) * 3) / 2 + rng.range (noise) - noise / 2);
                l = (l * vig.at (x, y)) >> 8;
                mixInto (p, l / 5, l, l / 4, 256);
            });
            break;
        }

        case glitch:
        {
            const float energy = amount * 0.6f + level;
            const int bands = (int) (energy * 12);
            const int maxShift = juce::jmax (4, (int) (w * (0.02f + 0.15f * energy)));
            RowCopy row (w);

            for (int i = 0; i < bands; ++i)
            {
                const int y0 = rng.range (h);
                const int bh = 2 + rng.range (juce::jmax (3, h / 12));
                const int shift = rng.range (maxShift * 2) - maxShift;
                const int split = rng.range (maxShift / 2 + 1);

                for (int y = y0; y < juce::jmin (h, y0 + bh); ++y)
                {
                    auto* line = bd.getLinePointer (y);
                    row.load (line, w);

                    for (int x = 0; x < w; ++x)
                    {
                        auto* p = line + x * 4;
                        p[2] = row.at (x + shift + split, w)[2];
                        p[1] = row.at (x + shift, w)[1];
                        p[0] = row.at (x + shift - split, w)[0];
                    }
                }
            }

            // Separazione RGB leggera su tutta l'immagine.
            const int split = (int) (energy * 6) * juce::jmax (1, w / 640);

            if (split > 0)
                for (int y = 0; y < h; ++y)
                {
                    auto* line = bd.getLinePointer (y);
                    row.load (line, w);

                    for (int x = 0; x < w; ++x)
                    {
                        line[x * 4 + 2] = row.at (x + split, w)[2];
                        line[x * 4 + 0] = row.at (x - split, w)[0];
                    }
                }
            break;
        }

        case sketch:
        {
            EdgeMap em (bd);
            forEachPixel (bd, [&] (u8* p, int x, int y)
            {
                const int v = clamp8 (255 - em.edge (x, y) * 3);
                const int paper = clamp8 (v - 8 + ((x * 7 + y * 13) & 15));
                mixInto (p, paper, paper, paper - 6, a256);
            });
            break;
        }

        case neon:
        {
            EdgeMap em (bd);
            const float hueShift = (float) (timeMs % 6000) / 6000.0f;

            // Il colore dipende solo dalla colonna: lo calcoliamo una volta per colonna.
            juce::HeapBlock<juce::PixelARGB> columnColour ((size_t) w);
            for (int x = 0; x < w; ++x)
                columnColour[x] = juce::Colour::fromHSV (std::fmod (hueShift + (float) x / (float) w * 0.6f, 1.0f),
                                                         0.9f, 1.0f, 1.0f).getPixelARGB();

            forEachPixel (bd, [&] (u8* p, int x, int y)
            {
                const int e = clamp8 (em.edge (x, y) * 3);
                const auto& c = columnColour[x];
                mixInto (p, (c.getRed() * e) >> 8, (c.getGreen() * e) >> 8, (c.getBlue() * e) >> 8, a256);
            });
            break;
        }

        case comic:
        {
            EdgeMap em (bd);
            const int threshold = 90 - (int) (amount * 50);

            forEachPixel (bd, [&] (u8* p, int x, int y)
            {
                if (em.edge (x, y) > threshold)
                {
                    p[0] = p[1] = p[2] = 15;
                    return;
                }

                const int l = lumaOf (p);
                for (int c = 0; c < 3; ++c)
                {
                    const int v = clamp8 (l + (p[c] - l) * 3 / 2);
                    p[c] = (u8) juce::jmin (255, (v / 64) * 72 + 30);
                }
            });
            break;
        }

        case duotone:
        {
            const juce::Colour dark (0xff2b0a5e), light (0xffffa060);

            forEachPixel (bd, [&] (u8* p, int, int)
            {
                const int l = lumaOf (p);
                mixInto (p, dark.getRed()   + (((light.getRed()   - dark.getRed())   * l) >> 8),
                            dark.getGreen() + (((light.getGreen() - dark.getGreen()) * l) >> 8),
                            dark.getBlue()  + (((light.getBlue()  - dark.getBlue())  * l) >> 8), a256);
            });
            break;
        }

        case kaleidoscope:
        {
            // Specchia la metà sinistra a destra; con intensità alta anche sopra -> sotto.
            for (int y = 0; y < h; ++y)
            {
                auto* line = bd.getLinePointer (y);
                for (int x = w / 2; x < w; ++x)
                    std::memcpy (line + x * 4, line + (w - 1 - x) * 4, 4);
            }

            if (amount >= 0.5f)
                for (int y = h / 2; y < h; ++y)
                    std::memcpy (bd.getLinePointer (y), bd.getLinePointer (h - 1 - y), (size_t) w * 4);
            break;
        }

        default:
            break;
    }
}
}

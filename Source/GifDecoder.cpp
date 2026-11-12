#include "GifDecoder.h"

const juce::Image& DecodedMedia::frameAt (juce::uint32 timeMs) const
{
    jassert (! frames.isEmpty());

    if (! isAnimated())
        return frames.getReference (0);

    int t = (int) (timeMs % (juce::uint32) totalMs);

    for (int i = 0; i < frames.size(); ++i)
    {
        t -= delaysMs[i];

        if (t < 0)
            return frames.getReference (i);
    }

    return frames.getReference (frames.size() - 1);
}

DecodedMedia DecodedMedia::decode (const juce::MemoryBlock& data)
{
    if (GifDecoder::isGif (data.getData(), data.getSize()))
    {
        auto gif = GifDecoder::decode (data.getData(), data.getSize());

        if (gif.isValid())
            return gif;
    }

    DecodedMedia result;
    auto img = juce::ImageFileFormat::loadFrom (data.getData(), data.getSize());

    if (img.isValid())
    {
        result.frames.add (img.convertedToFormat (juce::Image::ARGB));
        result.delaysMs.add (0);
    }

    return result;
}

//==============================================================================
namespace GifDecoder
{
namespace
{
    struct Reader
    {
        const juce::uint8* data;
        size_t size;
        size_t pos = 0;

        bool canRead (size_t n) const noexcept   { return pos + n <= size; }
        int  u8() noexcept                       { return canRead (1) ? data[pos++] : -1; }

        int u16() noexcept
        {
            if (! canRead (2)) { pos = size; return 0; }
            const int v = data[pos] | (data[pos + 1] << 8);
            pos += 2;
            return v;
        }

        void skip (size_t n) noexcept            { pos = juce::jmin (size, pos + n); }

        void skipSubBlocks() noexcept
        {
            for (;;)
            {
                const int len = u8();
                if (len <= 0) return;
                skip ((size_t) len);
            }
        }

        /** Legge i sotto-blocchi di dati concatenandoli. */
        std::vector<juce::uint8> readSubBlocks()
        {
            std::vector<juce::uint8> out;

            for (;;)
            {
                const int len = u8();
                if (len <= 0) break;
                if (! canRead ((size_t) len)) { pos = size; break; }
                out.insert (out.end(), data + pos, data + pos + len);
                pos += (size_t) len;
            }

            return out;
        }
    };

    struct Colour3 { juce::uint8 r, g, b; };

    /** Decompressione LZW di GIF. Restituisce gli indici colore (al massimo pixelCount). */
    std::vector<juce::uint8> lzwDecode (const std::vector<juce::uint8>& in, int minCodeSize, size_t pixelCount)
    {
        std::vector<juce::uint8> out;
        out.reserve (pixelCount);

        if (minCodeSize < 2 || minCodeSize > 11)
            return out;

        const int clearCode = 1 << minCodeSize;
        const int endCode = clearCode + 1;

        std::vector<juce::uint16> prefix (4096, 0);
        std::vector<juce::uint8> suffix (4096, 0);
        std::vector<juce::uint8> stack;
        stack.reserve (4096);

        for (int i = 0; i < clearCode; ++i)
            suffix[(size_t) i] = (juce::uint8) i;

        int codeSize = minCodeSize + 1;
        int nextCode = clearCode + 2;
        int prev = -1;
        juce::uint8 firstChar = 0;

        juce::uint32 bitBuffer = 0;
        int bitCount = 0;
        size_t inPos = 0;

        while (out.size() < pixelCount)
        {
            while (bitCount < codeSize && inPos < in.size())
            {
                bitBuffer |= (juce::uint32) in[inPos++] << bitCount;
                bitCount += 8;
            }

            if (bitCount < codeSize)
                break;

            const int code = (int) (bitBuffer & ((1u << codeSize) - 1));
            bitBuffer >>= codeSize;
            bitCount -= codeSize;

            if (code == clearCode)
            {
                codeSize = minCodeSize + 1;
                nextCode = clearCode + 2;
                prev = -1;
                continue;
            }

            if (code == endCode)
                break;

            if (prev < 0)
            {
                if (code >= clearCode)
                    break;   // dati corrotti

                out.push_back ((juce::uint8) code);
                firstChar = (juce::uint8) code;
                prev = code;
                continue;
            }

            if (code > nextCode)
                break;       // dati corrotti

            int cur = code;
            stack.clear();

            if (code == nextCode)
            {
                stack.push_back (firstChar);
                cur = prev;
            }

            while (cur >= clearCode)
            {
                stack.push_back (suffix[(size_t) cur]);
                cur = prefix[(size_t) cur];
            }

            stack.push_back ((juce::uint8) cur);
            firstChar = (juce::uint8) cur;

            for (auto it = stack.rbegin(); it != stack.rend() && out.size() < pixelCount; ++it)
                out.push_back (*it);

            if (nextCode < 4096)
            {
                prefix[(size_t) nextCode] = (juce::uint16) prev;
                suffix[(size_t) nextCode] = firstChar;
                ++nextCode;

                if (nextCode == (1 << codeSize) && codeSize < 12)
                    ++codeSize;
            }

            prev = code;
        }

        return out;
    }

    std::vector<Colour3> readColourTable (Reader& r, int count)
    {
        std::vector<Colour3> table ((size_t) count, Colour3 { 0, 0, 0 });

        for (auto& c : table)
        {
            c.r = (juce::uint8) juce::jmax (0, r.u8());
            c.g = (juce::uint8) juce::jmax (0, r.u8());
            c.b = (juce::uint8) juce::jmax (0, r.u8());
        }

        return table;
    }

    void clearRect (juce::Image& canvas, juce::Rectangle<int> area)
    {
        area = area.getIntersection (canvas.getBounds());

        if (area.isEmpty())
            return;

        juce::Image::BitmapData bd (canvas, juce::Image::BitmapData::writeOnly);

        for (int y = area.getY(); y < area.getBottom(); ++y)
            std::memset (bd.getPixelPointer (area.getX(), y), 0, (size_t) area.getWidth() * (size_t) bd.pixelStride);
    }
}

bool isGif (const void* data, size_t size)
{
    return size >= 6 && std::memcmp (data, "GIF8", 4) == 0;
}

DecodedMedia decode (const void* rawData, size_t size)
{
    DecodedMedia result;
    Reader r { static_cast<const juce::uint8*> (rawData), size };

    if (! isGif (rawData, size))
        return result;

    r.skip (6);
    const int width = r.u16();
    const int height = r.u16();
    const int packed = r.u8();
    r.skip (2);   // indice di sfondo, proporzioni

    if (width <= 0 || height <= 0 || width > 4096 || height > 4096)
        return result;

    std::vector<Colour3> globalTable;

    if (packed & 0x80)
        globalTable = readColourTable (r, 1 << ((packed & 7) + 1));

    // Limite di memoria: circa 400 MB di fotogrammi decodificati.
    const size_t frameBytes = (size_t) width * (size_t) height * 4;
    const int maxFrames = (int) juce::jlimit<size_t> (1, 1000, (size_t) 400 * 1024 * 1024 / frameBytes);

    juce::Image canvas (juce::Image::ARGB, width, height, true);

    int disposal = 0, delayCs = 0, transparentIndex = -1;

    while (r.pos < r.size && result.frames.size() < maxFrames)
    {
        const int block = r.u8();

        if (block == 0x3B || block < 0)   // fine file
            break;

        if (block == 0x21)                // estensione
        {
            const int label = r.u8();

            if (label == 0xF9)            // Graphic Control Extension
            {
                const int len = r.u8();
                const auto start = r.pos;
                const int p = r.u8();
                delayCs = r.u16();
                const int ti = r.u8();
                r.pos = start + (size_t) juce::jmax (0, len);

                disposal = (p >> 2) & 7;
                transparentIndex = (p & 1) ? ti : -1;
            }

            r.skipSubBlocks();
            continue;
        }

        if (block != 0x2C)                // blocco sconosciuto: file rovinato
            break;

        const int fx = r.u16(), fy = r.u16(), fw = r.u16(), fh = r.u16();
        const int ipacked = r.u8();
        const bool interlaced = (ipacked & 0x40) != 0;

        std::vector<Colour3> localTable;

        if (ipacked & 0x80)
            localTable = readColourTable (r, 1 << ((ipacked & 7) + 1));

        const auto& table = localTable.empty() ? globalTable : localTable;
        const int minCodeSize = r.u8();
        const auto compressed = r.readSubBlocks();

        if (fw <= 0 || fh <= 0)
            continue;

        const auto indices = lzwDecode (compressed, minCodeSize, (size_t) fw * (size_t) fh);

        juce::Image previous;
        if (disposal == 3)
            previous = canvas.createCopy();

        {
            juce::Image::BitmapData bd (canvas, juce::Image::BitmapData::readWrite);

            // Ordine delle righe per le GIF interlacciate.
            std::vector<int> rowOrder;
            rowOrder.reserve ((size_t) fh);

            if (interlaced)
            {
                for (int y = 0; y < fh; y += 8) rowOrder.push_back (y);
                for (int y = 4; y < fh; y += 8) rowOrder.push_back (y);
                for (int y = 2; y < fh; y += 4) rowOrder.push_back (y);
                for (int y = 1; y < fh; y += 2) rowOrder.push_back (y);
            }
            else
            {
                for (int y = 0; y < fh; ++y) rowOrder.push_back (y);
            }

            size_t i = 0;

            for (int row = 0; row < fh; ++row)
            {
                const int cy = fy + rowOrder[(size_t) row];

                for (int x = 0; x < fw; ++x, ++i)
                {
                    if (i >= indices.size())
                        break;

                    const int idx = indices[i];
                    const int cx = fx + x;

                    if (idx == transparentIndex || cx >= width || cy >= height || idx >= (int) table.size())
                        continue;

                    const auto& c = table[(size_t) idx];
                    auto* px = bd.getPixelPointer (cx, cy);
                    px[0] = c.b; px[1] = c.g; px[2] = c.r; px[3] = 255;   // ARGB in memoria: B G R A
                }
            }
        }

        result.frames.add (canvas.createCopy());
        // Come i browser: ritardo 0 o 1 centesimo => 100 ms.
        const int delayMs = delayCs <= 1 ? 100 : delayCs * 10;
        result.delaysMs.add (delayMs);
        result.totalMs += delayMs;

        if (disposal == 2)
            clearRect (canvas, { fx, fy, fw, fh });
        else if (disposal == 3 && previous.isValid())
            canvas = previous;

        disposal = 0;
        delayCs = 0;
        transparentIndex = -1;
    }

    if (result.frames.size() == 1)
        result.totalMs = 0;

    return result;
}
}

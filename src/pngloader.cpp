#include <algorithm>
#include <array>
#include <exception>
#include <istream>
#include <iterator>
#include <numeric>
#include <stdexcept>
#include <zlib.h>

#include "binfilehelper.h"
#include "png.h"


static const std::array<u8, 8> PNGHeader = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
static const std::array<u8, 4> IHDRTag{ 0x49, 0x48, 0x44, 0x52 };
static const std::array<u8, 4> IDATTag{ 0x49, 0x44, 0x41, 0x54 };
static const std::array<u8, 4> IENDTag{ 0x49, 0x45, 0x4e, 0x44 };

enum class ReadState {
    Header,
    Data,
    End
};

struct PngIHDR {
    u32 width;
    u32 height;
    u8 bitDepth;
    u8 colorType;
    u8 compressionMethod;
    u8 filterMethod;
    u8 interlaceMethod;
};

PngIHDR readPngIHDR(BinaryInputStream &in, size_t chunkLen);
Picture decodePngData(const PngIHDR &ihdr, const std::vector<u8> &data);

Picture readPNG(std::istream &inStream)
{
    BinaryInputStream in(inStream);

    if (!in.match(PNGHeader)) {
        throw std::runtime_error("wrong PNG header");
    }

    ReadState state = ReadState::Header;
    PngIHDR ihdr;
    std::vector<u8> compressedData;
    while (state != ReadState::End) {
        u32 chunkLen = in.read<u32>();
        in.getAndResetCRC();
        std::array<u8, 4> chunkType;
        in.read(chunkType);

        if (chunkType == IHDRTag) {
            if (state != ReadState::Header) {
                throw std::runtime_error("unexpected PNG IHDR chunk");
            }
            ihdr = readPngIHDR(in, chunkLen);            
            state = ReadState::Data;

        } else if (chunkType == IDATTag) {
            if (state != ReadState::Data) {
                throw std::runtime_error("unexpected PNG IDAT chunk");
            }
            in.read(std::back_inserter(compressedData), chunkLen);

        } else if (chunkType == IENDTag) {
            if (state != ReadState::Data) {
                throw std::runtime_error("unexpected PNG IEND chunk");
            }
            if (chunkLen != 0) {
                throw std::runtime_error("unexpected PNG IEND chunk length");
            }
            state = ReadState::End;

        } else {
            // skip unknown PNG chunks
            in.advance(chunkLen);
        }
        
        u32 calcChecksum = in.getAndResetCRC();
        u32 checksum = in.read<u32>();
        if (calcChecksum != checksum) {
            throw std::runtime_error("PNG chunk checksum wrong");
        }
    }

    // Calculate the uncompressed data size
    // 3 (RGB) or 4 (RGBA) bytes per pixel
    // 1 additional byte per line (for filter type)
    size_t imageDataSize = static_cast<size_t>(ihdr.width) * ihdr.height * (ihdr.colorType == 2 ? 3 : 4) + ihdr.height;
    std::vector<u8> imageData(imageDataSize);
    
    uLong imageDataLen = static_cast<uLong>(imageDataSize);
    int ret = uncompress(imageData.data(), &imageDataLen, compressedData.data(), static_cast<uLong>(compressedData.size()));
    if (ret != Z_OK) {
        switch (ret) {
        case Z_MEM_ERROR:
            throw std::runtime_error("png uncompress: out of memory");
        case Z_BUF_ERROR:
            throw std::runtime_error("png uncompress: not enough memory reservered");
        case Z_DATA_ERROR:
            throw std::runtime_error("png uncompress: data corrupted");
        default:
            throw std::runtime_error("png uncompress: unknown error");
        }
    }
    compressedData.clear();

    return decodePngData(ihdr, imageData);
}

PngIHDR readPngIHDR(BinaryInputStream &in, size_t chunkLen) {
    if (chunkLen != 13) {
        throw std::runtime_error("unexpected PNG IHDR chunk length");
    }
    PngIHDR h;
    h.width = in.read<u32>();
    h.height = in.read<u32>();
    h.bitDepth = in.read<u8>();
    h.colorType = in.read<u8>();
    h.compressionMethod = in.read<u8>();
    h.filterMethod = in.read<u8>();
    h.interlaceMethod = in.read<u8>();

    // check for unspecified values
    if (h.width == 0 || h.height == 0) {
        throw std::runtime_error("PNG has zero size");
    }
    if (h.compressionMethod != 0) {
        throw std::runtime_error("unexpected PNG compression method");
    }
    if (h.filterMethod != 0) {
        throw std::runtime_error("unexpected PNG filter method");
    }

    // check for unsupported formats
    if (h.bitDepth != 8) {
        throw std::runtime_error("only PNG with bit depth 8 bits/color supported");
    }
    if (h.colorType != 2 && h.colorType != 6) {
        throw std::runtime_error("only PNG with truecolor (and optionally alpha) supported");
    }
    if (h.interlaceMethod != 0) {
        throw std::runtime_error("only non-interlaced PNGs supported");
    }

    return h;
}

enum class FilterType {
    None = 0,
    Sub = 1,
    Up = 2,
    Average = 3,
    Paeth = 4
};

static UColor average(UColor lhs, UColor rhs) {
    return UColor{
        static_cast<u8>((lhs.r + rhs.r) / 2),
        static_cast<u8>((lhs.g + rhs.g) / 2),
        static_cast<u8>((lhs.b + rhs.b) / 2),
        static_cast<u8>((lhs.a + rhs.a) / 2)
    };
}

// PaethPredictor function as written in the PNG standard:
// https://www.w3.org/TR/2003/REC-PNG-20031110/#9Filter-type-4-Paeth
static u8 paeth(u8 a, u8 b, u8 c) {
    int p = static_cast<int>(a) + b - c;
    int pa = abs(p - a);
    int pb = abs(p - b);
    int pc = abs(p - c);
    if (pa <= pb && pa <= pc) {
        return a;
    } else if (pb <= pc) {
        return b;
    } else {
        return c;
    }
}

static UColor paeth(UColor a, UColor b, UColor c) {
    return UColor{
        paeth(a.r, b.r, c.r),
        paeth(a.g, b.g, c.g),
        paeth(a.b, b.b, c.b),
        paeth(a.a, b.a, c.a)
    };
}

Picture decodePngData(const PngIHDR &ihdr, const std::vector<u8> &data) {
    const bool alpha = ihdr.colorType == 6;
    Picture pic({ ihdr.width, ihdr.height });
    auto it = data.begin();

    std::vector<UColor> prevLine(ihdr.width, { 0, 0, 0, 0 });
    for (u32 y = 0; y < ihdr.height; y++) {
        FilterType filterType = static_cast<FilterType>(*it++);

        UColor prevPixel{ 0, 0, 0, 0 };
        UColor prevLinePixel{ 0, 0, 0, 0 };
        UColor prevLinePrevPixel{ 0, 0, 0, 0 };
        for (u32 x = 0; x < ihdr.width; x++) {
            prevLinePrevPixel = prevLinePixel;
            prevLinePixel = prevLine[x];
            UColor col{ *it++, *it++ , *it++, alpha ? *it++ : u8(255u) };

            switch (filterType) {
            case FilterType::None:
                break;
            case FilterType::Sub:
                col = col + prevPixel;
                break;
            case FilterType::Up:
                col = col + prevLinePixel;
                break;
            case FilterType::Average:
                col = col + average(prevPixel, prevLinePixel);
                break;
            case FilterType::Paeth:
                col = col + paeth(prevPixel, prevLinePixel, prevLinePrevPixel);
                break;
            default:
                throw std::runtime_error("unsupported PNG filter type");
            }
            if (!alpha) {
                col.a = 255u;
            }
            pic.set({ x, y }, col);
            prevLine[x] = prevPixel = col;
        }
    }

    return pic;
}

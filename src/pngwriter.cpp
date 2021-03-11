#include <algorithm>
#include <array>
#include <cassert>
#include <iterator>
#include <ostream>
#include <stdexcept>
#include <vector>
#include <zlib.h>

#include "png.h"

using u32net = std::array<u8, 4>;
static std::ostream &operator<<(std::ostream &s, const u32net &v) {
    s.write(reinterpret_cast<const char *>(v.data()), v.size() * sizeof * v.data());
    return s;
}
static u32net toNet32(u32 value) {
    return u32net {
        static_cast<u8>((value >> 24) & 0xff),
        static_cast<u8>((value >> 16) & 0xff),
        static_cast<u8>((value >> 8) & 0xff),
        static_cast<u8>(value & 0xff) };
}
using u16net = std::array<u8, 2>;
static u16net toNet16(u16 value) {
    return u16net{
        static_cast<u8>((value >> 8) & 0xff),
        static_cast<u8>(value & 0xff) };
}

static u32net calcCrc(const void *data, size_t len) {
    return toNet32(crc32(Z_NULL, reinterpret_cast<const unsigned char *>(data), static_cast<uInt>(len)));
}

#pragma pack(push, 1)
struct PNGHeader {
    const u8 pngHeader[8] = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
    struct Ihdr {
        const u8 length[4] = { 0x00, 0x00, 0x00, 0x0d };
        const u8 header[4] = { 0x49, 0x48, 0x44, 0x52 }; // IHDR
        const u32net width;
        const u32net height;
        const u8 bitDepth = 0x08; // 8 bits/pixel
        const u8 colorType = 0x06; // RGBA Truecolor
        const u8 compressionMethod = 0x00; // Deflate
        const u8 filterMethod = 0x00; // default filtering
        const u8 interlaceMethod = 0x00; // no interlace
        const u32net checksum;

        Ihdr(u32 width, u32 height) :
            width{ toNet32(width) },
            height{ toNet32(height) },
            checksum{ calcCrc(&header, sizeof * this - sizeof length - sizeof checksum) }
        {}
    } ihdr;

    PNGHeader(u32 width, u32 height) : ihdr{ width, height } {}
};

struct PNGGamma {
    struct Gama {
        const u8 length[4] = { 0x00, 0x00, 0x00, 0x04 };
        const u8 header[4] = { 0x67, 0x41, 0x4d, 0x41 }; // gAMA
        const u32net gamma;
        const u32net checksum;

        Gama() :
            gamma{ toNet32(45455u) }, // Gamma = 1/2.2
            checksum{ calcCrc(&header, sizeof * this - sizeof length - sizeof checksum) }
        {}
    } gama;
};

struct PNGChroma {
    struct Chrm {
        const u8 length[4] = { 0x00, 0x00, 0x00, 0x20 };
        const u8 header[4] = { 0x63, 0x48, 0x52, 0x4d }; // cHRM
        const u32net whiteX;
        const u32net whiteY;
        const u32net redX;
        const u32net redY;
        const u32net greenX;
        const u32net greenY;
        const u32net blueX;
        const u32net blueY;
        const u32net checksum;

        Chrm() :
            whiteX{ toNet32(31270u) }, // reference white point of sRGB
            whiteY{ toNet32(32900u) },
            redX{   toNet32(64000u) }, // chromatic values of sRGB
            redY{   toNet32(33000u) },
            greenX{ toNet32(30000u) },
            greenY{ toNet32(60000u) },
            blueX{  toNet32(15000u) },
            blueY{  toNet32( 6000u) },
            checksum{ calcCrc(&header, sizeof * this - sizeof length - sizeof checksum) } {}
    } chrm;
};

struct PNGSrgb {
    struct Srgb {
        const u8 length[4] = { 0x00, 0x00, 0x00, 0x01 };
        const u8 header[4] = { 0x73, 0x52, 0x47, 0x42 }; // sRGB
        const u8 intent = 0x00; // Perceptual rednering intent (Photographs)
        const u32net checksum;

        Srgb() :            
            checksum{ calcCrc(&header, sizeof * this - sizeof length - sizeof checksum) }
        {}
    } srgb;
};

struct APNGHeader {
    struct Actl {
        const u8 length[4] = { 0x00, 0x00, 0x00, 0x08 };
        const u8 header[4] = { 0x61, 0x63, 0x54, 0x4c }; // acTL
        const u32net num_frames;
        const u32net num_plays;
        const u32net checksum;

        Actl(u32 frameCount) :
            num_frames{ toNet32(frameCount) },
            num_plays{ toNet32(0) }, // 0 = unlimited
            checksum{ calcCrc(&header, sizeof * this - sizeof length - sizeof checksum) }
        {}
    } actl;

    APNGHeader(u32 frameCount) : actl{ frameCount } {}
};

struct APNGFrameControl {
    struct Fctl {
        const u8 length[4] = { 0x00, 0x00, 0x00, 0x1a };
        const u8 header[4] = { 0x66, 0x63, 0x54, 0x4c }; // fcTL
        const u32net sequence_number;
        const u32net width;
        const u32net height;
        const u8 x_offset[4] = { 0x00, 0x00, 0x00, 0x00 };
        const u8 y_offset[4] = { 0x00, 0x00, 0x00, 0x00 };
        const u16net delay_num;
        //const u8 delay_num[2] = { 0x00, 0x28 }; // delay = 40/1000 s = 25 fps
        const u8 delay_den[2] = { 0x03, 0xe8 };
        const u8 dispose_op = 0x00; // APNG_DISPOSE_OP_NONE
        const u8 blend_op = 0x00; // APNG_BLEND_OP_SOURCE
        const u32net checksum;

        Fctl(u32 width, u32 height, u32 seqNum, scalar fps) :
            sequence_number{ toNet32(seqNum) },
            width{ toNet32(width) },
            height{ toNet32(height) },
            delay_num{ toNet16(static_cast<u16>(1000.0f / fps)) },
            checksum{ calcCrc(&header, sizeof * this - sizeof length - sizeof checksum) }
        {}
    } fctl;

    APNGFrameControl(u32 width, u32 height, u32 seqNum, scalar fps) : fctl{ width, height, seqNum, fps } {}
};

static const u8 PNGDataTag[] = { 0x49, 0x44, 0x41, 0x54 }; // IDAT
static const u8 APNGDataTag[] = { 0x66, 0x64, 0x41, 0x54 }; // fdAT
static const u8 PNGFooterTag[] = {
    0x00, 0x00, 0x00, 0x1b, 0x7a, 0x54, 0x58, 0x74, 0x53, 0x6f, 0x66, 0x74, 0x77, 0x61, 0x72, 0x65,
    0x00, 0x00, 0x78, 0xda, 0x4b, 0xce, 0x49, 0xcd, 0xad, 0xd4, 0xcb, 0x2f, 0x4a, 0x07, 0x00, 0x11,
    0xe2, 0x03, 0x91, 0xc0, 0x62, 0x31, 0xa2, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae,
    0x42, 0x60, 0x82}; // IEND
#pragma pack(pop)

static const u32 USE_PNG_MODE = std::numeric_limits<u32>::max();

void writePNG(std::ostream &out, const Picture &picture, scalar gain, PNGFilterType filterType) {    
    writeAPNGStart(out, picture.size(), USE_PNG_MODE);
    writeAPNGFrame(out, picture, USE_PNG_MODE, 1.0f, gain, filterType);
    writeAPNGEnd(out);
}

void writeAPNGStart(std::ostream &out, UDim2 size, u32 frameCount) {
    const PNGHeader header(size.x, size.y);
    out.write(reinterpret_cast<const char *>(&header), sizeof header);
    const PNGGamma gamma;
    out.write(reinterpret_cast<const char *>(&gamma), sizeof gamma);
    const PNGChroma chroma;
    out.write(reinterpret_cast<const char *>(&chroma), sizeof chroma);
    // do not write an sRGB rendering intent, as this causes not intended colors in some viewers
    //const PNGSrgb srgb;
    //out.write(reinterpret_cast<const char *>(&srgb), sizeof srgb);

    if (frameCount != USE_PNG_MODE) {
        const APNGHeader apngHeader(frameCount);
        out.write(reinterpret_cast<const char *>(&apngHeader), sizeof apngHeader);
    }
}

void writeAPNGFrame(std::ostream &out, const Picture &picture, u32 frameNum, scalar fps, scalar gain, PNGFilterType filterType) {
    assert(filterType == PNGFilterType::None || filterType == PNGFilterType::Sub);
    auto size = picture.size();
    if (frameNum != USE_PNG_MODE) {
        const APNGFrameControl controlblock(size.x, size.y, frameNum == 0 ? 0 : frameNum * 2 - 1, fps);
        out.write(reinterpret_cast<const char *>(&controlblock), sizeof controlblock);
    }

    // only supporting filter type 0 and 1 of filter method 0:
    //  None = 0: encode every pixel as it is
    //  Sub  = 1: encode difference to previous pixel in line
    std::vector<u8> filteredData;
    filteredData.reserve(static_cast<size_t>(size.x) * size.y * 4 + size.y);
    for (u32 y = 0; y < size.y; y++) {
        filteredData.push_back(static_cast<u8>(filterType));
        UColor prevPixelColor{ 0, 0, 0, 0 };
        for (u32 x = 0; x < size.x; x++) {
            const UColor color = picture.get({ x, y }).scaleOut(gain);
            const auto encodedColor = (filterType == PNGFilterType::None ? color : color - prevPixelColor).rgba();
            filteredData.insert(filteredData.end(), std::begin(encodedColor), std::end(encodedColor));
            prevPixelColor = color;
        }
    }

    bool useAPngDataBlock = frameNum != USE_PNG_MODE && frameNum != 0;
    const auto &tag = useAPngDataBlock ? APNGDataTag : PNGDataTag;
    std::vector<u8> compressedData(std::begin(tag), std::end(tag));
    if (useAPngDataBlock) {
        const auto frameNumNet = toNet32(frameNum * 2);
        compressedData.insert(compressedData.end(), std::begin(frameNumNet), std::end(frameNumNet));
    }
    unsigned long compressedSize = compressBound(static_cast<uLong>(filteredData.size()));
    size_t hdrSize = compressedData.size();
    compressedData.resize(hdrSize + compressedSize);
    int ret = compress(&compressedData[hdrSize], &compressedSize, filteredData.data(), static_cast<uLong>(filteredData.size()));
    if (ret != Z_OK) {
        throw std::runtime_error("zlib compression failed");
    }
    compressedData.resize(compressedSize + hdrSize);

    out << toNet32(static_cast<u32>(compressedData.size() - 4));
    out.write(reinterpret_cast<const char *>(compressedData.data()), compressedData.size() * sizeof(u8));
    out << calcCrc(compressedData.data(), compressedData.size() * sizeof(u8));
}

void writeAPNGEnd(std::ostream & out) {
    out.write(reinterpret_cast<const char *>(PNGFooterTag), sizeof PNGFooterTag);
}

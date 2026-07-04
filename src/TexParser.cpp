#include "TexParser.h"
#include "stb_image.h"
#include <lz4.h>
#include <cstring>
#include <cstdint>

DecodedImage DecodeImage(const std::vector<uint8_t>& data) {
    DecodedImage result;
    if (data.empty()) return result;
    int w = 0, h = 0, ch = 0;
    unsigned char* pixels = stbi_load_from_memory(
        data.data(), static_cast<int>(data.size()), &w, &h, &ch, 4);
    if (!pixels || w <= 0 || h <= 0) return result;
    result.width = w; result.height = h; result.channels = 4;
    result.pixels.resize(static_cast<size_t>(w) * h * 4);
    std::memcpy(result.pixels.data(), pixels, result.pixels.size());
    stbi_image_free(pixels);
    return result;
}

static bool Read9Ver(const uint8_t*& ptr, size_t& rem, int& ver) {
    if (rem < 9) return false;
    char buf[9]; std::memcpy(buf, ptr, 9);
    ptr += 9; rem -= 9;
    if (buf[4] < '0' || buf[4] > '9') return false;
    int v = 0;
    for (int i = 4; i < 8; i++) {
        if (buf[i] >= '0' && buf[i] <= '9') v = v * 10 + (buf[i] - '0');
    }
    ver = v; return true;
}

static int32_t ReadI32(const uint8_t*& ptr, size_t& rem) {
    int32_t v = 0;
    if (rem >= 4) { std::memcpy(&v, ptr, 4); ptr += 4; rem -= 4; }
    return v;
}

// Check if data is MP4 video (ISO BMFF ftyp box) or WebM (EBML header)
static bool IsVideoData(const uint8_t* data, size_t size) {
    if (size >= 12 && std::memcmp(data + 4, "ftyp", 4) == 0) return true;
    if (size >= 4 && data[0] == 0x1A && data[1] == 0x45 && data[2] == 0xDF && data[3] == 0xA3) return true;
    return false;
}

DecodedImage DecodeTexFile(const std::vector<uint8_t>& data) {
    DecodedImage result;
    if (data.empty()) return result;

    const uint8_t* ptr = data.data();
    size_t rem = data.size();

    int texv = 0, texi = 0, texb = 0;
    if (!Read9Ver(ptr, rem, texv)) return result;
    if (!Read9Ver(ptr, rem, texi)) return result;

    int32_t format = ReadI32(ptr, rem);
    uint32_t flags = 0;
    if (rem >= 4) { std::memcpy(&flags, ptr, 4); ptr += 4; rem -= 4; }
    (void)flags;

    int32_t width  = ReadI32(ptr, rem);
    int32_t height = ReadI32(ptr, rem);
    int32_t mapW   = ReadI32(ptr, rem);
    int32_t mapH   = ReadI32(ptr, rem);
    int32_t unk    = ReadI32(ptr, rem);
    (void)mapW; (void)mapH; (void)unk;

    if (!Read9Ver(ptr, rem, texb)) return result;

    int32_t imageCount = ReadI32(ptr, rem);
    if (imageCount < 0) return result;
    if (imageCount == 0) imageCount = 1;

    int32_t imageType = -1;
    if (texb >= 3) imageType = ReadI32(ptr, rem);
    if (texb >= 4) ReadI32(ptr, rem); // reserved

    for (int32_t img = 0; img < imageCount && rem > 0; img++) {
        int32_t mipCount = ReadI32(ptr, rem);
        if (mipCount <= 0) continue;

        for (int32_t mip = 0; mip < mipCount && rem > 0; mip++) {
            int32_t mipW = ReadI32(ptr, rem);
            int32_t mipH = ReadI32(ptr, rem);

            int32_t lz4Flag = 0, decompSize = 0;
            if (texb >= 2) {
                lz4Flag    = ReadI32(ptr, rem);
                decompSize = ReadI32(ptr, rem);
            }
            int32_t srcSize = ReadI32(ptr, rem);
            if (srcSize <= 0 || srcSize > (int32_t)rem) break;

            const uint8_t* dataStart = ptr;
            size_t dataSize = (size_t)srcSize;

            // Skip video content (MP4/WebM)
            if (IsVideoData(dataStart, dataSize)) {
                ptr += srcSize; rem -= srcSize;
                continue;
            }

            // Read the raw data
            std::vector<uint8_t> rawData(dataStart, dataStart + srcSize);
            ptr += srcSize; rem -= srcSize;

            std::vector<uint8_t> pixels;

            if (lz4Flag && decompSize > 0) {
                pixels.resize((size_t)decompSize);
                int ret = LZ4_decompress_safe(
                    (const char*)rawData.data(), (char*)pixels.data(),
                    (int)rawData.size(), decompSize);
                if (ret < decompSize) continue;
            } else {
                pixels = std::move(rawData);
                decompSize = srcSize;
            }

            // Try stb_image first
            if (mip == 0) {
                result = DecodeImage(pixels);
                if (result.Valid()) return result;
            }

            // For RGBA8 (format=0), use raw decompressed pixels
            if (format == 0 && mip == 0) {
                if ((size_t)mipW * mipH * 4 <= pixels.size()) {
                    result.width = mipW;
                    result.height = mipH;
                    result.channels = 4;
                    result.pixels.assign(pixels.data(), pixels.data() + (size_t)mipW * mipH * 4);
                    return result;
                }
            }

            // For DXT5/BC3 (format=4), skip (can't decode in software)
            if (format == 4 && mip == 0) {
                result.width = -4; // negative signals BC3
                result.height = mipW;
                result.channels = mipH;
                result.pixels = std::move(pixels);
                return result;
            }
        }
    }
    return result;
}

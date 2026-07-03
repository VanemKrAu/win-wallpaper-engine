#include "TexParser.h"
#include "stb_image.h"
#include <cstring>

DecodedImage DecodeImage(const std::vector<uint8_t>& data) {
    DecodedImage result;
    if (data.empty()) return result;

    int w = 0, h = 0, ch = 0;
    unsigned char* pixels = stbi_load_from_memory(
        data.data(),
        static_cast<int>(data.size()),
        &w, &h, &ch, 4);

    if (!pixels || w <= 0 || h <= 0) return result;

    result.width    = w;
    result.height   = h;
    result.channels = 4;
    result.pixels.resize(static_cast<size_t>(w) * h * 4);
    std::memcpy(result.pixels.data(), pixels, result.pixels.size());

    stbi_image_free(pixels);
    return result;
}

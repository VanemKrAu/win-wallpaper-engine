#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct DecodedImage {
    int width  = 0;
    int height = 0;
    int channels = 4;
    std::vector<uint8_t> pixels;
    bool Valid() const { return !pixels.empty() && width > 0 && height > 0; }
};

// Decode standard image formats (PNG/JPEG/GIF/BMP) via stb_image
DecodedImage DecodeImage(const std::vector<uint8_t>& data);

// Decode Wallpaper Engine .tex format (may contain LZ4-compressed mipmaps)
// Returns the first valid mip level as a DecodedImage
DecodedImage DecodeTexFile(const std::vector<uint8_t>& data);

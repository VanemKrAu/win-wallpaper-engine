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

DecodedImage DecodeImage(const std::vector<uint8_t>& data);

#pragma once
#include <vector>
#include <cstdint>
#include <cstdio>

// FFmpeg video decoder for Wallpaper Engine .tex video textures
// Takes raw MP4 data, decodes frames to RGBA8

struct VideoFrame {
    std::vector<uint8_t> pixels;
    int width  = 0;
    int height = 0;
    bool valid = false;
};

class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder();

    // Open video from raw MP4 data (e.g., extracted from .tex file)
    bool Open(const uint8_t* data, size_t size);

    // Decode the next frame. Returns false at end of stream.
    bool DecodeNextFrame(VideoFrame& frame);

    // Seek to beginning
    bool SeekToStart();

    // Get video info
    int Width() const { return m_width; }
    int Height() const { return m_height; }
    double Duration() const { return m_duration; }
    double FrameRate() const { return m_fps; }
    bool IsOpen() const { return m_opened; }

    // Close and free resources
    void Close();

private:
    bool m_opened = false;
    int m_width = 0, m_height = 0;
    double m_duration = 0.0;
    double m_fps = 30.0;

    // Opaque FFmpeg context pointers (forward declared to avoid FFmpeg includes in header)
    void* m_fmtCtx = nullptr;   // AVFormatContext*
    void* m_codecCtx = nullptr; // AVCodecContext*
    void* m_swsCtx = nullptr;   // SwsContext*
    int m_videoStreamIdx = -1;
};

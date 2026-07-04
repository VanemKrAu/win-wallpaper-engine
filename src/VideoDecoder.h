#pragma once
#include <vector>
#include <cstdint>

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
    bool Open(const uint8_t* data, size_t size);
    bool OpenFile(const wchar_t* filePath);
    bool DecodeNextFrame(VideoFrame& frame);
    bool SeekToStart();
    int Width() const { return m_width; }
    int Height() const { return m_height; }
    double Duration() const { return m_duration; }
    double FrameRate() const { return m_fps; }
    bool IsOpen() const { return m_opened; }
    void Close();

private:
    bool m_opened = false;
    int m_width = 0, m_height = 0;
    double m_duration = 0.0;
    double m_fps = 30.0;
    void* m_fmtCtx = nullptr;
    void* m_codecCtx = nullptr;
    void* m_swsCtx = nullptr;
    int m_videoStreamIdx = -1;
};

#include "VideoDecoder.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

#include <cstring>

VideoDecoder::VideoDecoder() {}
VideoDecoder::~VideoDecoder() { Close(); }

bool VideoDecoder::Open(const uint8_t* data, size_t size) {
    Close();
    if (!data || size < 100) return false;
    static int s_idx = 0;
    wchar_t tmpPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tmpPath);
    wsprintf(tmpPath + wcslen(tmpPath), L"we_v%d.mp4", s_idx++);
    FILE* tmp = _wfopen(tmpPath, L"wb");
    if (!tmp) return false;
    fwrite(data, 1, size, tmp);
    fclose(tmp);
    return OpenFile(tmpPath);
}

bool VideoDecoder::OpenFile(const wchar_t* filePath) {
    Close();
    if (!filePath) return false;
    char pb[1024] = {0};
    WideCharToMultiByte(CP_UTF8, 0, filePath, -1, pb, 1024, NULL, NULL);
    AVFormatContext* fc = NULL;
    if (avformat_open_input(&fc, pb, NULL, NULL) < 0) return false;
    if (avformat_find_stream_info(fc, NULL) < 0) { avformat_close_input(&fc); return false; }
    int vi = -1;
    for (unsigned i = 0; i < fc->nb_streams; i++)
        if (fc->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) { vi = i; break; }
    if (vi < 0) { avformat_close_input(&fc); return false; }
    const AVCodec* cd = avcodec_find_decoder(fc->streams[vi]->codecpar->codec_id);
    if (!cd) { avformat_close_input(&fc); return false; }
    AVCodecContext* cc = avcodec_alloc_context3(cd);
    avcodec_parameters_to_context(cc, fc->streams[vi]->codecpar);
    if (avcodec_open2(cc, cd, NULL) < 0) { avcodec_free_context(&cc); avformat_close_input(&fc); return false; }
    m_fmtCtx = fc; m_codecCtx = cc; m_videoStreamIdx = vi;
    m_width = fc->streams[vi]->codecpar->width;
    m_height = fc->streams[vi]->codecpar->height;
    m_duration = (double)fc->duration / AV_TIME_BASE;
    if (fc->streams[vi]->r_frame_rate.num > 0)
        m_fps = (double)fc->streams[vi]->r_frame_rate.num / fc->streams[vi]->r_frame_rate.den;
    m_opened = true; return true;
}

bool VideoDecoder::DecodeNextFrame(VideoFrame& frame) {
    frame.valid = false;
    if (!m_opened) return false;
    AVFormatContext* fmtCtx = (AVFormatContext*)m_fmtCtx;
    AVCodecContext* codecCtx = (AVCodecContext*)m_codecCtx;
    AVPacket* packet = av_packet_alloc();
    AVFrame* avFrame = av_frame_alloc();
    bool gotFrame = false;
    while (!gotFrame && av_read_frame(fmtCtx, packet) >= 0) {
        if (packet->stream_index == m_videoStreamIdx) {
            int ret = avcodec_send_packet(codecCtx, packet);
            av_packet_unref(packet);
            if (ret >= 0 && avcodec_receive_frame(codecCtx, avFrame) >= 0)
                gotFrame = true;
        } else { av_packet_unref(packet); }
    }
    if (!gotFrame) {
        avcodec_send_packet(codecCtx, nullptr);
        if (avcodec_receive_frame(codecCtx, avFrame) >= 0) gotFrame = true;
    }
    if (gotFrame) {
        int w = avFrame->width, h = avFrame->height;
        frame.width = w; frame.height = h;
        frame.isRGBA = true;

        // Convert to RGBA via sws_scale
        SwsContext* swsCtx = (SwsContext*)m_swsCtx;
        swsCtx = sws_getCachedContext(swsCtx, w, h, (AVPixelFormat)avFrame->format,
            w, h, AV_PIX_FMT_RGBA, SWS_LANCZOS, NULL, NULL, NULL);
        m_swsCtx = swsCtx;
        if (swsCtx) {
            frame.rgba.resize((size_t)w * h * 4);
            uint8_t* dst[] = { frame.rgba.data() };
            int dstStride[] = { w * 4 };
            sws_scale(swsCtx, avFrame->data, avFrame->linesize, 0, h, dst, dstStride);
        }

        // Also extract NV12 planes for GPU-native path
        // Extract Y plane (full resolution)
        size_t ySize = (size_t)w * h;
        frame.yPlane.resize(ySize);
        if (avFrame->linesize[0] == w) {
            memcpy(frame.yPlane.data(), avFrame->data[0], ySize);
        } else {
            for (int row = 0; row < h; row++)
                memcpy(frame.yPlane.data() + row * w, avFrame->data[0] + row * avFrame->linesize[0], w);
        }

        // Extract UV plane (half resolution, interleaved)
        size_t uvW = (size_t)(w / 2), uvH = (size_t)(h / 2);
        size_t uvSize = uvW * uvH * 2;
        frame.uvPlane.resize(uvSize);
        if (avFrame->format == AV_PIX_FMT_YUV420P || avFrame->format == AV_PIX_FMT_YUVJ420P) {
            // Interleave U and V planes
            for (size_t row = 0; row < uvH; row++) {
                const uint8_t* uSrc = avFrame->data[1] + row * avFrame->linesize[1];
                const uint8_t* vSrc = avFrame->data[2] + row * avFrame->linesize[2];
                uint8_t* dst = frame.uvPlane.data() + row * uvW * 2;
                for (size_t col = 0; col < uvW; col++) {
                    *dst++ = *uSrc++;
                    *dst++ = *vSrc++;
                }
            }
        } else if (avFrame->format == AV_PIX_FMT_NV12) {
            memcpy(frame.uvPlane.data(), avFrame->data[1], uvSize);
        } else {
            // Convert to YUV420P first via sws_scale
            SwsContext* sws = sws_getContext(w, h, (AVPixelFormat)avFrame->format, w, h, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
            if (sws) {
                AVFrame* yuvFrame = av_frame_alloc();
                yuvFrame->width = w; yuvFrame->height = h; yuvFrame->format = AV_PIX_FMT_YUV420P;
                av_frame_get_buffer(yuvFrame, 0);
                sws_scale(sws, avFrame->data, avFrame->linesize, 0, h, yuvFrame->data, yuvFrame->linesize);
                // Y plane
                for (int row = 0; row < h; row++)
                    memcpy(frame.yPlane.data() + row * w, yuvFrame->data[0] + row * yuvFrame->linesize[0], w);
                // UV interleaved
                for (size_t row = 0; row < uvH; row++) {
                    const uint8_t* uSrc = yuvFrame->data[1] + row * yuvFrame->linesize[1];
                    const uint8_t* vSrc = yuvFrame->data[2] + row * yuvFrame->linesize[2];
                    uint8_t* dst = frame.uvPlane.data() + row * uvW * 2;
                    for (size_t col = 0; col < uvW; col++) { *dst++ = *uSrc++; *dst++ = *vSrc++; }
                }
                av_frame_free(&yuvFrame);
                sws_freeContext(sws);
            }
        }
        frame.valid = true;
    }
    av_frame_free(&avFrame);
    av_packet_free(&packet);
    return gotFrame;
}

bool VideoDecoder::SeekToStart() {
    if (!m_opened) return false;
    AVFormatContext* fmtCtx = (AVFormatContext*)m_fmtCtx;
    AVCodecContext* codecCtx = (AVCodecContext*)m_codecCtx;
    if (av_seek_frame(fmtCtx, m_videoStreamIdx, 0, AVSEEK_FLAG_BACKWARD) < 0) return false;
    avcodec_flush_buffers(codecCtx);
    return true;
}

void VideoDecoder::Close() {
    if (!m_opened) return;
    AVFormatContext* fmtCtx = (AVFormatContext*)m_fmtCtx;
    AVCodecContext* codecCtx = (AVCodecContext*)m_codecCtx;
    SwsContext* swsCtx = (SwsContext*)m_swsCtx;
    if (fmtCtx) avformat_close_input(&fmtCtx);
    if (codecCtx) avcodec_free_context(&codecCtx);
    if (swsCtx) sws_freeContext(swsCtx);
    m_fmtCtx = nullptr;
    m_codecCtx = nullptr;
    m_swsCtx = nullptr;
    m_opened = false;
}

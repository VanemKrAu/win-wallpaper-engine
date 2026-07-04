#include "VideoDecoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

#include <cstring>

// Callback for reading from memory buffer
struct MemBuffer {
    const uint8_t* data;
    size_t size;
    size_t pos;
};

static int ReadCallback(void* opaque, uint8_t* buf, int bufSize) {
    MemBuffer* mb = (MemBuffer*)opaque;
    int available = (int)(mb->size - mb->pos);
    int toRead = (bufSize < available) ? bufSize : available;
    if (toRead <= 0) return AVERROR_EOF;
    std::memcpy(buf, mb->data + mb->pos, (size_t)toRead);
    mb->pos += toRead;
    return toRead;
}

static int64_t SeekCallback(void* opaque, int64_t offset, int whence) {
    MemBuffer* mb = (MemBuffer*)opaque;
    switch (whence) {
    case SEEK_SET: mb->pos = (size_t)(offset >= 0 ? offset : 0); break;
    case SEEK_CUR: mb->pos = (size_t)((int64_t)mb->pos + offset); break;
    case SEEK_END: mb->pos = mb->size; break;
    case AVSEEK_SIZE: return (int64_t)mb->size;
    }
    if (mb->pos > mb->size) mb->pos = mb->size;
    return (int64_t)mb->pos;
}

VideoDecoder::VideoDecoder() {}
VideoDecoder::~VideoDecoder() { Close(); }

bool VideoDecoder::Open(const uint8_t* data, size_t size) {
    Close();
    if (!data || size < 100) return false;

    // Set up memory buffer for FFmpeg
    MemBuffer* mb = new MemBuffer();
    mb->data = data;
    mb->size = size;
    mb->pos = 0;

    // Create AVIO context
    AVIOContext* avio = avio_alloc_context(
        (unsigned char*)av_malloc(4096), 4096,
        AVIO_FLAG_READ, mb, ReadCallback, nullptr, SeekCallback);
    if (!avio) { delete mb; return false; }

    // Open format context
    AVFormatContext* fmtCtx = avformat_alloc_context();
    if (!fmtCtx) { av_free(avio->buffer); av_free(avio); delete mb; return false; }
    fmtCtx->pb = avio;
    fmtCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

    int ret = avformat_open_input(&fmtCtx, "", nullptr, nullptr);
    if (ret < 0) {
        avformat_free_context(fmtCtx);
        av_free(avio->buffer); av_free(avio);
        delete mb;
        return false;
    }

    ret = avformat_find_stream_info(fmtCtx, nullptr);
    if (ret < 0) {
        avformat_close_input(&fmtCtx);
        av_free(avio->buffer); av_free(avio);
        delete mb;
        return false;
    }

    // Find video stream
    int videoIdx = -1;
    for (unsigned i = 0; i < fmtCtx->nb_streams; i++) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoIdx = (int)i;
            break;
        }
    }
    if (videoIdx < 0) {
        avformat_close_input(&fmtCtx);
        av_free(avio->buffer); av_free(avio);
        delete mb;
        return false;
    }

    // Open decoder
    AVCodecParameters* params = fmtCtx->streams[videoIdx]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(params->codec_id);
    if (!codec) {
        avformat_close_input(&fmtCtx);
        av_free(avio->buffer); av_free(avio);
        delete mb;
        return false;
    }

    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx) {
        avformat_close_input(&fmtCtx);
        av_free(avio->buffer); av_free(avio);
        delete mb;
        return false;
    }

    ret = avcodec_parameters_to_context(codecCtx, params);
    if (ret < 0) {
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        av_free(avio->buffer); av_free(avio);
        delete mb;
        return false;
    }

    ret = avcodec_open2(codecCtx, codec, nullptr);
    if (ret < 0) {
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
        av_free(avio->buffer); av_free(avio);
        delete mb;
        return false;
    }

    // Store info
    m_fmtCtx = fmtCtx;
    m_codecCtx = codecCtx;
    m_videoStreamIdx = videoIdx;
    m_width = params->width;
    m_height = params->height;
    m_duration = (double)fmtCtx->duration / AV_TIME_BASE;

    AVStream* st = fmtCtx->streams[videoIdx];
    if (st->r_frame_rate.num > 0)
        m_fps = (double)st->r_frame_rate.num / st->r_frame_rate.den;

    m_opened = true;
    return true;
}

bool VideoDecoder::DecodeNextFrame(VideoFrame& frame) {
    frame.valid = false;
    if (!m_opened) return false;

    AVFormatContext* fmtCtx = (AVFormatContext*)m_fmtCtx;
    AVCodecContext* codecCtx = (AVCodecContext*)m_codecCtx;

    AVPacket* packet = av_packet_alloc();
    AVFrame* avFrame = av_frame_alloc();
    bool gotFrame = false;

    while (av_read_frame(fmtCtx, packet) >= 0) {
        if (packet->stream_index == m_videoStreamIdx) {
            int ret = avcodec_send_packet(codecCtx, packet);
            if (ret >= 0) {
                ret = avcodec_receive_frame(codecCtx, avFrame);
                if (ret >= 0) {
                    gotFrame = true;
                    break;
                }
            }
        }
        av_packet_unref(packet);
    }

    if (gotFrame) {
        // Convert frame to RGBA8
        int w = avFrame->width;
        int h = avFrame->height;

        // Create or reuse sws context
        SwsContext* swsCtx = (SwsContext*)m_swsCtx;
        swsCtx = sws_getCachedContext(swsCtx,
            w, h, (AVPixelFormat)avFrame->format,
            w, h, AV_PIX_FMT_RGBA,
            SWS_BILINEAR, nullptr, nullptr, nullptr);
        m_swsCtx = swsCtx;

        if (swsCtx) {
            frame.pixels.resize((size_t)w * h * 4);
            uint8_t* dst[] = { frame.pixels.data() };
            int dstStride[] = { w * 4 };
            sws_scale(swsCtx, avFrame->data, avFrame->linesize,
                0, h, dst, dstStride);
            frame.width = w;
            frame.height = h;
            frame.valid = true;
        }
    }

    av_frame_free(&avFrame);
    av_packet_free(&packet);
    return gotFrame;
}

bool VideoDecoder::SeekToStart() {
    if (!m_opened) return false;
    AVFormatContext* fmtCtx = (AVFormatContext*)m_fmtCtx;
    AVCodecContext* codecCtx = (AVCodecContext*)m_codecCtx;

    int ret = av_seek_frame(fmtCtx, m_videoStreamIdx, 0, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) return false;
    avcodec_flush_buffers(codecCtx);
    return true;
}

void VideoDecoder::Close() {
    if (!m_opened) return;

    AVFormatContext* fmtCtx = (AVFormatContext*)m_fmtCtx;
    AVCodecContext* codecCtx = (AVCodecContext*)m_codecCtx;
    SwsContext* swsCtx = (SwsContext*)m_swsCtx;

    if (fmtCtx) {
        AVIOContext* avio = fmtCtx->pb;
        avformat_close_input(&fmtCtx);
        if (avio) {
            av_freep(&avio->buffer);
            avio_context_free(&avio);
        }
    }
    if (codecCtx) avcodec_free_context(&codecCtx);
    if (swsCtx) sws_freeContext(swsCtx);

    // Free the memory buffer opaque
    // Note: MemBuffer was allocated in Open() but needs to be freed here
    // The opaque is stored in the AVIOContext which we freed above

    m_fmtCtx = nullptr;
    m_codecCtx = nullptr;
    m_swsCtx = nullptr;
    m_opened = false;
}

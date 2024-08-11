#include "video_processor.h"
#include "constants.h"
#include <iostream>
#include <cstring>

using namespace std;

VideoProcessor::VideoProcessor()
    : fmt_ctx(nullptr), codec_ctx(nullptr), frame(nullptr),
      frame_rgb(nullptr), packet(nullptr), sws_ctx(nullptr),
      rgb_buffer(nullptr), sized_ctx(nullptr), sized_buffer(nullptr), sized_tmp(nullptr),
      video_stream_idx(-1), width(0), height(0) {
}

VideoProcessor::~VideoProcessor() {
    close();
}

bool VideoProcessor::open(const string& path) {
    cout << "[vp] opening " << path << endl;
    // open video file
    if (avformat_open_input(&fmt_ctx, path.c_str(), nullptr, nullptr) < 0) {
        cerr << "Could not open video file: " << path << endl;
        return false;
    }

    cout << "[vp] finding stream info..." << endl;
    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        cerr << "Could not find stream info" << endl;
        return false;
    }

    cout << "[vp] finding video stream..." << endl;
    // find video stream
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx = i;
            break;
        }
    }

    if (video_stream_idx == -1) {
        cerr << "No video stream found" << endl;
        return false;
    }

    // get codec
    AVCodecParameters* codecpar = fmt_ctx->streams[video_stream_idx]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        cerr << "Codec not found" << endl;
        return false;
    }

    codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, codecpar);

    if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
        cerr << "Could not open codec" << endl;
        return false;
    }

    width = codec_ctx->width;
    height = codec_ctx->height;

    frame = av_frame_alloc();
    packet = av_packet_alloc();

    return true;
}

void VideoProcessor::close() {
    if (rgb_buffer) free(rgb_buffer);
    if (sized_buffer) free(sized_buffer);
    if (sized_tmp) free(sized_tmp);
    if (sized_ctx) sws_freeContext(sized_ctx);
    if (frame_rgb) av_frame_free(&frame_rgb);
    if (sws_ctx) sws_freeContext(sws_ctx);
    if (frame) av_frame_free(&frame);
    if (packet) av_packet_free(&packet);
    if (codec_ctx) avcodec_free_context(&codec_ctx);
    if (fmt_ctx) avformat_close_input(&fmt_ctx);
}

bool VideoProcessor::read_frame() {
    while (av_read_frame(fmt_ctx, packet) >= 0) {
        if (packet->stream_index == video_stream_idx) {
            int ret = avcodec_send_packet(codec_ctx, packet);
            if (ret < 0) {
                av_packet_unref(packet);
                return false;
            }

            ret = avcodec_receive_frame(codec_ctx, frame);
            av_packet_unref(packet);

            if (ret == 0) {
                return true;
            }
        } else {
            av_packet_unref(packet);
        }
    }
    return false;
}

AVRational VideoProcessor::get_fps() {
    AVRational r = av_guess_frame_rate(fmt_ctx, fmt_ctx->streams[video_stream_idx], nullptr);
    if (r.num <= 0 || r.den <= 0) {
        r.num = OUTPUT_FPS;  // no idea, fall back to the old default
        r.den = 1;
    }
    return r;
}

double VideoProcessor::get_duration() {
    return fmt_ctx->duration / (double)AV_TIME_BASE;
}

uint8_t* VideoProcessor::get_frame_rgb_sized(int out_w, int out_h) {
    // scale so it covers the whole picture then cut off whats sticking out.
    // squishing it to fit looks wrong
    float sc = (float)out_w / width;
    if ((float)out_h / height > sc) sc = (float)out_h / height;
    int sw_ = (int)(width * sc + 0.5f), sh_ = (int)(height * sc + 0.5f);
    if (sw_ < out_w) sw_ = out_w;
    if (sh_ < out_h) sh_ = out_h;

    if (!sized_ctx) {
        sized_ctx = sws_getContext(width, height, codec_ctx->pix_fmt,
                                   sw_, sh_, AV_PIX_FMT_RGB24,
                                   SWS_BILINEAR, nullptr, nullptr, nullptr);
        sized_tmp = (uint8_t*)malloc(sw_ * sh_ * 3);
        sized_buffer = (uint8_t*)malloc(out_w * out_h * 3);
        cout << "[vp] other video " << width << "x" << height << " -> " << sw_ << "x" << sh_
             << " cut to " << out_w << "x" << out_h << endl;
    }
    uint8_t* dst[4] = {sized_tmp, nullptr, nullptr, nullptr};
    int lines[4] = {sw_ * 3, 0, 0, 0};
    sws_scale(sized_ctx, frame->data, frame->linesize, 0, height, dst, lines);

    int ox = (sw_ - out_w) / 2, oy = (sh_ - out_h) / 2;
    for (int y = 0; y < out_h; y++) {
        memcpy(sized_buffer + y * out_w * 3, sized_tmp + ((y + oy) * sw_ + ox) * 3, out_w * 3);
    }
    return sized_buffer;
}

uint8_t* VideoProcessor::get_frame_data() {
    if (frame) {
        return frame->data[0];
    }
    return nullptr;
}

uint8_t* VideoProcessor::get_frame_rgb() {
    if (!frame) return nullptr;

    // initialize converter if needed
    if (!sws_ctx) {
        sws_ctx = sws_getContext(
            width, height, codec_ctx->pix_fmt,
            width, height, AV_PIX_FMT_RGB24,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );

        if (!sws_ctx) {
            cerr << "Failed to initialize swscale context" << endl;
            return nullptr;
        }

        // allocate RGB frame and buffer
        frame_rgb = av_frame_alloc();
        int numBytes = width * height * 3;  // RGB24 = 3 bytes per pixel
        rgb_buffer = (uint8_t*)malloc(numBytes);

        // setup frame_rgb to point to rgb_buffer
        frame_rgb->data[0] = rgb_buffer;
        frame_rgb->linesize[0] = width * 3;
    }

    // convert YUV to RGB
    sws_scale(
        sws_ctx,
        frame->data, frame->linesize, 0, height,
        frame_rgb->data, frame_rgb->linesize
    );

    return rgb_buffer;
}

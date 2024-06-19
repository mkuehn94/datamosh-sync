#include "video_processor.h"
#include "constants.h"
#include <iostream>

using namespace std;

VideoProcessor::VideoProcessor()
    : fmt_ctx(nullptr), codec_ctx(nullptr), frame(nullptr),
      frame_rgb(nullptr), packet(nullptr), sws_ctx(nullptr),
      rgb_buffer(nullptr), video_stream_idx(-1), width(0), height(0) {
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

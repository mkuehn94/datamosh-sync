#include "video_processor.h"
#include <iostream>

using namespace std;

VideoProcessor::VideoProcessor()
    : fmt_ctx(nullptr), codec_ctx(nullptr), frame(nullptr),
      packet(nullptr), video_stream_idx(-1), width(0), height(0) {
}

VideoProcessor::~VideoProcessor() {
    close();
}

bool VideoProcessor::open(const string& path) {
    // open video file
    if (avformat_open_input(&fmt_ctx, path.c_str(), nullptr, nullptr) < 0) {
        cerr << "Could not open video file: " << path << endl;
        return false;
    }

    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        cerr << "Could not find stream info" << endl;
        return false;
    }

    // find video stream
    for (int i = 0; i < fmt_ctx->nb_streams; i++) {
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

uint8_t* VideoProcessor::get_frame_data() {
    if (frame) {
        return frame->data[0];
    }
    return nullptr;
}

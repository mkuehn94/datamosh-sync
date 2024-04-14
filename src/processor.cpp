// main processing loop
// combines video, audio, and effects

#include "processor.h"
#include "constants.h"
#include <iostream>
#include <cmath>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

using namespace std;

VideoMosher::VideoMosher() {
    video = nullptr;
    audio = nullptr;
    effects = nullptr;
    current_frame = 0;
    output_ctx = nullptr;
    enc_ctx = nullptr;
    out_stream = nullptr;
    enc_frame = nullptr;
    enc_sws_ctx = nullptr;
    next_pts = 0;
}

VideoMosher::~VideoMosher() {
    finish_encoding();
    if (video) delete video;
    if (audio) delete audio;
    if (effects) delete effects;
}

bool VideoMosher::init(const string& video_path, const string& audio_path) {
    video = new VideoProcessor();
    audio = new AudioAnalyzer();
    effects = new EffectEngine();

    if (!video->open(video_path)) {
        cerr << "failed to open video" << endl;
        return false;
    }

    if (!audio->load(audio_path)) {
        cerr << "failed to load audio" << endl;
        return false;
    }

    audio->analyze();

    beat_times = audio->get_beat_times();
    cout << "initialized with " << beat_times.size() << " beats" << endl;

    return true;
}

bool VideoMosher::init_with_embedded_audio(const string& video_path) {
    video = new VideoProcessor();
    audio = new AudioAnalyzer();
    effects = new EffectEngine();

    if (!video->open(video_path)) {
        cerr << "failed to open video" << endl;
        return false;
    }

    // extract audio from the same video file
    if (!audio->load_from_video(video_path)) {
        cerr << "failed to extract audio from video" << endl;
        return false;
    }

    audio->analyze();

    beat_times = audio->get_beat_times();
    cout << "initialized with " << beat_times.size() << " beats (from video)" << endl;

    return true;
}

void VideoMosher::process_frame(uint8_t* frame_data, int width, int height, float time) {
    // check if we're on a beat
    bool on_beat = false;
    float beat_strength = 0.0;

    for (float beat_time : beat_times) {
        if (abs(time - beat_time) < 0.05) {  // 50ms window
            on_beat = true;
            beat_strength = 1.0;
            break;
        }
    }

    // get current audio energy
    float energy = audio->get_current_energy(time);

    // apply effects based on parameters and audio

    // always do some motion vector glitch
    float mv_intensity = params.glitch_intensity * energy;
    effects->motion_vector_glitch(frame_data, width, height, mv_intensity);

    // jpeg corruption on beats
    if (on_beat && params.jpeg_quality < 90) {
        effects->jpeg_corruption(frame_data, width, height, params.jpeg_quality);
    }

    // color shift gradually
    if (params.color_shift > 0.1) {
        effects->color_channel_shift(frame_data, width, height, params.color_shift);
    }

    // block shuffle on strong beats
    if (beat_strength > 0.8 && params.block_shuffle > 0.2) {
        effects->block_shuffle(frame_data, width, height, params.block_shuffle);
    }

    // pulse effect when beat sync is enabled
    if (params.beat_sync > 0.5 && on_beat) {
        effects->pulse_effect(frame_data, width, height, beat_strength);
    }

    // wave distortion based on time
    if (params.motion_blur > 0.2) {
        float phase = time * 3.14;
        effects->wave_distortion(frame_data, width, height, phase);
    }
}

bool VideoMosher::process_video(const string& output_path, ProcessParams p) {
    params = p;

    cout << "processing video..." << endl;
    cout << "glitch: " << params.glitch_intensity << endl;
    cout << "jpeg: " << params.jpeg_quality << endl;
    cout << "motion blur: " << params.motion_blur << endl;
    cout << "color shift: " << params.color_shift << endl;
    cout << "block shuffle: " << params.block_shuffle << endl;
    cout << "beat sync: " << params.beat_sync << endl;

    int width = video->get_width();
    int height = video->get_height();

    // setup output encoder
    if (!setup_encoder(output_path, width, height)) {
        cerr << "Failed to setup encoder" << endl;
        return false;
    }

    float time = 0.0;
    float dt = 1.0 / OUTPUT_FPS;
    int frame_count = 0;

    while (video->read_frame()) {
        uint8_t* frame_data = video->get_frame_rgb();  // get RGB not YUV

        if (frame_data && width > 0 && height > 0) {
            process_frame(frame_data, width, height, time);
            encode_frame(frame_data);  // encode processed frame
            frame_count++;

            if (frame_count % 30 == 0) {
                cout << "processed " << frame_count << " frames" << endl;
            }
        }

        time += dt;
    }

    cout << "done processing " << frame_count << " frames" << endl;

    finish_encoding();
    cout << "wrote output to: " << output_path << endl;

    return true;
}

bool VideoMosher::setup_encoder(const string& output_path, int width, int height) {
    // create output context
    avformat_alloc_output_context2(&output_ctx, nullptr, nullptr, output_path.c_str());
    if (!output_ctx) {
        cerr << "Could not create output context" << endl;
        return false;
    }
    
    // find H264 encoder
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        cerr << "H264 encoder not found" << endl;
        return false;
    }
    
    // create stream
    out_stream = avformat_new_stream(output_ctx, nullptr);
    if (!out_stream) {
        cerr << "Failed to create stream" << endl;
        return false;
    }
    
    // allocate encoder context
    enc_ctx = avcodec_alloc_context3(codec);
    enc_ctx->width = width;
    enc_ctx->height = height;
    enc_ctx->time_base = AVRational{1, OUTPUT_FPS};
    enc_ctx->framerate = AVRational{OUTPUT_FPS, 1};
    enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    enc_ctx->bit_rate = OUTPUT_BITRATE;
    
    if (output_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    
    // open encoder
    if (avcodec_open2(enc_ctx, codec, nullptr) < 0) {
        cerr << "Failed to open encoder" << endl;
        return false;
    }
    
    avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
    out_stream->time_base = enc_ctx->time_base;
    
    // open output file
    if (!(output_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&output_ctx->pb, output_path.c_str(), AVIO_FLAG_WRITE) < 0) {
            cerr << "Could not open output file" << endl;
            return false;
        }
    }
    
    // write header
    if (avformat_write_header(output_ctx, nullptr) < 0) {
        cerr << "Error writing header" << endl;
        return false;
    }
    
    // setup frame for encoding
    enc_frame = av_frame_alloc();
    enc_frame->format = AV_PIX_FMT_YUV420P;
    enc_frame->width = width;
    enc_frame->height = height;
    av_frame_get_buffer(enc_frame, 0);
    
    // setup RGB to YUV converter
    enc_sws_ctx = sws_getContext(
        width, height, AV_PIX_FMT_RGB24,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    next_pts = 0;
    
    cout << "encoder setup complete: " << width << "x" << height << " @ " << OUTPUT_FPS << " fps" << endl;
    
    return true;
}

void VideoMosher::encode_frame(uint8_t* rgb_data) {
    if (!enc_ctx || !enc_frame || !enc_sws_ctx) return;
    
    int width = enc_ctx->width;
    int height = enc_ctx->height;
    
    // convert RGB to YUV420P
    uint8_t* src_data[1] = { rgb_data };
    int src_linesize[1] = { width * 3 };
    
    sws_scale(
        enc_sws_ctx,
        src_data, src_linesize, 0, height,
        enc_frame->data, enc_frame->linesize
    );
    
    enc_frame->pts = next_pts++;
    
    // send frame to encoder
    int ret = avcodec_send_frame(enc_ctx, enc_frame);
    if (ret < 0) {
        cerr << "Error sending frame to encoder" << endl;
        return;
    }
    
    // receive encoded packets
    AVPacket* pkt = av_packet_alloc();
    while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            cerr << "Error encoding frame" << endl;
            break;
        }
        
        av_packet_rescale_ts(pkt, enc_ctx->time_base, out_stream->time_base);
        pkt->stream_index = out_stream->index;
        
        av_interleaved_write_frame(output_ctx, pkt);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
}

void VideoMosher::finish_encoding() {
    if (!output_ctx) return;
    
    // flush encoder
    if (enc_ctx) {
        avcodec_send_frame(enc_ctx, nullptr);
        
        AVPacket* pkt = av_packet_alloc();
        int ret;
        while ((ret = avcodec_receive_packet(enc_ctx, pkt)) >= 0) {
            av_packet_rescale_ts(pkt, enc_ctx->time_base, out_stream->time_base);
            pkt->stream_index = out_stream->index;
            av_interleaved_write_frame(output_ctx, pkt);
            av_packet_unref(pkt);
        }
        av_packet_free(&pkt);
    }
    
    // write trailer
    av_write_trailer(output_ctx);
    
    // cleanup
    if (enc_sws_ctx) sws_freeContext(enc_sws_ctx);
    if (enc_frame) av_frame_free(&enc_frame);
    if (enc_ctx) avcodec_free_context(&enc_ctx);
    if (output_ctx && !(output_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&output_ctx->pb);
    }
    if (output_ctx) avformat_free_context(output_ctx);
    
    output_ctx = nullptr;
    enc_ctx = nullptr;
    enc_frame = nullptr;
    enc_sws_ctx = nullptr;
}

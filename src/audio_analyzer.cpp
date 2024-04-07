#include "audio_analyzer.h"
#include <iostream>
#include <cmath>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

using namespace std;

AudioAnalyzer::AudioAnalyzer() : duration(0), fmt_ctx(nullptr), audio_stream_idx(-1) {
}

AudioAnalyzer::~AudioAnalyzer() {
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
    }
}

bool AudioAnalyzer::load(const string& path) {
    // TODO: actually load audio file with ffmpeg
    cout << "Loading audio: " << path << endl;
    return true;
}

bool AudioAnalyzer::load_from_video(const string& video_path) {
    cout << "Extracting audio from video: " << video_path << endl;

    // open video file
    if (avformat_open_input(&fmt_ctx, video_path.c_str(), nullptr, nullptr) < 0) {
        cerr << "Could not open video file" << endl;
        return false;
    }

    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        cerr << "Could not find stream info" << endl;
        return false;
    }

    // find audio stream
    audio_stream_idx = -1;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_idx = i;
            cout << "Found audio stream at index " << i << endl;
            break;
        }
    }

    if (audio_stream_idx == -1) {
        cerr << "No audio stream found in video" << endl;
        return false;
    }

    // get duration
    duration = fmt_ctx->duration / (float)AV_TIME_BASE;
    cout << "Audio duration: " << duration << " seconds" << endl;

    return true;
}

void AudioAnalyzer::analyze() {
    // very simple beat detection
    // TODO: make this better

    cout << "Analyzing audio..." << endl;

    // fake some beats for now
    for (float t = 0.5; t < 10.0; t += 0.5) {
        beat_times.push_back(t);
    }

    cout << "Found " << beat_times.size() << " beats" << endl;
}

float AudioAnalyzer::get_current_energy(float time) {
    // return fake energy for now
    return 0.5 + 0.5 * sin(time * 2.0);
}

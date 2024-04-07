// main processing loop
// combines video, audio, and effects

#include "processor.h"
#include "constants.h"
#include <iostream>
#include <cmath>

using namespace std;

VideoMosher::VideoMosher() {
    video = nullptr;
    audio = nullptr;
    effects = nullptr;
    current_frame = 0;
    output_ctx = nullptr;
}

VideoMosher::~VideoMosher() {
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

    // TODO: setup output encoder
    // for now just process frames

    float time = 0.0;
    float dt = 1.0 / OUTPUT_FPS;
    int frame_count = 0;

    while (video->read_frame()) {
        uint8_t* frame_data = video->get_frame_data();
        int width = video->get_width();
        int height = video->get_height();

        if (frame_data) {
            process_frame(frame_data, width, height, time);
            frame_count++;

            if (frame_count % 30 == 0) {
                cout << "processed " << frame_count << " frames" << endl;
            }
        }

        time += dt;
    }

    cout << "done processing " << frame_count << " frames" << endl;

    // TODO: write output file

    return true;
}

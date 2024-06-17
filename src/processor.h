#ifndef PROCESSOR_H
#define PROCESSOR_H

#include "video_processor.h"
#include "audio_analyzer.h"
#include "effects.h"
#include "constants.h"
#include <string>
#include <vector>

struct ProcessParams {
    float glitch_intensity;
    float jpeg_quality;
    float motion_blur;
    float color_shift;
    float block_shuffle;
    float beat_sync;
};

class VideoMosher {
public:
    VideoMosher();
    ~VideoMosher();

    bool init(const std::string& video_path, const std::string& audio_path);
    bool init_with_embedded_audio(const std::string& video_path);  // use video's audio track
    bool process_video(const std::string& output_path, ProcessParams params);

private:
    // returns true if this frame landed on a beat
    bool process_frame(uint8_t* frame_data, int width, int height, float time);
    bool setup_encoder(const std::string& output_path, int width, int height);
    void encode_frame(uint8_t* rgb_data);
    void finish_encoding();

    // dumb block-matching motion tracker. finds whatever keeps moving in
    // the same area (a car, a person) so we can mess with it specifically
    void motion_track(uint8_t* frame_data, int width, int height);
    // bool copy_audio_stream();  // REMOVED - was causing issues

    VideoProcessor* video;
    AudioAnalyzer* audio;
    EffectEngine* effects;

    ProcessParams params;
    std::vector<float> beat_times;
    int current_frame;
    uint8_t* prev_rgb;    // for trails effect
    int prev_rgb_size;
    uint8_t* ghost_buf;   // persistent ghost layer - never cleared, only stamped into
    int ghost_buf_size;
    uint8_t* held_frame;  // copy of last encoded frame, for the freeze-hold mosh
    int held_frame_size;
    float last_energy;    // audio energy at current frame, saved for the hold logic

    // object tracking state
    int track_x, track_y, track_w, track_h;  // smoothed bbox of "the object"
    int track_persistence;                    // frames it's been alive
    float track_dx, track_dy;                 // smoothed motion direction
    uint8_t* history[HISTORY_LEN];            // ring buffer of processed frames
    int hist_pos;
    bool have_prev;                           // prev_rgb valid yet?

    // world merging - last frame of the previous scene, bleeds into the new one
    uint8_t* world_buf;
    int world_buf_size;
    float world_fade;                         // 1.0 right after a cut, decays slowly
    bool scene_cut;                           // motion_track sets this

    // stuck overlay state - frozen patch that refuses to update
    uint8_t* stuck_buf;
    int stuck_x, stuck_y, stuck_w, stuck_h;
    int stuck_timer;                          // frames left frozen, 0 = not stuck

    // void state - the color being keyed out (smoothed, else it flickers)
    float void_r, void_g, void_b;
    bool void_color_init;
    int void_off_x, void_off_y;               // where the void looks into

    AVFormatContext* output_ctx;
    AVCodecContext* enc_ctx;
    AVStream* out_stream;
    // AVStream* audio_out_stream;  // REMOVED
    AVFrame* enc_frame;
    struct SwsContext* enc_sws_ctx;
    int64_t next_pts;

    // std::string input_video_path;  // REMOVED
};

#endif

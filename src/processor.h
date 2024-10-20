#ifndef PROCESSOR_H
#define PROCESSOR_H

#include "video_processor.h"
#include "audio_analyzer.h"
#include "effects.h"
#include "constants.h"
#include "warp.h"
#include "arrange.h"
#include <string>
#include <vector>

struct ProcessParams {
    float glitch_intensity;
    float jpeg_quality;
    float motion_blur;
    float color_shift;
    float block_shuffle;
    float beat_sync;
    float rot;       // decoder that lost its keyframes
    float warp;      // hijacked motion vectors, gravity wells
    float quake;     // blocks living in different moments
    float spectral;  // frequencies living in different moments
    float groove;    // motion sampler, replays the vectors of the last bar
    float texswap;   // blocks wearing the texture of other blocks
    float recursion; // tiny copies of the frame inside the frame
    float hom;       // hall of mirrors, the noclip screen that never clears
    float parts[FX_COUNT];  // how much of the song every effect plays, 1 = always
    float swing;            // rise and fall with the music, 0 = sliders stay flat
    float cross;            // how much of the picture belongs to the other video
};

class VideoMosher {
public:
    VideoMosher();
    ~VideoMosher();

    bool init(const std::string& video_path, const std::string& audio_path);
    bool init_with_embedded_audio(const std::string& video_path);  // use video's audio track
    bool process_video(const std::string& output_path, ProcessParams params);
    bool open_other(const std::string& path);  // second video, optional
    float song_limit;  // seconds, 0 = whole thing

private:
    // returns true if this frame landed on a beat
    bool process_frame(uint8_t* frame_data, int width, int height, float time);
    void play_section(float time);  // which effects are on right now
    void swing_params(float time);  // slider = mean, music moves it
    bool setup_encoder(const std::string& output_path, int width, int height, AVRational rate);
    void encode_frame(uint8_t* rgb_data);
    void finish_encoding();

    // dumb block-matching motion tracker. finds whatever keeps moving in
    // the same area (a car, a person) so we can mess with it specifically
    void motion_track(uint8_t* frame_data, int width, int height);
    // bool copy_audio_stream();  // REMOVED - was causing issues

    VideoProcessor* video;
    VideoProcessor* other;     // second video or null
    WarpEngine* other_eyes;    // only here to measure the motion of the other video
    uint8_t* other_frame;      // this frame of it, already the right size
    float dim_punch;           // kicks rip the other world open, falls off
    long a_read, b_read;
    bool catch_up(VideoProcessor* v, long& read, double src_fps, long frame_index, double out_fps);
    AudioAnalyzer* audio;
    EffectEngine* effects;
    WarpEngine* warp;
    bool last_on_beat;

    ProcessParams params;       // what the effects read, gets zeroed when out of a section
    ProcessParams slider_params;  // what the user actually set
    ProcessParams swung_params;   // that + the music moving it around. this frame
    std::vector<float> intensity;
    double swing_sum[FX_COUNT], swing_sq[FX_COUNT];  // to print what really happened at the end
    long swing_frames;
    std::vector<Section> sections;
    bool use_parts;
    bool fx_was_on[FX_COUNT];
    bool stuck_allowed;
    std::string source_path;
    std::vector<float> beat_times;
    int current_frame;
    float fps;            // of the source, NOT always 30
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
    int stuck_wait;                           // frames its been waiting for a beat to let go
    float last_treble;
    int glitch_left;                          // frames of glitch left from the last hat

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

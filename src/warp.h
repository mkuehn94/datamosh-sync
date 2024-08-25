#ifndef WARP_H
#define WARP_H

#include <cstdint>
#include <vector>

// everything in here pretends to be a video decoder that has gone wrong.
// the old effects push pixels around, this one has actual motion vectors
// and residuals and dct coefficients and then lies about them
// quake = blocks in different moments, spectral = FREQUENCIES in different moments

struct AudioFrame {
    float energy;
    float bass;
    float treble;
    bool kick;   // only true on the first frame of a beat
    float hit;   // how strong that kick is. NOT the same as bass, see processor.cpp
    bool cut;    // scene changed
};

class WarpEngine {
public:
    WarpEngine();
    ~WarpEngine();

    void init(int w, int h, float fps);

    // has to get the CLEAN frame, before any effect touched it
    void estimate_motion(const uint8_t* rgb);
    bool scene_cut() { return cut_detected; }

    // second video. eyes = other engine that ran estimate_motion on it, amount = how
    // much of the picture listens to it. null = just the one video like before
    void look_at_other(const uint8_t* rgb, WarpEngine* eyes, float amount);

    void rot_and_warp(uint8_t* data, float rot, float warp, float groove, const AudioFrame& a);
    void block_recursion(uint8_t* data, float amount, const AudioFrame& a);
    void texture_swap(uint8_t* data, float amount, const AudioFrame& a);
    void hall_of_mirrors(uint8_t* data, float amount, const AudioFrame& a);
    void time_quake(uint8_t* data, float amount, const AudioFrame& a);
    void spectral_lag(uint8_t* data, float amount, const AudioFrame& a);

    // effect comes back after its section was off. these keep old pictures
    // around and they would be from a minute ago
    void keep_quake_tape() { quake_tape = true; }  // keeps recording while its off
    void wake_quake();
    void wake_spectral();
    void wake_recursion();
    void wake_mirrors();

private:
    // gravity well = a spot where the motion vectors go wrong
    struct Well {
        float x, y, radius;
        float pull;   // + sucks picture in, - spits it out
        float spin;
        float age, life;
    };
    // shockwave through time, spawned on kicks
    struct Ring {
        float x, y, r, speed, strength;
    };

    void build_warp_field(float warp, const AudioFrame& a);
    void update_sampler(const AudioFrame& a);
    void start_refresh();

    int w, h, gx, gy;
    float fps, dt;
    float scale;  // effects were tuned on 1080 tall, everything in pixels gets this

    // motion estimation
    uint8_t* luma, *prev_luma, *small, *prev_small;
    int sw, sh;
    std::vector<short> mvx, mvy;
    std::vector<int> block_err;   // avg abs luma difference per pixel after matching
    bool have_prev_luma;
    bool cut_detected;

    // rot
    uint8_t* canvas, *canvas_tmp, *prev_src;
    bool canvas_ready;
    std::vector<float> fx, fy;           // warp vectors, pixels/frame
    std::vector<float> rem_x, rem_y;     // leftover subpixel part
    std::vector<Well> wells;
    float tunnel;                        // global warp speed, kicked by bass
    float van_x, van_y;                  // vanishing point
    float refresh_t;                     // >=0 = keyframe wipe in progress
    int refresh_kind;
    float refresh_x, refresh_y;
    int kicks_since_refresh;

    // the other video
    const uint8_t* other_rgb;
    WarpEngine* other_eyes;
    float other_amount;
    uint8_t* other_prev;
    int other_bar, other_kicks;  // which chunks belong to it changes every few kicks
    std::vector<float> other_field;
    int other_field_bar;
    unsigned other_seed;
    void make_other_field();

    // quake
    std::vector<uint8_t*> ring_frames;
    bool quake_tape;
    int quake_levels;
    int ring_pos, ring_count;
    std::vector<float> depth;            // seconds behind the present, per block
    std::vector<Ring> rings;

    // spectral
    std::vector<float> coef;             // 64 per 8x8 block
    bool coef_ready;
    bool spectral_flip;
    int spectral_kicks;
    unsigned frame_no;

    unsigned warp_seed, rot_seed, quake_seed;

    // motion sampler. records the vectors of one bar and plays them back
    // like a drum loop
    std::vector<std::vector<short> > rec_x, rec_y, loop_x, loop_y;
    std::vector<int> rec_kicks, loop_kicks;   // which frames had a kick
    int bar_kicks, loop_pos, loop_slice, bar_no;
    float groove_punch;  // 1 right on a kick then falls off

    // recursion
    struct Cell {
        int x, y, cw, ch;
        int tiles;
        float age, life;
    };
    std::vector<Cell> cells;
    uint8_t* recur_prev;
    unsigned recur_seed;

    // texture swap
    std::vector<int> tex_off_x, tex_off_y;   // per chunk, where it steals from
    int tex_kicks;
    float frame_crawl;
    unsigned tex_seed;

    // hall of mirrors, the noclip thing. a frame buffer that nobody clears
    uint8_t* hom_buf;
    uint8_t* hom_ref;               // guess of what the world looks like without the objects
    uint8_t* hom_ref_tmp;
    std::vector<float> hom_age;     // per chunk, how long since it was cleared
    unsigned hom_seed;
};

#endif

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

    void rot_and_warp(uint8_t* data, float rot, float warp, const AudioFrame& a);
    void time_quake(uint8_t* data, float amount, const AudioFrame& a);
    void spectral_lag(uint8_t* data, float amount, const AudioFrame& a);

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

    // quake
    std::vector<uint8_t*> ring_frames;
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
};

#endif

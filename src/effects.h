#ifndef EFFECTS_H
#define EFFECTS_H

#include <cstdint>
#include <vector>

// datamosh effect engine
class EffectEngine {
public:
    EffectEngine();
    ~EffectEngine();

    // various crazy effects
    void jpeg_corruption(uint8_t* data, int width, int height, float quality);
    void motion_vector_glitch(uint8_t* data, int width, int height, float intensity);
    void color_channel_shift(uint8_t* data, int width, int height, float amount);
    void block_shuffle(uint8_t* data, int width, int height, float intensity);
    void repeat_pframes(uint8_t* data, int width, int height, int repeat_count);
    void remove_iframes(uint8_t* data, int width, int height);

    // beat-synced effects
    void pulse_effect(uint8_t* data, int width, int height, float beat_strength);
    void wave_distortion(uint8_t* data, int width, int height, float phase);

    // color palette swap
    void color_remap(uint8_t* data, int width, int height, int mode);

    // extra effects
    void scanlines(uint8_t* data, int width, int height, int intensity);
    void pixelate(uint8_t* data, int width, int height, int block);
    void trails(uint8_t* data, int width, int height, uint8_t* prev_frame, float amount);
    void glitch_bands(uint8_t* data, int width, int height, float intensity);  // horizontal band glitches
    void datamosh_streak(uint8_t* data, int width, int height, int seed, int streak_len);  // copy block from elsewhere, like real datamosh

    // color encoding corruption - scambles YCbCr mapping so codec bakes in wrong colors
    // do this BEFORE encoding, the compression then amplifies it
    void color_encode_corrupt(uint8_t* data, int width, int height, float intensity);

    // ghost accumulation - noclip effect
    // stamps blocks from current frame into ghost layer (never cleared)
    // ghost layer bleeds into output each frame
    void ghost_stamp(uint8_t* data, int width, int height,
                     uint8_t* ghost, int num_stamps, int frame_count);
    void ghost_composite(uint8_t* data, int width, int height,
                         uint8_t* ghost, float strength);

private:
    uint8_t* temp_buffer;
    int buffer_size;
};

#endif

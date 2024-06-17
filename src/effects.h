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

    // persistent object effects - for when the tracker found something
    // that's been moving in the same area for a while
    // echo_region: copy the region from an OLD frame back in, object gets a twin
    // vector_smear: drag the region along its own motion, leading edge stretches
    // ghost_stamp_region: stamp the region into the ghost layer along its path
    void echo_region(uint8_t* data, int width, int height,
                     uint8_t* old_frame,
                     int rx, int ry, int rw, int rh,
                     int off_x, int off_y, float amount);
    void vector_smear(uint8_t* data, int width, int height,
                      int rx, int ry, int rw, int rh,
                      float dx, float dy, float amount);
    void ghost_stamp_region(uint8_t* data, int width, int height,
                            uint8_t* ghost,
                            int rx, int ry, int rw, int rh,
                            float dx, float dy, int num);

    // world merge - shadows of the new scene open up into the old one.
    // feed it the last frame of the previous scene + a fade factor
    void world_merge(uint8_t* data, int width, int height,
                     uint8_t* old_world, float fade);

    // mask composite - ghost layer shows up as SOLID objects sitting in
    // the frame, not bright haze. this is what makes stacks look stacked
    void mask_composite(uint8_t* data, int width, int height,
                        uint8_t* ghost, float strength);

    // stuck overlay - the classic broken-stream artifact. grab a rect,
    // freeze it, paste it over the live video while everything behind
    // it keeps moving. grab once, paste every frame, then release
    void grab_rect(uint8_t* data, int width, int height,
                   uint8_t* out, int rx, int ry, int rw, int rh);
    void paste_rect(uint8_t* data, int width, int height,
                    uint8_t* patch, int rx, int ry, int rw, int rh);

    // VOID - every pixel matching a color gets replaced by a chunk of
    // the same frame from somewhere else. holes into the wrong reality
    void void_replace(uint8_t* data, int width, int height,
                      int tr, int tg, int tb, int tolerance,
                      int off_x, int off_y, float amount);

private:
    uint8_t* temp_buffer;
    int buffer_size;
};

#endif

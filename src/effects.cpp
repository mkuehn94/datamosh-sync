#include "effects.h"
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <iostream>

using namespace std;

EffectEngine::EffectEngine() {
    temp_buffer = NULL;
    buffer_size = 0;
}

EffectEngine::~EffectEngine() {
    if (temp_buffer) {
        free(temp_buffer);
    }
}

void EffectEngine::jpeg_corruption(uint8_t* data, int width, int height, float quality) {
    // Legacy placeholder: XOR noise on decoded RGB, not JPEG/DCT corruption.

    int size = width * height * 3;

    if (!temp_buffer || buffer_size < size) {
        if (temp_buffer) free(temp_buffer);
        temp_buffer = (uint8_t*)malloc(size);
        buffer_size = size;
    }

    // TODO: actually implement jpeg corruption
    // for now just add some noise
    for (int i = 0; i < size; i++) {
        if (rand() % 100 < quality) {
            data[i] = data[i] ^ (rand() % 32);
        }
    }
}

void EffectEngine::motion_vector_glitch(uint8_t* data, int width, int height, float intensity) {
    // Random block displacement on decoded RGB; does not edit encoded vectors.

    int block_size = 16;

    for (int y = 0; y < height; y += block_size) {
        for (int x = 0; x < width; x += block_size) {
            if ((float)rand() / RAND_MAX < intensity) {
                // randomly offset this block
                int offset_x = (rand() % 40) - 20;
                int offset_y = (rand() % 40) - 20;

                int src_x = x + offset_x;
                int src_y = y + offset_y;

                if (src_x >= 0 && src_x < width - block_size &&
                    src_y >= 0 && src_y < height - block_size) {

                    // copy block
                    for (int by = 0; by < block_size; by++) {
                        int dst_idx = ((y + by) * width + x) * 3;
                        int src_idx = ((src_y + by) * width + src_x) * 3;
                        memcpy(&data[dst_idx], &data[src_idx], block_size * 3);
                    }
                }
            }
        }
    }
}

void EffectEngine::color_channel_shift(uint8_t* data, int width, int height, float amount) {
    int shift = (int)(amount * 20);

    int size = width * height;

    // shift red channel
    for (int i = size - 1; i >= shift; i--) {
        data[i * 3] = data[(i - shift) * 3];
    }
}

void EffectEngine::block_shuffle(uint8_t* data, int width, int height, float intensity) {
    // shuffle macroblocks around
    int block_size = 16;
    int blocks_x = width / block_size;
    int blocks_y = height / block_size;

    for (int i = 0; i < (int)(blocks_x * blocks_y * intensity); i++) {
        int bx1 = rand() % blocks_x;
        int by1 = rand() % blocks_y;
        int bx2 = rand() % blocks_x;
        int by2 = rand() % blocks_y;

        // swap blocks
        // TODO: optimize this
        for (int y = 0; y < block_size; y++) {
            int idx1 = ((by1 * block_size + y) * width + bx1 * block_size) * 3;
            int idx2 = ((by2 * block_size + y) * width + bx2 * block_size) * 3;

            for (int x = 0; x < block_size * 3; x++) {
                uint8_t tmp = data[idx1 + x];
                data[idx1 + x] = data[idx2 + x];
                data[idx2 + x] = tmp;
            }
        }
    }
}

void EffectEngine::repeat_pframes(uint8_t* data, int width, int height, int repeat_count) {
    // this needs to work on encoded stream level
    // TODO: implement properly
    cout << "repeat_pframes not implemented yet" << endl;
}

void EffectEngine::remove_iframes(uint8_t* data, int width, int height) {
    // remove I-frames to create datamosh trails
    // TODO: implement
    cout << "remove_iframes not implemented yet" << endl;
}

void EffectEngine::pulse_effect(uint8_t* data, int width, int height, float beat_strength) {
    // scale effect based on beat
    float scale = 1.0 + beat_strength * 0.2;

    int center_x = width / 2;
    int center_y = height / 2;

    // radial zoom effect (simple version)
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int dx = x - center_x;
            int dy = y - center_y;

            int src_x = center_x + (int)(dx / scale);
            int src_y = center_y + (int)(dy / scale);

            if (src_x >= 0 && src_x < width && src_y >= 0 && src_y < height) {
                int dst_idx = (y * width + x) * 3;
                int src_idx = (src_y * width + src_x) * 3;

                data[dst_idx + 0] = data[src_idx + 0];
                data[dst_idx + 1] = data[src_idx + 1];
                data[dst_idx + 2] = data[src_idx + 2];
            }
        }
    }
}

void EffectEngine::wave_distortion(uint8_t* data, int width, int height, float phase) {
    // wavy distortion effect

    for (int y = 0; y < height; y++) {
        int offset = (int)(sin(y * 0.1 + phase) * 10);

        if (offset > 0) {
            for (int x = width - 1; x >= offset; x--) {
                int dst_idx = (y * width + x) * 3;
                int src_idx = (y * width + x - offset) * 3;

                data[dst_idx + 0] = data[src_idx + 0];
                data[dst_idx + 1] = data[src_idx + 1];
                data[dst_idx + 2] = data[src_idx + 2];
            }
        }
    }
}

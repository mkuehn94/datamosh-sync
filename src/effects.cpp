#include "effects.h"
#include "constants.h"
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
    int size = width * height * 3;

    // need a temp buffer or we read/write same memory = corruption
    uint8_t* tmp = (uint8_t*)malloc(size);
    memcpy(tmp, data, size);

    // radial zoom effect
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int dx = x - center_x;
            int dy = y - center_y;

            int src_x = center_x + (int)(dx / scale);
            int src_y = center_y + (int)(dy / scale);

            if (src_x >= 0 && src_x < width && src_y >= 0 && src_y < height) {
                int dst_idx = (y * width + x) * 3;
                int src_idx = (src_y * width + src_x) * 3;

                data[dst_idx + 0] = tmp[src_idx + 0];
                data[dst_idx + 1] = tmp[src_idx + 1];
                data[dst_idx + 2] = tmp[src_idx + 2];
            }
        }
    }
    free(tmp);
}

void EffectEngine::wave_distortion(uint8_t* data, int width, int height, float phase) {
    // wavy distortion effect
    int size = width * height * 3;
    uint8_t* tmp = (uint8_t*)malloc(size);
    memcpy(tmp, data, size);

    for (int y = 0; y < height; y++) {
        int offset = (int)(sin(y * 0.1 + phase) * 10);

        for (int x = 0; x < width; x++) {
            int src_x = x + offset;
            if (src_x < 0) src_x = 0;
            if (src_x >= width) src_x = width - 1;

            int dst_idx = (y * width + x) * 3;
            int src_idx = (y * width + src_x) * 3;

            data[dst_idx + 0] = tmp[src_idx + 0];
            data[dst_idx + 1] = tmp[src_idx + 1];
            data[dst_idx + 2] = tmp[src_idx + 2];
        }
    }
    free(tmp);
}

void EffectEngine::color_remap(uint8_t* data, int width, int height, int mode) {
    // swap color channels or remap palettes
    // mode 0 = RGB to BGR
    // mode 1 = RGB to GBR  
    // mode 2 = invert colors
    // mode 3 = psychedelic (weird math on colors)
    
    int size = width * height;
    
    for (int i = 0; i < size; i++) {
        int idx = i * 3;
        uint8_t r = data[idx + 0];
        uint8_t g = data[idx + 1];
        uint8_t b = data[idx + 2];
        
        switch(mode) {
            case 0: // BGR
                data[idx + 0] = b;
                data[idx + 2] = r;
                break;
            case 1: // GBR rotate
                data[idx + 0] = g;
                data[idx + 1] = b;
                data[idx + 2] = r;
                break;
            case 2: // invert
                data[idx + 0] = 255 - r;
                data[idx + 1] = 255 - g;
                data[idx + 2] = 255 - b;
                break;
            case 3: // psychedelic
                data[idx + 0] = (r + 128) % 256;
                data[idx + 1] = (g * 2) % 256;
                data[idx + 2] = (b ^ 128);
                break;
            case 4: // heat map kinda
                // map brightness to heat colors
                int brightness = (r + g + b) / 3;
                if (brightness < 85) {
                    data[idx + 0] = brightness * 3;
                    data[idx + 1] = 0;
                    data[idx + 2] = 0;
                } else if (brightness < 170) {
                    data[idx + 0] = 255;
                    data[idx + 1] = (brightness - 85) * 3;
                    data[idx + 2] = 0;
                } else {
                    data[idx + 0] = 255;
                    data[idx + 1] = 255;
                    data[idx + 2] = (brightness - 170) * 3;
                }
                break;
        }
    }
}

void EffectEngine::scanlines(uint8_t* data, int width, int height, int intensity) {
    // dark horizontal lines like an old CRT
    for (int y = 0; y < height; y += 2) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 3;
            data[idx+0] = data[idx+0] * (100 - intensity) / 100;
            data[idx+1] = data[idx+1] * (100 - intensity) / 100;
            data[idx+2] = data[idx+2] * (100 - intensity) / 100;
        }
    }
}

void EffectEngine::pixelate(uint8_t* data, int width, int height, int block) {
    if (block < 2) return;
    for (int y = 0; y < height; y += block) {
        for (int x = 0; x < width; x += block) {
            // sample top-left pixel of block
            int idx = (y * width + x) * 3;
            uint8_t r = data[idx+0];
            uint8_t g = data[idx+1];
            uint8_t b = data[idx+2];
            // fill block
            for (int dy = 0; dy < block && y+dy < height; dy++) {
                for (int dx = 0; dx < block && x+dx < width; dx++) {
                    int i = ((y+dy) * width + (x+dx)) * 3;
                    data[i+0] = r;
                    data[i+1] = g;
                    data[i+2] = b;
                }
            }
        }
    }
}

void EffectEngine::trails(uint8_t* data, int width, int height, uint8_t* prev_frame, float amount) {
    if (!prev_frame) return;
    // blend current frame with previous - creates motion trails
    int size = width * height * 3;
    for (int i = 0; i < size; i++) {
        data[i] = (uint8_t)(data[i] * (1.0f - amount) + prev_frame[i] * amount);
    }
}

void EffectEngine::glitch_bands(uint8_t* data, int width, int height, float intensity) {
    // randomly shift horizontal bands left/right
    // looks more like real datamosh corruption
    int num_bands = (int)(intensity * 20);
    for (int i = 0; i < num_bands; i++) {
        int y = rand() % height;
        int band_h = rand() % 30 + 5;
        int shift = (rand() % 80) - 40;  // -40 to +40 px

        for (int dy = 0; dy < band_h && y + dy < height; dy++) {
            int row = y + dy;
            if (shift > 0) {
                for (int x = width - 1; x >= shift; x--) {
                    int dst = (row * width + x) * 3;
                    int src = (row * width + x - shift) * 3;
                    data[dst+0] = data[src+0];
                    data[dst+1] = data[src+1];
                    data[dst+2] = data[src+2];
                }
            } else if (shift < 0) {
                for (int x = 0; x < width + shift; x++) {
                    int dst = (row * width + x) * 3;
                    int src = (row * width + x - shift) * 3;
                    data[dst+0] = data[src+0];
                    data[dst+1] = data[src+1];
                    data[dst+2] = data[src+2];
                }
            }
        }
    }
}

void EffectEngine::datamosh_streak(uint8_t* data, int width, int height, int seed, int streak_len) {
    // pick a random macroblock and repeat it across the frame
    // this is closer to real datamosh P-frame corruption
    srand(seed);
    int block = MACROBLOCK_SIZE;
    int bx = (rand() % (width  / block)) * block;
    int by = (rand() % (height / block)) * block;

    int num_streaks = 3 + rand() % 5;
    for (int s = 0; s < num_streaks; s++) {
        int dst_bx = (rand() % (width  / block)) * block;
        int dst_by = (rand() % (height / block)) * block;

        for (int rep = 0; rep < streak_len; rep++) {
            int dst_x = dst_bx + rep * block;
            if (dst_x + block > width) break;

            for (int dy = 0; dy < block && by + dy < height; dy++) {
                for (int dx = 0; dx < block && bx + dx < width; dx++) {
                    int src = ((by + dy) * width + (bx + dx)) * 3;
                    int dst = ((dst_by + dy) * width + (dst_x + dx)) * 3;
                    if (dst + 2 < width * height * 3)
                        memcpy(&data[dst], &data[src], 3);
                }
            }
        }
    }
}

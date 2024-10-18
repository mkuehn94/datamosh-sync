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

// ========= COLOR ENCODING CORRUPTION =========
// mess with colors in ways that the codec then bakes in as "wrong"
// the H264 encoder + YUV420P subsampling amplifies this
void EffectEngine::color_encode_corrupt(uint8_t* data, int width, int height, float intensity) {
    if (intensity <= 0.01f) return;

    int block = MACROBLOCK_SIZE;
    // iterate per macroblock
    for (int by = 0; by < height; by += block) {
        for (int bx = 0; bx < width; bx += block) {
            // each block gets a random corruption based on intensity
            if ((float)rand() / RAND_MAX > intensity) continue;

            int bh = (by + block > height) ? height - by : block;
            int bw = (bx + block > width)  ? width  - bx : block;

            // pick a corruption mode per block, makes it look like codec chroma errors
            int mode = rand() % 6;

            for (int y = 0; y < bh; y++) {
                for (int x = 0; x < bw; x++) {
                    int idx = ((by + y) * width + (bx + x)) * 3;
                    uint8_t r = data[idx + 0];
                    uint8_t g = data[idx + 1];
                    uint8_t b = data[idx + 2];

                    switch (mode) {
                        case 0: // swap R/B - looks like Cb/Cr swap in encoded stream
                            data[idx + 0] = b;
                            data[idx + 2] = r;
                            break;
                        case 1: // shift R up, B down - luma/chroma mismatch
                            data[idx + 0] = (uint8_t)(r >> 2);  // posterize red
                            data[idx + 2] = (uint8_t)(b & 0xC0);  // crush blue to 2 bits
                            break;
                        case 2: // full posterize - hard color bands, codec preserves these
                            data[idx + 0] = (uint8_t)(r & 0xE0);
                            data[idx + 1] = (uint8_t)(g & 0xE0);
                            data[idx + 2] = (uint8_t)(b & 0xE0);
                            break;
                        case 3: // invert chroma only - looks like a color negative
                            data[idx + 0] = (uint8_t)(255 - r);
                            data[idx + 2] = (uint8_t)(255 - b);
                            break;
                        case 4: // amplify - swap green to red, crush others
                            data[idx + 0] = g;
                            data[idx + 1] = (uint8_t)(g >> 1);
                            data[idx + 2] = (uint8_t)(b >> 3);
                            break;
                        case 5: // XOR noise on color - looks like chroma corruption
                            data[idx + 0] = r ^ 0x55;
                            data[idx + 1] = g ^ 0xAA;
                            data[idx + 2] = b ^ 0x55;
                            break;
                    }
                }
            }
        }
    }
}

// ========= GHOST ACCUMULATION (noclip effect) =========
// stamps blocks from current frame into ghost buffer at offset positions
// ghost buffer NEVER clears, so old stamps persist forever
// this builds up the "same object rendered many times" look
void EffectEngine::ghost_stamp(uint8_t* data, int width, int height,
                                uint8_t* ghost, int num_stamps, int frame_count) {
    if (!ghost) return;

    int block = MACROBLOCK_SIZE;
    // seed per frame so we get variety but deterministic per frame
    srand(frame_count * 1337 + 42);

    for (int s = 0; s < num_stamps; s++) {
        // pick source block
        int src_bx = (rand() % (width  / block)) * block;
        int src_by = (rand() % (height / block)) * block;
        // pick destination in ghost buffer (offset so it moves over time)
        int dx_off = (rand() % 60) - 30;
        int dy_off = (rand() % 40) - 20;
        int dst_bx = src_bx + dx_off;
        int dst_by = src_by + dy_off;

        if (dst_bx < 0) dst_bx = 0;
        if (dst_by < 0) dst_by = 0;
        if (dst_bx + block > width)  dst_bx = width  - block;
        if (dst_by + block > height) dst_by = height - block;

        // copy block from data to ghost (overwrite, never clear)
        for (int y = 0; y < block && src_by + y < height && dst_by + y < height; y++) {
            int src_row = (src_by + y) * width + src_bx;
            int dst_row = (dst_by + y) * width + dst_bx;
            memcpy(&ghost[dst_row * 3], &data[src_row * 3], block * 3);
        }
    }
}

// blend ghost layer onto output
void EffectEngine::ghost_composite(uint8_t* data, int width, int height,
                                    uint8_t* ghost, float strength) {
    if (!ghost || strength <= 0.01f) return;

    int size = width * height * 3;
    for (int i = 0; i < size; i++) {
        // additive blend - ghosts brighten the image, look like persistent echoes
        int v = data[i] + (int)(ghost[i] * strength);
        if (v > 255) v = 255;
        data[i] = (uint8_t)v;
    }
}

// ========= ECHO REGION (time delay twin) =========
// copies a rectangle from an OLD frame into the current one, offset a bit.
// if the old frame came from the processed history you get feedback -
// echoes of echoes of echoes. object drags its own past behind it
void EffectEngine::echo_region(uint8_t* data, int width, int height,
                               uint8_t* old_frame,
                               int rx, int ry, int rw, int rh,
                               int off_x, int off_y, float amount) {
    if (!old_frame || rw <= 0 || rh <= 0) return;
    if (amount > 1.0f) amount = 1.0f;

    for (int y = 0; y < rh; y++) {
        int src_y = ry + y;
        int dst_y = ry + y + off_y;
        if (src_y < 0 || src_y >= height) continue;
        if (dst_y < 0 || dst_y >= height) continue;

        for (int x = 0; x < rw; x++) {
            int src_x = rx + x;
            int dst_x = rx + x + off_x;
            if (src_x < 0 || src_x >= width) continue;
            if (dst_x < 0 || dst_x >= width) continue;

            int si = (src_y * width + src_x) * 3;
            int di = (dst_y * width + dst_x) * 3;

            // blend, not overwrite - hard edges look pasted on
            data[di+0] = (uint8_t)(data[di+0] * (1.0f - amount) + old_frame[si+0] * amount);
            data[di+1] = (uint8_t)(data[di+1] * (1.0f - amount) + old_frame[si+1] * amount);
            data[di+2] = (uint8_t)(data[di+2] * (1.0f - amount) + old_frame[si+2] * amount);
        }
    }
}

// ========= VECTOR SMEAR =========
// drags a region's pixels along a direction. pixels closer to the leading
// edge get dragged harder, so the object stretches out along its own path.
// looks like the codec's motion prediction gave up on this thing specifically
void EffectEngine::vector_smear(uint8_t* data, int width, int height,
                                int rx, int ry, int rw, int rh,
                                float dx, float dy, float amount) {
    if (rw <= 0 || rh <= 0 || amount <= 0.01f) return;
    if (amount > 1.0f) amount = 1.0f;

    // clamp region to frame
    if (rx < 0) { rw += rx; rx = 0; }
    if (ry < 0) { rh += ry; ry = 0; }
    if (rx + rw > width)  rw = width  - rx;
    if (ry + rh > height) rh = height - ry;
    if (rw <= 0 || rh <= 0) return;

    uint8_t* tmp = (uint8_t*)malloc(rw * rh * 3);
    for (int y = 0; y < rh; y++) {
        memcpy(&tmp[y * rw * 3], &data[((ry + y) * width + rx) * 3], rw * 3);
    }

    float mag = sqrtf(dx * dx + dy * dy);

    for (int y = 0; y < rh; y++) {
        for (int x = 0; x < rw; x++) {
            // how far along the motion this pixel sits (0 = trailing, 1 = leading)
            float along = 0.5f;
            if (mag > 0.01f) {
                along = ((x - rw / 2) * dx / mag + (y - rh / 2) * dy / mag)
                        / (float)(rw + rh) + 0.5f;
                if (along < 0.0f) along = 0.0f;
                if (along > 1.0f) along = 1.0f;
            }

            int sx = x - (int)(dx * (0.5f + along));
            int sy = y - (int)(dy * (0.5f + along));
            if (sx < 0) sx = 0;
            if (sx >= rw) sx = rw - 1;
            if (sy < 0) sy = 0;
            if (sy >= rh) sy = rh - 1;

            int si = (sy * rw + sx) * 3;
            int di = ((ry + y) * width + rx + x) * 3;

            float a = amount * (0.4f + 0.6f * along);
            data[di+0] = (uint8_t)(data[di+0] * (1.0f - a) + tmp[si+0] * a);
            data[di+1] = (uint8_t)(data[di+1] * (1.0f - a) + tmp[si+1] * a);
            data[di+2] = (uint8_t)(data[di+2] * (1.0f - a) + tmp[si+2] * a);
        }
    }

    free(tmp);
}

// ========= GHOST STAMP REGION =========
// stamps one specific region into the ghost layer at a few spots trailing
// back along its motion path. ghost layer never clears so a persistent
// object leaves permanent copies of itself behind. a car driving through
// the frame basically paints itself onto the road forever
void EffectEngine::ghost_stamp_region(uint8_t* data, int width, int height,
                                      uint8_t* ghost,
                                      int rx, int ry, int rw, int rh,
                                      float dx, float dy, int num) {
    if (!ghost || rw <= 0 || rh <= 0) return;

    int block = MACROBLOCK_SIZE;

    for (int s = 0; s < num; s++) {
        // each stamp lands further back along the path
        int ox = rx - (int)(dx * (s + 1) * 4);
        int oy = ry - (int)(dy * (s + 1) * 4);

        // stamp block by block so it degrades like macroblock copies
        for (int y = 0; y < rh; y += block) {
            for (int x = 0; x < rw; x += block) {
                for (int py = 0; py < block && y + py < rh; py++) {
                    int src_y = ry + y + py;
                    int dst_y = oy + y + py;
                    if (src_y < 0 || src_y >= height) continue;
                    if (dst_y < 0 || dst_y >= height) continue;

                    int src_x = rx + x;
                    int dst_x = ox + x;
                    int len = block;
                    if (src_x < 0 || dst_x < 0) continue;
                    if (src_x + len > width) len = width - src_x;
                    if (dst_x + len > width) len = width - dst_x;
                    if (len <= 0) continue;

                    int si = (src_y * width + src_x) * 3;
                    int di = (dst_y * width + dst_x) * 3;
                    memcpy(&ghost[di], &data[si], len * 3);
                }
            }
        }
    }
}

// ========= WORLD MERGE =========
// two scenes occupying the same frame. dark parts of the new world become
// windows into the old one - shadows, night sky, dark clothing all bleed
// through into a place that isnt there anymore
void EffectEngine::world_merge(uint8_t* data, int width, int height,
                               uint8_t* old_world, float fade) {
    if (!old_world || fade <= 0.01f) return;

    int size = width * height;
    for (int i = 0; i < size; i++) {
        int idx = i * 3;
        // rough luma of the NEW world
        int luma = (data[idx] * 2 + data[idx+1] * 3 + data[idx+2]) / 6;
        // darker new pixel = more old world shows through
        float a = fade * (1.0f - luma / 255.0f);
        if (a > 0.9f) a = 0.9f;  // never fully replace or it just looks like a cut

        data[idx+0] = (uint8_t)(data[idx+0] * (1.0f - a) + old_world[idx+0] * a);
        data[idx+1] = (uint8_t)(data[idx+1] * (1.0f - a) + old_world[idx+1] * a);
        data[idx+2] = (uint8_t)(data[idx+2] * (1.0f - a) + old_world[idx+2] * a);
    }
}

// ========= STUCK OVERLAY =========
// you know the one - stream shits itself and one chunk of the picture
// just stops. people walk behind it, cars drive behind it, it does not
// care. it stays until the decoder remembers what reality was
void EffectEngine::grab_rect(uint8_t* data, int width, int height,
                             uint8_t* out, int rx, int ry, int rw, int rh) {
    for (int y = 0; y < rh; y++) {
        if (ry + y >= height) break;
        memcpy(&out[y * rw * 3], &data[((ry + y) * width + rx) * 3], rw * 3);
    }
}

// ========= VOID =========
// match a color, delete it, and let something else show through the hole -
// specifically another part of the same frame, displaced. the sky becomes
// a window into the road. the road becomes a window into a building
void EffectEngine::void_replace(uint8_t* data, int width, int height,
                                int tr, int tg, int tb, int tolerance,
                                int off_x, int off_y, float amount, const uint8_t* window) {
    if (amount <= 0.01f) return;
    // window = second video. then the hole looks over there, otherwise into ourselves
    const uint8_t* look = window ? window : data;

    int tol2 = tolerance * tolerance;

    // iterate AGAINST the displacement so we always read pixels that
    // havent been voided yet. learned this one the hard way, forward
    // iteration turns the whole void into one flat smear
    int y0 = (off_y >= 0) ? height - 1 : 0;
    int y1 = (off_y >= 0) ? -1 : height;
    int ys = (off_y >= 0) ? -1 : 1;
    int x0 = (off_x >= 0) ? width - 1 : 0;
    int x1 = (off_x >= 0) ? -1 : width;
    int xs = (off_x >= 0) ? -1 : 1;

    for (int y = y0; y != y1; y += ys) {
        int sy = y + off_y;
        if (sy < 0 || sy >= height) continue;

        for (int x = x0; x != x1; x += xs) {
            int sx = x + off_x;
            if (sx < 0 || sx >= width) continue;

            int idx = (y * width + x) * 3;
            int dr = data[idx+0] - tr;
            int dg = data[idx+1] - tg;
            int db = data[idx+2] - tb;

            // squared distance, no sqrt needed
            if (dr*dr + dg*dg + db*db < tol2) {
                int src = (sy * width + sx) * 3;
                data[idx+0] = (uint8_t)(data[idx+0] * (1.0f - amount) + look[src+0] * amount);
                data[idx+1] = (uint8_t)(data[idx+1] * (1.0f - amount) + look[src+1] * amount);
                data[idx+2] = (uint8_t)(data[idx+2] * (1.0f - amount) + look[src+2] * amount);
            }
        }
    }
}

// ========= MASK COMPOSITE =========
// ghost layer as solid objects instead of additive haze. wherever something
// got stamped, it sits in the frame like its actually there. stamped objects
// pile up behind moving things and just... stay. static stacks
void EffectEngine::mask_composite(uint8_t* data, int width, int height,
                                  uint8_t* ghost, float strength) {
    if (!ghost || strength <= 0.01f) return;
    if (strength > 1.0f) strength = 1.0f;

    int size = width * height * 3;
    for (int i = 0; i < size; i += 3) {
        int g = ghost[i] + ghost[i+1] + ghost[i+2];
        if (g > 24) {  // not black = something was stamped here
            data[i+0] = (uint8_t)(data[i+0] * (1.0f - strength) + ghost[i+0] * strength);
            data[i+1] = (uint8_t)(data[i+1] * (1.0f - strength) + ghost[i+1] * strength);
            data[i+2] = (uint8_t)(data[i+2] * (1.0f - strength) + ghost[i+2] * strength);
        }
    }
}

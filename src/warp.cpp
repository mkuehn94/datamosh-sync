// warp.cpp - fake decoder that lost it's mind
// see warp.h for the idea

#include "warp.h"
#include "constants.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <iostream>

using namespace std;

#define BLOCK MACROBLOCK_SIZE

static inline uint8_t clamp8(int v) {
    return v < 0 ? 0 : (v > 255 ? 255 : v);
}

static inline float rnd() {
    return rand() / (float)RAND_MAX;
}

WarpEngine::WarpEngine() {
    w = h = gx = gy = sw = sh = 0;
    fps = 30; dt = 1.0f / 30; scale = 1;
    luma = prev_luma = small = prev_small = nullptr;
    canvas = canvas_tmp = prev_src = nullptr;
    have_prev_luma = false;
    cut_detected = false;
    canvas_ready = false;
    tunnel = 0;
    van_x = van_y = 0;
    refresh_t = -1;
    refresh_kind = 0;
    refresh_x = refresh_y = 0;
    kicks_since_refresh = 0;
    ring_pos = ring_count = 0;
    coef_ready = false;
    spectral_flip = false;
    spectral_kicks = 0;
    frame_no = 0;
}

WarpEngine::~WarpEngine() {
    free(luma); free(prev_luma); free(small); free(prev_small);
    free(canvas); free(canvas_tmp); free(prev_src);
    for (size_t i = 0; i < ring_frames.size(); i++) free(ring_frames[i]);
}

void WarpEngine::init(int width, int height, float f) {
    w = width; h = height;
    fps = f; dt = 1.0f / f;
    scale = h / 1080.0f;
    gx = w / BLOCK;
    gy = h / BLOCK;
    sw = w / 4;
    sh = h / 4;

    luma = (uint8_t*)malloc(w * h);
    prev_luma = (uint8_t*)malloc(w * h);
    small = (uint8_t*)malloc(sw * sh);
    prev_small = (uint8_t*)malloc(sw * sh);
    canvas = (uint8_t*)malloc(w * h * 3);
    canvas_tmp = (uint8_t*)malloc(w * h * 3);
    prev_src = (uint8_t*)malloc(w * h * 3);

    mvx.assign(gx * gy, 0);
    mvy.assign(gx * gy, 0);
    block_err.assign(gx * gy, 0);
    fx.assign(gx * gy, 0);
    fy.assign(gx * gy, 0);
    rem_x.assign(gx * gy, 0);
    rem_y.assign(gx * gy, 0);
    depth.assign(gx * gy, 0);

    van_x = w / 2.0f;
    van_y = h / 2.0f;
}

// ===== motion estimation =====
// two stage: full search on a 1/4 size picture, then refine at full res.
// way better than the one in motion_track which only looks at +-8 in steps
// of 4. that one is fine for "is something moving", not for real vectors

void WarpEngine::estimate_motion(const uint8_t* rgb) {
    #pragma omp parallel for
    for (int i = 0; i < w * h; i++) {
        luma[i] = (rgb[i*3] + 2 * rgb[i*3+1] + rgb[i*3+2]) >> 2;
    }
    #pragma omp parallel for
    for (int y = 0; y < sh; y++) {
        for (int x = 0; x < sw; x++) {
            int s = 0;
            for (int j = 0; j < 4; j++)
                for (int i = 0; i < 4; i++)
                    s += luma[(y*4+j) * w + x*4+i];
            small[y * sw + x] = s >> 4;
        }
    }

    cut_detected = false;
    if (!have_prev_luma) {
        memcpy(prev_luma, luma, w * h);
        memcpy(prev_small, small, sw * sh);
        have_prev_luma = true;
        return;
    }

    const int R = 8;   // +-32 px at full res
    const int sb = BLOCK / 4;

    #pragma omp parallel for schedule(dynamic, 4)
    for (int by = 0; by < gy; by++) {
        for (int bx = 0; bx < gx; bx++) {
            int cx = bx * sb, cy = by * sb;
            int best = 1 << 30, bdx = 0, bdy = 0;
            for (int dy = -R; dy <= R; dy++) {
                if (cy + dy < 0 || cy + dy + sb > sh) continue;
                for (int dx = -R; dx <= R; dx++) {
                    if (cx + dx < 0 || cx + dx + sb > sw) continue;
                    int sad = 0;
                    for (int y = 0; y < sb; y++) {
                        const uint8_t* a = small + (cy + y) * sw + cx;
                        const uint8_t* b = prev_small + (cy + dy + y) * sw + cx + dx;
                        for (int x = 0; x < sb; x++) sad += abs(a[x] - b[x]);
                    }
                    // lean towards short vectors or flat areas (sky!!) get
                    // random garbage vectors and the whole sky boils
                    sad += (abs(dx) + abs(dy)) * 2;
                    if (sad < best) { best = sad; bdx = dx; bdy = dy; }
                }
            }
            mvx[by * gx + bx] = bdx * 4;
            mvy[by * gx + bx] = bdy * 4;
        }
    }

    // refine +-3 at full res, keep the real matching error too
    int bad = 0;
    #pragma omp parallel for schedule(dynamic, 4) reduction(+:bad)
    for (int by = 0; by < gy; by++) {
        for (int bx = 0; bx < gx; bx++) {
            int idx = by * gx + bx;
            int cx = bx * BLOCK, cy = by * BLOCK;
            int best = 1 << 30, bdx = mvx[idx], bdy = mvy[idx];
            int ox = bdx, oy = bdy;
            for (int dy = oy - 3; dy <= oy + 3; dy++) {
                if (cy + dy < 0 || cy + dy + BLOCK > h) continue;
                for (int dx = ox - 3; dx <= ox + 3; dx++) {
                    if (cx + dx < 0 || cx + dx + BLOCK > w) continue;
                    int sad = 0;
                    for (int y = 0; y < BLOCK; y += 2) {
                        const uint8_t* a = luma + (cy + y) * w + cx;
                        const uint8_t* b = prev_luma + (cy + dy + y) * w + cx + dx;
                        for (int x = 0; x < BLOCK; x += 2) sad += abs(a[x] - b[x]);
                    }
                    if (sad < best) { best = sad; bdx = dx; bdy = dy; }
                }
            }
            mvx[idx] = bdx;
            mvy[idx] = bdy;
            block_err[idx] = best / ((BLOCK / 2) * (BLOCK / 2));
            if (block_err[idx] > 28) bad++;
        }
    }

    cut_detected = bad > (gx * gy) * 6 / 10;
    if (cut_detected) cout << "[warp] scene cut" << endl;

    memcpy(prev_luma, luma, w * h);
    memcpy(prev_small, small, sw * sh);
}

// ===== warp field =====

void WarpEngine::build_warp_field(float warp, const AudioFrame& a) {
    float frame_scale = 30.0f * dt * scale;  // pixels-per-frame tuned at 30fps/1080

    if (a.kick) {
        // bass hit punches the warp drive
        tunnel = max(tunnel, warp * (0.35f + 0.65f * a.bass));

        // and tears a hole somewhere. harder hits = more holes
        int n = 1 + (a.bass > 0.7f ? 1 : 0);
        for (int i = 0; i < n && wells.size() < 8; i++) {
            Well wl;
            wl.x = w * (0.1f + 0.8f * rnd());
            wl.y = h * (0.1f + 0.8f * rnd());
            wl.radius = h * (0.15f + 0.35f * rnd());
            wl.pull = (rnd() < 0.7f ? 1.0f : -0.8f) * 22.0f * warp * (0.5f + a.bass);
            wl.spin = (rnd() < 0.5f ? -1.0f : 1.0f) * 28.0f * warp * rnd();
            wl.age = 0;
            wl.life = 0.4f + 1.4f * rnd();
            wells.push_back(wl);
        }

        // vanishing point wanders to a new spot on the kick
        van_x = w * (0.3f + 0.4f * rnd());
        van_y = h * (0.3f + 0.4f * rnd());
    }
    tunnel *= pow(0.02f, dt);  // gone in about a second

    for (size_t i = 0; i < wells.size();) {
        wells[i].age += dt;
        if (wells[i].age > wells[i].life) wells.erase(wells.begin() + i);
        else i++;
    }

    float tun = (tunnel + warp * 0.03f + warp * a.treble * 0.04f) * 0.11f;

    #pragma omp parallel for
    for (int by = 0; by < gy; by++) {
        for (int bx = 0; bx < gx; bx++) {
            float cx = bx * BLOCK + BLOCK * 0.5f;
            float cy = by * BLOCK + BLOCK * 0.5f;

            // tunnel: read from closer to the vanishing point = picture
            // streams outward = flying into it
            float vx = (van_x - cx) * tun;
            float vy = (van_y - cy) * tun;

            for (size_t i = 0; i < wells.size(); i++) {
                const Well& wl = wells[i];
                float dx = cx - wl.x;
                float dy = cy - wl.y;
                float r2 = (dx*dx + dy*dy) / (wl.radius * wl.radius);
                if (r2 > 6.0f) continue;
                float fall = exp(-r2 * 1.6f);
                float t = wl.age / wl.life;
                float env = min(1.0f, t * 12.0f) * (1.0f - t) * (1.0f - t);
                float len = sqrt(dx*dx + dy*dy) + 1.0f;
                float nx = dx / len, ny = dy / len;
                // reading further out = stuff falls IN
                vx += (nx * wl.pull - ny * wl.spin) * fall * env;
                vy += (ny * wl.pull + nx * wl.spin) * fall * env;
            }
            fx[by * gx + bx] = vx * frame_scale;
            fy[by * gx + bx] = vy * frame_scale;
        }
    }
}

void WarpEngine::start_refresh() {
    refresh_t = 0;
    refresh_kind = rand() % 3;
    refresh_x = w * rnd();
    refresh_y = h * rnd();
}

// ===== rot =====
// this is literally what a decoder does:
//    out(x) = ref(x + mv) + residual
// except ref is OUR canvas and not the real previous frame, and the
// keyframes that would fix it never come. the residual still gets computed
// from the real video so moving things keep punching through the mush as
// outlines. thats THE datamosh look, and we get to do it per block, on
// purpose, in sync, with vectors we made up

void WarpEngine::rot_and_warp(uint8_t* data, float rot, float warp, const AudioFrame& a) {
    int size = w * h * 3;

    if (!canvas_ready) {
        memcpy(canvas, data, size);
        memcpy(prev_src, data, size);
        canvas_ready = true;
        return;
    }
    if (rot < 0.01f && warp < 0.01f) {
        memcpy(prev_src, data, size);
        return;
    }

    build_warp_field(warp, a);

    // how fast the picture heals. block by block, never a crossfade.
    // crossfading is what made the old stuff look like movie maker
    float r = max(rot, warp * 0.5f);
    float heal = (1.0f - r) * (1.0f - r) * 0.22f + 0.0015f;
    float luma_heal = (1.0f - r) * 0.28f + 0.012f;
    heal *= 30.0f * dt;
    luma_heal *= 30.0f * dt;

    // "keyframes": a wipe of clean blocks every few kicks. more rot = rarer
    if (a.kick) {
        kicks_since_refresh++;
        int every = 2 + (int)(r * r * 22.0f);
        if (kicks_since_refresh >= every && refresh_t < 0) {
            start_refresh();
            kicks_since_refresh = 0;
        }
    }
    float wipe_dur = 0.45f;
    float wipe_a = -1, wipe_b = -1;
    if (refresh_t >= 0) {
        wipe_a = refresh_t / wipe_dur;
        wipe_b = (refresh_t + dt) / wipe_dur;
        refresh_t += dt;
        if (refresh_t > wipe_dur) refresh_t = -1;
    }
    float diag = sqrt((float)(w*w + h*h));

    // rand() isnt thread safe, roll the dice up front
    vector<unsigned char> dice(gx * gy);
    for (int i = 0; i < gx * gy; i++) dice[i] = rand() & 255;

    #pragma omp parallel for schedule(dynamic, 4)
    for (int by = 0; by < gy; by++) {
        for (int bx = 0; bx < gx; bx++) {
            int idx = by * gx + bx;
            int x0 = bx * BLOCK, y0 = by * BLOCK;
            int mx = mvx[idx], my = mvy[idx];

            // hijacked vector = real motion + the warp field
            // leftover subpixel stuff carries to the next frame, otherwise
            // anything under half a pixel rounds to 0 and just never moves
            float want_x = fx[idx] + rem_x[idx];
            float want_y = fy[idx] + rem_y[idx];
            int wx = (int)floor(want_x + 0.5f);
            int wy = (int)floor(want_y + 0.5f);
            rem_x[idx] = want_x - wx;
            rem_y[idx] = want_y - wy;
            int vx = mx + wx;
            int vy = my + wy;

            // what would an encoder do with this block
            int mode = 0;  // 0 = inter, 1 = intra (clean), 2 = no residual, 3 = luma only
            // dice are per CLUSTER not per block. single random blocks look
            // like confetti, encoders make decisions in chunks
            float d = dice[(by / 2) * gx + bx / 2] / 255.0f;
            float d2 = dice[((by / 4) * gx + bx / 4 + 1) % (gx * gy)] / 255.0f;

            if (a.cut) {
                // THE moment. real mosh = you delete the i-frame here, new
                // scene never arrives, its motion moves the old scene
                mode = d < r ? 2 : 1;
            } else if (block_err[idx] > 40) {
                // nothing in the ref matches, encoder would go intra here.
                // rot refuses most of the time
                mode = d < r * 0.97f ? 2 : 1;
            } else if (d < heal) {
                mode = 1;
            } else if (d2 < luma_heal) {
                mode = 3;
            } else if (block_err[idx] < 5) {
                // barely changed = P-skip, a real encoder wouldnt send a
                // residual either. without this sensor noise and the source
                // compression add up in the canvas into grain
                mode = 2;
            }

            // keyframe wipe passing over this block?
            if (wipe_a >= 0) {
                float cx = x0 + BLOCK * 0.5f, cy = y0 + BLOCK * 0.5f;
                float p;
                if (refresh_kind == 0) p = cx / w;
                else if (refresh_kind == 1) p = cy / h;
                else p = sqrt((cx-refresh_x)*(cx-refresh_x) + (cy-refresh_y)*(cy-refresh_y)) / diag * 1.4f;
                if (p >= wipe_a && p < wipe_b) mode = 1;
            }

            for (int y = 0; y < BLOCK; y++) {
                for (int x = 0; x < BLOCK; x++) {
                    int px = x0 + x, py = y0 + y;

                    int di = (py * w + px) * 3;
                    if (mode == 1) {
                        canvas_tmp[di] = data[di];
                        canvas_tmp[di+1] = data[di+1];
                        canvas_tmp[di+2] = data[di+2];
                        continue;
                    }

                    int sx = px + vx, sy = py + vy;
                    if (sx < 0) sx = 0; else if (sx >= w) sx = w - 1;
                    if (sy < 0) sy = 0; else if (sy >= h) sy = h - 1;
                    int ci = (sy * w + sx) * 3;

                    int r0 = canvas[ci], g0 = canvas[ci+1], b0 = canvas[ci+2];

                    if (mode != 2) {
                        int rx = px + mx, ry = py + my;
                        if (rx < 0) rx = 0; else if (rx >= w) rx = w - 1;
                        if (ry < 0) ry = 0; else if (ry >= h) ry = h - 1;
                        int pi = (ry * w + rx) * 3;
                        r0 += data[di]   - prev_src[pi];
                        g0 += data[di+1] - prev_src[pi+1];
                        b0 += data[di+2] - prev_src[pi+2];
                    }

                    if (mode == 3) {
                        // brightness comes back, color doesnt. so the street
                        // is sharp again but still painted in the colors of
                        // wherever we were 5 seconds ago
                        int yn = (data[di] + 2*data[di+1] + data[di+2]) >> 2;
                        int yc = (r0 + 2*g0 + b0) >> 2;
                        int dl = yn - yc;
                        r0 += dl; g0 += dl; b0 += dl;
                    }

                    canvas_tmp[di] = clamp8(r0);
                    canvas_tmp[di+1] = clamp8(g0);
                    canvas_tmp[di+2] = clamp8(b0);
                }
            }
        }
    }

    // the strip at the right/bottom that doesnt fill a whole block just
    // takes the source. nobody looks there. (1080 isnt divisible by 16)
    for (int y = gy * BLOCK; y < h; y++)
        memcpy(canvas_tmp + y * w * 3, data + y * w * 3, w * 3);
    if (gx * BLOCK < w) {
        for (int y = 0; y < h; y++) {
            int o = (y * w + gx * BLOCK) * 3;
            memcpy(canvas_tmp + o, data + o, (w - gx * BLOCK) * 3);
        }
    }

    memcpy(prev_src, data, size);
    swap(canvas, canvas_tmp);
    memcpy(data, canvas, size);
}

// ===== time quake =====
// keep the last couple seconds of output around and let every macroblock
// pick how far back it wants to read from. a kick sends a ring across the
// picture, whatever it passes gets thrown into the past and then slowly
// catches up again. fast moving stuff also sinks by itself so it shreds
// into layers of its own past
//
// depth gets quantized so you get hard edged sheets of time and not a smooth
// blur

void WarpEngine::time_quake(uint8_t* data, float amount, const AudioFrame& a) {
    if (amount < 0.01f) return;

    int size = w * h * 3;
    int cap = (int)(QUAKE_SECONDS * fps);

    // lazy alloc. at 1440x1080 60fps this is about 560MB
    if ((int)ring_frames.size() < cap) {
        ring_frames.assign(cap, nullptr);
        for (int i = 0; i < cap; i++) ring_frames[i] = (uint8_t*)malloc(size);
        cout << "[warp] quake buffer: " << (cap * (long)size) / (1024 * 1024) << " MB" << endl;
    }

    memcpy(ring_frames[ring_pos], data, size);
    int now = ring_pos;
    ring_pos = (ring_pos + 1) % cap;
    if (ring_count < cap) ring_count++;

    if (a.kick) {
        Ring rg;
        rg.x = w * (0.15f + 0.7f * rnd());
        rg.y = h * (0.15f + 0.7f * rnd());
        rg.r = 0;
        rg.speed = h * (1.2f + 1.6f * rnd());   // crosses the frame in under a sec
        rg.strength = amount * (0.4f + 0.6f * a.bass);
        rings.push_back(rg);
        if (rings.size() > 6) rings.erase(rings.begin());
    }
    for (size_t i = 0; i < rings.size();) {
        rings[i].r += rings[i].speed * dt;
        if (rings[i].r > (w + h) * 1.2f) rings.erase(rings.begin() + i);
        else i++;
    }

    float band = h * 0.13f;
    float max_depth = QUAKE_SECONDS * 0.98f;
    // the past catches up. more quake = time stays broken for longer
    float catch_up = pow(0.5f, dt / (0.2f + 0.9f * amount));
    float sink = amount * 0.5f * dt;
    int levels = 3 + (int)(a.treble * 3);   // hats make the sheets finer

    vector<float> ox(gx * gy, 0.0f), oy(gx * gy, 0.0f), depth2(gx * gy);

    // motion magnitude averaged over a neighborhood. single block vectors
    // are noisy as hell, using them directly gave speckle instead of sheets
    #pragma omp parallel for schedule(dynamic, 4)
    for (int by = 0; by < gy; by++) {
        for (int bx = 0; bx < gx; bx++) {
            int idx = by * gx + bx;
            float cx = bx * BLOCK + BLOCK * 0.5f, cy = by * BLOCK + BLOCK * 0.5f;
            float dp = depth[idx] * catch_up;

            float mv_sum = 0;
            int n = 0;
            for (int j = max(0, by - 3); j <= min(gy - 1, by + 3); j += 2) {
                for (int i = max(0, bx - 3); i <= min(gx - 1, bx + 3); i += 2) {
                    mv_sum += abs(mvx[j * gx + i]) + abs(mvy[j * gx + i]);
                    n++;
                }
            }
            // 32 is a lot of motion per frame at 1080
            dp += min(1.0f, mv_sum / n / (32.0f * scale)) * sink;

            for (size_t i = 0; i < rings.size(); i++) {
                float dx = cx - rings[i].x;
                float dy = cy - rings[i].y;
                float len = sqrt(dx*dx + dy*dy) + 1.0f;
                float u = (len - rings[i].r) / band;
                if (u > -1.0f && u < 1.0f) {
                    float prof = min(1.0f, (1.0f - u * u) * 2.0f);
                    float hit = rings[i].strength * prof * max_depth;
                    if (hit > dp) dp = hit;
                    // space bends where the ring is, like its refracting
                    float bend = rings[i].strength * (1.0f - u * u) * 70.0f * scale * (u < 0 ? -1.0f : 1.0f);
                    ox[idx] += dx / len * bend;
                    oy[idx] += dy / len * bend;
                }
            }
            depth2[idx] = dp > max_depth ? max_depth : dp;
        }
    }

    // little blur so the sheets have shape
    #pragma omp parallel for
    for (int by = 0; by < gy; by++) {
        for (int bx = 0; bx < gx; bx++) {
            float t = 0;
            int n = 0;
            for (int j = max(0, by - 1); j <= min(gy - 1, by + 1); j++) {
                for (int i = max(0, bx - 1); i <= min(gx - 1, bx + 1); i++) {
                    t += depth2[j * gx + i];
                    n++;
                }
            }
            depth[by * gx + bx] = t / n;
        }
    }

    #pragma omp parallel for schedule(dynamic, 4)
    for (int by = 0; by < gy; by++) {
        for (int bx = 0; bx < gx; bx++) {
            int idx = by * gx + bx;
            int x0 = bx * BLOCK, y0 = by * BLOCK;

            float q = floor(depth[idx] / max_depth * levels + 0.5f) / levels;
            int back = (int)(q * max_depth * fps);
            if (back >= ring_count) back = ring_count - 1;

            const uint8_t* f = ring_frames[(now - back + cap * 2) % cap];

            int sx0 = x0 + (int)ox[idx];
            int sy0 = y0 + (int)oy[idx];
            if (sx0 < 0) sx0 = 0; else if (sx0 > w - BLOCK) sx0 = w - BLOCK;
            if (sy0 < 0) sy0 = 0; else if (sy0 > h - BLOCK) sy0 = h - BLOCK;

            if (back == 0 && sx0 == x0 && sy0 == y0) continue;  // already right

            for (int y = 0; y < BLOCK; y++) {
                memcpy(data + ((y0 + y) * w + x0) * 3,
                       f + ((sy0 + y) * w + sx0) * 3,
                       BLOCK * 3);
            }
        }
    }
}

// ===== spectral lag =====
// real 8x8 dct like a jpeg/h264 block, one held value per coefficient.
// every frequency band updates at a different rate so the outlines of a car
// are from a second ago while its brightness is from right now. every few
// kicks the order flips, then the big blurry shapes are the old ones and the
// fine detail is live
//
// NOTE sample and hold, NOT a running average. the average just blurs the
// whole video (looks like a dream sequence from a bad soap). held values stay
// sharp and thats what makes it look like two moments stuck together

static float dct_tab[8][8];
static bool dct_ready = false;

static void dct_setup() {
    for (int u = 0; u < 8; u++) {
        float c = u == 0 ? sqrt(0.125f) : 0.5f;
        for (int x = 0; x < 8; x++) {
            dct_tab[u][x] = c * cos((2 * x + 1) * u * M_PI / 16.0);
        }
    }
    dct_ready = true;
}

static void dct8(const float* in, float* out) {
    float t[64];
    for (int y = 0; y < 8; y++) {
        for (int u = 0; u < 8; u++) {
            float s = 0;
            for (int x = 0; x < 8; x++) s += dct_tab[u][x] * in[y * 8 + x];
            t[y * 8 + u] = s;
        }
    }
    for (int u = 0; u < 8; u++) {
        for (int v = 0; v < 8; v++) {
            float s = 0;
            for (int y = 0; y < 8; y++) s += dct_tab[v][y] * t[y * 8 + u];
            out[v * 8 + u] = s;
        }
    }
}

static void idct8(const float* in, float* out) {
    float t[64];
    for (int u = 0; u < 8; u++) {
        for (int y = 0; y < 8; y++) {
            float s = 0;
            for (int v = 0; v < 8; v++) s += dct_tab[v][y] * in[v * 8 + u];
            t[y * 8 + u] = s;
        }
    }
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            float s = 0;
            for (int u = 0; u < 8; u++) s += dct_tab[u][x] * t[y * 8 + u];
            out[y * 8 + x] = s;
        }
    }
}

void WarpEngine::spectral_lag(uint8_t* data, float amount, const AudioFrame& a) {
    if (amount < 0.01f) return;
    if (!dct_ready) dct_setup();

    int bw = w / 8, bh = h / 8;
    bool first = !coef_ready;
    if (first) {
        coef.assign(bw * bh * 64, 0.0f);
        coef_ready = true;
    }

    if (a.kick) {
        spectral_kicks++;
        if (spectral_kicks % 8 == 0) spectral_flip = !spectral_flip;
    }

    // low bands stay live, high bands get held (or the other way round when
    // flipped). more amount = more of the picture is old, both ways. flipped
    // has to stay tiny, holding even a few low bands is basically a freeze
    // frame with a checkerboard on it
    int split = spectral_flip ? (int)(amount * 1.5f) : 4 - (int)(amount * 3.0f);

    // chance per frame that a chunk grabs a new held image
    float grab = min(1.0f, dt / (0.1f + amount * 1.6f));
    if (a.kick) grab = 0.25f;  // kicks refresh a good part of it at once

    float crush = amount * (0.3f + a.bass);
    // old detail gets boosted, at 1:1 it just looked like scratches on a lens
    float gain = 1.0f + amount * 1.5f;

    frame_no++;
    unsigned frame_seed = frame_no * 2654435761u;

    #pragma omp parallel for schedule(dynamic, 8)
    for (int by = 0; by < bh; by++) {
        float px[64], c[64], res[64];
        for (int bx = 0; bx < bw; bx++) {
            float* m = &coef[(by * bw + bx) * 64];
            int x0 = bx * 8, y0 = by * 8;

            for (int y = 0; y < 8; y++) {
                const uint8_t* p = data + ((y0 + y) * w + x0) * 3;
                for (int x = 0; x < 8; x++) {
                    px[y * 8 + x] = (p[x*3] + 2 * p[x*3+1] + p[x*3+2]) * 0.25f - 128.0f;
                }
            }
            dct8(px, c);

            // 6x6 chunks of 8x8 blocks grab together. per block or per
            // coefficient was just noise, looked like pebbled glass. a
            // chunk holds a readable piece of the old picture
            unsigned r = (((unsigned)(by / 6) * 73856093u) ^ ((unsigned)(bx / 6) * 19349663u) ^ frame_seed) * 2654435761u;
            bool grabbing = first || (r >> 16) / 65536.0f < grab;

            for (int v = 0; v < 8; v++) {
                for (int u = 0; u < 8; u++) {
                    int i = v * 8 + u;
                    int f = u + v;
                    bool held = spectral_flip ? (f <= split) : (f > split);
                    if (!held) {
                        m[i] = c[i];
                    } else if (grabbing) {
                        // never quantize DC or the blocks flash
                        float q = 1.0f + f * 3.0f * crush;
                        m[i] = i == 0 ? c[i] : floor(c[i] / q + 0.5f) * q * gain;
                    }
                }
            }

            idct8(m, res);

            // only luma goes through here, add the difference to all channels
            // and the color stays live
            for (int y = 0; y < 8; y++) {
                uint8_t* p = data + ((y0 + y) * w + x0) * 3;
                for (int x = 0; x < 8; x++) {
                    float cur = (p[x*3] + 2 * p[x*3+1] + p[x*3+2]) * 0.25f - 128.0f;
                    int dl = (int)(res[y * 8 + x] - cur);
                    p[x*3]   = clamp8(p[x*3] + dl);
                    p[x*3+1] = clamp8(p[x*3+1] + dl);
                    p[x*3+2] = clamp8(p[x*3+2] + dl);
                }
            }
        }
    }
}

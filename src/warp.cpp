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
            int mode = 0;  // 0 = inter, 1 = intra (clean), 2 = skip residual, 3 = luma only
            // dice are per CLUSTER not per block. single random blocks look
            // like confetti, encoders make decisions in chunks
            float d = dice[(by / 2) * gx + bx / 2] / 255.0f;
            float d2 = dice[(by / 4) * gx + bx / 4 + 1] / 255.0f;

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

// filled in next
void WarpEngine::time_quake(uint8_t*, float, const AudioFrame&) {}
void WarpEngine::spectral_lag(uint8_t*, float, const AudioFrame&) {}

// warp.cpp - fake decoder that lost its mind
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

// own random stream per effect instead of rand(). otherwise moving ANY
// slider reshuffles everything and you cant tell what that slider did
static inline float rnd(unsigned* seed) {
    return rand_r(seed) / (float)RAND_MAX;
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
    quake_tape = false;
    other_rgb = nullptr;
    other_eyes = nullptr;
    other_amount = 0;
    other_prev = nullptr;
    other_bar = 0;
    other_field_bar = -1;
    other_seed = 212223;
    other_kicks = 0;
    quake_levels = 3;
    coef_ready = false;
    spectral_flip = false;
    spectral_kicks = 0;
    frame_no = 0;
    warp_seed = 1234;
    rot_seed = 5678;
    quake_seed = 91011;
    bar_kicks = 0;
    loop_pos = 0;
    loop_slice = 0;
    bar_no = 0;
    groove_punch = 0;
    recur_prev = nullptr;
    hom_buf = nullptr;
    hom_ref = nullptr;
    hom_ref_tmp = nullptr;
    hom_seed = 181920;
    recur_seed = 121314;
    tex_kicks = 0;
    frame_crawl = 0;
    tex_seed = 151617;
}

WarpEngine::~WarpEngine() {
    free(luma); free(prev_luma); free(small); free(prev_small);
    free(canvas); free(canvas_tmp); free(prev_src);
    free(other_prev);
    free(recur_prev);
    free(hom_buf);
    free(hom_ref);
    free(hom_ref_tmp);
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
            wl.x = w * (0.1f + 0.8f * rnd(&warp_seed));
            wl.y = h * (0.1f + 0.8f * rnd(&warp_seed));
            wl.radius = h * (0.15f + 0.35f * rnd(&warp_seed));
            wl.pull = (rnd(&warp_seed) < 0.7f ? 1.0f : -0.8f) * 22.0f * warp * (0.5f + a.bass);
            wl.spin = (rnd(&warp_seed) < 0.5f ? -1.0f : 1.0f) * 28.0f * warp * rnd(&warp_seed);
            wl.age = 0;
            wl.life = 0.4f + 1.4f * rnd(&warp_seed);
            wells.push_back(wl);
        }

        // vanishing point wanders to a new spot on the kick
        van_x = w * (0.3f + 0.4f * rnd(&warp_seed));
        van_y = h * (0.3f + 0.4f * rnd(&warp_seed));
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
    refresh_kind = rand_r(&rot_seed) % 3;
    refresh_x = w * rnd(&rot_seed);
    refresh_y = h * rnd(&rot_seed);
}

// ===== motion sampler =====
// tape the vectors of one bar, then replay them like a sample. every kick
// jumps to the next slice of the loop so it stays locked to the music

void WarpEngine::update_sampler(const AudioFrame& a) {
    // the taped vectors alone dont do anything you can see on a kick, its just
    // 1 or 2 px of different motion. so the taped chunks lurch on the hit and
    // go slack in between
    if (a.kick) groove_punch = 0.4f + 0.6f * a.hit;
    else groove_punch *= pow(0.02f, dt / 0.15f);

    rec_x.push_back(mvx);
    rec_y.push_back(mvy);
    if (a.kick) {
        rec_kicks.push_back((int)rec_x.size() - 1);
        bar_kicks++;
    }

    bool bar_done = a.kick && bar_kicks >= SAMPLER_BAR_KICKS;
    // no kicks for ages (quiet part), dont tape forever
    if (rec_x.size() > (size_t)(fps * 6)) bar_done = true;

    if (bar_done) {
        loop_x = rec_x;
        loop_y = rec_y;
        loop_kicks = rec_kicks;
        rec_x.clear();
        rec_y.clear();
        rec_kicks.clear();
        bar_kicks = 0;
        loop_pos = 0;
        loop_slice = 0;
        bar_no++;
    } else if (a.kick && !loop_kicks.empty()) {
        loop_slice++;
        if (loop_slice >= (int)loop_kicks.size()) loop_slice = 0;
        loop_pos = loop_kicks[loop_slice];
    } else if (!loop_x.empty()) {
        loop_pos++;
        if (loop_pos >= (int)loop_x.size()) loop_pos = 0;
    }
}

// ===== rot =====
// this is literally what a decoder does:
//    out(x) = ref(x + mv) + residual
// except ref is OUR canvas and not the real previous frame, and the
// keyframes that would fix it never come. the residual still gets computed
// from the real video so moving things keep punching through the mush as
// outlines. thats THE datamosh look, and we get to do it per block, on
// purpose, in sync, with vectors we made up

void WarpEngine::look_at_other(const uint8_t* rgb, WarpEngine* eyes, float amount) {
    other_rgb = rgb;
    other_eyes = eyes;
    other_amount = amount;
    if (!other_prev) {
        make_other_field();
        other_prev = (uint8_t*)malloc(w * h * 3);
        memcpy(other_prev, rgb, w * h * 3);
    }
}

// where the other video shows through. random noise blurred a lot so you get blobs
// and rifts with messy edges and not squares. 0-1 evenly so amount .3 really is 30%
void WarpEngine::make_other_field() {
    vector<float> f(gx * gy), t(gx * gy);
    for (int i = 0; i < gx * gy; i++) f[i] = rnd(&other_seed);
    for (int pass = 0; pass < 3; pass++) {
        for (int by = 0; by < gy; by++) {
            for (int bx = 0; bx < gx; bx++) {
                float s = 0;
                int n = 0;
                for (int j = max(0, by - 3); j <= min(gy - 1, by + 3); j++) {
                    for (int i = max(0, bx - 3); i <= min(gx - 1, bx + 3); i++) {
                        s += f[j * gx + i];
                        n++;
                    }
                }
                t[by * gx + bx] = s / n;
            }
        }
        f = t;
    }
    // little bit of the raw noise back on so the edges arent so smooth
    for (int i = 0; i < gx * gy; i++) f[i] += (rnd(&other_seed) - 0.5f) * 0.02f;

    vector<float> sorted = f;
    sort(sorted.begin(), sorted.end());
    other_field.resize(gx * gy);
    for (int i = 0; i < gx * gy; i++) {
        other_field[i] = (lower_bound(sorted.begin(), sorted.end(), f[i]) - sorted.begin()) / (float)(gx * gy);
    }
    other_field_bar = other_bar;
}

void WarpEngine::rot_and_warp(uint8_t* data, float rot, float warp, float groove, const AudioFrame& a) {
    int size = w * h * 3;

    if (!canvas_ready) {
        memcpy(canvas, data, size);
        memcpy(prev_src, data, size);
        canvas_ready = true;
        return;
    }
    if (groove > 0.01f) update_sampler(a);

    // other video on: has to run even with rot at 0 or nothing of it ever shows
    if (rot < 0.01f && warp < 0.01f && groove < 0.01f && !other_eyes) {
        memcpy(prev_src, data, size);
        canvas_ready = false;  // next time it starts rotting from the picture of THAT moment
        return;
    }

    build_warp_field(warp, a);

    // how fast the picture heals. block by block, never a crossfade.
    // crossfading is what made the old stuff look like movie maker
    float r = rot + (1.0f - rot) * warp * 0.5f;
    // the sampler needs the picture to stick around or you never see it
    r = r + (1.0f - r) * groove * 0.6f;
    float heal = pow(1.0f - r, 3.0f) * 0.5f + 0.0015f;
    float luma_heal = pow(1.0f - r, 2.0f) * 0.5f + 0.012f;
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
    for (int i = 0; i < gx * gy; i++) dice[i] = rand_r(&rot_seed) & 255;

    #pragma omp parallel for schedule(dynamic, 4)
    for (int by = 0; by < gy; by++) {
        for (int bx = 0; bx < gx; bx++) {
            int idx = by * gx + bx;
            int x0 = bx * BLOCK, y0 = by * BLOCK;
            int mx = mvx[idx], my = mvy[idx];

            // this chunk belongs to the other video? then the motion comes from there.
            // this is the whole trick, the street moves like its in tokyo
            // the whole block comes from over there, motion residual and healing
            const uint8_t* src = data;
            const uint8_t* prv = prev_src;
            int berr = block_err[idx];
            if (other_eyes) {
                if (other_field[idx] < other_amount) {
                    mx = other_eyes->mvx[idx];
                    my = other_eyes->mvy[idx];
                    berr = other_eyes->block_err[idx];
                    src = other_rgb;
                    prv = other_prev;
                }
            }

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

            // this chunk is possessed by the tape? then it moves how the
            // picture moved a bar ago and not how its moving now. which chunks
            // changes every bar
            if (groove > 0.01f && !loop_x.empty()) {
                unsigned hh = ((unsigned)(by / 4) * 73856093u) ^ ((unsigned)(bx / 4) * 19349663u) ^ ((unsigned)bar_no * 83492791u);
                hh = hh * 2654435761u;
                if ((hh >> 16) / 65536.0f < groove * 1.4f) {
                    float lurch = 0.25f + 5.0f * groove_punch;
                    vx = (int)(loop_x[loop_pos][idx] * lurch) + wx;
                    vy = (int)(loop_y[loop_pos][idx] * lurch) + wy;
                }
            }

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
            } else if (berr > 40) {
                // nothing in the ref matches, encoder would go intra here.
                // rot refuses most of the time
                mode = d < r * 0.97f ? 2 : 1;
            } else if (d < heal) {
                mode = 1;
            } else if (d2 < luma_heal) {
                mode = 3;
            } else if (berr < 5) {
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
                        canvas_tmp[di] = src[di];
                        canvas_tmp[di+1] = src[di+1];
                        canvas_tmp[di+2] = src[di+2];
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
                        r0 += src[di]   - prv[pi];
                        g0 += src[di+1] - prv[pi+1];
                        b0 += src[di+2] - prv[pi+2];
                    }

                    if (mode == 3) {
                        // brightness comes back, color doesnt. so the street
                        // is sharp again but still painted in the colors of
                        // wherever we were 5 seconds ago
                        int yn = (src[di] + 2*src[di+1] + src[di+2]) >> 2;
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
    if (other_prev) memcpy(other_prev, other_rgb, w * h * 3);
    if (other_eyes && a.kick) {
        other_kicks++;
        if (other_kicks % 8 == 0) other_bar++;
    }
    if (other_eyes && other_bar != other_field_bar) make_other_field();
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
    if (amount < 0.01f && !quake_tape) return;

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

    // only here to keep the tape running. otherwise a quake that starts on a
    // drop has no past to throw anything into for the first seconds
    if (amount < 0.01f) return;

    // low slider = only the hardest kicks make a ring at all
    float kick_roll = rnd(&quake_seed);
    float kick_x = rnd(&quake_seed), kick_y = rnd(&quake_seed), kick_s = rnd(&quake_seed);
    // only the hard ones, every kick would be 3 rings a second
    if (a.kick && kick_roll < amount * 2.0f && a.hit > 0.9f - 0.3f * amount) {
        Ring rg;
        rg.x = w * (0.15f + 0.7f * kick_x);
        rg.y = h * (0.15f + 0.7f * kick_y);
        rg.r = h * 0.25f;  // starts a bit out, r=0 means you see nothing on the actual hit
        // gone quick so you can tell which kick made it, the sheets stay behind
        rg.speed = h * (3.5f + 3.0f * kick_s);
        rg.strength = amount * (0.4f + 0.6f * a.hit);
        rings.push_back(rg);
        if (rings.size() > 6) rings.erase(rings.begin());
    }
    for (size_t i = 0; i < rings.size();) {
        rings[i].r += rings[i].speed * dt;
        if (rings[i].r > (w + h) * 1.2f) rings.erase(rings.begin() + i);
        else i++;
    }

    float band = h * (0.03f + 0.1f * amount);
    float max_depth = QUAKE_SECONDS * 0.98f;
    // the past catches up. more quake = time stays broken for longer
    float catch_up = pow(0.5f, dt / (0.05f + 1.05f * amount));
    float sink = amount * amount * 0.5f * dt;
    // hats make the sheets finer. only on the kick, every frame it makes all the
    // blocks jump to a different moment
    if (a.kick) quake_levels = 3 + (int)(a.treble * 3);
    int levels = quake_levels;

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

            // low slider = only some chunks take part, they swap on every flip
            unsigned r2 = (((unsigned)(by / 6) * 83492791u) ^ ((unsigned)(bx / 6) * 40503u) ^ ((unsigned)(spectral_kicks / 8) * 2654435761u)) * 2654435761u;
            bool in_on_it = (r2 >> 16) / 65536.0f < amount * 1.6f;

            for (int v = 0; v < 8; v++) {
                for (int u = 0; u < 8; u++) {
                    int i = v * 8 + u;
                    int f = u + v;
                    bool held = spectral_flip ? (f <= split) : (f > split);
                    if (!held || !in_on_it) {
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

// ===== block recursion =====
// on a kick some rectangles fill up with tiny copies of the whole frame. they
// read from the LAST output, which already has the old tiny copies in it, so
// you get copies inside copies inside copies. also written into the rot canvas
// so when a cell dies it doesnt just vanish, it melts away
//
// nearest neighbor, one pixel at a time. slow and jaggy and thats the look

void WarpEngine::block_recursion(uint8_t* data, float amount, const AudioFrame& a) {
    if (amount < 0.01f) return;

    int size = w * h * 3;
    if (!recur_prev) {
        recur_prev = (uint8_t*)malloc(size);
        memcpy(recur_prev, data, size);
    }

    if (a.kick) {
        int n = (int)(amount * 5.0f + rnd(&recur_seed));
        for (int i = 0; i < n; i++) {
            if (cells.size() > 24) break;
            Cell c;
            // size in macroblocks, keeps the frame shape so the copies arent squished
            int mult = 1;
            if (h >= 800) mult = 2;
            c.ch = BLOCK * mult * (2 + rand_r(&recur_seed) % (2 + (int)(amount * 9.0f)));
            c.cw = (c.ch * w / h) / BLOCK * BLOCK;
            if (c.cw < BLOCK) c.cw = BLOCK;
            if (c.ch > h) c.ch = h;
            if (c.cw > w) c.cw = w;
            c.x = (rand_r(&recur_seed) % (w - c.cw + 1)) / BLOCK * BLOCK;
            c.y = (rand_r(&recur_seed) % (h - c.ch + 1)) / BLOCK * BLOCK;
            c.tiles = 1 + rand_r(&recur_seed) % 3;
            c.age = 0;
            c.life = 0.12f + rnd(&recur_seed) * amount * 0.9f;
            cells.push_back(c);
        }
        // the really big hits: whole frame goes for a moment
        if (amount > 0.5f && a.bass > 0.85f && rnd(&recur_seed) < amount * 0.5f) {
            Cell c;
            c.x = 0; c.y = 0;
            c.cw = w; c.ch = h;
            c.tiles = 2 + rand_r(&recur_seed) % 2;
            c.age = 0;
            c.life = 0.1f;
            cells.push_back(c);
        }
    }

    for (size_t i = 0; i < cells.size(); i++) {
        Cell& c = cells[i];
        // slowly fall into the copy while it lives
        float zoom = 1.0f - 0.35f * (c.age / c.life);
        int tile_w = c.cw / c.tiles;
        int tile_h = c.ch / c.tiles;
        if (tile_w < 1) tile_w = 1;
        if (tile_h < 1) tile_h = 1;

        for (int y = 0; y < c.ch; y++) {
            for (int x = 0; x < c.cw; x++) {
                float u = (x % tile_w) / (float)tile_w;
                float v = (y % tile_h) / (float)tile_h;
                int sx = (int)((0.5f + (u - 0.5f) * zoom) * w);
                int sy = (int)((0.5f + (v - 0.5f) * zoom) * h);
                if (sx >= w) sx = w - 1;
                if (sy >= h) sy = h - 1;

                int src = (sy * w + sx) * 3;
                int dst = ((c.y + y) * w + c.x + x) * 3;
                data[dst] = recur_prev[src];
                data[dst+1] = recur_prev[src+1];
                data[dst+2] = recur_prev[src+2];
                canvas[dst] = recur_prev[src];
                canvas[dst+1] = recur_prev[src+1];
                canvas[dst+2] = recur_prev[src+2];
            }
        }
        c.age += dt;
    }

    for (size_t i = 0; i < cells.size();) {
        if (cells[i].age > cells[i].life) cells.erase(cells.begin() + i);
        else i++;
    }

    memcpy(recur_prev, data, size);
}

// ===== texture swap =====
// every pixel = big blurry brightness + fine stuff on top. keep your own
// big brightness and wear the fine stuff of a chunk from somewhere else. road
// with a house on it, colors stay. kicks pick new places to steal from and
// the stolen piece slowly crawls so it looks alive
//
// not in the dct, 8x8 is too small for this. a window is bigger than 8 pixels,
// so its a grid of averages

void WarpEngine::texture_swap(uint8_t* data, float amount, const AudioFrame& a) {
    if (amount < 0.01f) return;

    int cell = 32;
    if (h >= 800) cell = 64;
    int chunk = cell * 3;

    int gw = w / cell + 1, gh = h / cell + 1;
    int cw = w / chunk + 1, ch = h / chunk + 1;

    bool first = tex_off_x.empty();
    if (first) {
        tex_off_x.assign(cw * ch, 0);
        tex_off_y.assign(cw * ch, 0);
    }

    // copy of the luma, we write into data while reading this
    vector<int> lum(w * h);
    for (int i = 0; i < w * h; i++) {
        lum[i] = (data[i*3] + 2 * data[i*3+1] + data[i*3+2]) / 4;
    }

    // average brightness per grid cell
    vector<int> cell_sum(gw * gh, 0), cell_n(gw * gh, 0);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int g = (y / cell) * gw + x / cell;
            cell_sum[g] += lum[y * w + x];
            cell_n[g]++;
        }
    }
    vector<int> cell_avg(gw * gh, 0);
    for (int i = 0; i < gw * gh; i++) {
        if (cell_n[i] > 0) cell_avg[i] = cell_sum[i] / cell_n[i];
    }

    // pick new places. tries a few random spots and keeps the one with the
    // most going on, stealing from flat sky just gives you a blurry road
    if (a.kick || first) {
        tex_kicks++;
        float reroll = 0.15f + 0.35f * a.bass;
        if (first || tex_kicks % 8 == 0) reroll = 1.0f;

        for (int i = 0; i < cw * ch; i++) {
            if (rnd(&tex_seed) > reroll) continue;
            float best = -1;
            int best_x = 0, best_y = 0;
            for (int t = 0; t < 6; t++) {
                int sx = rand_r(&tex_seed) % w;
                int sy = rand_r(&tex_seed) % h;
                // how busy is it there, only looks at every 4th pixel
                float busy = 0;
                for (int yy = sy; yy < sy + chunk && yy < h; yy += 4) {
                    for (int xx = sx; xx < sx + chunk && xx < w; xx += 4) {
                        busy += abs(lum[yy * w + xx] - cell_avg[(yy / cell) * gw + xx / cell]);
                    }
                }
                if (busy > best) {
                    best = busy;
                    best_x = sx;
                    best_y = sy;
                }
            }
            tex_off_x[i] = best_x;
            tex_off_y[i] = best_y;
        }
    }

    float gain = 1.0f + amount * 0.4f;
    int kick_group = tex_kicks / 8;
    int crawl = (int)frame_crawl;

    #pragma omp parallel for schedule(dynamic, 8)
    for (int y = 0; y < h; y++) {
        int cy = y / chunk;
        for (int x = 0; x < w; x++) {
            int cx = x / chunk;

            // low slider = only some chunks join in
            unsigned hh = ((unsigned)cy * 73856093u) ^ ((unsigned)cx * 19349663u) ^ ((unsigned)kick_group * 83492791u);
            hh = hh * 2654435761u;
            if ((hh >> 16) / 65536.0f > amount * 1.6f) continue;

            int ci = cy * cw + cx;
            int sx = (tex_off_x[ci] + (x - cx * chunk) + crawl) % w;
            int sy = (tex_off_y[ci] + (y - cy * chunk)) % h;

            int own_low = cell_avg[(y / cell) * gw + x / cell];
            int src_low = cell_avg[(sy / cell) * gw + sx / cell];
            int stolen = (int)((lum[sy * w + sx] - src_low) * gain);

            int dl = own_low + stolen - lum[y * w + x];
            int di = (y * w + x) * 3;
            data[di]   = clamp8(data[di] + dl);
            data[di+1] = clamp8(data[di+1] + dl);
            data[di+2] = clamp8(data[di+2] + dl);
        }
    }

    // pixels per second, treble speeds it up
    frame_crawl += (10.0f + a.treble * 40.0f) * scale * 2.0f * dt;
}

// ===== hall of mirrors =====
// what happens when you noclip out of the map. nothing clears the screen, so
// the world stays where it was and whatever moves gets drawn on top over and
// over and leaves copies of itself
//
// problem: on a drone shot EVERYTHING moves. so first guess what the camera is
// doing (fit a flat plane through all the vectors, x and y separately), then
// only stuff that moves DIFFERENT from that counts as an object

void WarpEngine::hall_of_mirrors(uint8_t* data, float amount, const AudioFrame& a) {
    if (amount < 0.01f) return;

    int size = w * h * 3;
    int chunk = h / 8;
    if (chunk < 16) chunk = 16;
    int cw = w / chunk + 1, ch = h / chunk + 1;

    if (!hom_buf) {
        hom_buf = (uint8_t*)malloc(size);
        hom_ref = (uint8_t*)malloc(w * h);
        hom_ref_tmp = (uint8_t*)malloc(w * h);
        memcpy(hom_buf, data, size);
        memcpy(hom_ref, luma, w * h);
        hom_age.assign(cw * ch, 0.0f);
        return;
    }

    // which blocks can we even trust
    // flat blocks (sky, road) dont know where they went, their vectors are junk
    vector<char> usable(gx * gy, 0);
    for (int by = 0; by < gy; by++) {
        for (int bx = 0; bx < gx; bx++) {
            int lo = 255, hi = 0;
            for (int y = 0; y < BLOCK; y += 2) {
                for (int x = 0; x < BLOCK; x += 2) {
                    int v = luma[(by * BLOCK + y) * w + bx * BLOCK + x];
                    if (v < lo) lo = v;
                    if (v > hi) hi = v;
                }
            }
            if (hi - lo > 24 && block_err[by * gx + bx] < 30) usable[by * gx + bx] = 1;
        }
    }

    // fit the camera
    // vx = ax*x + bx*y + cx, vy = ay*x + by*y + cy. least squares written out
    // by hand, twice, second time without the blocks that didnt fit the first
    float ax = 0, bx_ = 0, cx_ = 0, ay = 0, by_ = 0, cy_ = 0;
    vector<char> inlier = usable;
    for (int pass = 0; pass < 2; pass++) {
        double sxx = 0, sxy = 0, syy = 0, sx = 0, sy = 0, n = 0;
        double sxu = 0, syu = 0, su = 0, sxv = 0, syv = 0, sv = 0;
        for (int by = 0; by < gy; by++) {
            for (int bx = 0; bx < gx; bx++) {
                int i = by * gx + bx;
                if (!inlier[i]) continue;
                double x = bx, y = by;
                double u = mvx[i], v = mvy[i];
                sxx += x * x; sxy += x * y; syy += y * y;
                sx += x; sy += y; n += 1;
                sxu += x * u; syu += y * u; su += u;
                sxv += x * v; syv += y * v; sv += v;
            }
        }
        double det = sxx * (syy * n - sy * sy) - sxy * (sxy * n - sy * sx) + sx * (sxy * sy - syy * sx);
        if (n < 12 || fabs(det) < 1e-3) break;  // nothing to go on, camera stays 0

        ax  = (sxu * (syy * n - sy * sy) - sxy * (syu * n - sy * su) + sx * (syu * sy - syy * su)) / det;
        bx_ = (sxx * (syu * n - sy * su) - sxu * (sxy * n - sy * sx) + sx * (sxy * su - syu * sx)) / det;
        cx_ = (sxx * (syy * su - sy * syu) - sxy * (sxy * su - sx * syu) + sxu * (sxy * sy - syy * sx)) / det;
        ay  = (sxv * (syy * n - sy * sy) - sxy * (syv * n - sy * sv) + sx * (syv * sy - syy * sv)) / det;
        by_ = (sxx * (syv * n - sy * sv) - sxv * (sxy * n - sy * sx) + sx * (sxy * sv - syv * sx)) / det;
        cy_ = (sxx * (syy * sv - sy * syv) - sxy * (sxy * sv - sx * syv) + sxv * (sxy * sy - syy * sx)) / det;

        // throw out whatever is far from the fit
        for (int by = 0; by < gy; by++) {
            for (int bx = 0; bx < gx; bx++) {
                int i = by * gx + bx;
                float dx = mvx[i] - (ax * bx + bx_ * by + cx_);
                float dy = mvy[i] - (ay * bx + by_ * by + cy_);
                inlier[i] = usable[i] && fabs(dx) + fabs(dy) < 6.0f;
            }
        }
    }

    // what moves different from the camera
    vector<float> off(gx * gy, 0.0f);
    vector<float> sorted;
    for (int by = 0; by < gy; by++) {
        for (int bx = 0; bx < gx; bx++) {
            int i = by * gx + bx;
            if (!usable[i]) continue;
            float dx = mvx[i] - (ax * bx + bx_ * by + cx_);
            float dy = mvy[i] - (ay * bx + by_ * by + cy_);
            off[i] = fabs(dx) + fabs(dy);
            sorted.push_back(off[i]);
        }
    }
    // never more than the top ~15% counts, the plane fit isnt great on a drone
    // and otherwise half the buildings count as objects
    float limit = 3.0f + 3.0f * scale;
    if (!sorted.empty()) {
        sort(sorted.begin(), sorted.end());
        float top = sorted[(size_t)(sorted.size() * 0.85f)];
        if (top > limit) limit = top;
    }

    vector<char> obj(gx * gy, 0);
    for (int by = 0; by < gy; by++) {
        for (int bx = 0; bx < gx; bx++) {
            int i = by * gx + bx;
            if (off[i] <= limit) continue;
            // lonely blocks are nearly always wrong matches
            int friends = 0;
            for (int j = max(0, by - 1); j <= min(gy - 1, by + 1); j++) {
                for (int k = max(0, bx - 1); k <= min(gx - 1, bx + 1); k++) {
                    if (off[j * gx + k] > limit) friends++;
                }
            }
            if (friends >= 3) obj[i] = 1;  // counts itself
        }
    }
    // grow twice, the middle of a car is flat so it never gets a vector
    vector<char> grown = obj;
    for (int ring = 0; ring < 2; ring++) {
        vector<char> before = grown;
        for (int by = 0; by < gy; by++) {
            for (int bx = 0; bx < gx; bx++) {
                if (!before[by * gx + bx]) continue;
                for (int j = max(0, by - 1); j <= min(gy - 1, by + 1); j++) {
                    for (int k = max(0, bx - 1); k <= min(gx - 1, bx + 1); k++) {
                        grown[j * gx + k] = 1;
                    }
                }
            }
        }
    }

    // move the background guess along with the camera
    // compare with the empty world and not the last frame. the spot an object just
    // left changed too and would get redrawn with road
    for (int y = 0; y < h; y++) {
        float yb = y / (float)BLOCK;
        for (int x = 0; x < w; x++) {
            float xb = x / (float)BLOCK;
            int qx = x + (int)floor(ax * xb + bx_ * yb + cx_ + 0.5f);
            int qy = y + (int)floor(ay * xb + by_ * yb + cy_ + 0.5f);
            if (qx < 0) qx = 0;
            if (qx >= w) qx = w - 1;
            if (qy < 0) qy = 0;
            if (qy >= h) qy = h - 1;
            hom_ref_tmp[y * w + x] = hom_ref[qy * w + qx];
        }
    }
    if (a.cut) memcpy(hom_ref_tmp, luma, w * h);  // new world, nothing to compare with

    // draw the objects into the buffer that never clears
    // pixel by pixel so you get the outline of the car and not a square
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int i = y * w + x;
            int gb = (y / BLOCK) * gx + x / BLOCK;
            bool in_grown = y / BLOCK < gy && x / BLOCK < gx && grown[gb];
            int diff = abs(luma[i] - hom_ref_tmp[i]);

            if (in_grown && diff > 18) {
                hom_buf[i*3] = data[i*3];
                hom_buf[i*3+1] = data[i*3+1];
                hom_buf[i*3+2] = data[i*3+2];
                // object stays out of the background guess. almost, or a
                // car that parks would count as moving forever
                hom_ref[i] = (hom_ref_tmp[i] * 99 + luma[i]) / 100;
            } else {
                hom_ref[i] = (hom_ref_tmp[i] * 4 + luma[i]) / 5;
            }
        }
    }

    // clearing
    // more slider = world stays stale for longer. clears land on kicks
    float hold = 0.1f + amount * amount * 6.0f;
    for (int i = 0; i < cw * ch; i++) {
        hom_age[i] += dt;
        bool clear = false;
        if (a.kick && hom_age[i] > hold * (0.8f + 0.4f * rnd(&hom_seed))) clear = true;
        if (hom_age[i] > hold * 2.0f) clear = true;  // no kicks around, cant wait forever
        if (!clear) continue;

        hom_age[i] = 0;
        int x0 = (i % cw) * chunk, y0 = (i / cw) * chunk;
        for (int y = y0; y < y0 + chunk && y < h; y++) {
            int len = chunk;
            if (x0 + len > w) len = w - x0;
            if (len > 0) memcpy(hom_buf + (y * w + x0) * 3, data + (y * w + x0) * 3, len * 3);
        }
    }

    memcpy(data, hom_buf, size);
}

// ===== coming back after a section off =====

void WarpEngine::wake_quake() {
    // ring_pos = 0;
    // ring_count = 0;
    rings.clear();
    depth.assign(gx * gy, 0);
}

void WarpEngine::wake_spectral() {
    coef_ready = false;
}

void WarpEngine::wake_recursion() {
    cells.clear();
    free(recur_prev);
    recur_prev = nullptr;
}

void WarpEngine::wake_mirrors() {
    free(hom_buf);
    free(hom_ref);
    free(hom_ref_tmp);
    hom_buf = nullptr;
    hom_ref = nullptr;
    hom_ref_tmp = nullptr;
}

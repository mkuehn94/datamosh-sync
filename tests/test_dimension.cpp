#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>
#include "../src/warp.h"
#include "../src/arrange.h"

using std::vector;

static vector<uint8_t> flat_fusion(float amount, int width = 160, int height = 96) {
    WarpEngine main, other;
    main.init(width, height, 30);
    other.init(width, height, 30);
    vector<uint8_t> clean(width * height * 3, 30), donor(clean.size(), 180);
    vector<uint8_t> frame = clean;
    const AudioFrame quiet = {};
    for (int i = 0; i < 6; i++) {
        frame = clean;
        main.look_at_other(donor.data(), &other, amount);
        main.rot_and_warp(frame.data(), 0, 0, 0, quiet);
        int expected = amount <= 0 ? 30 : (amount >= 1 ? 105 : 68);
        // Initialization, healing, residuals, and non-block-aligned tails must
        // all use the same balance. No flat source-colored islands anywhere.
        assert(std::all_of(frame.begin(), frame.end(), [expected](uint8_t v) { return v == expected; }));
    }
    return frame;
}

static vector<vector<uint8_t>> sequence(bool swap, float amount, bool attach = true) {
    const int w = 163, h = 99;
    WarpEngine main, other;
    main.init(w, h, 30);
    other.init(w, h, 30);
    vector<uint8_t> a(w * h * 3), b(a.size());
    vector<vector<uint8_t>> result;
    for (int t = 0; t < 14; t++) {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                for (int c = 0; c < 3; c++) {
                    int i = (y * w + x) * 3 + c;
                    a[i] = ((x + 2*t) * 3 + y * 7 + c * 41) % 256;
                    b[i] = (x * 11 + (y + t) * 5 + c * 29 + (t >= 7 ? 90 : 0)) % 256;
                }
            }
        }
        vector<uint8_t> frame = swap ? b : a;
        const vector<uint8_t>& donor = swap ? a : b;
        main.estimate_motion(frame.data());
        other.estimate_motion(donor.data());
        if (attach) main.look_at_other(donor.data(), &other, amount);
        AudioFrame audio = {0.7f, 0.8f, 0.4f, t % 3 == 0, 0.9f, main.scene_cut()};
        main.rot_and_warp(frame.data(), 0.35f, 0.2f, 0.3f, audio);
        result.push_back(frame);
    }
    return result;
}

int main() {
    flat_fusion(0);
    flat_fusion(-1);
    flat_fusion(0.5f);
    flat_fusion(1);
    flat_fusion(2);
    flat_fusion(1, 163, 99);

    // Stronger than checking a single mask: initialization, motion, residuals,
    // healing, sampler and scene-cut handling must stay symmetric over time.
    vector<vector<uint8_t>> ab = sequence(false, 1);
    vector<vector<uint8_t>> ba = sequence(true, 1);
    assert(ab == ba);
    assert(ab.front() != ab.back());  // still evolving, not a frozen blend
    assert(sequence(false, 0) == sequence(false, 0, false));

    // Drops, breaths and every other song role must not elect an active world.
    Section s = {};
    s.start = 1;
    s.end = 5;
    for (int role = 0; role < ROLE_COUNT; role++) {
        s.role = role;
        for (float time : {1.0f, 3.0f, 5.0f}) {
            assert(dimension_at(s, time, 1) == 1);
            assert(dimension_at(s, time, 0.5f) == 0.5f);
            assert(dimension_at(s, time, 0) == 0);
        }
    }
    assert(dimension_at(s, 3, 2) == 1);
    assert(dimension_at(s, 3, -1) == 0);
    std::puts("dimension fusion ok");
}

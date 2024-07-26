// who plays when. see arrange.h

#include "arrange.h"
#include <cmath>
#include <algorithm>

using namespace std;

const char* fx_name(int fx) {
    static const char* names[FX_COUNT] = {
        "glitch", "blur", "void", "stuck", "beat sync",
        "rot", "warp", "quake", "spectral", "sampler",
        "texswap", "recursion", "mirrors"
    };
    return names[fx];
}

const char* role_name(int role) {
    static const char* names[ROLE_COUNT] = {
        "", "lead-in", "build", "after", "breakdown", "CLIMAX", "DROP", "breath"
    };
    return names[role];
}

float hardness_at(const Section& s, float time) {
    float f = (time - s.start) / (s.end - s.start);
    if (f < 0) f = 0;
    if (f > 1) f = 1;
    return s.hard_start + (s.hard_end - s.hard_start) * f;
}

// who wants what kind of moment
static bool likes_drop(int fx) {
    return fx == FX_QUAKE || fx == FX_RECURSION || fx == FX_WARP || fx == FX_VOID
        || fx == FX_BEATSYNC || fx == FX_GLITCH;
}
static bool likes_build(int fx) {
    return fx == FX_ROT || fx == FX_WARP || fx == FX_GROOVE || fx == FX_HOM;
}
// the part before the first big moment. smooth stuff that slowly goes wrong, the
// sharp ones wait for the drop or the drop isnt a surprise anymore
static bool likes_leadin(int fx) {
    return fx == FX_ROT || fx == FX_SPECTRAL || fx == FX_HOM || fx == FX_GROOVE || fx == FX_BLUR;
}
static bool likes_breakdown(int fx) {
    return fx == FX_ROT || fx == FX_BLUR || fx == FX_SPECTRAL;
}

// what kind of moment does every effect want
//                             energy bass treble busy motion rise
static const float taste[FX_COUNT][F_COUNT] = {
    /* glitch: loud + hats    */ { 1.0f,  0.0f,  0.8f,  0.5f,  0.0f,  0.0f},
    /* blur: quiet smear      */ {-0.8f,  0.0f, -0.3f, -0.5f,  0.4f,  0.0f},
    /* void: right after build*/ { 0.2f,  0.5f, -0.4f,  0.0f,  0.0f,  0.6f},
    /* stuck: busy stuff      */ { 0.5f,  0.0f,  0.5f,  0.8f,  0.0f,  0.0f},
    /* beat sync: needs kicks */ { 0.6f,  1.0f,  0.0f,  0.8f,  0.0f,  0.0f},
    /* rot: melts when calm   */ {-0.5f,  0.2f, -0.6f, -0.4f,  0.8f,  0.0f},
    /* warp: bass + movement  */ { 0.3f,  1.0f, -0.2f,  0.0f,  1.0f,  0.5f},
    /* quake: the big hits    */ { 0.6f,  1.0f,  0.0f,  0.2f,  0.2f,  0.8f},
    /* spectral: quiet + airy */ {-0.6f, -0.6f,  0.6f, -0.2f,  0.0f,  0.0f},
    /* sampler: steady groove */ { 0.2f,  0.2f,  0.0f,  1.0f,  0.6f,  0.0f},
    /* texswap: hats          */ { 0.0f, -0.4f,  1.0f,  0.5f,  0.0f,  0.0f},
    /* recursion: kick drums  */ { 0.8f,  1.0f,  0.2f,  0.5f, -0.3f,  0.4f},
    /* mirrors: needs movers  */ { 0.0f,  0.2f, -0.2f,  0.0f,  1.2f,  0.3f},
};

// how violent is every effect at slider 0 / .25 / .5 / .75 / 1. measured: every
// effect alone on 3 clips of the drone video, half "how much of
// the picture is wrecked" and half "how hard does it jump between frames" (98th
// percentile, so beat sync counts for its hits and not its average). then ^0.7 or
// everything except quake sits at the bottom. 5 sec clips, so rot is a bit
// underrated, it would get worse if it had longer
static const float violence[FX_COUNT][5] = {
    /* glitch    */ {0.00f, 0.08f, 0.11f, 0.13f, 0.14f},
    /* blur      */ {0.00f, 0.07f, 0.11f, 0.14f, 0.18f},
    /* void      */ {0.00f, 0.05f, 0.07f, 0.10f, 0.13f},
    /* stuck     */ {0.13f, 0.13f, 0.13f, 0.13f, 0.13f},  // doesnt get worse with its slider, only longer
    /* beat sync */ {0.00f, 0.32f, 0.44f, 0.53f, 0.60f},
    /* rot       */ {0.00f, 0.07f, 0.13f, 0.22f, 0.32f},
    /* warp      */ {0.00f, 0.17f, 0.30f, 0.42f, 0.54f},
    /* quake     */ {0.00f, 0.55f, 0.94f, 0.99f, 1.00f},
    /* spectral  */ {0.00f, 0.14f, 0.23f, 0.28f, 0.29f},
    /* sampler   */ {0.00f, 0.17f, 0.29f, 0.41f, 0.44f},
    /* texswap   */ {0.00f, 0.31f, 0.47f, 0.56f, 0.59f},
    /* recursion */ {0.00f, 0.08f, 0.15f, 0.25f, 0.44f},
    /* mirrors   */ {0.00f, 0.48f, 0.45f, 0.48f, 0.48f},
};

float aggression(int fx, float amount, float void_tol) {
    if (amount <= 0) return 0;
    if (amount > 1) amount = 1;
    float pos = amount * 4;
    int i = (int)pos;
    if (i > 3) i = 3;
    float a = violence[fx][i] + (violence[fx][i+1] - violence[fx][i]) * (pos - i);
    // low tolerance = way bigger voids. measured 0 against 65
    if (fx == FX_VOID) a *= 1.0f + 1.16f * (65.0f - void_tol) / 65.0f;
    return a > 1 ? 1 : a;
}

static float how_much_it_fits(int fx, const Section& s) {
    float sum = 0, total = 0;
    for (int f = 0; f < F_COUNT; f++) {
        sum += taste[fx][f] * (s.feat[f] - 0.5f);
        total += fabs(taste[fx][f]);
    }
    return sum / total;
}

// how violent is a whole section. NOT just adding up, the picture can only get
// so wrecked. second effect on an already destroyed picture adds less
static float section_violence(const Section& s, const vector<float>& w) {
    float clean = 1;
    for (int fx = 0; fx < FX_COUNT; fx++) {
        // the .7 is so one effect can never "fill" a section by itself. quake
        // alone measures near 1 and then everything else left the climax
        if (s.on[fx]) clean *= 1.0f - w[fx] * 0.7f;
    }
    return 1.0f - clean;
}

static float taste_score(int fx, const Section& s) {
    float t = how_much_it_fits(fx, s);
    if (s.role == ROLE_DROP && likes_drop(fx)) t += 0.3f;
    if (s.role == ROLE_BUILD && likes_build(fx)) t += 0.15f;
    if (s.role == ROLE_BREAKDOWN) t += likes_breakdown(fx) ? 0.25f : -0.3f;
    return t;
}

// the first pass deals out sections one effect at a time, so the last ones get
// the leftovers and it can never fix a section that ended up empty. so: every
// section gets a target for how violent it should be, then keep swapping
// sections between effects as long as that gets closer to the targets.
// not optimal, stops at the first spot where no single swap helps. good enough
static void polish(vector<Section>& sections, const vector<vector<float> >& wants,
                   const float* parts, const bool* used) {
    int n = sections.size();
    float song = sections.back().end - sections[0].start;

    // sharpen the music curve then scale it to what the sliders can actually
    // pay for. target the budget cant reach is useless
    vector<float> target(n);
    float have = 0, want = 0;
    for (int i = 0; i < n; i++) {
        float len = sections[i].end - sections[i].start;
        float m = sections[i].music;
        target[i] = m * m;
        have += len * section_violence(sections[i], wants[i]);
        want += len * target[i];
    }
    float k = want > 0.001f ? have / want : 1;
    for (int i = 0; i < n; i++) {
        target[i] *= k;
        int r = sections[i].role;
        if (r == ROLE_DROP || r == ROLE_CLIMAX) target[i] = k * 1.3f;
        // quiet parts still want a little something gentle, just not nothing.
        // the breath is the only place thats supposed to be empty
        if (target[i] < 0.12f && r != ROLE_BREATH) target[i] = 0.12f;
        // quiet music doesnt mean nothing happens. first impression of the
        // whole video, this one doesnt get scaled down with the rest
        if (r == ROLE_LEADIN) target[i] = sections[i].music;
        if (target[i] > 1) target[i] = 1;
    }

    #define MISS(i) ((sections[i].end - sections[i].start) * \
                     (section_violence(sections[i], wants[i]) - target[i]) * \
                     (section_violence(sections[i], wants[i]) - target[i]))

    for (int pass = 0; pass < 40; pass++) {
        bool changed = false;
        for (int fx = 0; fx < FX_COUNT; fx++) {
            if (!used[fx] || parts[fx] < 0.005f || parts[fx] > 0.995f) continue;

            float goal = parts[fx] * song;
            float covered = 0;
            for (int i = 0; i < n; i++) {
                if (sections[i].on[fx]) covered += sections[i].end - sections[i].start;
            }

            for (int a = 0; a < n; a++) {
                if (!sections[a].on[fx]) continue;
                for (int b = 0; b < n; b++) {
                    if (sections[b].on[fx] || sections[b].role == ROLE_BREATH) continue;
                    if (sections[b].role == ROLE_LEADIN && !likes_leadin(fx)) continue;

                    float la = sections[a].end - sections[a].start;
                    float lb = sections[b].end - sections[b].start;
                    float new_cov = covered - la + lb;
                    // percent has to stay about right, thats the deal with the slider
                    if (fabs(new_cov - goal) > max(fabs(covered - goal), 0.03f * song)) continue;

                    float before = MISS(a) + MISS(b)
                                 - 0.02f * (la * taste_score(fx, sections[a]));
                    sections[a].on[fx] = false;
                    sections[b].on[fx] = true;
                    float after = MISS(a) + MISS(b)
                                - 0.02f * (lb * taste_score(fx, sections[b]));

                    if (after < before - 0.0001f) {
                        covered = new_cov;
                        changed = true;
                        break;  // a is off now, on to the next one
                    }
                    sections[a].on[fx] = true;
                    sections[b].on[fx] = false;
                }
            }
        }
        if (!changed) break;
    }
}

// turn it down until it fits the moment. how hard slider stays the ceiling, and it
// never goes under a quarter or you cant see the effect anymore
static float scale_for(int fx, float amount, float void_tol, float limit) {
    if (fx == FX_STUCK || amount <= 0) return 1;
    for (float s = 1.0f; s > 0.25f; s -= 0.02f) {
        if (aggression(fx, amount * s, void_tol) <= limit) return s;
    }
    return 0.25f;
}

void assign_parts(vector<Section>& sections, const float* parts, const bool* used,
                  const float* amounts, float void_tol) {
    if (sections.empty()) return;

    for (size_t i = 0; i < sections.size(); i++) {
        for (int fx = 0; fx < FX_COUNT; fx++) {
            sections[i].on[fx] = false;
            sections[i].fx_scale[fx] = 1;
            sections[i].fx_aggro[fx] = 0;
        }
    }

    // what every effect would do in every section, after turning it down
    // [section][fx]. calculated up front because the loop below asks a lot
    vector<vector<float> > wants(sections.size(), vector<float>(FX_COUNT, 0.0f));
    for (size_t i = 0; i < sections.size(); i++) {
        Section& s = sections[i];
        float hard = (s.hard_start + s.hard_end) / 2;
        for (int fx = 0; fx < FX_COUNT; fx++) {
            if (!used[fx]) continue;
            // drop and climax are supposed to be too much. everywhere else a
            // bit over the music is ok, way over isnt
            bool anything_goes = s.role == ROLE_DROP || s.role == ROLE_CLIMAX;
            if (!anything_goes) s.fx_scale[fx] = scale_for(fx, amounts[fx] * hard, void_tol, s.music + 0.25f);
            wants[i][fx] = aggression(fx, amounts[fx] * hard * s.fx_scale[fx], void_tol);
        }
    }

    float song = sections.back().end - sections[0].start;
    vector<int> crowd(sections.size(), 0);
    vector<float> violent_already(sections.size(), 0.0f);

    // pickiest effect chooses first
    int order[FX_COUNT];
    for (int i = 0; i < FX_COUNT; i++) order[i] = i;
    for (int i = 0; i < FX_COUNT; i++) {
        for (int j = 0; j + 1 < FX_COUNT; j++) {
            if (parts[order[j]] > parts[order[j+1]]) {
                int t = order[j]; order[j] = order[j+1]; order[j+1] = t;
            }
        }
    }

    for (int o = 0; o < FX_COUNT; o++) {
        int fx = order[o];
        if (!used[fx] || parts[fx] < 0.005f) continue;

        if (parts[fx] > 0.995f) {
            for (size_t i = 0; i < sections.size(); i++) {
                sections[i].on[fx] = true;
                crowd[i]++;
                violent_already[i] += wants[i][fx];
            }
            continue;
        }

        float target = parts[fx] * song;
        float covered = 0;
        float at_climax = 0;
        while (covered < target) {
            int best = -1;
            float best_score = -1000;
            for (size_t i = 0; i < sections.size(); i++) {
                if (sections[i].on[fx]) continue;
                int role = sections[i].role;
                float len = sections[i].end - sections[i].start;
                // moment of nothing right before the hit, thats why the hit hits
                if (role == ROLE_BREATH) continue;
                if (role == ROLE_LEADIN && !likes_leadin(fx)) continue;
                // climax gets a lot but not everything or the whole rest of
                // the song is empty
                if (role == ROLE_CLIMAX && covered > 0 && at_climax + len > target * 0.5f) continue;
                // dont take a piece that lands further from the target than
                // stopping here would. first piece always goes, slider isnt 0
                if (covered > 0 && covered + len - target > target - covered) continue;

                float score = how_much_it_fits(fx, sections[i]);
                // already full of other effects? thats the "just effects on
                // top of each other" thing, go somewhere else. EXCEPT the climax,
                // thats the one place where piling everything up is the point
                if (role == ROLE_CLIMAX) score += 0.25f + 0.03f * crowd[i];
                else score -= 0.05f * crowd[i];

                // the actual point: how violent is this effect at this slider
                // against how intense is the music here. too much is worse
                // than too little, a gentle effect on a loud part is fine
                float a = wants[i][fx];
                float m = sections[i].music;
                score -= a > m ? 0.6f * (a - m) : 0.2f * (m - a);
                // and every section only has so much violence to give. climax
                // doesnt care, thats its job
                float over = violent_already[i] + a - (0.2f + 1.2f * m);
                if (role != ROLE_CLIMAX && role != ROLE_DROP && over > 0) score -= 0.5f * over;

                if (role == ROLE_DROP && likes_drop(fx)) score += 1.0f;
                if (role == ROLE_BUILD && likes_build(fx)) score += 0.15f;
                if (role == ROLE_BREAKDOWN) score += likes_breakdown(fx) ? 0.25f : -0.3f;
                if (role == ROLE_AFTER) score -= 0.1f;  // thins out
                // and try to stay together, 2 sec on 2 sec off just flickers
                if (i > 0 && sections[i-1].on[fx]) score += 0.05f;
                if (i + 1 < sections.size() && sections[i+1].on[fx]) score += 0.05f;

                if (score > best_score) {
                    best_score = score;
                    best = (int)i;
                }
            }
            if (best < 0) break;
            sections[best].on[fx] = true;
            crowd[best]++;
            violent_already[best] += wants[best][fx];
            covered += sections[best].end - sections[best].start;
            if (sections[best].role == ROLE_CLIMAX) at_climax += sections[best].end - sections[best].start;
        }
    }

    polish(sections, wants, parts, used);

    for (size_t i = 0; i < sections.size(); i++) {
        for (int fx = 0; fx < FX_COUNT; fx++) {
            sections[i].fx_aggro[fx] = sections[i].on[fx] ? wants[i][fx] : 0;
        }
    }
}

const Section* section_at(const vector<Section>& sections, float time) {
    if (sections.empty()) return nullptr;
    for (size_t i = 0; i < sections.size(); i++) {
        if (time < sections[i].end) return &sections[i];
    }
    return &sections.back();  // video a bit longer than the audio
}

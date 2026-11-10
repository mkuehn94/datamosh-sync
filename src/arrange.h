#ifndef ARRANGE_H
#define ARRANGE_H

#include <vector>

// the "parts" sliders. the song gets cut into sections and every effect only
// plays in the sections that fit it, like tracks in a daw. slider at 1 = plays
// the whole time, .3 = plays in about 30% of the song
//
// no ffmpeg or gtk in here, the gui compiles this file too so what
// you see on the track is the exact same decision the render makes

enum Fx {
    FX_GLITCH, FX_BLUR, FX_VOID, FX_STUCK, FX_BEATSYNC,
    FX_ROT, FX_WARP, FX_QUAKE, FX_SPECTRAL, FX_GROOVE,
    FX_TEXSWAP, FX_RECURSION, FX_HOM,
    FX_COUNT
};

enum Feat { F_ENERGY, F_BASS, F_TREBLE, F_BUSY, F_MOTION, F_RISE, F_COUNT };

// what this part of the song is doing. order matters, higher wins if a section
// could be two things
enum Role { ROLE_NORMAL, ROLE_LEADIN, ROLE_BUILD, ROLE_AFTER, ROLE_BREAKDOWN, ROLE_CLIMAX, ROLE_DROP, ROLE_BREATH, ROLE_COUNT };

struct Section {
    float start, end;
    float feat[F_COUNT];   // all 0-1, relative to the rest of this song
    bool on[FX_COUNT];
    int role;
    // multiplies the "how hard" sliders, goes from start to end across the section
    float hard_start, hard_end;
    float music;                // how intense is the song right here, 0-1
    float fx_scale[FX_COUNT];   // effect too violent for this moment gets turned down
    float fx_aggro[FX_COUNT];   // how violent it really ends up, 0-1. the track draws this
    int world;                  // reserved legacy track field; always 0
};

const char* fx_name(int fx);
const char* role_name(int role);
float hardness_at(const Section& s, float time);
// Fixed fusion balance: 0 = first video, 1 = equal fusion. No role-based gating.
float dimension_at(const Section& s, float time, float cross);
float aggression(int fx, float amount, float void_tol);
void arrange_two_videos(bool two);  // effects are more violent with two, other table
// the slider is the mean, the music moves it around that. z = how intense right
// now, in standard deviations, swing = the global rise and fall slider
float fx_spread(int fx);
float swung_value(int fx, float mean, float z, float swing);
// amounts = the how hard sliders, stuck goes by the glitch slider
void assign_parts(std::vector<Section>& sections, const float* parts, const bool* used,
                  const float* amounts, float void_tol);
// the AUTO button. finds good "how much of the song" values for this song and these
// how hard sliders. parts = where to start, gets overwritten. returns how bad it is
float track_cost(const std::vector<Section>& sections, const bool* used);
float auto_parts(std::vector<Section>& sections, float* parts, const bool* used,
                 const float* amounts, float void_tol);
const Section* section_at(const std::vector<Section>& sections, float time);

#endif

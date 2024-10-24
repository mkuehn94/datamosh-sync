#ifndef AUDIO_ANALYZER_H
#define AUDIO_ANALYZER_H

#include <string>
#include <vector>
#include "arrange.h"

// beat detection and audio analysis
class AudioAnalyzer {
public:
    AudioAnalyzer();
    ~AudioAnalyzer();

    bool load(const std::string& path);
    bool load_from_video(const std::string& video_path);  // extract audio from video
    void analyze();

    std::vector<float> get_beat_times() { return beat_times; }
    float get_duration() { return duration; }
    void limit_to(float seconds);  // two videos: nothing after the shorter one matters
    float get_current_energy(float time);
    float get_bass(float time);    // kick drum curve, 0-1, snappy
    float get_treble(float time);  // hats/snare curve, 0-1, snappy

private:
    float curve_at(const std::vector<float>& c, float time);

    std::vector<float> samples;  // mono float pcm
    std::vector<float> beat_times;
    std::vector<float> energy_curve;
    std::vector<float> bass_curve;
    std::vector<float> treble_curve;
    float duration;
};

// listens to the song + looks at the video and cuts it into sections
std::vector<Section> cut_song(AudioAnalyzer* audio, const std::string& video_path);
// how intense is the song, 20 values per second, mean 0 std 1. what makes the
// effects rise and fall
std::vector<float> intensity_curve(AudioAnalyzer* audio);
void print_sections(const std::vector<Section>& sections);  // the gui reads this
void print_track(const std::vector<Section>& sections);     // for the terminal

#endif

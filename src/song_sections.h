#ifndef SONG_SECTIONS_H
#define SONG_SECTIONS_H

#include "arrange.h"
#include "audio_analyzer.h"
#include <string>
#include <vector>

// listens to the song + looks at the video and cuts it into sections
std::vector<Section> cut_song(AudioAnalyzer* audio, const std::string& video_path);
// how intense is the song, 20 values per second, mean 0 std 1. what makes the
// effects rise and fall
std::vector<float> intensity_curve(AudioAnalyzer* audio);
void print_sections(const std::vector<Section>& sections);  // the gui reads this
void print_track(const std::vector<Section>& sections);     // for the terminal

#endif

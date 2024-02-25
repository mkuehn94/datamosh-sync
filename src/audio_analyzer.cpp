#include "audio_analyzer.h"
#include <iostream>
#include <cmath>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

using namespace std;

AudioAnalyzer::AudioAnalyzer() : duration(0) {
}

AudioAnalyzer::~AudioAnalyzer() {
}

bool AudioAnalyzer::load(const string& path) {
    // TODO: actually load audio file with ffmpeg
    cout << "Loading audio: " << path << endl;
    return true;
}

void AudioAnalyzer::analyze() {
    // very simple beat detection
    // TODO: make this better

    cout << "Analyzing audio..." << endl;

    // fake some beats for now
    for (float t = 0.5; t < 10.0; t += 0.5) {
        beat_times.push_back(t);
    }

    cout << "Found " << beat_times.size() << " beats" << endl;
}

float AudioAnalyzer::get_current_energy(float time) {
    // return fake energy for now
    return 0.5 + 0.5 * sin(time * 2.0);
}

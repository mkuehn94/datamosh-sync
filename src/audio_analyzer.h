#ifndef AUDIO_ANALYZER_H
#define AUDIO_ANALYZER_H

#include <string>
#include <vector>

// beat detection and audio analysis
class AudioAnalyzer {
public:
    AudioAnalyzer();
    ~AudioAnalyzer();

    bool load(const std::string& path);
    void analyze();

    std::vector<float> get_beat_times() { return beat_times; }
    float get_current_energy(float time);

private:
    std::vector<float> beat_times;
    std::vector<float> energy_curve;
    float duration;
};

#endif

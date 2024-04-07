#ifndef AUDIO_ANALYZER_H
#define AUDIO_ANALYZER_H

#include <string>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

// beat detection and audio analysis
class AudioAnalyzer {
public:
    AudioAnalyzer();
    ~AudioAnalyzer();

    bool load(const std::string& path);
    bool load_from_video(const std::string& video_path);  // extract audio from video
    void analyze();

    std::vector<float> get_beat_times() { return beat_times; }
    float get_current_energy(float time);

private:
    std::vector<float> beat_times;
    std::vector<float> energy_curve;
    float duration;

    AVFormatContext* fmt_ctx;
    int audio_stream_idx;
};

#endif

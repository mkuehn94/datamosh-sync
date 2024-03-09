#ifndef PROCESSOR_H
#define PROCESSOR_H

#include "video_processor.h"
#include "audio_analyzer.h"
#include "effects.h"
#include <string>
#include <vector>

struct ProcessParams {
    float glitch_intensity;
    float jpeg_quality;
    float motion_blur;
    float color_shift;
    float block_shuffle;
    float beat_sync;
};

class VideoMosher {
public:
    VideoMosher();
    ~VideoMosher();

    bool init(const std::string& video_path, const std::string& audio_path);
    bool process_video(const std::string& output_path, ProcessParams params);

private:
    void process_frame(uint8_t* frame_data, int width, int height, float time);

    VideoProcessor* video;
    AudioAnalyzer* audio;
    EffectEngine* effects;

    ProcessParams params;
    std::vector<float> beat_times;
    int current_frame;

    AVFormatContext* output_ctx;
};

#endif

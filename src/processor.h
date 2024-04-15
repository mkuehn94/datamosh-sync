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
    bool init_with_embedded_audio(const std::string& video_path);  // use video's audio track
    bool process_video(const std::string& output_path, ProcessParams params);

private:
    void process_frame(uint8_t* frame_data, int width, int height, float time);
    bool setup_encoder(const std::string& output_path, int width, int height);
    void encode_frame(uint8_t* rgb_data);
    void finish_encoding();
    bool copy_audio_stream();  // copy audio from input to output

    VideoProcessor* video;
    AudioAnalyzer* audio;
    EffectEngine* effects;

    ProcessParams params;
    std::vector<float> beat_times;
    int current_frame;

    AVFormatContext* output_ctx;
    AVCodecContext* enc_ctx;
    AVStream* out_stream;
    AVStream* audio_out_stream;  // for audio
    AVFrame* enc_frame;
    struct SwsContext* enc_sws_ctx;
    int64_t next_pts;

    std::string input_video_path;  // need this for audio copy
};

#endif

#ifndef VIDEO_PROCESSOR_H
#define VIDEO_PROCESSOR_H

#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

class VideoProcessor {
public:
    VideoProcessor();
    ~VideoProcessor();

    bool open(const std::string& path);
    void close();

    bool read_frame();
    uint8_t* get_frame_data();
    uint8_t* get_frame_rgb();  // convert to RGB format
    int get_width() { return width; }
    int get_height() { return height; }
    AVRational get_fps();  // real framerate of the source
    double get_duration();
    // for the second video, has to be the same size as the first one
    uint8_t* get_frame_rgb_sized(int out_w, int out_h);

private:
    AVFormatContext* fmt_ctx;
    AVCodecContext* codec_ctx;
    AVFrame* frame;
    AVFrame* frame_rgb;  // converted RGB frame
    AVPacket* packet;
    struct SwsContext* sws_ctx;  // for YUV->RGB conversion
    uint8_t* rgb_buffer;
    struct SwsContext* sized_ctx;
    uint8_t* sized_buffer;
    int video_stream_idx;
    int width, height;
};

#endif

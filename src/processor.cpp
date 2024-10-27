// main processing loop
// combines video, audio, and effects

#include "processor.h"
#include "constants.h"
#include <cstring>
#include <iostream>
#include <cmath>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

using namespace std;

// ----- reading video, thin ffmpeg wrapper -----

VideoProcessor::VideoProcessor()
    : fmt_ctx(nullptr), codec_ctx(nullptr), frame(nullptr),
      frame_rgb(nullptr), packet(nullptr), sws_ctx(nullptr),
      rgb_buffer(nullptr), sized_ctx(nullptr), sized_buffer(nullptr), sized_tmp(nullptr),
      video_stream_idx(-1), width(0), height(0) {
}

VideoProcessor::~VideoProcessor() {
    close();
}

bool VideoProcessor::open(const string& path) {
    cout << "[vp] opening " << path << endl;
    if (avformat_open_input(&fmt_ctx, path.c_str(), nullptr, nullptr) < 0) {
        cerr << "Could not open video file: " << path << endl;
        return false;
    }

    cout << "[vp] finding stream info..." << endl;
    avformat_find_stream_info(fmt_ctx, nullptr);

    cout << "[vp] finding video stream..." << endl;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx = i;
            break;
        }
    }

    if (video_stream_idx == -1) {
        cerr << "No video stream found" << endl;
        return false;
    }

    AVCodecParameters* codecpar = fmt_ctx->streams[video_stream_idx]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, codecpar);

    if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
        cerr << "Could not open codec" << endl;
        return false;
    }

    width = codec_ctx->width;
    height = codec_ctx->height;

    frame = av_frame_alloc();
    packet = av_packet_alloc();

    return true;
}

void VideoProcessor::close() {
    if (rgb_buffer) free(rgb_buffer);
    if (sized_buffer) free(sized_buffer);
    if (sized_tmp) free(sized_tmp);
    if (sized_ctx) sws_freeContext(sized_ctx);
    if (frame_rgb) av_frame_free(&frame_rgb);
    if (sws_ctx) sws_freeContext(sws_ctx);
    if (frame) av_frame_free(&frame);
    if (packet) av_packet_free(&packet);
    if (codec_ctx) avcodec_free_context(&codec_ctx);
    if (fmt_ctx) avformat_close_input(&fmt_ctx);
}

bool VideoProcessor::read_frame() {
    while (av_read_frame(fmt_ctx, packet) >= 0) {
        if (packet->stream_index == video_stream_idx) {
            int ret = avcodec_send_packet(codec_ctx, packet);
            if (ret < 0) {
                av_packet_unref(packet);
                return false;
            }

            ret = avcodec_receive_frame(codec_ctx, frame);
            av_packet_unref(packet);

            if (ret == 0) {
                return true;
            }
        } else {
            av_packet_unref(packet);
        }
    }
    return false;
}

AVRational VideoProcessor::get_fps() {
    AVRational r = av_guess_frame_rate(fmt_ctx, fmt_ctx->streams[video_stream_idx], nullptr);
    if (r.num <= 0 || r.den <= 0) {
        r.num = OUTPUT_FPS;  // no idea, fall back to the old default
        r.den = 1;
    }
    return r;
}

double VideoProcessor::get_duration() {
    return fmt_ctx->duration / (double)AV_TIME_BASE;
}

uint8_t* VideoProcessor::get_frame_rgb_sized(int out_w, int out_h) {
    // scale so it covers the whole picture then cut off whats sticking out.
    // squishing it to fit looks wrong
    float sc = (float)out_w / width;
    if ((float)out_h / height > sc) sc = (float)out_h / height;
    int sw_ = (int)(width * sc + 0.5f), sh_ = (int)(height * sc + 0.5f);
    if (sw_ < out_w) sw_ = out_w;
    if (sh_ < out_h) sh_ = out_h;

    if (!sized_ctx) {
        sized_ctx = sws_getContext(width, height, codec_ctx->pix_fmt,
                                   sw_, sh_, AV_PIX_FMT_RGB24,
                                   SWS_BILINEAR, nullptr, nullptr, nullptr);
        sized_tmp = (uint8_t*)malloc(sw_ * sh_ * 3);
        sized_buffer = (uint8_t*)malloc(out_w * out_h * 3);
        cout << "[vp] other video " << width << "x" << height << " -> " << sw_ << "x" << sh_
             << " cut to " << out_w << "x" << out_h << endl;
    }
    uint8_t* dst[4] = {sized_tmp, nullptr, nullptr, nullptr};
    int lines[4] = {sw_ * 3, 0, 0, 0};
    sws_scale(sized_ctx, frame->data, frame->linesize, 0, height, dst, lines);

    int ox = (sw_ - out_w) / 2, oy = (sh_ - out_h) / 2;
    for (int y = 0; y < out_h; y++) {
        memcpy(sized_buffer + y * out_w * 3, sized_tmp + ((y + oy) * sw_ + ox) * 3, out_w * 3);
    }
    return sized_buffer;
}

uint8_t* VideoProcessor::get_frame_data() {
    if (frame) {
        return frame->data[0];
    }
    return nullptr;
}

uint8_t* VideoProcessor::get_frame_rgb() {
    if (!sws_ctx) {
        sws_ctx = sws_getContext(
            width, height, codec_ctx->pix_fmt,
            width, height, AV_PIX_FMT_RGB24,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );

        frame_rgb = av_frame_alloc();
        rgb_buffer = (uint8_t*)malloc(width * height * 3);
        frame_rgb->data[0] = rgb_buffer;
        frame_rgb->linesize[0] = width * 3;
    }

    sws_scale(sws_ctx, frame->data, frame->linesize, 0, height,
              frame_rgb->data, frame_rgb->linesize);
    return rgb_buffer;
}


VideoMosher::VideoMosher() {
    video = nullptr;
    audio = nullptr;
    effects = nullptr;
    warp = nullptr;
    last_on_beat = false;
    current_frame = 0;
    output_ctx = nullptr;
    enc_ctx = nullptr;
    out_stream = nullptr;
    enc_frame = nullptr;
    enc_sws_ctx = nullptr;
    next_pts = 0;
    prev_rgb = nullptr;
    prev_rgb_size = 0;
    ghost_buf = nullptr;
    ghost_buf_size = 0;
    held_frame = nullptr;
    held_frame_size = 0;
    last_energy = 0.0f;
    fps = OUTPUT_FPS;

    track_x = track_y = track_w = track_h = 0;
    track_persistence = 0;
    track_dx = track_dy = 0.0f;
    for (int i = 0; i < HISTORY_LEN; i++) history[i] = nullptr;
    hist_pos = 0;
    have_prev = false;

    world_buf = nullptr;
    world_buf_size = 0;
    world_fade = 0.0f;
    scene_cut = false;

    stuck_buf = nullptr;
    stuck_x = stuck_y = stuck_w = stuck_h = 0;
    stuck_timer = 0;
    stuck_wait = 0;
    last_treble = 0;
    glitch_left = 0;
    use_parts = false;
    song_limit = 0;
    other = nullptr;
    other_eyes = nullptr;
    other_frame = nullptr;
    dim_punch = 0;
    a_read = b_read = 0;
    swing_frames = 0;
    for (int i = 0; i < FX_COUNT; i++) swing_sum[i] = swing_sq[i] = 0;
    stuck_allowed = true;
    for (int i = 0; i < FX_COUNT; i++) fx_was_on[i] = true;
    void_r = void_g = void_b = 0.0f;
    void_color_init = false;
    void_off_x = 0;
    void_off_y = 0;
}

VideoMosher::~VideoMosher() {
    finish_encoding();
    if (prev_rgb) free(prev_rgb);
    if (ghost_buf) free(ghost_buf);
    if (held_frame) free(held_frame);
    for (int i = 0; i < HISTORY_LEN; i++) {
        if (history[i]) free(history[i]);
    }
    if (world_buf) free(world_buf);
    if (stuck_buf) free(stuck_buf);
    if (video) delete video;
    if (audio) delete audio;
    if (effects) delete effects;
    if (warp) delete warp;
}

bool VideoMosher::init(const string& video_path, const string& audio_path) {
    // input_video_path = video_path;  // save for audio copy later
    source_path = video_path;

    video = new VideoProcessor();
    audio = new AudioAnalyzer();
    effects = new EffectEngine();
    warp = new WarpEngine();

    if (!video->open(video_path)) {
        cerr << "failed to open video" << endl;
        return false;
    }

    if (!audio->load(audio_path)) {
        cerr << "failed to load audio" << endl;
        return false;
    }

    audio->limit_to(song_limit);
    audio->analyze();

    beat_times = audio->get_beat_times();
    cout << "initialized with " << beat_times.size() << " beats" << endl;

    return true;
}

bool VideoMosher::init_with_embedded_audio(const string& video_path) {
    // input_video_path = video_path;  // save for audio copy later
    source_path = video_path;

    cout << "[debug] creating objects..." << endl;
    video = new VideoProcessor();
    audio = new AudioAnalyzer();
    effects = new EffectEngine();
    warp = new WarpEngine();

    cout << "[debug] opening video..." << endl;
    if (!video->open(video_path)) {
        cerr << "failed to open video" << endl;
        return false;
    }

    cout << "[debug] loading audio from video..." << endl;
    // extract audio from the same video file
    if (!audio->load_from_video(video_path)) {
        cerr << "failed to extract audio from video" << endl;
        return false;
    }

    cout << "[debug] analyzing beats..." << endl;
    audio->limit_to(song_limit);
    audio->analyze();

    beat_times = audio->get_beat_times();
    cout << "initialized with " << beat_times.size() << " beats (from video)" << endl;

    return true;
}

// out of its section an effect just gets slider 0 for that frame. all the
// effects already know what to do with 0 so nothing below had to change
void VideoMosher::swing_params(float time) {
    swung_params = slider_params;
    if (intensity.empty()) return;

    int i = (int)(time * 20);
    if (i < 0) i = 0;
    if (i >= (int)intensity.size()) i = intensity.size() - 1;
    float z = intensity[i];
    float s = slider_params.swing;

    swung_params.glitch_intensity = swung_value(FX_GLITCH, slider_params.glitch_intensity, z, s);
    swung_params.motion_blur = swung_value(FX_BLUR, slider_params.motion_blur, z, s);
    swung_params.color_shift = swung_value(FX_VOID, slider_params.color_shift, z, s);
    swung_params.beat_sync = swung_value(FX_BEATSYNC, slider_params.beat_sync, z, s);
    swung_params.rot = swung_value(FX_ROT, slider_params.rot, z, s);
    swung_params.warp = swung_value(FX_WARP, slider_params.warp, z, s);
    swung_params.quake = swung_value(FX_QUAKE, slider_params.quake, z, s);
    swung_params.spectral = swung_value(FX_SPECTRAL, slider_params.spectral, z, s);
    swung_params.groove = swung_value(FX_GROOVE, slider_params.groove, z, s);
    swung_params.texswap = swung_value(FX_TEXSWAP, slider_params.texswap, z, s);
    swung_params.recursion = swung_value(FX_RECURSION, slider_params.recursion, z, s);
    swung_params.hom = swung_value(FX_HOM, slider_params.hom, z, s);

    // keep score
    float now[FX_COUNT] = {
        swung_params.glitch_intensity, swung_params.motion_blur, swung_params.color_shift, 0, swung_params.beat_sync,
        swung_params.rot, swung_params.warp, swung_params.quake, swung_params.spectral, swung_params.groove,
        swung_params.texswap, swung_params.recursion, swung_params.hom
    };
    for (int fx = 0; fx < FX_COUNT; fx++) {
        swing_sum[fx] += now[fx];
        swing_sq[fx] += now[fx] * now[fx];
    }
    swing_frames++;
}

void VideoMosher::play_section(float time) {
    const Section* s = section_at(sections, time);
    if (!s) return;

    params = swung_params;

    // build gets worse and worse, breath is almost nothing, etc
    float hard = hardness_at(*s, time);
    params.glitch_intensity *= hard;
    params.motion_blur *= hard;
    params.color_shift *= hard;
    params.beat_sync *= hard;
    params.rot *= hard;
    params.warp *= hard;
    params.quake *= hard;
    params.spectral *= hard;
    params.groove *= hard;
    params.texswap *= hard;
    params.recursion *= hard;
    params.hom *= hard;

    // and on top of that per effect, the ones too violent for this moment
    params.glitch_intensity *= s->fx_scale[FX_GLITCH];
    params.motion_blur *= s->fx_scale[FX_BLUR];
    params.color_shift *= s->fx_scale[FX_VOID];
    params.beat_sync *= s->fx_scale[FX_BEATSYNC];
    params.rot *= s->fx_scale[FX_ROT];
    params.warp *= s->fx_scale[FX_WARP];
    params.quake *= s->fx_scale[FX_QUAKE];
    params.spectral *= s->fx_scale[FX_SPECTRAL];
    params.groove *= s->fx_scale[FX_GROOVE];
    params.texswap *= s->fx_scale[FX_TEXSWAP];
    params.recursion *= s->fx_scale[FX_RECURSION];
    params.hom *= s->fx_scale[FX_HOM];

    if (!s->on[FX_GLITCH]) params.glitch_intensity = 0;
    if (!s->on[FX_BLUR]) params.motion_blur = 0;
    if (!s->on[FX_VOID]) params.color_shift = 0;
    if (!s->on[FX_BEATSYNC]) params.beat_sync = 0;
    if (!s->on[FX_ROT]) params.rot = 0;
    if (!s->on[FX_WARP]) params.warp = 0;
    if (!s->on[FX_QUAKE]) params.quake = 0;
    if (!s->on[FX_SPECTRAL]) params.spectral = 0;
    if (!s->on[FX_GROOVE]) params.groove = 0;
    if (!s->on[FX_TEXSWAP]) params.texswap = 0;
    if (!s->on[FX_RECURSION]) params.recursion = 0;
    if (!s->on[FX_HOM]) params.hom = 0;

    stuck_allowed = s->on[FX_STUCK];
    if (!stuck_allowed) stuck_timer = 0;  // stuck patch shouldnt outlive its section

    // starts again fresh, or you get a flash of a street from a minute ago
    if (s->on[FX_QUAKE] && !fx_was_on[FX_QUAKE]) warp->wake_quake();
    if (s->on[FX_SPECTRAL] && !fx_was_on[FX_SPECTRAL]) warp->wake_spectral();
    if (s->on[FX_RECURSION] && !fx_was_on[FX_RECURSION]) warp->wake_recursion();
    if (s->on[FX_HOM] && !fx_was_on[FX_HOM]) {
        warp->wake_mirrors();
        if (other_eyes) other_eyes->wake_mirrors();  // or its background guess goes stale
    }
    if (s->on[FX_BLUR] && !fx_was_on[FX_BLUR] && ghost_buf) memset(ghost_buf, 0, ghost_buf_size);

    for (int i = 0; i < FX_COUNT; i++) fx_was_on[i] = s->on[i];
}

bool VideoMosher::open_other(const string& path) {
    other = new VideoProcessor();
    if (!other->open(path)) {
        cerr << "cant open the other video: " << path << endl;
        delete other;
        other = nullptr;
        return false;
    }
    return true;
}

// reads until this video is where the output is. with the same fps thats one read
bool VideoMosher::catch_up(VideoProcessor* v, long& read, double src_fps, long frame_index, double out_fps) {
    long want = (long)((frame_index + 1) * src_fps / out_fps + 0.0001);
    if (want <= read) want = read + 1;
    while (read < want) {
        if (!v->read_frame()) return false;
        read++;
    }
    return true;
}

bool VideoMosher::process_frame(uint8_t* frame_data, int width, int height, float time) {
    swing_params(time);
    params = swung_params;
    if (use_parts) play_section(time);

    bool on_beat = false;
    float beat_strength = 0.0;

    for (float beat_time : beat_times) {
        if (abs(time - beat_time) < 0.05f) {
            on_beat = true;
            beat_strength = 1.0;
            break;
        }
    }

    float energy = audio->get_current_energy(time);
    last_energy = energy;  // process_video needs this for the freeze-hold

    int frame_size = width * height * 3;
    if (!prev_rgb || prev_rgb_size != frame_size) {
        if (prev_rgb) free(prev_rgb);
        prev_rgb = (uint8_t*)malloc(frame_size);
        prev_rgb_size = frame_size;
    }
    // ghost buffer: persistent across frames, never cleared
    if (!ghost_buf || ghost_buf_size != frame_size) {
        if (ghost_buf) free(ghost_buf);
        ghost_buf = (uint8_t*)malloc(frame_size);
        ghost_buf_size = frame_size;
        memset(ghost_buf, 0, frame_size);  // start black
    }
    // figure out whats moving BEFORE we overwrite prev_rgb
    if (have_prev) {
        motion_track(frame_data, width, height);
    }
    // real vectors for the rot, also needs the clean frame
    warp->estimate_motion(frame_data);
    if (warp->scene_cut()) scene_cut = true;

    AudioFrame af;
    af.energy = energy;
    af.bass = audio->get_bass(time);
    af.treble = audio->get_treble(time);
    af.kick = on_beat && !last_on_beat;  // on_beat stays true for a few frames
    // the kick frame is ~50ms BEFORE the beat (the +-0.05 above) so a.bass right
    // here is still whats left of the last hit. have to look ahead for the real one
    af.hit = 0;
    for (int look = 0; look < 6; look++) {
        float b = audio->get_bass(time + look * 0.025f);
        if (b > af.hit) af.hit = b;
    }
    af.cut = warp->scene_cut();

    // how much of the other world right now. section says the base, kicks rip it open
    // and it closes again
    if (other) {
        const Section* ds = section_at(sections, time);
        if (af.kick) dim_punch = 0.4f + 0.6f * af.hit;
        else dim_punch *= pow(0.05f, 1.0f / fps / 0.3f);
        float dim = ds ? dimension_at(*ds, time, slider_params.cross) : slider_params.cross;
        // breath stays clean no matter how hard the kick is
        if (dim > 0) dim += dim_punch * slider_params.cross * 0.5f;
        warp->look_at_other(other_frame, other_eyes, dim);
    }
    last_on_beat = on_beat;

    // hats + snares, treble curve jumping up. same idea as the kick but for the top end
    bool tick = af.treble > 0.45f && af.treble > last_treble + 0.12f;
    last_treble = af.treble;
    if (tick) glitch_left = (int)(0.07f * fps) + 1;  // about 70ms

    uint8_t* prev_for_trails = (uint8_t*)malloc(frame_size);
    memcpy(prev_for_trails, prev_rgb, frame_size);
    memcpy(prev_rgb, frame_data, frame_size);
    have_prev = true;

    // ===== THE ROT =====
    // has to come first and has to see the clean video. had it at the end
    // of the chain first but then the residuals of all the other effects
    // (zoom pulse especially) pile up in the canvas and in about 5 seconds
    // the whole picture is tv static
    warp->rot_and_warp(frame_data, params.rot, params.warp, params.groove, af);
    bool rotting = params.rot > 0.01f || params.warp > 0.01f || params.groove > 0.01f;
    warp->block_recursion(frame_data, params.recursion, af);

    // ============================================================
    // KILLED the old pipeline (motion_vector_glitch, datamosh_streak,
    // jpeg_corruption, color_encode_corrupt, color_remap, scanlines,
    // pixelate, wave everywhere). it looked like movie maker confetti.
    // real mosh is smear + merge + stack + lag. nothing else.
    // functions still exist in effects.cpp if i ever want them back
    // ============================================================

    // 1. SCENE CUT -> worlds merge
    // motion_track flagged a cut: save the last frame of the old world
    // and let it bleed into the new one for the next few seconds
    if (scene_cut) {
        if (!world_buf || world_buf_size != frame_size) {
            if (world_buf) free(world_buf);
            world_buf = (uint8_t*)malloc(frame_size);
            world_buf_size = frame_size;
        }
        memcpy(world_buf, prev_for_trails, frame_size);  // old world's final frame
        world_fade = 1.0f;
        track_persistence = 0;  // new world, new subject
        scene_cut = false;
    }

    // 2. trails - temporal smear, the base layer of the whole look.
    // slider scales it DIRECTLY now. used to be gated at >0.1 which
    // meant mid-low settings did literally nothing (the "gui bug")
    // while rotting trail against the last OUTPUT. the clean frame would
    // just leak the real video back in as haze (had it switched off for
    // that reason but then this slider did nothing at all with rot on)
    if (params.motion_blur > 0.02f) {
        float trail_amt = params.motion_blur * (0.25f + 0.55f * energy);
        uint8_t* trail_ref = (rotting && held_frame) ? held_frame : prev_for_trails;
        if (trail_ref) {
            int n = width * height * 3;
            for (int i = 0; i < n; i++) {
                frame_data[i] = (uint8_t)(frame_data[i] * (1.0f - trail_amt) + trail_ref[i] * trail_amt);
            }
        }
    }

    // 3. world merge - shadows of the new scene open into the old one.
    // decays over ~4 seconds. during a freeze-hold the encoder smears
    // the merge even further which is exactly what we want
    if (world_fade > 0.01f) {
        effects->world_merge(frame_data, width, height, world_buf, world_fade * 0.8f);
        world_fade *= 0.985f;
    }

    // 3.5 THE VOID - auto-key on the sky color, matched pixels become a
    // window into a displaced chunk of the same frame. the displacement
    // jumps on beats because of course it does
    if (params.color_shift > 0.02f && energy > 0.25f) {
        // learn the void color from the top strip (usually sky)
        long sr = 0, sg = 0, sb = 0, n = 0;
        for (int y = 0; y < height / 8; y += 4) {
            for (int x = 0; x < width; x += 4) {
                int idx = (y * width + x) * 3;
                sr += frame_data[idx];
                sg += frame_data[idx+1];
                sb += frame_data[idx+2];
                n++;
            }
        }
        if (n > 0) {
            if (!void_color_init) {
                void_r = sr / n; void_g = sg / n; void_b = sb / n;
                void_color_init = true;
            } else {
                // heavy smoothing or the void edge flickers like a
                // broken greenscreen. learned that one the hard way
                void_r = void_r * 0.98f + (sr / n) * 0.02f;
                void_g = void_g * 0.98f + (sg / n) * 0.02f;
                void_b = void_b * 0.98f + (sb / n) * 0.02f;
            }
        }
        if (af.kick) {
            // new place for the void to look into, block aligned
            void_off_x = ((rand() % 2) * 2 - 1) * width / 4;
            void_off_x = (void_off_x / MACROBLOCK_SIZE) * MACROBLOCK_SIZE;
            void_off_y = ((rand() % 5) - 2) * MACROBLOCK_SIZE;
        }
        // tolerance comes from the old jpeg slider (repurposed, that
        // effect is dead). low slider value = huge void, high = surgical
        int tol = 10 + (int)((100.0f - params.jpeg_quality) * 0.5f);
        // color slider = how deep the void cuts
        float void_amt = params.color_shift * (0.35f + 0.55f * energy);
        effects->void_replace(frame_data, width, height,
                              (int)void_r, (int)void_g, (int)void_b, tol,
                              void_off_x, void_off_y, void_amt, other_frame);
    }

    // 4. glitch bands - only on real energy peaks, the ONE corruption
    // effect that survived the purge because it looks like actual
    // bitstream damage and not a snapchat filter
    // only on hats and snares, harder because its so short
    if (glitch_left > 0 && energy > 0.2f && params.glitch_intensity > 0.05f) {
        effects->glitch_bands(frame_data, width, height,
                              params.glitch_intensity * (0.5f + energy * 0.6f));
    }
    if (glitch_left > 0) glitch_left--;

    // 5. pulse zoom on beats - beatsync slider scales the punch
    if (on_beat && params.beat_sync > 0.05f) {
        effects->pulse_effect(frame_data, width, height,
                              beat_strength * params.beat_sync);
    }

    // 6. ghost layer composite - SOLID stacks, not haze. whatever got
    // stamped (objects, mostly) sits in the frame like its really there
    if (params.motion_blur > 0.02f) {
        float ghost_str = 0.2f + params.motion_blur * 0.45f;
        effects->mask_composite(frame_data, width, height, ghost_buf, ghost_str);
    }

    free(prev_for_trails);

    // ===== PERSISTENT OBJECT FX =====
    // if something keeps moving in the same area for a while (car driving
    // through the shot, person walking around) it must be the subject -
    // so it gets the special treatment. the longer it persists the harder
    // it gets hit. THIS is what makes it look trippy instead of just broken
    // if the "object" is most of the frame its not an object, thats the camera
    // moving. blending the entire picture with itself from half a second
    // ago turned everything into grey fog
    bool whole_frame = track_w * track_h > width * height / 2;
    if (track_persistence > PERSIST_FRAMES && params.glitch_intensity > 0.05f && !whole_frame) {
        float p = (track_persistence - PERSIST_FRAMES) / 60.0f;
        if (p > 1.0f) p = 1.0f;
        // glitch slider scales how hard the subject gets hit
        float g = 0.3f + 0.7f * params.glitch_intensity;

        // TIME LAG: the region shows what was there ~half a second ago.
        // the object visibly lags behind itself, like its timeline slipped.
        // history holds processed frames so the lag compounds
        int lag_idx = (hist_pos + HISTORY_LEN - ECHO_LAG_FRAMES) % HISTORY_LEN;
        if (history[lag_idx]) {
            effects->echo_region(frame_data, width, height, history[lag_idx],
                                 track_x, track_y, track_w, track_h,
                                 0, 0, (0.45f + 0.35f * p) * g);
        }

        // smear it along its own direction of travel
        if (fabs(track_dx) + fabs(track_dy) > 1.0f) {
            effects->vector_smear(frame_data, width, height,
                                  track_x, track_y, track_w, track_h,
                                  track_dx * (2.0f + energy * 4.0f),
                                  track_dy * (2.0f + energy * 4.0f),
                                  (0.25f + 0.35f * p) * g);
        }

        // STATIC STACKING: stamp the object into the ghost layer at its
        // EXACT current position every frame. as it moves, solid copies
        // of it pile up behind it and just stay there. a car crossing
        // the frame leaves a traffic jam of itself
        effects->ghost_stamp_region(frame_data, width, height, ghost_buf,
                                    track_x, track_y, track_w, track_h,
                                    0, 0, 1);
    }

    warp->texture_swap(frame_data, params.texswap, af);
    warp->spectral_lag(frame_data, params.spectral, af);
    warp->time_quake(frame_data, params.quake, af);
    if (other_eyes && other_frame) other_eyes->find_mirror_objects(other_frame, params.hom, af);
    warp->hall_of_mirrors(frame_data, params.hom, af);

    // stash this frame in history for future echoes
    // TODO: this eats RAM like crazy on big videos, whatever
    if (!history[hist_pos]) {
        history[hist_pos] = (uint8_t*)malloc(frame_size);
    }
    memcpy(history[hist_pos], frame_data, frame_size);
    hist_pos = (hist_pos + 1) % HISTORY_LEN;

    // STUCK OVERLAY - paste LAST so it stays frozen no matter what the
    // other effects do behind it. history above stores the UNstuck frame
    // so lag/echo dont get confused by the patch
    if (stuck_timer > 0) {
        // full overwrite, no blending. its STUCK, not translucent
        for (int y = 0; y < stuck_h; y++) {
            if (stuck_y + y >= height) break;
            memcpy(&frame_data[((stuck_y + y) * width + stuck_x) * 3],
                   &stuck_buf[y * stuck_w * 3], stuck_w * 3);
        }
        // patch only lets go on a beat. waits 2 sec max or a quiet part keeps it forever
        if (stuck_timer == 1 && !on_beat && stuck_wait < fps * 2) {
            stuck_wait++;
        } else {
            stuck_timer--;
            stuck_wait = 0;
        }
    } else if (on_beat && stuck_allowed && energy > 0.55f &&
               (rand() % 100) < (int)(slider_params.glitch_intensity * 35) &&
               slider_params.glitch_intensity > 0.05f) {
        // grab a chunk and freeze it for 1-4 seconds. block aligned so
        // the encoder treats it as truly static = cheapest P-frames ever
        stuck_w = MACROBLOCK_SIZE * (6 + rand() % 14);
        stuck_h = MACROBLOCK_SIZE * (4 + rand() % 10);
        if (stuck_w > width)  stuck_w = width;
        if (stuck_h > height) stuck_h = height;
        int range_x = width - stuck_w;
        int range_y = height - stuck_h;
        if (range_x < 1) range_x = 1;
        if (range_y < 1) range_y = 1;
        stuck_x = (rand() % range_x) / MACROBLOCK_SIZE * MACROBLOCK_SIZE;
        stuck_y = (rand() % range_y) / MACROBLOCK_SIZE * MACROBLOCK_SIZE;
        if (stuck_buf) free(stuck_buf);
        stuck_buf = (uint8_t*)malloc(stuck_w * stuck_h * 3);
        // two videos: patch comes from the other one, a window frozen open
        effects->grab_rect(other_frame ? other_frame : frame_data, width, height, stuck_buf,
                           stuck_x, stuck_y, stuck_w, stuck_h);
        // old shuffle slider (repurposed) = how long it stays stuck
        // these were frame counts from when everything was 30fps
        stuck_timer = (int)((20 + params.block_shuffle * 120 + rand() % 60) * fps / 30.0f);
        cout << "[stuck] " << stuck_timer / fps << "s" << endl;
    }

    return af.kick;
}

void VideoMosher::motion_track(uint8_t* frame_data, int width, int height) {
    // super cheap motion estimation: 16px blocks, SAD on rough luma,
    // search +-8 px. not accurate at all but we dont need accuracy,
    // we need "something has been moving HERE for a while"

    int block = MACROBLOCK_SIZE;
    int gx = width / block;
    int gy = height / block;

    int moving = 0;
    int minx = width, miny = height, maxx = 0, maxy = 0;
    float sum_dx = 0, sum_dy = 0;

    for (int by = 0; by < gy; by++) {
        for (int bx = 0; bx < gx; bx++) {
            int cx = bx * block;
            int cy = by * block;

            // find best match for this block in the previous frame
            int best_sad = 0x7fffffff;
            int best_dx = 0, best_dy = 0;

            for (int sy = -8; sy <= 8; sy += 4) {
                for (int sx = -8; sx <= 8; sx += 4) {
                    int px = cx + sx;
                    int py = cy + sy;
                    if (px < 0 || py < 0 || px + block > width || py + block > height)
                        continue;

                    int sad = 0;
                    for (int y = 0; y < block; y += 4) {
                        for (int x = 0; x < block; x += 4) {
                            int ci = ((cy + y) * width + cx + x) * 3;
                            int pi = ((py + y) * width + px + x) * 3;
                            int cl = frame_data[ci] + frame_data[ci+1] + frame_data[ci+2];
                            int pl = prev_rgb[pi] + prev_rgb[pi+1] + prev_rgb[pi+2];
                            sad += abs(cl - pl);
                        }
                    }

                    if (sad < best_sad) {
                        best_sad = sad;
                        best_dx = sx;
                        best_dy = sy;
                    }
                }
            }

            // "moving" = block found a better match somewhere else, but the
            // match is still decent (huge sad = scene cut / noise, skip)
            int mv = abs(best_dx) + abs(best_dy);
            if (mv >= 4 && best_sad < 30000) {
                moving++;
                sum_dx += best_dx;
                sum_dy += best_dy;
                if (cx < minx) minx = cx;
                if (cy < miny) miny = cy;
                if (cx + block > maxx) maxx = cx + block;
                if (cy + block > maxy) maxy = cy + block;
            }
        }
    }

    if (moving > 3) {
        // smooth the bbox so it doesnt jitter all over the place
        float s = 0.3f;
        if (track_persistence == 0) s = 1.0f;  // snap on first detection
        track_x = (int)(track_x * (1 - s) + minx * s);
        track_y = (int)(track_y * (1 - s) + miny * s);
        track_w = (int)(track_w * (1 - s) + (maxx - minx) * s);
        track_h = (int)(track_h * (1 - s) + (maxy - miny) * s);
        track_dx = track_dx * (1 - s) + (sum_dx / moving) * s;
        track_dy = track_dy * (1 - s) + (sum_dy / moving) * s;

        track_persistence++;
        if (track_persistence > 600) track_persistence = 600;  // cap it
    } else {
        // nothing moving. decays instead of resetting, feels better when
        // motion pauses for a couple frames (car at a red light etc)
        track_persistence -= 2;
        if (track_persistence < 0) track_persistence = 0;
    }
}


bool VideoMosher::process_video(const string& output_path, ProcessParams p) {
    params = p;
    slider_params = p;
    swung_params = p;
    if (p.swing > 0.001f) intensity = intensity_curve(audio);

    // only bother listening to the whole song if someone moved a parts slider
    float amounts[FX_COUNT] = {
        p.glitch_intensity, p.motion_blur, p.color_shift, p.glitch_intensity, p.beat_sync,
        p.rot, p.warp, p.quake, p.spectral, p.groove,
        p.texswap, p.recursion, p.hom
    };
    bool used[FX_COUNT];
    for (int i = 0; i < FX_COUNT; i++) {
        used[i] = amounts[i] > 0.01f;
        if (used[i] && p.parts[i] < 0.995f) use_parts = true;
    }
    // two videos always needs to know the song, for the worlds
    if (other) use_parts = true;
    if (use_parts) {
        sections = cut_song(audio, source_path);
        arrange_two_videos(other != nullptr);
        assign_parts(sections, p.parts, used, amounts, p.jpeg_quality);
        print_track(sections);
        if (used[FX_QUAKE]) warp->keep_quake_tape();
    }

    cout << "processing video..." << endl;
    cout << "glitch: " << params.glitch_intensity << endl;
    cout << "jpeg: " << params.jpeg_quality << endl;
    cout << "motion blur: " << params.motion_blur << endl;
    cout << "color shift: " << params.color_shift << endl;
    cout << "block shuffle: " << params.block_shuffle << endl;
    cout << "beat sync: " << params.beat_sync << endl;
    cout << "rot: " << params.rot << "  warp: " << params.warp
         << "  quake: " << params.quake << "  spectral: " << params.spectral << endl;
    cout << "groove: " << params.groove << "  texswap: " << params.texswap
         << "  recursion: " << params.recursion << "  hom: " << params.hom << endl;

    int width = video->get_width();
    int height = video->get_height();
    AVRational src_fps = video->get_fps();
    double a_fps = av_q2d(src_fps), b_fps = 0;
    // two videos: output goes at the slower one
    if (other) {
        AVRational o = other->get_fps();
        b_fps = av_q2d(o);
        if (b_fps < a_fps) src_fps = o;
        cout << "[other] fps " << a_fps << " + " << b_fps << " -> " << av_q2d(src_fps) << endl;
    }
    fps = av_q2d(src_fps);
    cout << "source fps: " << fps << endl;

    warp->init(width, height, fps);

    // setup output encoder
    if (!setup_encoder(output_path, width, height, src_fps)) {
        cerr << "Failed to setup encoder" << endl;
        return false;
    }

    float time = 0.0;
    // was hardcoded 1/30. on a 60fps source that made every beat land at
    // half the real time
    float dt = 1.0 / fps;
    int frame_count = 0;

    long out_index = -1;
    uint8_t* other_rgb = nullptr;
    while (true) {
        out_index++;
        if (!other) {
            if (!video->read_frame()) break;
        } else {
            if (!catch_up(video, a_read, a_fps, out_index, fps)) break;
            if (!catch_up(other, b_read, b_fps, out_index, fps)) {
                cout << "[other] ran out first at frame " << out_index << endl;
                break;
            }
            other_rgb = other->get_frame_rgb_sized(width, height);
            if (!other_eyes) {
                other_eyes = new WarpEngine();
                other_eyes->init(width, height, fps);
            }
        }
        uint8_t* frame_data = video->get_frame_rgb();  // get RGB not YUV

        // world flipped? then the other one is the main video and everything just
        // swaps. first frame after looks like a scene cut to the motion search, and
        // the rot treats that like a dropped keyframe. thats the hit
        if (other) {
            const Section* now_s = section_at(sections, time);
            if (now_s && now_s->world == 1) {
                uint8_t* t = frame_data;
                frame_data = other_rgb;
                other_rgb = t;
            }
            other_frame = other_rgb;
            other_eyes->estimate_motion(other_rgb);
        }

        if (frame_data && width > 0 && height > 0) {
            bool beat = process_frame(frame_data, width, height, time);
            encode_frame(frame_data);  // encode processed frame
            frame_count++;
            current_frame++;

            // keep a copy of what we just encoded for the freeze-hold
            int fsz = width * height * 3;
            if (!held_frame || held_frame_size != fsz) {
                if (held_frame) free(held_frame);
                held_frame = (uint8_t*)malloc(fsz);
                held_frame_size = fsz;
            }
            memcpy(held_frame, frame_data, fsz);

            // FREEZE-HOLD MOSH: on hard beats just keep encoding the SAME
            // image while the video runs on underneath. with the giant GOP
            // the encoder emits tiny do-nothing P-frames, and when we snap
            // back to live video everything smears like crazy. frame count
            // stays the same so the music still syncs.
            if (beat && params.beat_sync > 0.1f && last_energy > 0.55f) {
                // numbers were tuned at 30fps
                int hold = (1 + (int)(last_energy * 5.0f * params.beat_sync)) * fps / 30.0f;
                for (int h = 0; h < hold; h++) {
                    out_index++;
                    if (!other) {
                        if (!video->read_frame()) break;  // consume real frames, toss em
                    } else {
                        if (!catch_up(video, a_read, a_fps, out_index, fps)) break;
                        if (!catch_up(other, b_read, b_fps, out_index, fps)) break;
                    }
                    encode_frame(held_frame);
                    frame_count++;
                    time += dt;
                }
            }

            if (frame_count % 30 == 0) {
                cout << "processed " << frame_count << " frames" << endl;
            }
        }

        time += dt;
    }

    cout << "done processing " << frame_count << " frames" << endl;

    // what the sliders really did, before the parts and how hard stuff on top
    if (swing_frames > 0) {
        for (int fx = 0; fx < FX_COUNT; fx++) {
            double m = swing_sum[fx] / swing_frames;
            double v = swing_sq[fx] / swing_frames - m * m;
            printf("[swing] %-10s mean %.3f sd %.3f\n", fx_name(fx), m, sqrt(v > 0 ? v : 0));
        }
        fflush(stdout);
    }

    finish_encoding();
    cout << "wrote output to: " << output_path << endl;

    return true;
}

bool VideoMosher::setup_encoder(const string& output_path, int width, int height, AVRational rate) {
    // create output context
    avformat_alloc_output_context2(&output_ctx, nullptr, nullptr, output_path.c_str());
    
    // find H264 encoder
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    
    // create stream
    out_stream = avformat_new_stream(output_ctx, nullptr);
    
    // allocate encoder context
    enc_ctx = avcodec_alloc_context3(codec);
    enc_ctx->width = width;
    enc_ctx->height = height;
    enc_ctx->time_base = av_inv_q(rate);
    enc_ctx->framerate = rate;
    enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    enc_ctx->bit_rate = OUTPUT_BITRATE;

    // DATAMOSH ENCODER SETTINGS - this is where the real look comes from
    // keyframes basically never = P-frames reference frames from ages ago
    // = errors and smears accumulate instead of getting reset every 2 seconds
    enc_ctx->gop_size = 100000;   // one keyframe at the start, then never again
    enc_ctx->max_b_frames = 0;    // P-frames only, the classic mosh chain

    // kill scene cut detection or x264 sneaks keyframes in anyway
    // (priv_data only exists for libx264, so check first)
    if (enc_ctx->priv_data) {
        av_opt_set(enc_ctx->priv_data, "sc_threshold", "0", 0);
        av_opt_set(enc_ctx->priv_data, "keyint_min", "100000", 0);
        // fewer refs = prediction drifts harder. 2 seems like the sweet spot,
        // 1 made everything turn to mush too fast
        av_opt_set(enc_ctx->priv_data, "refs", "2", 0);
    }

    if (output_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    
    // open encoder
    if (avcodec_open2(enc_ctx, codec, nullptr) < 0) {
        cerr << "Failed to open encoder" << endl;
        return false;
    }
    
    avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
    out_stream->time_base = enc_ctx->time_base;
    
    // open output file
    if (!(output_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&output_ctx->pb, output_path.c_str(), AVIO_FLAG_WRITE) < 0) {
            cerr << "Could not open output file" << endl;
            return false;
        }
    }
    
    // write header
    avformat_write_header(output_ctx, nullptr);
    
    // setup frame for encoding
    enc_frame = av_frame_alloc();
    enc_frame->format = AV_PIX_FMT_YUV420P;
    enc_frame->width = width;
    enc_frame->height = height;
    av_frame_get_buffer(enc_frame, 0);
    
    // setup RGB to YUV converter
    enc_sws_ctx = sws_getContext(
        width, height, AV_PIX_FMT_RGB24,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    next_pts = 0;

    cout << "encoder setup complete: " << width << "x" << height << " @ " << fps << " fps" << endl;

    // try to copy audio stream
    // TODO: this is kinda hacky but works
    // TEMP DISABLED: bad_alloc on large videos, need to fix
    /*
    if (!copy_audio_stream()) {
        cout << "warning: could not copy audio (output will be silent)" << endl;
        // not fatal, video still works
    }
    */
    cout << "audio copying disabled temporarily (memory issues)" << endl;

    return true;
}

void VideoMosher::encode_frame(uint8_t* rgb_data) {
    int width = enc_ctx->width;
    int height = enc_ctx->height;
    
    // convert RGB to YUV420P
    uint8_t* src_data[1] = { rgb_data };
    int src_linesize[1] = { width * 3 };
    
    sws_scale(
        enc_sws_ctx,
        src_data, src_linesize, 0, height,
        enc_frame->data, enc_frame->linesize
    );
    
    enc_frame->pts = next_pts++;
    
    // send frame to encoder
    int ret = avcodec_send_frame(enc_ctx, enc_frame);
    
    // receive encoded packets
    AVPacket* pkt = av_packet_alloc();
    while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            cerr << "Error encoding frame" << endl;
            break;
        }
        
        av_packet_rescale_ts(pkt, enc_ctx->time_base, out_stream->time_base);
        pkt->stream_index = out_stream->index;
        
        av_interleaved_write_frame(output_ctx, pkt);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
}

void VideoMosher::finish_encoding() {
    if (!output_ctx) return;  // already finished or never started

    cout << "finishing encoding..." << endl;

    // flush encoder
    if (enc_ctx) {
        avcodec_send_frame(enc_ctx, nullptr);

        AVPacket* pkt = av_packet_alloc();
        int ret;
        while ((ret = avcodec_receive_packet(enc_ctx, pkt)) >= 0) {
            av_packet_rescale_ts(pkt, enc_ctx->time_base, out_stream->time_base);
            pkt->stream_index = out_stream->index;
            av_interleaved_write_frame(output_ctx, pkt);
            av_packet_unref(pkt);
        }
        av_packet_free(&pkt);
    }

    // copy audio packets if we have audio stream
    // DISABLED - broken, need to fix later
    // TODO: this whole section needs rewrite
    /*
    if (audio_out_stream) {
        cout << "copying audio packets..." << endl;
        AVFormatContext* input_ctx = nullptr;

        if (avformat_open_input(&input_ctx, nullptr, nullptr, nullptr) >= 0) {
            avformat_find_stream_info(input_ctx, nullptr);

            // find audio stream
            int audio_idx = -1;
            for (unsigned int i = 0; i < input_ctx->nb_streams; i++) {
                if (input_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                    audio_idx = i;
                    break;
                }
            }

            if (audio_idx >= 0) {
                AVPacket* pkt = av_packet_alloc();
                while (av_read_frame(input_ctx, pkt) >= 0) {
                    if (pkt->stream_index == audio_idx) {
                        av_packet_rescale_ts(pkt, input_ctx->streams[audio_idx]->time_base,
                                           nullptr);
                        pkt->stream_index = 1;
                        av_interleaved_write_frame(output_ctx, pkt);
                    }
                    av_packet_unref(pkt);
                }
                av_packet_free(&pkt);
                cout << "audio copied" << endl;
            }

            avformat_close_input(&input_ctx);
        }
    }
    */

    // write trailer
    if (output_ctx) {
        cout << "writing trailer..." << endl;
        av_write_trailer(output_ctx);
        cout << "trailer written" << endl;
    }

    // cleanup
    cout << "cleaning up encoder..." << endl;
    if (enc_sws_ctx) {
        sws_freeContext(enc_sws_ctx);
        enc_sws_ctx = nullptr;
    }
    // Cleanup disabled pending an ownership fix; these allocations leak.
    enc_frame = nullptr;
    enc_ctx = nullptr;
    output_ctx = nullptr;
    cout << "encoder cleanup done (skipped some frees to avoid crash)" << endl;
}


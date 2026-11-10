// datamosh music sync - main entry
// TODO: clean this up later

#include <cstdio>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include "audio_analyzer.h"
#include "processor.h"

using namespace std;

string get_basename(const string& path) {
    size_t slash = path.rfind('/');
    string name = (slash == string::npos) ? path : path.substr(slash + 1);
    size_t dot = name.rfind('.');
    if (dot != string::npos) name = name.substr(0, dot);
    return name;
}

// shorter of the two videos, thats how long the output gets
static float shorter_video(const string& a, const string& b) {
    VideoProcessor va, vb;
    if (!va.open(a) || !vb.open(b)) return 0;
    double d = va.get_duration() < vb.get_duration() ? va.get_duration() : vb.get_duration();
    printf("[other] output will be %gs\n", d);
    return d;
}

// music from both = ffmpeg mixes them into a file first, we just use that one
static string mix_music(const string& a, const string& b) {
    string out = "mixed_music_tmp.m4a";
    pid_t pid = fork();
    if (pid == 0) {
        execlp("ffmpeg", "ffmpeg", "-nostdin", "-loglevel", "error", "-y", "-i", a.c_str(), "-i", b.c_str(),
               "-filter_complex", "amix=inputs=2:duration=shortest", "-vn", "-c:a", "aac", out.c_str(), (char*)nullptr);
        _exit(1);
    }
    int status;
    waitpid(pid, &status, 0);
    return out;
}

// atof stops at a comma, "0,5" would silently be 0
static float parse_arg(const char* a) {
    string s = a;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == ',') s[i] = '.';
    }
    return atof(s.c_str());  // TODO strtof
}

int main(int argc, char** argv) {
    // GUI reads progress through a pipe, where stdout would otherwise be buffered.
    setvbuf(stdout, nullptr, _IOLBF, 0);
    printf("datamosh-sync v0.3\n");

    // --other b.mp4 anywhere in the args, pull it out so the rest stays where it was
    string other_path = "";
    float cross_amount = 1.0f;
    string music = "a";
    {
        int out = 1;
        for (int i = 1; i < argc; i++) {
            if (string(argv[i]) == "--other" && i + 1 < argc) {
                other_path = argv[++i];
            } else if (string(argv[i]) == "--cross" && i + 1 < argc) {
                cross_amount = parse_arg(argv[++i]);
            } else if (string(argv[i]) == "--music" && i + 1 < argc) {
                music = argv[++i];
            } else {
                argv[out++] = argv[i];
            }
        }
        argc = out;
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <video> [audio]\n", argv[0]);
        fprintf(stderr, "  If audio is omitted, will try to extract from video\n");
        return 1;
    }

    // the gui asks for this to draw the track, doesnt render anything
    if (string(argv[1]) == "--sections") {
        if (argc < 3) return 1;
        AudioAnalyzer song;
        string song_path = argv[2];
        if (!other_path.empty() && music == "b") song_path = other_path;
        if (!other_path.empty() && music == "both") song_path = mix_music(argv[2], other_path);
        if (!song.load(song_path)) return 1;
        if (!other_path.empty()) song.limit_to(shorter_video(argv[2], other_path));
        if (!other_path.empty() && music == "both") remove(song_path.c_str());
        song.analyze();
        print_sections(cut_song(&song, argv[2]));
        // quarter sec is plenty for drawing it
        vector<float> z = intensity_curve(&song);
        for (size_t i = 0; i < z.size(); i += 5) printf("SWING %.3f\n", z[i]);
        fflush(stdout);
        return 0;
    }

    string video_path = argv[1];
    string audio_path = "";
    bool use_embedded_audio = false;

    if (argc >= 3) {
        audio_path = argv[2];
        if (audio_path.empty()) {
            use_embedded_audio = true;
        }
    } else {
        printf("No separate audio file, trying embedded audio...\n");
        use_embedded_audio = true;
    }

    string mixed_file = "";
    if (!other_path.empty() && music != "a") {
        use_embedded_audio = false;
        if (music == "b") {
            audio_path = other_path;
        } else {
            mixed_file = mix_music(video_path, other_path);
            audio_path = mixed_file;
        }
    }

    string basename = get_basename(video_path);
    if (!other_path.empty()) basename += "_x_" + get_basename(other_path);
    string tmp_path = basename + "_munched_noaudio.mp4";
    string out_path = basename + "_munched.mp4";

    VideoMosher mosher;
    if (!other_path.empty()) mosher.song_limit = shorter_video(video_path, other_path);

    printf("initializing...\n");

    if (use_embedded_audio) {
        if (!mosher.init_with_embedded_audio(video_path)) {
            fprintf(stderr, "Failed to initialize (no audio in video?)\n");
            return 1;
        }
    } else {
        if (!mosher.init(video_path, audio_path)) {
            fprintf(stderr, "Failed to initialize\n");
            return 1;
        }
    }

    if (!other_path.empty() && !mosher.open_other(other_path)) return 1;

    ProcessParams params = {
        0.5f,   // glitch_intensity
        65.0f,  // jpeg_quality
        0.3f,   // motion_blur
        0.2f,   // color_shift
        0.3f,   // block_shuffle
        0.7f,   // beat_sync
        0.45f,  // rot
        0.3f,   // warp
        0.25f,  // quake
        0.2f,   // spectral
        0.1f,   // groove
        0.12f,  // texswap
        0.1f,   // recursion
        0.12f   // hall of mirrors, was .25
    };

    // optional params via extra args: <glitch> <jpeg> <mblur> <color> <shuffle> <beatsync>
    // only override if all 6 are provided
    if (argc >= 9) {
        params.glitch_intensity = parse_arg(argv[3]);
        params.jpeg_quality     = parse_arg(argv[4]);
        params.motion_blur      = parse_arg(argv[5]);
        params.color_shift      = parse_arg(argv[6]);
        params.block_shuffle    = parse_arg(argv[7]);
        params.beat_sync        = parse_arg(argv[8]);
        printf("using custom params from args\n");
    }
    // <rot> <warp> <quake> <spectral> go after those
    if (argc >= 13) {
        params.rot      = parse_arg(argv[9]);
        params.warp     = parse_arg(argv[10]);
        params.quake    = parse_arg(argv[11]);
        params.spectral = parse_arg(argv[12]);
    }
    // and <groove> <texswap> <recursion> after that
    if (argc >= 16) {
        params.groove    = parse_arg(argv[13]);
        params.texswap   = parse_arg(argv[14]);
        params.recursion = parse_arg(argv[15]);
    }
    if (argc >= 17) params.hom = parse_arg(argv[16]);

    // and the parts sliders, same order as enum Fx. they start at 1 = always on
    // <glitch> <blur> <void> <stuck> <beatsync> <rot> <warp> <quake> <spectral>
    // <groove> <texswap> <recursion> <hom>
    for (int i = 0; i < FX_COUNT; i++) {
        params.parts[i] = 1.0f;
        if (argc > 17 + i) params.parts[i] = parse_arg(argv[17 + i]);
        if (params.parts[i] < 0) params.parts[i] = 0;
        if (params.parts[i] > 1) params.parts[i] = 1;
    }

    // last one, rise and fall. 0 = sliders stay where they are
    params.swing = 1.0f;
    if (argc > 30) params.swing = parse_arg(argv[30]);
    params.cross = cross_amount;

    printf("processing...\n");
    if (!mosher.process_video(tmp_path, params)) {
        fprintf(stderr, "Processing failed\n");
        return 1;
    }

    // mux audio back in - doing this in C++ was a nightmare
    // just call ffmpeg directly, way simpler
    // if separate audio was given, use that; otherwise use the video's own audio
    string audio_src = (use_embedded_audio || audio_path.empty()) ? video_path : audio_path;
    printf("muxing audio from %s...\n", audio_src.c_str());

    int ret = mux_audio(tmp_path, audio_src, out_path);
    if (ret != 0) {
        fprintf(stderr, "audio mux failed (exit %d), output has no audio\n", ret);
        rename(tmp_path.c_str(), out_path.c_str());
    } else {
        remove(tmp_path.c_str());
        printf("output: %s\n", out_path.c_str());
    }

    if (!mixed_file.empty()) remove(mixed_file.c_str());

    printf("done!\n");
    return 0;
}

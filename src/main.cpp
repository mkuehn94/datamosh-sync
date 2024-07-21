// datamosh music sync - main entry
// TODO: clean this up later

#include <iostream>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include "video_processor.h"
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

// run ffmpeg to mux audio into video
// returns 0 on success
int mux_audio(const string& video, const string& audio_src, const string& out) {
    pid_t pid = fork();
    if (pid == 0) {
        // child
        execlp("ffmpeg", "ffmpeg",
               "-i", video.c_str(),
               "-i", audio_src.c_str(),
               "-c:v", "copy",
               "-c:a", "copy",
               "-map", "0:v:0",
               "-map", "1:a:0",
               "-shortest",
               out.c_str(),
               "-y",
               "-loglevel", "error",
               (char*)nullptr);
        exit(1);  // execlp failed
    } else if (pid < 0) {
        return -1;  // fork failed
    }
    int status;
    waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
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
    cout << "datamosh-sync v0.3" << endl;

    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <video> [audio]" << endl;
        cerr << "  If audio is omitted, will try to extract from video" << endl;
        return 1;
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
        cout << "No separate audio file, trying embedded audio..." << endl;
        use_embedded_audio = true;
    }

    string basename = get_basename(video_path);
    string tmp_path = basename + "_munched_noaudio.mp4";
    string out_path = basename + "_munched.mp4";

    VideoMosher mosher;

    cout << "initializing..." << endl;

    if (use_embedded_audio) {
        if (!mosher.init_with_embedded_audio(video_path)) {
            cerr << "Failed to initialize (no audio in video?)" << endl;
            return 1;
        }
    } else {
        if (!mosher.init(video_path, audio_path)) {
            cerr << "Failed to initialize" << endl;
            return 1;
        }
    }

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
        0.1f    // recursion
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
        cout << "using custom params from args" << endl;
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

    cout << "processing..." << endl;
    if (!mosher.process_video(tmp_path, params)) {
        cerr << "Processing failed" << endl;
        return 1;
    }

    // mux audio back in - doing this in C++ was a nightmare
    // just call ffmpeg directly, way simpler
    // if separate audio was given, use that; otherwise use the video's own audio
    string audio_src = (use_embedded_audio || audio_path.empty()) ? video_path : audio_path;
    cout << "muxing audio from " << audio_src << "..." << endl;

    int ret = mux_audio(tmp_path, audio_src, out_path);
    if (ret != 0) {
        cerr << "audio mux failed (exit " << ret << "), output has no audio" << endl;
        rename(tmp_path.c_str(), out_path.c_str());
    } else {
        remove(tmp_path.c_str());
        cout << "output: " << out_path << endl;
    }

    cout << "done!" << endl;
    return 0;
}

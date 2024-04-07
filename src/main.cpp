// datamosh music sync - main entry
// TODO: clean this up later

#include <iostream>
#include <string>
#include "video_processor.h"
#include "audio_analyzer.h"
#include "processor.h"

using namespace std;

int main(int argc, char** argv) {
    cout << "datamosh-sync v0.2" << endl;

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
    } else {
        cout << "No separate audio file, trying embedded audio..." << endl;
        use_embedded_audio = true;
    }

    // quick test - process a video
    VideoMosher mosher;

    cout << "initializing..." << endl;

    // initialize differently based on audio source
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

    // default parameters
    ProcessParams params = {
        0.5f,   // glitch_intensity
        65.0f,  // jpeg_quality
        0.3f,   // motion_blur
        0.2f,   // color_shift
        0.3f,   // block_shuffle
        0.7f    // beat_sync
    };

    cout << "processing..." << endl;
    if (!mosher.process_video("output.mp4", params)) {
        cerr << "Processing failed" << endl;
        return 1;
    }

    cout << "done!" << endl;
    return 0;
}

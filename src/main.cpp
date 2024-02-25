// datamosh music sync - main entry
// TODO: clean this up later

#include <iostream>
#include <string>
#include "video_processor.h"
#include "audio_analyzer.h"
#include "gui.h"

using namespace std;

int main(int argc, char** argv) {
    cout << "datamosh-sync v0.1" << endl;

    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <video> <audio>" << endl;
        return 1;
    }

    string video_path = argv[1];
    string audio_path = argv[2];

    // init gui
    GUI gui;
    if (!gui.init()) {
        cerr << "Failed to init GUI" << endl;
        return 1;
    }

    gui.run(video_path, audio_path);

    return 0;
}

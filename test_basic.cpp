#include <cstdio>
#include "src/processor.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s video\n", argv[0]);
        return 1;
    }
    VideoProcessor vp;
    if (!vp.open(argv[1])) return 1;
    if (vp.get_width() <= 0 || vp.get_height() <= 0 || !vp.read_frame()) return 1;
    if (!vp.get_frame_rgb()) return 1;
    vp.close();
    vp.close();  // Explicit close followed by destruction must also be safe.
    if (!vp.open(argv[1]) || !vp.read_frame() || !vp.get_frame_rgb()) return 1;
    printf("video reader ok\n");
    return 0;
}

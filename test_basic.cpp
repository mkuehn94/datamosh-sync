// test if basic objects work at all
#include <iostream>
#include "src/processor.h"

int main() {
    std::cout << "creating VideoProcessor..." << std::endl;
    VideoProcessor* vp = new VideoProcessor();
    std::cout << "created ok" << std::endl;

    std::cout << "trying to open..." << std::endl;
    if (vp->open("test_small.mp4")) {
        std::cout << "opened ok!" << std::endl;
    } else {
        std::cout << "failed to open" << std::endl;
        delete vp;
        return 1;
    }

    delete vp;
    std::cout << "done" << std::endl;
    return 0;
}

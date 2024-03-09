// quick debug test
// trying to figure out why colors look weird

#include <iostream>
#include "src/effects.h"

#define TEST_WIDTH 320
#define TEST_HEIGHT 240

int main() {
    uint8_t* test_frame = new uint8_t[TEST_WIDTH * TEST_HEIGHT * 3];

    // fill with gradient
    for (int y = 0; y < TEST_HEIGHT; y++) {
        for (int x = 0; x < TEST_WIDTH; x++) {
            int idx = (y * TEST_WIDTH + x) * 3;
            test_frame[idx + 0] = (x * 255) / TEST_WIDTH;  // R
            test_frame[idx + 1] = (y * 255) / TEST_HEIGHT; // G
            test_frame[idx + 2] = 128;                      // B constant
        }
    }

    EffectEngine fx;

    std::cout << "testing color shift..." << std::endl;
    fx.color_channel_shift(test_frame, TEST_WIDTH, TEST_HEIGHT, 0.5);

    std::cout << "testing block shuffle..." << std::endl;
    fx.block_shuffle(test_frame, TEST_WIDTH, TEST_HEIGHT, 0.3);

    // check some pixels
    std::cout << "pixel (0,0): "
              << (int)test_frame[0] << " "
              << (int)test_frame[1] << " "
              << (int)test_frame[2] << std::endl;

    std::cout << "pixel (100,100): ";
    int idx = (100 * TEST_WIDTH + 100) * 3;
    std::cout << (int)test_frame[idx] << " "
              << (int)test_frame[idx+1] << " "
              << (int)test_frame[idx+2] << std::endl;

    delete[] test_frame;

    return 0;
}

// compile: g++ -o debug_test debug_test.cpp src/effects.cpp -I.

#include <cassert>
#include <cstdio>
#include "src/effects.h"

int main() {
    EffectEngine effects;
    uint8_t pixels[] = {10, 20, 30, 40, 50, 60, 70, 80, 90};
    effects.color_channel_shift(pixels, 3, 1, 0.05f);
    const uint8_t expected[] = {10, 20, 30, 10, 50, 60, 40, 80, 90};
    for (int i = 0; i < 9; i++) assert(pixels[i] == expected[i]);
    printf("color shift ok\n");
    return 0;
}

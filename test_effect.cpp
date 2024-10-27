#include <cassert>
#include <cstdio>
#include <cstring>
#include "src/effects.h"

int main() {
    EffectEngine effects;
    uint8_t current[] = {100, 120, 140, 160, 180, 200};
    uint8_t old[] = {20, 40, 60, 80, 100, 120};
    const uint8_t blended[] = {60, 80, 100, 120, 140, 160};
    effects.trails(current, 2, 1, old, 0.5f);
    assert(memcmp(current, blended, sizeof(current)) == 0);
    effects.trails(current, 2, 1, nullptr, 0.5f);
    assert(memcmp(current, blended, sizeof(current)) == 0);

    uint8_t pixel[] = {10, 20, 30};
    effects.color_remap(pixel, 1, 1, 0);
    assert(pixel[0] == 30 && pixel[1] == 20 && pixel[2] == 10);
    effects.color_remap(pixel, 1, 1, 2);
    assert(pixel[0] == 225 && pixel[1] == 235 && pixel[2] == 245);

    uint8_t image[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    uint8_t patch[6] = {};
    const uint8_t expected[] = {4, 5, 6, 10, 11, 12};
    effects.grab_rect(image, 2, 2, patch, 1, 0, 1, 2);
    assert(memcmp(patch, expected, sizeof(patch)) == 0);
    printf("effects ok\n");
    return 0;
}

// trying to corrupt h264 stream directly
// this might be the key to real datamoshing

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// h264 NAL unit types
#define NAL_SLICE 1
#define NAL_IDR 5
#define NAL_SEI 6
#define NAL_SPS 7
#define NAL_PPS 8

static int at_start_code(uint8_t* p) {
    return p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3] == 1;
}

void remove_iframes(const char* input, const char* output) {
    FILE* in = fopen(input, "rb");
    if (!in) {
        fprintf(stderr, "cant open input\n");
        return;
    }

    fseek(in, 0, SEEK_END);
    long size = ftell(in);
    fseek(in, 0, SEEK_SET);

    uint8_t* buffer = (uint8_t*)malloc(size);  // file is still called .cpp, whatever
    fread(buffer, 1, size, in);
    fclose(in);

    printf("read %ld bytes\n", size);

    // walk the stream once, keep everything that isnt an I-frame or the
    // parameter sets
    FILE* out = fopen(output, "wb");
    static const uint8_t start[] = {0, 0, 0, 1};
    int found = 0;

    for (long i = 0; i < size - 4; i++) {
        if (at_start_code(buffer + i)) {
            long offset = i + 4;
            int type = buffer[offset] & 0x1F;

            long next = offset;
            while (next < size - 4 && !at_start_code(buffer + next)) {
                next++;
            }
            found++;

            if (type != NAL_IDR && type != NAL_SPS && type != NAL_PPS) {
                fwrite(start, 1, 4, out);
                fwrite(buffer + offset, 1, next - offset, out);
            } else {
                printf("skipping I-frame at %ld\n", offset);
            }

            i = next - 1;
        }
    }

    printf("found %d NAL units\n", found);

    fclose(out);
    free(buffer);

    printf("done\n");
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s input.h264 output.h264\n", argv[0]);
        return 1;
    }

    remove_iframes(argv[1], argv[2]);

    return 0;
}

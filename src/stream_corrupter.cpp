// trying to corrupt h264 stream directly
// this might be the key to real datamoshing

#include <iostream>
#include <fstream>
#include <vector>

using namespace std;

// h264 NAL unit types
#define NAL_SLICE 1
#define NAL_IDR 5
#define NAL_SEI 6
#define NAL_SPS 7
#define NAL_PPS 8

struct NALUnit {
    int type;
    int offset;
    int size;
};

void remove_iframes(const char* input, const char* output) {
    ifstream in(input, ios::binary);
    if (!in) {
        cerr << "cant open input" << endl;
        return;
    }

    in.seekg(0, ios::end);
    int size = in.tellg();
    in.seekg(0, ios::beg);

    uint8_t* buffer = new uint8_t[size];
    in.read((char*)buffer, size);
    in.close();

    cout << "read " << size << " bytes" << endl;

    // walk the stream once, keep everything that isnt an I-frame or the
    // parameter sets. start codes are 0x00 0x00 0x00 0x01
    ofstream out(output, ios::binary);
    int found = 0;

    for (int i = 0; i < size - 4; i++) {
        if (buffer[i] == 0 && buffer[i+1] == 0 && buffer[i+2] == 0 && buffer[i+3] == 1) {
            int offset = i + 4;
            int type = buffer[i+4] & 0x1F;

            int next = offset;
            while (next < size - 4) {
                if (buffer[next] == 0 && buffer[next+1] == 0 &&
                    buffer[next+2] == 0 && buffer[next+3] == 1) {
                    break;
                }
                next++;
            }
            found++;

            if (type != NAL_IDR && type != NAL_SPS && type != NAL_PPS) {
                uint8_t start[] = {0, 0, 0, 1};
                out.write((char*)start, 4);
                out.write((char*)&buffer[offset], next - offset);
            } else {
                cout << "skipping I-frame at " << offset << endl;
            }

            i = next - 1;
        }
    }

    cout << "found " << found << " NAL units" << endl;

    out.close();
    delete[] buffer;

    cout << "done" << endl;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        cerr << "usage: " << argv[0] << " input.h264 output.h264" << endl;
        return 1;
    }

    remove_iframes(argv[1], argv[2]);

    return 0;
}

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

vector<NALUnit> find_nal_units(uint8_t* data, int size) {
    vector<NALUnit> units;

    // look for start codes: 0x00 0x00 0x00 0x01
    for (int i = 0; i < size - 4; i++) {
        if (data[i] == 0 && data[i+1] == 0 && data[i+2] == 0 && data[i+3] == 1) {
            NALUnit nal;
            nal.offset = i + 4;
            nal.type = data[i+4] & 0x1F;

            // find next start code
            int next = i + 4;
            while (next < size - 4) {
                if (data[next] == 0 && data[next+1] == 0 &&
                    data[next+2] == 0 && data[next+3] == 1) {
                    break;
                }
                next++;
            }
            nal.size = next - nal.offset;

            units.push_back(nal);
            i = next - 1;
        }
    }

    return units;
}

void remove_iframes(const char* input, const char* output) {
    // read file
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

    // find NAL units
    vector<NALUnit> nals = find_nal_units(buffer, size);
    cout << "found " << nals.size() << " NAL units" << endl;

    // write output without I-frames
    ofstream out(output, ios::binary);

    for (const NALUnit& nal : nals) {
        if (nal.type != NAL_IDR && nal.type != NAL_SPS && nal.type != NAL_PPS) {
            // write start code
            uint8_t start[] = {0, 0, 0, 1};
            out.write((char*)start, 4);
            out.write((char*)&buffer[nal.offset], nal.size);
        } else {
            cout << "skipping I-frame at " << nal.offset << endl;
        }
    }

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

import numpy
from PIL import Image
import sys
import os

# opencv is gone, one reshape and the raw rgb frames from ffmpeg are enough
def read_frames(path, w, h):
    data = open(path, "rb").read()
    n = len(data) // (w * h * 3)
    return numpy.frombuffer(data, dtype=numpy.uint8).reshape(n, h, w, 3)

def read_beats(path):
    out = []
    for line in open(path):
        p = line.split()
        if p:
            out.append(int(float(p[0])))
    return out

def quantize(vecs, beats, fps):
    # one vector per beat: the beat the source frame falls into decides
    # where the block starts, everything between two beats is the freeze
    # of that block. the first vector of a beat wins.
    out = []
    seen = []
    for (f, x, y, ww, hh, dx, dy, mode) in vecs:
        b = 0
        while b + 1 < len(beats) and beats[b + 1] <= int(f * 1000 / fps):
            b += 1
        if b in seen:
            continue
        seen.append(b)
        out.append((b, x, y, ww, hh, dx, dy, mode))
    return out

def read_vecs(path):
    out = []
    for line in open(path):
        p = line.split()
        if len(p) < 8:
            continue
        out.append((int(p[0]), int(p[1]), int(p[2]), int(p[3]), int(p[4]),
                    int(p[5]), int(p[6]), p[7]))
    return out

def mosh(frames, vecs, w, h):
    out = frames.copy()
    for (f, x, y, ww, hh, dx, dy, _mode) in vecs:
        if f + 1 >= frames.shape[0]:
            continue
        src = frames[f][y:y + hh, x:x + ww]
        dst = out[f + 1]
        ny = y + dy
        nx = x + dx
        if ny < 0 or nx < 0 or ny + hh > h or nx + ww > w:
            continue
        dst[ny:ny + hh, nx:nx + ww] = src
    return out

def main():    frames = read_frames(sys.argv[1], int(sys.argv[2]), int(sys.argv[3]))
    vecs = read_vecs(sys.argv[4])
    beats = read_beats(sys.argv[5]) if len(sys.argv) > 5 else []
    print(frames.shape, len(vecs), len(beats))
    w = int(sys.argv[2])
    h = int(sys.argv[3])
    if beats:
        vecs = quantize(vecs, beats, 24)
    out = frames.copy()
    for (f, x, y, ww, hh, dx, dy, _mode) in vecs:
        if f + 1 >= frames.shape[0]:
            continue
        src = frames[f][y:y + hh, x:x + ww]
        dst = out[f + 1]
        ny = y + dy
        nx = x + dx
        if ny < 0 or nx < 0 or ny + hh > h or nx + ww > w:
            continue
        dst[ny:ny + hh, nx:nx + ww] = src
    os.makedirs("out", exist_ok=True)
    open("out/frames.rgb", "wb").write(out.reshape(-1).tobytes())

main()

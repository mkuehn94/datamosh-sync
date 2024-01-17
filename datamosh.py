import numpy
from PIL import Image
import sys

# opencv is gone, one reshape and the raw rgb frames from ffmpeg are enough
def read_frames(path, w, h):
    data = open(path, "rb").read()
    n = len(data) // (w * h * 3)
    return numpy.frombuffer(data, dtype=numpy.uint8).reshape(n, h, w, 3)

def read_vecs(path):
    out = []
    for line in open(path):
        p = line.split()
        if len(p) < 8:
            continue
        out.append((int(p[0]), int(p[1]), int(p[2]), int(p[3]), int(p[4]),
                    int(p[5]), int(p[6]), p[7]))
    return out

def main():
    if len(sys.argv) < 4:
        print("usage: datamosh frames.rgb w h vec.mv")
        return
    frames = read_frames(sys.argv[1], int(sys.argv[2]), int(sys.argv[3]))
    vecs = read_vecs(sys.argv[4])
    print(frames.shape, len(vecs))
    w = int(sys.argv[2])
    h = int(sys.argv[3])
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
    open("out/frames.rgb", "wb").write(out.reshape(-1).tobytes())

main()

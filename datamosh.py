import numpy
from PIL import Image
import sys
import os
import math
from bisect import bisect_right

# opencv is gone, one reshape and the raw rgb frames from ffmpeg are enough
def read_frames(path, w, h):
    with open(path, "rb") as source:
        data = source.read()
    n = len(data) // (w * h * 3)
    return numpy.frombuffer(data, dtype=numpy.uint8).reshape(n, h, w, 3)

def read_beats(path):
    out = []
    with open(path) as source:
        for line in source:
            p = line.split()
            if p:
                out.append(float(p[0]))
    return out

def quantize(vecs, beats, fps):
    if not math.isfinite(fps) or fps <= 0:
        raise ValueError("fps must be positive and finite")
    if any(not math.isfinite(t) or t < 0 for t in beats):
        raise ValueError("beat timestamps must be nonnegative and finite")
    if any(a >= b for a, b in zip(beats, beats[1:])):
        raise ValueError("beat timestamps must be strictly increasing")
    if not beats:
        return list(vecs)

    # First vector in sidecar order per beat wins. Keep its source frame:
    # a beat number is not a frame number.
    out = []
    seen = set()
    for v in vecs:
        b = bisect_right(beats, v[0] * 1000 / fps) - 1
        if b < 0 or b in seen:
            continue
        seen.add(b)
        out.append(v)
    return out

def read_vecs(path):
    out = []
    with open(path) as source:
        for line in source:
            p = line.split()
            if len(p) < 8:
                continue
            out.append((int(p[0]), int(p[1]), int(p[2]), int(p[3]), int(p[4]),
                        int(p[5]), int(p[6]), p[7]))
    return out

def mosh(frames, vecs, w, h, beats=None, fps=24):
    if w <= 0 or h <= 0 or frames.shape[1:] != (h, w, 3):
        raise ValueError("frames must have shape (count, height, width, 3)")
    beats = [] if beats is None else list(beats)
    vecs = quantize(vecs, beats, fps)
    out = frames.copy()
    for (f, x, y, ww, hh, dx, dy, _mode) in vecs:
        if f < 0 or f + 1 >= frames.shape[0]:
            continue
        if ww <= 0 or hh <= 0 or x < 0 or y < 0 or x + ww > w or y + hh > h:
            continue
        ny = y + dy
        nx = x + dx
        if ny < 0 or nx < 0 or ny + hh > h or nx + ww > w:
            continue

        stop = f + 2
        if beats:
            b = bisect_right(beats, f * 1000 / fps) - 1
            # Exclusive boundary: first output frame at or after the next beat.
            stop = math.ceil(beats[b + 1] * fps / 1000) if b + 1 < len(beats) else len(frames)
            stop = min(stop, len(frames))
        src = frames[f, y:y + hh, x:x + ww]
        # Hold only this displaced patch; the rest of every frame stays live.
        out[f + 1:stop, ny:ny + hh, nx:nx + ww] = src
    return out

def main():
    if len(sys.argv) < 5:
        print("usage: datamosh frames.rgb w h vec.mv [beats.txt [fps]]")
        return
    w = int(sys.argv[2])
    h = int(sys.argv[3])
    frames = read_frames(sys.argv[1], w, h)
    vecs = read_vecs(sys.argv[4])
    beats = read_beats(sys.argv[5]) if len(sys.argv) > 5 else []
    print(frames.shape, len(vecs), len(beats))
    fps = float(sys.argv[6]) if len(sys.argv) > 6 else 24
    out = mosh(frames, vecs, w, h, beats=beats, fps=fps)
    os.makedirs("out/frames", exist_ok=True)
    n = 0
    while n < out.shape[0]:
        img = Image.fromarray(out[n])
        img.save("out/frames/f%04d.ppm" % n)
        n += 1

if __name__ == "__main__":
    main()

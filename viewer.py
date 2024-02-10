import curses
import numpy
import sys

def main():
    if len(sys.argv) < 4:
        print("usage: viewer frames.rgb w h")
        return
    w = int(sys.argv[2])
    h = int(sys.argv[3])
    data = open(sys.argv[1], "rb").read()
    n = len(data) // (w * h * 3)
    frames = numpy.frombuffer(data, dtype=numpy.uint8).reshape(n, h, w, 3)
    std = curses.initscr()
    curses.noecho()
    i = 0
    while i < n:
        row = 1
        y = 0
        while y < h // 2:
            x = 0
            while x < w:
                c = frames[i][y * 2, x]
                std.addch(row + y, x, 35 if c[0] > 128 else 46)
                x += 1
            y += 1
        std.refresh()
        i += 1
    curses.endwin()

main()

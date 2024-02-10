import sys
import tkinter as tk

import numpy
from PIL import Image, ImageTk


def main(argv=None):
    args = sys.argv[1:] if argv is None else argv
    if len(args) != 3:
        print("usage: viewer2 frames.rgb w h", file=sys.stderr)
        return 1
    w, h = int(args[1]), int(args[2])
    if w <= 0 or h <= 0:
        raise ValueError("width and height must be positive")
    with open(args[0], "rb") as source:
        data = source.read()
    frame_size = w * h * 3
    if not data or len(data) % frame_size:
        raise ValueError("RGB input must contain whole frames")
    frames = numpy.frombuffer(data, dtype=numpy.uint8).reshape(-1, h, w, 3)

    root = tk.Tk()
    root.title("datamosh viewer")
    label = tk.Label(root)
    label.pack()
    index = 0

    def next_frame():
        nonlocal index
        img = Image.fromarray(frames[index])
        img = img.resize((w * 4, h * 4), Image.Resampling.NEAREST)
        photo = ImageTk.PhotoImage(img, master=root)
        label.configure(image=photo)
        label.image = photo  # Tk does not retain the Python reference.
        index = (index + 1) % len(frames)
        root.after(40, next_frame)

    next_frame()
    root.mainloop()
    return 0


if __name__ == "__main__":
    sys.exit(main())

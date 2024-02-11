datamoshing where the artifacts follow the beat instead of whatever
the encoder thought was funny.

the pipeline is three steps, all of it ffmpeg + numpy:

    scripts/frames.sh clip.mp4 frames.rgb
    datamosh.py frames.rgb W H blocks.mv [beats.txt [fps]]
    scripts/gif.sh                     # out/frames -> mp4 + gif

the block matcher is not in this repo, it writes the sidecar:

    f x y w h dx dy mode

one line per macroblock, `f` the frame the vector belongs to. `mode` is
`hold`, `drop` or `freeze`, nothing reads `drop` yet.

with a beats.txt (one ms timestamp per line, scripts/beats.sh makes one
from a wav) the vectors get quantized: one vector per beat, first one
wins, the rest of the beat is the freeze of that block. that is the
whole sync idea, the beat grid decides where a block starts.

viewer.py plays raw RGB in the terminal. viewer2.py opens a Tk window
and needs tkinter, Pillow and a display.
## todo

- `drop` vectors, a block that drops should pull the frame behind
  it instead of freezing
- beat grid from the wav inside the python instead of scripts/beats.sh
- raw RGB uses FFmpeg rawvideo; PPM sequences use image2

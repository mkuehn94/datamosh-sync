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

## C++ implementation

trying a C++ version with proper GUI and real-time control. way faster.

src/ contains the new implementation:
- video_processor: ffmpeg wrapper for reading video
- audio_analyzer: beat detection (placeholder for now)
- effects: all the datamosh effects
- gui: gtk3 interface with parameter sliders
- stream_corrupter: experimental h264 NAL manipulation

build with `make`, run with `./datamosh-sync video.mp4 audio.mp3`

effects implemented:
- motion vector glitching
- jpeg corruption
- block shuffling
- color channel shifts  
- beat-synced pulse/zoom
- wave distortion

still need to:
- get real beat detection working
- implement actual I-frame removal
- add video preview to GUI
- optimize the slow effects
- add export functionality

## todo

- `drop` vectors, a block that drops should pull the frame behind
  it instead of freezing
- beat grid from the wav inside the python instead of scripts/beats.sh
- raw RGB uses FFmpeg rawvideo; PPM sequences use image2

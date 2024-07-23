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
- audio_analyzer: real beat detection now (spectral flux), bass/treble curves
- warp: fake decoder. rot, warp, time quake, spectral lag
- effects: all the datamosh effects
- gui: gtk3 interface with parameter sliders
- stream_corrupter: experimental h264 NAL manipulation

build with `make`, run with:
```
./datamosh-sync video.mp4              # uses embedded audio
./datamosh-sync video.mp4 audio.mp3    # separate audio file
```

the good stuff (src/warp.cpp), all 0-1, all follow the music:

- ROT: decoder that lost its keyframes. real vectors + real residuals applied
  to a canvas that never gets refreshed. brightness heals before color so
  the street gets sharp again in the colors of the last place
- WARP: same thing but the vectors get hijacked. bass kicks a tunnel, kicks
  rip gravity wells into the picture
- TIME QUAKE: every macroblock reads from a different moment, kicks send a
  shockwave through time. RAM: width*height*3*fps*2 bytes (560MB for 1440x1080 at 60fps)
- SPECTRAL LAG: real 8x8 dct, frequencies held at different times. edges
  from now, brightness from seconds ago. flips every 8 kicks
- MOTION SAMPLER: tapes the vectors of 8 beats and loops them like a drum
  sample on the current picture. high = stained glass
- TEXTURE SWAP: blocks keep their brightness and wear the detail of another
  place. road with windows on it
- RECURSION: kicks fill rectangles with tiny copies of the frame, copies
  inside copies, then they melt
- HALL OF MIRRORS: the noclip thing. screen doesnt clear, moving stuff gets
  drawn on top over and over and leaves copies. works on a moving camera too,
  it guesses the camera motion and an empty background. clears land on kicks

```
./datamosh-sync video.mp4 "" <glitch> <void_tol> <blur> <void_depth> <stuck> <beatsync> <rot> <warp> <quake> <spectral> <groove> <texswap> <recursion> <hom>

# defaults
./datamosh-sync video.mp4 "" 0.5 65 0.3 0.2 0.3 0.7 0.45 0.3 0.25 0.2 0.1 0.12 0.1 0.12
# melt
./datamosh-sync video.mp4 "" 0 100 0 0 0 0 0.92 1 0.8 1 0 0 0 0
# oil painting / stained glass
./datamosh-sync video.mp4 "" 0 100 0 0 0 0.3 0.5 0.2 0 0 0.8 0 0 0
# shuffled city collage
./datamosh-sync video.mp4 "" 0 100 0 0 0 0.3 0.3 0.2 0.15 0 0 0.7 0.35 0
# noclip, cars pile up behind themselves
./datamosh-sync video.mp4 "" 0 100 0 0 0 0.3 0 0 0 0 0 0 0 0.6
```

NOTE dont put everything up high at once, its just grey noise. pick 2 or 3

old effects list:
- motion vector glitching
- jpeg corruption
- block shuffling
- color channel shifts  
- beat-synced pulse/zoom
- wave distortion

still need to:
- implement actual I-frame removal
- add video preview to GUI
- optimize the slow effects
- add export functionality

## todo

- `drop` vectors, a block that drops should pull the frame behind
  it instead of freezing
- beat grid from the wav inside the python instead of scripts/beats.sh
- raw RGB uses FFmpeg rawvideo; PPM sequences use image2

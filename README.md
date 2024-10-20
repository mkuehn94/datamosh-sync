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
# arranged: everything up high but only in its own parts of the song
./datamosh-sync video.mp4 "" 0.5 65 0.5 0.5 0.5 0.7 0.8 0.6 0.7 0.6 0.5 0.6 0.5 0.6  0.25 0.2 0.2 0.15 0.3 0.3 0.2 0.25 0.2 0.2 0.15 0.15 0.25
```

## parts of the song

every effect has a second slider, how much of the song it plays. 1 = the whole
time, .3 = about 30%. so you can turn things up high and they dont all pile up,
it plays more like a track than effects on top of each other

![the track](docs/track.png)

how it decides:

- cuts the song where it starts sounding different (4 sec before vs 4 sec
  after) and at hard cuts in the video. everything lands on a beat. parts
  longer than 6 sec get chopped
- every effect has a taste, see the table in src/arrange.cpp. warp wants bass
  and movement, spectral wants quiet and airy, recursion wants kick drums.
  done by ear, change it if you hear it different
- pickiest effect chooses first, sections that are already full count against
  it so stuff spreads out. same settings = same track every time

it also guesses the big picture of the song from a slow "tension" curve (loud +
bright + busy + bass):

- lead-in: everything before the first big moment. quiet doesnt mean nothing
  happens, smooth effects only (rot, spectral, mirrors, sampler, blur) and it
  slowly gets more. the sharp ones wait for the drop
- build: tension keeps climbing for 8+ sec. "how hard" ramps up from 35% to the
  full slider so it gets worse and worse
- drop: loud + bass jump out of nowhere. right before it the picture goes almost
  clean for a second, then the big effects come in together
- breakdown: was loud, now clearly isnt. calm effects like rot and blur get it
- climax: strongest 16 sec of the song, later counts a bit more. only place where
  stacking effects is encouraged. after that it falls apart slowly
- same loop all the way through = no moments, it says so in the terminal

how violent every effect is counts too, combined with its how hard slider. quake
at .25 is already rougher than texture swap at .5, glitch at 1 is still harmless.
those numbers got measured, every effect alone at 4 strengths on 3 clips (table in
src/arrange.cpp, half "how wrecked is the picture", half "how hard does it jump
between frames"). then:

- every section gets a target for how violent it should be, from the music. it
  gets scaled to what your sliders can pay for
- sections get swapped between effects until it gets close to those targets
- effect way too violent for its moment gets turned down, slider is the ceiling
- two effects dont add up, the picture can only get so wrecked. but no single
  effect can fill a section alone, or quake takes the whole climax by itself

on the track: brighter block = more violent right there

its all guesses from loudness and brightness. no idea about melody or vocals. the
strip on top of the track shows what it thinks, white line = how hard right now

in the gui: set how much of the song every effect gets, hit CALCULATE TRACK and
then the track shows up. move a slider and its gone until you calculate again
(only the first time for a video takes a while). MOSH IT calculates by itself if
you didnt. while it renders a red line goes along and the names of whats playing
go yellow

on the command line they go after the other 14, same order as above but without
void_tol:

```
<glitch> <blur> <void> <stuck> <beatsync> <rot> <warp> <quake> <spectral> <groove> <texswap> <recursion> <hom>
```

all of them left out or at 1 = same output as before, it doesnt even listen for
sections and no build/drop stuff happens. the terminal prints the track too

NOTE
- percent is only roughly, it can only take whole sections. the smallest ones
  are 2-6 sec so on a 90 sec song .01 is really more like 3-6%
- effect starts fresh every time its section starts. rot builds slowly so in
  a 4 sec section it barely gets going, turn it up or give it more parts
- climax and drops eat a lot of the percent, so the parts in between can get
  pretty empty. give things more parts if thats too clean for you
- a slider at 1 keeps its section all the time but still follows the how hard
  ramp, also in the breath before a drop

NOTE dont put everything up high at once, its just grey noise. pick 2 or 3

## rise and fall

the slider isnt a fixed value anymore, its the MEAN. the value goes up and down
around it with how intense the music is right now (half quarter second, half 2
seconds, so it moves but doesnt shake on every kick)

how far it moves depends on the effect, thats the +- on the track and the table
in fx_spread in src/arrange.cpp. glitch moves +-.3, quake +-.05, hall of mirrors
+-.04 because at those a small change is already a different video. picked by hand

white line on the track = the real value there, bottom of the row 0, top 1. the
terminal prints what really happened at the end, look for [swing]

global slider RISE AND FALL, 0 = flat like before. on the command line its the very
last arg, after the 13 parts ones. left out = 1

## two videos at once

```
./datamosh-sync --other other.mp4 --music a --cross 0.5 video.mp4 "" 0 100 0 0 0 0 0.25 0.05 0 0 0 0 0 0
```

or in the gui, the second box. chunks of the picture stop being the first video and
become the other one, with its motion, residuals and healing. the chunks are blobs
of noise and change every 8 kicks

- `--music` a, b or both. the track gets made from whatever you pick
- `--cross` how much of the picture is the other video
- output gets the slower fps of the two and stops at the shorter one. other video
  gets cropped to the size of the first
- about 1.6x slower

every effect is in on it now:

- ROT / WARP: chunks move and heal like the other video
- TIME QUAKE: every other sheet of time is the other video, shockwaves tear holes
- SPECTRAL LAG: their shapes in our colors in the blobs, edges like chalk around them.
  look at it full size, half of it doesnt show on a thumbnail
- TEXTURE SWAP: wearing the surfaces of the other video
- HALL OF MIRRORS: moving things from BOTH get stamped on the frozen world
- RECURSION: tiny copies are often the other video, big hits the whole screen
- VOID: the sky and whatever else got keyed out is a window to the other video
- STUCK: frozen window into the other video
- MOTION SAMPLER: tapes the motion of the other video

keep rot low (.25 or so, not the .45 default) or it turns to mush and you cant tell
theres two videos. the track doesnt control the blobs yet, they stay on all song
even in the breath before a drop

```
# all of it, arranged
./datamosh-sync --other other.mp4 --music a --cross 0.35 video.mp4 "" 0.5 30 0.2 0.7 0.5 0.6 0.25 0.15 0.35 0.6 0.2 0.5 0.6 0.5  0.25 0.2 0.25 0.15 0.3 0.3 0.2 0.25 0.25 0.2 0.2 0.2 0.25 1
```



every effect alone at max, everything else off. 3 random 5 sec clips from a drone video,
screenshot is the last frame of the clip unless it says otherwise. full size is 1440x1080, these are shrunk

### nothing on, for comparison

```
./datamosh-sync video.mp4 "" 0 100 0 0 0 0 0 0 0 0 0 0 0 0
```

| | | |
|---|---|---|
| ![nothing on, for comparison 1](docs/effects/off_1.jpg) | ![nothing on, for comparison 2](docs/effects/off_2.jpg) | ![nothing on, for comparison 3](docs/effects/off_3.jpg) |

### ROT

```
./datamosh-sync video.mp4 "" 0 100 0 0 0 0 1 0 0 0 0 0 0 0
```

| | | |
|---|---|---|
| ![ROT 1](docs/effects/rot_1.jpg) | ![ROT 2](docs/effects/rot_2.jpg) | ![ROT 3](docs/effects/rot_3.jpg) |

### WARP

```
./datamosh-sync video.mp4 "" 0 100 0 0 0 0 0 1 0 0 0 0 0 0
```

| | | |
|---|---|---|
| ![WARP 1](docs/effects/warp_1.jpg) | ![WARP 2](docs/effects/warp_2.jpg) | ![WARP 3](docs/effects/warp_3.jpg) |

### TIME QUAKE

look at the block edges, cars and trees dont line up

```
./datamosh-sync video.mp4 "" 0 100 0 0 0 0 0 0 1 0 0 0 0 0
```

| | | |
|---|---|---|
| ![TIME QUAKE 1](docs/effects/quake_1.jpg) | ![TIME QUAKE 2](docs/effects/quake_2.jpg) | ![TIME QUAKE 3](docs/effects/quake_3.jpg) |

### SPECTRAL LAG

subtle in a still, look at the edges

```
./datamosh-sync video.mp4 "" 0 100 0 0 0 0 0 0 0 1 0 0 0 0
```

| | | |
|---|---|---|
| ![SPECTRAL LAG 1](docs/effects/spectral_1.jpg) | ![SPECTRAL LAG 2](docs/effects/spectral_2.jpg) | ![SPECTRAL LAG 3](docs/effects/spectral_3.jpg) |

### MOTION SAMPLER

```
./datamosh-sync video.mp4 "" 0 100 0 0 0 0 0 0 0 0 1 0 0 0
```

| | | |
|---|---|---|
| ![MOTION SAMPLER 1](docs/effects/groove_1.jpg) | ![MOTION SAMPLER 2](docs/effects/groove_2.jpg) | ![MOTION SAMPLER 3](docs/effects/groove_3.jpg) |

### TEXTURE SWAP

```
./datamosh-sync video.mp4 "" 0 100 0 0 0 0 0 0 0 0 0 1 0 0
```

| | | |
|---|---|---|
| ![TEXTURE SWAP 1](docs/effects/texswap_1.jpg) | ![TEXTURE SWAP 2](docs/effects/texswap_2.jpg) | ![TEXTURE SWAP 3](docs/effects/texswap_3.jpg) |

### RECURSION

```
./datamosh-sync video.mp4 "" 0 100 0 0 0 0 0 0 0 0 0 0 1 0
```

| | | |
|---|---|---|
| ![RECURSION 1](docs/effects/recursion_1.jpg) | ![RECURSION 2](docs/effects/recursion_2.jpg) | ![RECURSION 3](docs/effects/recursion_3.jpg) |

### HALL OF MIRRORS

```
./datamosh-sync video.mp4 "" 0 100 0 0 0 0 0 0 0 0 0 0 0 1
```

| | | |
|---|---|---|
| ![HALL OF MIRRORS 1](docs/effects/hom_1.jpg) | ![HALL OF MIRRORS 2](docs/effects/hom_2.jpg) | ![HALL OF MIRRORS 3](docs/effects/hom_3.jpg) |

### motion blur / trails

```
./datamosh-sync video.mp4 "" 0 100 1 0 0 0 0 0 0 0 0 0 0 0
```

| | | |
|---|---|---|
| ![motion blur / trails 1](docs/effects/blur_1.jpg) | ![motion blur / trails 2](docs/effects/blur_2.jpg) | ![motion blur / trails 3](docs/effects/blur_3.jpg) |

### THE VOID

void depth 1 + void tolerance 0. tolerance does nothing by itself. *worst frame of the clip*, not the end one

```
./datamosh-sync video.mp4 "" 0 0 0 1 0 0 0 0 0 0 0 0 0 0
```

| | | |
|---|---|---|
| ![THE VOID 1](docs/effects/void_1.jpg) | ![THE VOID 2](docs/effects/void_2.jpg) | ![THE VOID 3](docs/effects/void_3.jpg) |

### stuck overlay

glitch has to be on or nothing ever gets stuck, so glitch is at .5 here

```
./datamosh-sync video.mp4 "" 0.5 100 0 0 1 0 0 0 0 0 0 0 0 0
```

| | | |
|---|---|---|
| ![stuck overlay 1](docs/effects/stuck_1.jpg) | ![stuck overlay 2](docs/effects/stuck_2.jpg) | ![stuck overlay 3](docs/effects/stuck_3.jpg) |

### glitch

only hits on loud parts, the patches are the stuck overlay it also triggers. *worst frame of the clip*, not the end one

```
./datamosh-sync video.mp4 "" 1 100 0 0 0 0 0 0 0 0 0 0 0 0
```

| | | |
|---|---|---|
| ![glitch 1](docs/effects/glitch_1.jpg) | ![glitch 2](docs/effects/glitch_2.jpg) | ![glitch 3](docs/effects/glitch_3.jpg) |

### beat sync

honestly you cant see this one in a still, its a zoom punch + freeze on the beat. *worst frame of the clip*, not the end one

```
./datamosh-sync video.mp4 "" 0 100 0 0 0 1 0 0 0 0 0 0 0 0
```

| | | |
|---|---|---|
| ![beat sync 1](docs/effects/beatsync_1.jpg) | ![beat sync 2](docs/effects/beatsync_2.jpg) | ![beat sync 3](docs/effects/beatsync_3.jpg) |

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

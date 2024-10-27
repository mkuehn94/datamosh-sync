## datamosh-sync 

A tool to datamosh one or two videos following a song.
This was created for music videos and just "digital art" and for me
to learn more about encoding.

This was tested only on linux around kernel 6.6. If ports are wanted just create
an issue, same for bugs, i'll try to fix it.

![the track](docs/track.png)

This tool can be used fully from CLI for there is also a convenient GUI.
Input one or two videos and optionally a mp3 track (otherwise the audio
track of one of the videos will be used)

On the left you can set the scale of the effects. Be careful, some effect are REALLY
sensitive to small changes and can cause a complete indistinguishable noise
like pattern. (i.e. HALL).
All of the effects are explained visually later anyways, which is much easier than
to explain by text. Actually as you can see to scale/intensity still changes according
to the music, so imagine this to be a mean of some distribution.

On the right you can set how much of the footage is affected in percent. 1 = effect for
the whole output video, 0.1 = effect active 10% of the video.

If you don't want to set those things manually (i don't blame you, long videos take 
quite a lot of time to process) click on the AUTO PARTS button, which searches for a 
good combination according to your music.

## The Effects

You can look up how they work internally, but here is a very simple example of the
effects in action:
Those are every effect on max (1) on the same input footage (some drone footage of mine)
### nothing on, for comparison

| | | |
|---|---|---|
| ![nothing on, for comparison 1](docs/effects/off_1.jpg) | ![nothing on, for comparison 2](docs/effects/off_2.jpg) | ![nothing on, for comparison 3](docs/effects/off_3.jpg) |

### ROT

simulates the decoder losing its keyframes: image and motion stay behind to a certain degree. Especially colors from earlier scenes "shift" into new scenes, which makes this quite unique.
```
./datamosh-sync video.mp4 "" 0 100 0 0 0 0 1 0 0 0 0 0 0 0
```

| | | |
|---|---|---|
| ![ROT 1](docs/effects/rot_1.jpg) | ![ROT 2](docs/effects/rot_2.jpg) | ![ROT 3](docs/effects/rot_3.jpg) |

### WARP

distorts the image from rot in a direction vector in a certain block size. direction vector is controlled by the music (kick push them out of the center, bass pulls them back)
```
./datamosh-sync video.mp4 "" 0 100 0 0 0 0 0 1 0 0 0 0 0 0
```

| | | |
|---|---|---|
| ![WARP 1](docs/effects/warp_1.jpg) | ![WARP 2](docs/effects/warp_2.jpg) | ![WARP 3](docs/effects/warp_3.jpg) |

### TIME QUAKE

one of the more silly type of effects, The "Circle" borrows frames from a slightly different time of the video. kicks in the music are simulated like "shokwaves". Sometimes i turn this fully off because it feels kind of "forced"
 
```
./datamosh-sync video.mp4 "" 0 100 0 0 0 0 0 0 1 0 0 0 0 0
```

| | | |
|---|---|---|
| ![TIME QUAKE 1](docs/effects/quake_1.jpg) | ![TIME QUAKE 2](docs/effects/quake_2.jpg) | ![TIME QUAKE 3](docs/effects/quake_3.jpg) |

### SPECTRAL LAG

This actually IS a strong effect, but hard to capture on screenshots. Image blocks are split into different frequencies using 8x8 DCT with different delays. Look how there are Edges of objects from frames way before.

```
./datamosh-sync video.mp4 "" 0 100 0 0 0 0 0 0 0 1 0 0 0 0
```

| | | |
|---|---|---|
| ![SPECTRAL LAG 1](docs/effects/spectral_1.jpg) | ![SPECTRAL LAG 2](docs/effects/spectral_2.jpg) | ![SPECTRAL LAG 3](docs/effects/spectral_3.jpg) |

### MOTION SAMPLER

8 Beats of motion vectors are recorded and looped over the current picture based on music, resulting in the smearing and almost stained glass like effect.
```
./datamosh-sync video.mp4 "" 0 100 0 0 0 0 0 0 0 0 1 0 0 0
```

| | | |
|---|---|---|
| ![MOTION SAMPLER 1](docs/effects/groove_1.jpg) | ![MOTION SAMPLER 2](docs/effects/groove_2.jpg) | ![MOTION SAMPLER 3](docs/effects/groove_3.jpg) |

### TEXTURE SWAP

quite a chaotic effect, it swaps the textures of blocks randomly. But look at how lighting is completely unaffected by this even on full scale.
```
./datamosh-sync video.mp4 "" 0 100 0 0 0 0 0 0 0 0 0 1 0 0
```

| | | |
|---|---|---|
| ![TEXTURE SWAP 1](docs/effects/texswap_1.jpg) | ![TEXTURE SWAP 2](docs/effects/texswap_2.jpg) | ![TEXTURE SWAP 3](docs/effects/texswap_3.jpg) |

### RECURSION

copies into copies into copies :D
```
./datamosh-sync video.mp4 "" 0 100 0 0 0 0 0 0 0 0 0 0 1 0
```

| | | |
|---|---|---|
| ![RECURSION 1](docs/effects/recursion_1.jpg) | ![RECURSION 2](docs/effects/recursion_2.jpg) | ![RECURSION 3](docs/effects/recursion_3.jpg) |

### HALL OF MIRRORS

My favorite effect. The image stays persistant but moving objects "lag behind" or fail to update. It's the exact effect some videogames give you for a corrupted framebuffer.
```
./datamosh-sync video.mp4 "" 0 100 0 0 0 0 0 0 0 0 0 0 0 1
```

| | | |
|---|---|---|
| ![HALL OF MIRRORS 1](docs/effects/hom_1.jpg) | ![HALL OF MIRRORS 2](docs/effects/hom_2.jpg) | ![HALL OF MIRRORS 3](docs/effects/hom_3.jpg) |

### motion blur / trails

only really interning in combination with other effects tbh.
```
./datamosh-sync video.mp4 "" 0 100 1 0 0 0 0 0 0 0 0 0 0 0
```

| | | |
|---|---|---|
| ![motion blur / trails 1](docs/effects/blur_1.jpg) | ![motion blur / trails 2](docs/effects/blur_2.jpg) | ![motion blur / trails 3](docs/effects/blur_3.jpg) |

### THE VOID

VOID scale 1, tolerance = 0

```
./datamosh-sync video.mp4 "" 0 0 0 1 0 0 0 0 0 0 0 0 0 0
```

| | | |
|---|---|---|
| ![THE VOID 1](docs/effects/void_1.jpg) | ![THE VOID 2](docs/effects/void_2.jpg) | ![THE VOID 3](docs/effects/void_3.jpg) |

### stuck overlay

for glitch to work

```
./datamosh-sync video.mp4 "" 0.5 100 0 0 1 0 0 0 0 0 0 0 0 0
```

| | | |
|---|---|---|
| ![stuck overlay 1](docs/effects/stuck_1.jpg) | ![stuck overlay 2](docs/effects/stuck_2.jpg) | ![stuck overlay 3](docs/effects/stuck_3.jpg) |

### glitch

One of the more famous and replicated effects in video editing etc.

```
./datamosh-sync video.mp4 "" 1 100 0 0 0 0 0 0 0 0 0 0 0 0
```

| | | |
|---|---|---|
| ![glitch 1](docs/effects/glitch_1.jpg) | ![glitch 2](docs/effects/glitch_2.jpg) | ![glitch 3](docs/effects/glitch_3.jpg) |


## C++ implementation

Basic GTK GUI. effects apply on the decoded frames before the encoding.
So the existing motion vectors and not directly altered.

src/ 
- processor: wrapper for ffmpeg. reading video & the main loop
- audio_analyzer: using spectral flux to detect essential parts of the song, bass/treble curves
- warp: "fake" decoder used for effects: rot, warp, time quake, spectral lag
- effects: all the effects. (feel free to use any in your own project if you like it)
- gui: simple GTK interface with sliders and buttons and track preview.
- stream_corrupter: experimental h264 NAL manipulation

## Build and Use

install all necessary dependencies them simply `make`. If it doesn't work refer to [HOW TO BUILD](./BUILD.md)
```
./datamosh-sync video.mp4               # uses embedded audio
./datamosh-sync video.mp4 audio.mp3     # separate audio file
./datamosh-sync video.mp4 video2.mp4    # moshes two video together with roughly 50% of both, track auto calculated.
```

all 0-1 effects follow the music:

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

or hit AUTO PARTS, it finds the values for you. simulated annealing: 6 runs of 3000
tries each from random places, every try deals the whole track out and scores it
(track_cost in src/arrange.cpp). how well the violence fits the music, too many effects
at once, empty loud parts, effects where they dont fit, flickering, and everybody gets a
fair share with the violent ones less. it looks at your how hard sliders so set those
first. takes a few seconds and the window hangs meanwhile. same input = same answer.
the score is just a set of rules, so it finds the best track by those, you can
still move stuff afterwards

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

or in the gui, the second box. it isnt 50/50, you always know which world youre in:
one video is the main one and the other tears in as a few big rifts

- `--cross` / DIMENSION slider: how far the other world tears in. 0 = never
- kicks rip the rifts open, then they close again. the blobs change every 8 kicks
- build opens them further and further, breath before a drop is totally clean
- a DROP flips which video is the main one, the climax is the only place they
  really merge. the row on top of the track shows which world, line = how far open
- `--music` a, b or both. the track gets made from whatever you pick
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
theres two videos. quake can flip the whole picture to the other video for a moment
even when the track says otherwise, i kept it

two videos have their own violence table in src/arrange.cpp, they hit harder

## Legacy code and remaining work

Some older effects remain in src/effects.cpp but are not used by the current
render loop. The function named jpeg_corruption is an RGB-noise placeholder,
not actual JPEG/DCT corruption. Codec-level I-frame/P-frame methods are stubs.

FFmpeg supports raw RGB through `-f rawvideo -pix_fmt rgb24`, which frames.sh
uses. Processed Python frames are saved as a PPM image sequence for gif.sh.

See [BUILD.md](BUILD.md) for dependencies and tests, and
[KNOWN_ISSUES.md](KNOWN_ISSUES.md) / [TODO.md](TODO.md) for remaining work.


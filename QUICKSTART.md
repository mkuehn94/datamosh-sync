# Quick start

Install dependencies from [BUILD.md](BUILD.md), then:

```sh
make
./datamosh-sync your_video.mp4
```

Uses embedded audio and writes `<video_basename>_munched.mp4` in the current
directory. For separate audio:

```sh
./datamosh-sync your_video.mp4 your_music.mp3
```

For GUI:

```sh
./datamosh-gui
```

Select input, adjust effects, then MOSH IT. CALCULATE TRACK previews effect
sections; it is not a live video preview. Audio analysis uses spectral flux,
not placeholder beats. CLI renders video and muxes audio through FFmpeg.

[README.md](README.md) describes effect parameters and two-video rendering.
[KNOWN_ISSUES.md](KNOWN_ISSUES.md) lists remaining limitations.

Test without downloading sample clips:

```sh
./test.sh
```

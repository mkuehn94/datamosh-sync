# Build and test

Linux build needs C++11, OpenMP, FFmpeg development libraries and GTK 3.
The `ffmpeg` and `ffprobe` commands must also be on PATH for audio and tests.

Ubuntu/Debian:
```sh
sudo apt install build-essential pkg-config ffmpeg libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libgtk-3-dev python3-numpy python3-pil python3-tk
```

Arch Linux:
```sh
sudo pacman -S base-devel pkgconf ffmpeg gtk3 python-numpy python-pillow tk
```

Build both CLI and GUI:
```sh
make
```

Build CLI only (no GTK required):
```sh
make datamosh-sync
```

Run:
```sh
./datamosh-sync input_video.mp4
./datamosh-sync input_video.mp4 music.mp3
./datamosh-gui
```

CLI renders `<input_basename>_munched.mp4` in the current directory. With a
second video, name includes both basenames. It renders a silent intermediate,
then runs FFmpeg to mux audio. If muxing fails, it keeps the silent video and
prints a warning.

## Tests

```sh
make check
./test.sh
```

`make check` runs pixel-effect assertions, legacy Python block-shift checks,
and script/viewer tests. `test.sh` also generates a short test video, exercises
the video reader, renders it and verifies decodable video and audio streams.
It does not need private clips. Python tests need NumPy and Pillow; Tk test
needs tkinter and a display. With Xvfb installed:

```sh
xvfb-run -a make check
```

These are regression and smoke checks, not exhaustive coverage of all effects.

## Troubleshooting

- Missing `gtk/gtk.h`: install GTK 3 development files, or build CLI only.
- Missing FFmpeg headers or link libraries: install development packages above.
  For a nonstandard install, supply include/link paths through `CFLAGS` and
  `LDFLAGS`; Makefile does not use FFmpeg's pkg-config metadata.
- Encoder unavailable: FFmpeg build must provide an H.264 encoder, normally
  libx264. Odd image dimensions may fail with YUV420P output.
- Stale objects after switching revisions: `make clean && make`.

`make clean` removes binaries and `obj/`. Generated binaries and Python caches
are ignored, not source artifacts. macOS is not currently validated; default
OpenMP/compiler flags target Linux.

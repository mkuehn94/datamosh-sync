# Building datamosh-sync

## Prerequisites

You need these libraries installed:

### Ubuntu/Debian:
```bash
sudo apt install build-essential pkg-config
sudo apt install libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
```

### Arch Linux:
```bash
sudo pacman -S base-devel ffmpeg
```

### macOS (Homebrew):
```bash
brew install ffmpeg pkg-config
```

## Build

Just run make:
```bash
make
```

If it works, you'll see:
```
g++ -Wall -Wextra -g -std=c++11 -c src/main.cpp -o obj/main.o
g++ -Wall -Wextra -g -std=c++11 -c src/video_processor.cpp -o obj/video_processor.o
...
Built datamosh-sync
```

## Usage

### With embedded audio (easier):
```bash
./datamosh-sync input_video.mp4
```

### With separate audio file:
```bash
./datamosh-sync input_video.mp4 music.mp3
```

The tool will:
1. Load video and audio
2. Analyze audio for beats (fake beats for now, placeholder)
3. Apply datamosh effects synced to beats
4. Process frames with effects

**NOTE:** Output encoding not implemented yet, so it processes but doesn't save!
Check TODO.md for what's still missing.

## Troubleshooting

### "libavformat not found"
Install ffmpeg dev libraries (see Prerequisites above)

### "cannot find -lavcodec"
Your ffmpeg might be in a non-standard location. Try:
```bash
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
make clean && make
```

### Segfault when running
Probably trying to read a corrupt video or format we don't handle.
Try with a simple h264 .mp4 file first.

## Clean build

```bash
make clean
make
```

## What gets built

- Binary: `datamosh-sync` 
- Object files: `obj/*.o`
- No installation needed, run from current directory

---

If it compiles but crashes or doesn't work, that's expected!
This is WIP and several features are stubs. See notes.txt and TODO.md.

# Quick Start

## 1. Install dependencies

**Arch Linux:**
```bash
sudo pacman -S base-devel ffmpeg
```

**Ubuntu/Debian:**
```bash
sudo apt install build-essential libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
```

## 2. Build

```bash
make
```

## 3. Run

With a video that has audio:
```bash
./datamosh-sync your_video.mp4
```

Or with separate audio:
```bash
./datamosh-sync your_video.mp4 your_music.mp3
```

## What you'll see:

```
datamosh-sync v0.2
initializing...
Loading audio from video: your_video.mp4
Found audio stream at index 1
Audio duration: 45.3 seconds
Analyzing audio...
Found 20 beats
initialized with 20 beats (from video)
processing...
processed 30 frames
processed 60 frames
...
done!
```

## Current status:

✅ Compiles
✅ Loads video
✅ Extracts audio
✅ Processes frames with effects
❌ Output encoding (doesn't save yet!)

The effects are applied but output isn't written to file yet.
That's the next big TODO item.

## Effects applied:

The tool applies these in order:
- Motion vector glitching (block displacement)
- JPEG corruption on beats
- Color channel shifts
- Block shuffling on strong beats
- Pulse/zoom synchronized to beats
- Wave distortion

All modulated by the (placeholder) beat detection.

---

**tl;dr:** make, then ./datamosh-sync video.mp4

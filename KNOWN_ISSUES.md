# Known limitations

CLI and GUI build, and short generated-video smoke tests exercise rendering
and audio muxing. This is not a guarantee that every input or effect combination
works.

- GUI has a section/parameter track, not a live video preview. AUTO PARTS can
  block the window while optimizing.
- Input audio is required for analysis. `--music` selects audio from the first,
  second, or both videos. Silent-video fallback is not implemented.
- Output audio is stream-copied. A codec incompatible with MP4 can make muxing
  fail; CLI then keeps a silent video and prints a warning.
- Frame timing assumes a constant rate derived from the source. Variable-rate
  video is not handled with a full timestamp-preserving pipeline. Decoder EOF
  handling also needs coverage for delayed/B-frames.
- YUV420P/H.264 output normally needs even dimensions and an available encoder.
- Effects can use substantial RAM, particularly time quake's frame tape. Long
  videos also require audio-analysis storage. There is no memory-budget control.
- Error propagation is incomplete for some FFmpeg encode/write calls, and some
  subprocess launch paths still need proper pipe/fork failure handling.
- Legacy `jpeg_corruption` is RGB noise, not JPEG coefficient editing;
  `repeat_pframes` and `remove_iframes` in EffectEngine are stubs. They are not
  called by the current render loop. `src/gui.cpp` is an older unused GUI.
- Standalone `stream_corrupter.cpp` is experimental. It recognizes only
  four-byte Annex B start codes; removing parameter sets can make output
  undecodable. It is not a general H.264 datamosher.
- Python sidecar workflow has no block matcher in this repo. Sidecar mode is
  ignored (`hold`, `drop`, `freeze` are not distinct operations). With a beat
  grid, the selected patch holds until the next beat; without it, a vector
  affects only f+1. `scripts/beats.sh` finds ends of silence, not musical beats
  in general.
- Tests cover selected operations and a short render, not visual quality or all
  input formats. See [BUILD.md](BUILD.md) for commands.


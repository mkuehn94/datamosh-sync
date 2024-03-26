# TODO

## Critical
- [ ] Output file encoding (can't save videos yet!)
- [ ] Preview window in GUI (shows blank currently)
- [ ] Real beat detection (using placeholder beats)

## Important  
- [ ] Fix color channel shift (broken somehow)
- [ ] Optimize block operations (very slow on HD video)
- [ ] Better memory management (lots of malloc/free)

## Nice to have
- [ ] Load/save presets from GUI
- [ ] Multiple effect profiles per song
- [ ] Real-time preview while adjusting
- [ ] Undo/redo for effect chains
- [ ] Export settings/project files

## Experimental
- [ ] Actual I-frame removal (h264 stream level)
- [ ] Motion vector extraction from stream
- [ ] Custom codec for datamosh format
- [ ] GPU acceleration for effects

## Known Issues
- Stream corrupter output won't play (NAL parsing issue?)
- Color shift overwrites instead of shifting
- GUI sliders don't update parameters in real-time
- No progress indication during processing
- Memory leaks in VideoProcessor (need valgrind)
- Beat detection window too wide (100ms would be better)

## Ideas to try
- Feedback loop: frame N uses data from frame N-k
- Displacement maps from audio spectrum
- Particle effects on beats
- Edge detection + selective corruption
- Keep faces intact, glitch background only
- Circular buffer for smooth trails
- Voronoi diagram for block boundaries
- FFT-based color mapping


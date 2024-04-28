# Known Issues

## Current Bugs

### bad_alloc crash (BLOCKING)
**Status:** Currently broken  
**Symptom:** `std::bad_alloc` crash during initialization
**Started:** During audio embedding changes
**Affected:** All videos, even small 2-second test files

The program crashes immediately after `initializing...` before any processing.

**Workaround:** Use output.mp4 from earlier successful run (174MB, has video but no audio)

### Audio Not Embedded  
**Status:** Disabled due to above crash  
**Symptom:** Output video has no sound

**Workaround:** Use `add_audio.sh` script:
```bash
./add_audio.sh input_with_audio.mp4 output.mp4 final.mp4
```

### Pulse & Wave Effects Disabled
**Status:** Commented out  
**Reason:** Memory corruption - read/write same buffer  

These effects need to use a temporary buffer.

## Working Features
- ✅ Motion vector glitching
- ✅ JPEG corruption
- ✅ Block shuffling  
- ✅ Color channel shifts
- ✅ Color remap (implemented but disabled)
- ✅ Output video encoding (worked before audio changes)

## Files That Work
- `output.mp4` - 174MB video from successful run before audio broke
- Build system compiles fine
- No compiler errors

## Next Steps
1. Find where bad_alloc happens (likely in audio mux code)
2. Fix memory allocation issue
3. Re-enable audio copying
4. Fix pulse/wave effects to use temp buffers
5. Re-enable color_remap


# Known Issues

## ⚠️  PROJECT IS BROKEN ⚠️

**The main branch is currently non-functional.**


## Current Bugs

### bad_alloc crash (CRITICAL - UNFIXED)
**Status:** BROKEN - crashes immediately  
**Symptom:** Crashes in video->open() before any debug output  
**Started:** During audio embedding changes  
**Affected:** ALL code paths, even with audio code commented out

Something fundamentally broke. Tried:
- ✗ Commenting out audio mux code
- ✗ Removing new member variables
- ✗ Fixing compilation errors
- ✗ Adding debug prints everywhere

**It still crashes** before printing anything from video_processor.

### Audio Not Embedded  
**Status:** Never worked, now completely removed  
**Symptom:** Attempted to add, broke everything

**Workaround:** Use `add_audio.sh` script after processing:
```bash
make clean && make
./datamosh-sync input.mp4  
# Then add audio:
./add_audio.sh input.mp4 output.mp4 final.mp4
```

## What Went Wrong

Attempted to add:
1. Audio stream copying from input
2. Color remap effect (5 modes)
3. Member variables `input_video_path` and `audio_out_stream`

Result: Complete breakage, unfixable without revert.

## Files That Actually Work

- `output.mp4` - 174MB video from successful run
- Build system (compiles, but binary crashes)

## Lesson Learned

Should have:
- Tested incrementally
- Made smaller commits
- Not used sed on C++ code
- Actually tested before committing


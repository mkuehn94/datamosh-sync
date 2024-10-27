#!/bin/sh
set -eu

make all check obj/test_basic
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
trap 'exit 1' HUP INT TERM

ffmpeg -hide_banner -loglevel error -y \
  -f lavfi -i testsrc2=size=128x96:rate=24:duration=2 \
  -f lavfi -i sine=frequency=440:duration=2 \
  -c:v libx264 -pix_fmt yuv420p -c:a aac -shortest "$work/input.mp4"
./obj/test_basic "$work/input.mp4"
# Keep output beside the generated fixture, not in the repository.
backend=$(pwd)/datamosh-sync
(cd "$work" && OMP_NUM_THREADS=2 "$backend" input.mp4)
ffprobe -v error -show_entries stream=codec_type -of csv=p=0 "$work/input_munched.mp4" > "$work/streams"
grep -qx video "$work/streams"
grep -qx audio "$work/streams"
ffmpeg -hide_banner -loglevel error -i "$work/input_munched.mp4" -f null -
echo "render smoke test ok"

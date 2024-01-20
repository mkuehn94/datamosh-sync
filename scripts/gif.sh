#!/bin/sh
set -eu
# datamosh.py writes the processed PPM sequence starting at frame zero.
ffmpeg -hide_banner -loglevel error -y -framerate "${1:-24}" -start_number 0 \
  -i "out/frames/f%04d.ppm" -c:v libx264 -pix_fmt yuv420p out/moshed.mp4
ffmpeg -hide_banner -loglevel error -y -i out/moshed.mp4 -loop 0 out/moshed.gif

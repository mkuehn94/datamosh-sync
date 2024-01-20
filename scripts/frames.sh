#!/bin/sh
set -eu
if [ "$#" -ne 2 ]; then
  echo "usage: $0 input_video output.rgb" >&2
  exit 1
fi
ffmpeg -hide_banner -loglevel error -y -i "$1" -an -sn -dn \
  -fps_mode passthrough -pix_fmt rgb24 -f rawvideo "$2"

#!/bin/sh
set -eu
if [ "$#" -ne 1 ]; then
  echo "usage: $0 audio_file" >&2
  exit 1
fi
# Rough grid from ends of silence, not a general musical beat detector.
log=$(mktemp)
trap 'rm -f "$log"' EXIT
trap 'exit 1' HUP INT TERM
if ! ffmpeg -hide_banner -i "$1" -af silencedetect=n=-30dB:d=0.18 -f null - 2>"$log"; then
  cat "$log" >&2
  exit 1
fi
awk '{ for (i = 1; i < NF; i++) if ($i == "silence_end:") print int($(i + 1) * 1000) }' "$log"

#!/bin/bash
# quick script to add audio back to output
# because doing it in C++ is annoying

if [ $# -lt 2 ]; then
    echo "Usage: $0 <input_with_audio.mp4> <video_only.mp4> [output.mp4]"
    exit 1
fi

INPUT=$1
VIDEO_ONLY=$2
OUTPUT=${3:-output_with_audio.mp4}

echo "Copying audio from $INPUT to $VIDEO_ONLY..."

ffmpeg -i "$VIDEO_ONLY" -i "$INPUT" -c:v copy -c:a copy -map 0:v:0 -map 1:a:0 -shortest "$OUTPUT" -y

if [ $? -eq 0 ]; then
    echo "Done! Output: $OUTPUT"
else
    echo "Failed to add audio"
    exit 1
fi

#!/bin/bash

# quick test script

echo "building..."
make clean
make

if [ $? -ne 0 ]; then
    echo "build failed"
    exit 1
fi

echo "testing with sample video..."
./datamosh-sync clips/test.mp4 clips/beat.mp3

echo "done"

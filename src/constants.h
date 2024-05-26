#ifndef CONSTANTS_H
#define CONSTANTS_H

// effect strength multipliers
#define GLITCH_MULTIPLIER 2.5
#define JPEG_MIN_QUALITY 5
#define JPEG_MAX_QUALITY 95

// block sizes for effects
#define MACROBLOCK_SIZE 16
#define MOTION_SEARCH_RANGE 40

// beat detection thresholds
#define BEAT_ENERGY_THRESHOLD 0.7
#define BEAT_MIN_INTERVAL 0.15  // seconds

// gui stuff
#define PREVIEW_WIDTH 640
#define PREVIEW_HEIGHT 360
#define WINDOW_TITLE "datamosh-sync v0.2"

// output settings
#define OUTPUT_BITRATE 8000000
#define OUTPUT_FPS 30

// debug flags
//#define DEBUG_MOTION_VECTORS
//#define DEBUG_BEAT_DETECTION
#define VERBOSE_OUTPUT

// trying different values
#define CORRUPTION_RATE 15  // was 20, too much
#define SHIFT_AMOUNT 8      // was 20, way too much
// causes tearing when it gets too high
//#define SHIFT_AMOUNT 14

// persistent object tracking
#define HISTORY_LEN 24        // ring buffer of processed frames (~0.8s at 30fps)
#define PERSIST_FRAMES 20     // frames something has to keep moving before we call it an object
#define ECHO_LAG_FRAMES 15    // how far back in time the echo twin comes from

#endif

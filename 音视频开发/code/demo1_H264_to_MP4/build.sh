#!/usr/bin/env bash
set -euo pipefail

CC=${CC:-gcc}
CFLAGS=${CFLAGS:-"-std=c99 -Wall -Wextra -O2"}
OUT_DIR="build"
OUT_EXE="$OUT_DIR/h264_to_mp4.exe"

mkdir -p "$OUT_DIR"

"$CC" $CFLAGS \
    -Iinclude \
    src/h264_mp4_muxer.c \
    examples/h264_to_mp4.c \
    -o "$OUT_EXE"

echo "Built: $OUT_EXE"
echo "Run:   ./$OUT_EXE output.h264 output.mp4"

#!/bin/bash
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"; cd "$SCRIPT_DIR/build" || { mkdir -p "$SCRIPT_DIR/build"; cd "$SCRIPT_DIR/build" || exit 1; }; [ ! -f "Makefile" ] && cmake ..; make -j$(nproc) && echo "Build successful! Run with: ./src/gqrx" || echo "Build failed!"

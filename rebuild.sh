#!/bin/bash

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Check for cache clear argument
if [[ "$1" == "-c" ]]; then
    echo "Clearing build cache..."
    rm -rf "$SCRIPT_DIR/build"
fi

# Create and enter build directory
cd "$SCRIPT_DIR/build" || { 
    mkdir -p "$SCRIPT_DIR/build"
    cd "$SCRIPT_DIR/build" || exit 1
}

# Run cmake if Makefile doesn't exist
[ ! -f "Makefile" ] && cmake ..

# Build
make -j$(nproc) && echo "Build successful! Run with: ./src/gqrx" || echo "Build failed!"

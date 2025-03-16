#!/bin/bash
# Simple FM transmission script using hackrf_transfer directly

# Parameters
FREQ=88000000  # 88 MHz
SAMPLE_RATE=2000000  # 2 MHz
GAIN=14
WAV_FILE="$(dirname "$0")/audio/bgmusic.wav"

# Check file exists
if [ ! -f "$WAV_FILE" ]; then
    echo "ERROR: Audio file not found: $WAV_FILE"
    exit 1
fi

# Convert WAV to raw format suitable for hackrf_transfer
echo "Converting WAV file to raw format..."
sox "$WAV_FILE" -t raw -r "$SAMPLE_RATE" -c 1 -b 16 -e signed-integer - | \
hackrf_transfer -t - -f "$FREQ" -s "$SAMPLE_RATE" -a 1 -x "$GAIN"

echo "Transmission complete" 
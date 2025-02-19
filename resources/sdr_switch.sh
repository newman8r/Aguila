#!/bin/bash

# Simple SDR device configuration manager
GQRX_CONFIG_DIR="$HOME/.config/gqrx"
DEFAULT_CONF="$GQRX_CONFIG_DIR/default.conf"

# Make sure GQRX isn't running
if pgrep -x "gqrx" > /dev/null; then
    echo "❌ Please close GQRX before switching devices"
    exit 1
fi

# Create config directory if it doesn't exist
mkdir -p "$GQRX_CONFIG_DIR"

# Function to switch to a device
switch_to_device() {
    local device=$1
    local device_conf="$GQRX_CONFIG_DIR/${device}.conf"
    
    # If default.conf exists, back it up with current timestamp
    if [ -f "$DEFAULT_CONF" ]; then
        mv "$DEFAULT_CONF" "$GQRX_CONFIG_DIR/backup_$(date +%Y%m%d_%H%M%S).conf"
    fi
    
    # If we have a saved config for this device, restore it
    if [ -f "$device_conf" ]; then
        echo "✅ Restoring saved configuration for $device"
        cp "$device_conf" "$DEFAULT_CONF"
    else
        echo "ℹ️ No saved configuration for $device, GQRX will create fresh config"
    fi
    
    echo "🚀 Ready to launch GQRX with $device configuration"
}

# Show usage if no device specified
if [ $# -eq 0 ]; then
    echo "Usage: $0 <device-name>"
    echo "Example devices: hackrf, rtlsdr"
    exit 1
fi

# Switch to specified device
switch_to_device "$1"

# After successful run, save current config for device
trap 'cp "$DEFAULT_CONF" "$GQRX_CONFIG_DIR/$1.conf" 2>/dev/null' EXIT 
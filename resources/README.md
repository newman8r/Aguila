# Aguila SDR Resources

This directory contains various SDR-related tools and utilities for use with HackRF and other SDR hardware.

## Available Tools

### Modulation Tools

| File | Description | Documentation |
|------|-------------|--------------|
| **simple_fsk.py** | Binary FSK transmitter with text message support | [FSK_TRANSMITTER.md](FSK_TRANSMITTER.md) |
| **simple_psk.py** | Binary PSK (BPSK) transmitter | - |
| **kevin_qpsk.py** | QPSK transmitter based on Kevin Vivas' implementation | - |
| **ook_transmitter.py** | On-Off Keying (OOK) transmitter | - |
| **simple_ook.py** | Simplified On-Off Keying with long duration | - |

### Utility Tools

| File | Description |
|------|-------------|
| **tuning_tool.py** | Utility for finding optimal frequency correction values |
| **fm_transmitter.py** | Simple FM transmitter |
| **real_fm_transmit.py** | Enhanced FM transmitter using real audio |

## Usage Examples

### FSK Transmitter

```bash
# Basic usage
python3 simple_fsk.py -f 434.6 -m "Hello World!"

# Advanced usage with parameter control
python3 simple_fsk.py -f 434.6 -b 500 -d 100000 -s 1000000 -g 40 -c -69 -m "Custom message"
```

### OOK Transmitter

```bash
# Basic usage
python3 ook_transmitter.py -f 434.6 -c -69
```

## Parameter Guide

Most tools accept the following common parameters:

| Parameter | Description | Typical Value |
|-----------|-------------|---------------|
| `-f`, `--freq` | Frequency in MHz | 433-435 MHz |
| `-g`, `--gain` | Transmit gain in dB | 0-47 dB |
| `-c`, `--correction` | Frequency correction in ppm | Device-specific |
| `-s`, `--samplerate` | Sample rate in Hz | 1-2 MHz |

## Receiving Signals

Most signals transmitted by these tools can be received and decoded using:

1. **URH (Universal Radio Hacker)** - Good for digital protocols
2. **GQRX** - Good for visualization and generic receiving
3. **GNU Radio** - For building custom receivers

## Development

Feel free to extend these tools or create new ones. Please update this README when adding new tools to the resources directory. 
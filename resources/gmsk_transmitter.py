#!/usr/bin/env python3
"""
GMSK Transmitter for Aguila
Uses HackRF to transmit binary data using Gaussian Minimum Shift Keying
Modified with high-contrast settings for easy decoding in URH/GQRX
"""

import sys
import os
import argparse
import time
import numpy as np
from gnuradio import gr, digital, blocks, filter
import osmosdr

class GMSKTransmitter(gr.top_block):
    def __init__(self, freq=433.0e6, message="Hello World", 
                 baud_rate=300, bt=0.2, gain=20, sample_rate=48000,
                 use_simple_pattern=True, freq_correction_ppm=0):
        gr.top_block.__init__(self, "GMSK Transmitter")
        
        # Parameters
        self.freq = freq
        self.gain = gain
        self.sample_rate = sample_rate
        self.message = message
        self.baud_rate = baud_rate
        # Lower BT value means more bandwidth, more distinctive modulation
        self.bt = bt  # Bandwidth-time product for GMSK filter
        self.samples_per_symbol = int(sample_rate / baud_rate)
        self.use_simple_pattern = use_simple_pattern
        self.freq_correction_ppm = freq_correction_ppm
        
        # Apply frequency correction if specified
        if freq_correction_ppm != 0:
            freq_offset = freq * (freq_correction_ppm / 1e6)
            self.corrected_freq = freq + freq_offset
            print(f"  - Applying {freq_correction_ppm} ppm correction")
            print(f"  - Original frequency: {freq/1e6:.3f} MHz")
            print(f"  - Corrected frequency: {self.corrected_freq/1e6:.3f} MHz")
        else:
            self.corrected_freq = freq
        
        print(f"Initializing GMSK transmitter with parameters:")
        print(f"  - Frequency: {self.corrected_freq/1e6:.3f} MHz")
        print(f"  - Baud rate: {baud_rate} bps (SLOWER for easier detection)")
        print(f"  - BT product: {bt} (LOWER for sharper transitions)")
        print(f"  - Gain: {gain} (MODERATE to avoid carrier dominance)")
        print(f"  - Sample rate: {sample_rate/1e3:.1f} kHz")
        print(f"  - Samples per symbol: {self.samples_per_symbol}")
        
        # Create data pattern with manual pauses
        if use_simple_pattern:
            # Very simple repeating pattern for easy decoding
            print(f"  - Using super high-contrast test pattern")
            
            # Create a much simpler pattern with LONGER sequences
            # This creates a very clear 11111111... then 00000000... pattern
            pattern1 = bytes([0xFF] * 32)  # All 1's repeated (LONGER)
            pattern2 = bytes([0x00] * 32)  # All 0's repeated (LONGER)
            
            # Add a large gap with zeros in between patterns
            pause_bytes = bytes([0x00] * 64)  # Even longer pause (DOUBLE)
            
            # Construct the full pattern
            data_pattern = pattern1 + pause_bytes + pattern2 + pause_bytes + pattern1
            
            # Repeat it several times
            self.data = list(data_pattern) * 5
            print(f"  - Pattern length: {len(self.data)} bytes")
            print(f"  - Pattern: 32x FF + 64x 00 + 32x 00 + 64x 00 + 32x FF (repeated 5 times)")
        else:
            # Convert string to bytes with clearer markers
            print(f"  - Message: {message}")
            # Preamble with very clear pattern
            preamble = bytes([0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF])
            message_bytes = bytes(message, 'utf-8')
            # Postamble with clear pattern
            postamble = bytes([0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF])
            
            # Add large zero gaps for easier detection
            pause_bytes = bytes([0x00] * 48)
            self.data = list(preamble + pause_bytes + message_bytes + pause_bytes + postamble)
        
        # Blocks
        # 1. Vector source for data
        self.source = blocks.vector_source_b(self.data, repeat=True)
        
        # 2. Bytes to bits (unpack)
        self.unpacker = blocks.unpack_k_bits_bb(8)
        
        # 3. GMSK modulator with increased deviation
        self.gmsk_mod = digital.gmsk_mod(
            samples_per_symbol=self.samples_per_symbol,
            bt=self.bt,
            verbose=True
        )
        
        # 4. Connect to HackRF - using simple method that works
        print("Connecting to HackRF device...")
        try:
            self.hackrf_sink = osmosdr.sink('hackrf=0')
            print(f"HackRF device connected successfully")
        except Exception as e:
            print(f"ERROR: Failed to connect to HackRF device: {e}")
            raise
        
        # Configure HackRF parameters
        print("Configuring HackRF parameters...")
        self.hackrf_sink.set_sample_rate(self.sample_rate)
        self.hackrf_sink.set_center_freq(self.corrected_freq)
        self.hackrf_sink.set_gain(self.gain)
        
        # Set moderate IF and BB gain to avoid carrier dominance
        self.hackrf_sink.set_if_gain(20)    # More moderate IF gain
        self.hackrf_sink.set_bb_gain(10)    # More moderate baseband gain
        
        # Set wider bandwidth for more distinctive signal
        self.hackrf_sink.set_bandwidth(self.sample_rate * 2)
        self.hackrf_sink.set_antenna("TX")
        
        # Connect the blocks
        print("Connecting GNU Radio blocks...")
        self.connect(self.source, self.unpacker, self.gmsk_mod, self.hackrf_sink)
        print("GMSK transmitter initialized successfully")
        
    def start_transmission(self):
        try:
            print(f"Starting GMSK transmission at {self.corrected_freq/1e6:.3f} MHz")
            self.start()
            print("Transmission started successfully")
        except Exception as e:
            print(f"ERROR starting transmission: {e}")
            import traceback
            traceback.print_exc()
            raise
        
    def stop_transmission(self):
        try:
            print("Stopping transmission")
            self.stop()
            self.wait()
            print("Transmission stopped successfully")
        except Exception as e:
            print(f"ERROR stopping transmission: {e}")
            import traceback
            traceback.print_exc()

def check_hackrf_available():
    """Check if HackRF device is available and connected"""
    try:
        print("Checking for HackRF device...")
        devices = osmosdr.device.find()
        if len(devices) == 0:
            print("ERROR: No SDR devices found")
            return False
            
        # List all available devices
        print("Available devices:")
        for i, dev in enumerate(devices):
            dev_str = str(dev)
            print(f"  {i}: {dev_str}")
            
        # If we get here, we have devices to try
        return True
    except Exception as e:
        print(f"ERROR checking SDR availability: {e}")
        import traceback
        traceback.print_exc()
        return False

def main():
    parser = argparse.ArgumentParser(description="Aguila GMSK Transmitter")
    parser.add_argument("-f", "--frequency", type=float, default=433.0,
                      help="Frequency in MHz (default: 433.0)")
    parser.add_argument("-m", "--message", type=str, default="Hello World from Aguila GMSK Transmitter",
                      help="Message to transmit (default: Hello World)")
    parser.add_argument("-b", "--baud", type=int, default=300,
                      help="Baud rate in bps (default: 300)")
    parser.add_argument("-t", "--bt", type=float, default=0.2,
                      help="BT product for GMSK filter (default: 0.2)")
    parser.add_argument("-g", "--gain", type=int, default=20,
                      help="RF gain (default: 20)")
    parser.add_argument("-s", "--samplerate", type=int, default=48000,
                      help="Sample rate in Hz (default: 48000)")
    parser.add_argument("-p", "--pattern", action="store_true",
                      help="Use simple alternating pattern instead of message")
    parser.add_argument("-c", "--correction", type=float, default=0,
                      help="Frequency correction in ppm (default: 0)")
    parser.add_argument("-d", "--debug", action="store_true",
                      help="Enable extra debug output")
    
    args = parser.parse_args()
    
    try:
        # Convert frequency from MHz to Hz
        freq_hz = args.frequency * 1e6
        
        # Check if HackRF is available
        if not check_hackrf_available():
            print("ERROR: Unable to proceed without HackRF device")
            return 1
        
        if args.pattern:
            print(f"Transmitting simple test pattern at {args.frequency} MHz")
        else:
            print(f"Transmitting '{args.message}' at {args.frequency} MHz")
        
        # Create and start GMSK transmitter
        tx = GMSKTransmitter(
            freq=freq_hz, 
            message=args.message, 
            baud_rate=args.baud,
            bt=args.bt,
            gain=args.gain,
            sample_rate=args.samplerate,
            use_simple_pattern=args.pattern,
            freq_correction_ppm=args.correction
        )
        tx.start_transmission()
        
        try:
            # Run until interrupted
            print("Transmission running. Press Ctrl+C to stop.")
            while True:
                time.sleep(0.1)
        except KeyboardInterrupt:
            print("Transmission interrupted by user")
        finally:
            tx.stop_transmission()
        
        print("Transmission completed successfully")
        return 0
        
    except Exception as e:
        print(f"ERROR: GMSK transmission failed: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main()) 
#!/usr/bin/env python3
"""
GMSK Transmitter for Aguila
Uses HackRF to transmit binary data using Gaussian Minimum Shift Keying
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
                 baud_rate=9600, bt=0.5, gain=14, sample_rate=2e6):
        gr.top_block.__init__(self, "GMSK Transmitter")
        
        # Parameters
        self.freq = freq
        self.gain = gain
        self.sample_rate = sample_rate
        self.message = message
        self.baud_rate = baud_rate
        self.bt = bt  # Bandwidth-time product for GMSK filter
        self.samples_per_symbol = int(sample_rate / baud_rate)
        
        print(f"Initializing GMSK transmitter with parameters:")
        print(f"  - Frequency: {freq/1e6:.3f} MHz")
        print(f"  - Message: {message}")
        print(f"  - Baud rate: {baud_rate} bps")
        print(f"  - BT product: {bt}")
        print(f"  - Gain: {gain}")
        print(f"  - Sample rate: {sample_rate/1e6:.1f} MHz")
        print(f"  - Samples per symbol: {self.samples_per_symbol}")
        
        # Convert string to bytes
        self.data = list(bytes(message, 'utf-8'))
        
        # Blocks
        # 1. Vector source for data
        self.source = blocks.vector_source_b(self.data, repeat=True)
        
        # 2. Bytes to bits (unpack)
        self.unpacker = blocks.unpack_k_bits_bb(8)
        
        # 3. GMSK modulator
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
        self.hackrf_sink.set_center_freq(self.freq)
        self.hackrf_sink.set_gain(self.gain)
        self.hackrf_sink.set_bandwidth(self.sample_rate)
        self.hackrf_sink.set_antenna("TX")
        
        # Connect the blocks
        print("Connecting GNU Radio blocks...")
        self.connect(self.source, self.unpacker, self.gmsk_mod, self.hackrf_sink)
        print("GMSK transmitter initialized successfully")
        
    def start_transmission(self):
        try:
            print(f"Starting GMSK transmission at {self.freq/1e6:.3f} MHz")
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
    parser.add_argument("-b", "--baud", type=int, default=9600,
                      help="Baud rate in bps (default: 9600)")
    parser.add_argument("-t", "--bt", type=float, default=0.5,
                      help="BT product for GMSK filter (default: 0.5)")
    parser.add_argument("-g", "--gain", type=int, default=14,
                      help="RF gain (default: 14)")
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
        
        print(f"Transmitting '{args.message}' at {args.frequency} MHz")
        
        # Create and start GMSK transmitter
        tx = GMSKTransmitter(
            freq=freq_hz, 
            message=args.message, 
            baud_rate=args.baud,
            bt=args.bt,
            gain=args.gain
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
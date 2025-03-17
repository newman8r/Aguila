#!/usr/bin/env python3
"""
ULTRA SIMPLE PSK Transmitter for Aguila
Created from scratch to demonstrate basic PSK modulation
Designed to be clearly visible in URH/GQRX
"""

import sys
import os
import argparse
import time
import numpy as np
from gnuradio import gr, blocks
import osmosdr

class SimplePSKTransmitter(gr.top_block):
    def __init__(self, freq_mhz=433, baud_rate=1, gain=40, sample_rate=2000000):
        gr.top_block.__init__(self, "Simple PSK Transmitter")
        
        self.freq_mhz = freq_mhz
        self.baud_rate = baud_rate  # Super slow for visibility
        self.gain = gain
        self.sample_rate = sample_rate
        self.samples_per_symbol = int(self.sample_rate / self.baud_rate)
        
        self.setup_blocks()
        
    def setup_blocks(self):
        print(f"Initializing ULTRA SIMPLE PSK transmitter with parameters:")
        print(f"  - Frequency: {self.freq_mhz} MHz")
        print(f"  - Baud rate: {self.baud_rate} bps (EXTREMELY SLOW for visibility)")
        print(f"  - Gain: {self.gain}")
        print(f"  - Sample rate: {self.sample_rate/1e3:.1f} kHz")
        print(f"  - Samples per symbol: {self.samples_per_symbol}")
        
        # Create PSK modulated data
        self.data = self.create_psk_modulation()
        
        # Blocks
        # 1. Vector source for data
        self.source = blocks.vector_source_c(self.data, repeat=True)
        
        # 2. Connect to HackRF
        print("Connecting to HackRF device...")
        try:
            self.hackrf_sink = osmosdr.sink(args="hackrf=0")
            print(f"HackRF device connected successfully")
        except Exception as e:
            print(f"ERROR: Failed to connect to HackRF device: {e}")
            raise
        
        # Configure HackRF parameters
        print("Configuring HackRF parameters...")
        self.hackrf_sink.set_sample_rate(self.sample_rate)
        self.hackrf_sink.set_center_freq(self.freq_mhz * 1e6)
        self.hackrf_sink.set_gain(self.gain)
        self.hackrf_sink.set_if_gain(40)
        self.hackrf_sink.set_bb_gain(20)
        self.hackrf_sink.set_bandwidth(self.sample_rate/2)
        self.hackrf_sink.set_antenna("TX")
        
        # Connect the blocks
        print("Connecting GNU Radio blocks...")
        self.connect(self.source, self.hackrf_sink)
        print("PSK transmitter initialized successfully")
    
    def create_psk_modulation(self):
        """Create the simplest possible PSK modulation - just alternating phases"""
        print(f"  - Creating ULTRA SIMPLE PSK modulation")
        
        # We'll create a signal that alternates between 0 and 180 degrees
        # This is the most basic form of PSK (BPSK)
        
        # Number of samples per symbol - VERY high for visibility
        samples_per_symbol = self.samples_per_symbol
        
        # Create a simple alternating bit pattern: 0101010101...
        # Each bit will be held for 'samples_per_symbol' samples
        bits = [0, 1, 0, 1, 0, 1, 0, 1, 0, 1]  # Simple alternating pattern
        
        # Create the PSK signal
        psk_signal = []
        
        for bit in bits:
            # Map bit to phase:
            # 0 -> 0 degrees (1 + 0j)
            # 1 -> 180 degrees (-1 + 0j)
            phase = 0 if bit == 0 else np.pi  # 0 or 180 degrees
            
            # Generate complex samples for this symbol
            # e^(jθ) = cos(θ) + j*sin(θ)
            symbol_value = np.exp(1j * phase)
            
            # Hold this value for the entire symbol duration
            symbol_samples = [symbol_value] * samples_per_symbol
            
            # Add to our signal
            psk_signal.extend(symbol_samples)
            
            print(f"  - Added symbol: bit={bit}, phase={phase/np.pi:.1f}π, duration={samples_per_symbol/self.sample_rate:.1f} seconds")
        
        print(f"  - Created PSK signal with {len(psk_signal)} samples")
        print(f"  - Each symbol lasts for {samples_per_symbol/self.sample_rate:.1f} seconds")
        print(f"  - Total signal duration: {len(psk_signal)/self.sample_rate:.1f} seconds")
        
        return psk_signal
    
    def start_transmission(self):
        try:
            print(f"Starting PSK transmission at {self.freq_mhz} MHz")
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
    """Check if HackRF is available"""
    try:
        import subprocess
        result = subprocess.run(['hackrf_info'], 
                               stdout=subprocess.PIPE, 
                               stderr=subprocess.PIPE,
                               text=True)
        if "HackRF" in result.stdout:
            return True
        else:
            print("HackRF not found in hackrf_info output")
            return False
    except Exception as e:
        print(f"Error checking HackRF availability: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description='Simple PSK Transmitter')
    parser.add_argument('-f', '--freq', type=float, default=433,
                        help='Transmit frequency in MHz (default: 433)')
    parser.add_argument('-b', '--baud', type=int, default=1,
                        help='Baud rate in symbols/sec (default: 1 - EXTREMELY SLOW for visibility)')
    parser.add_argument('-g', '--gain', type=int, default=40,
                        help='Transmit gain (default: 40)')
    parser.add_argument('-s', '--samplerate', type=int, default=2000000,
                        help='Sample rate (default: 2000000)')
    parser.add_argument('-c', '--correction', type=int, default=0,
                        help='Frequency correction in ppm (default: 0)')
    
    args = parser.parse_args()
    
    try:
        # Check if HackRF is available
        if not check_hackrf_available():
            print("ERROR: Unable to proceed without HackRF device")
            return 1
        
        freq_mhz = args.freq
        if args.correction != 0:
            # Apply frequency correction
            correction_factor = args.correction / 1e6
            freq_mhz = freq_mhz * (1 + correction_factor)
            print(f"Applied frequency correction: {args.correction} ppm")
            print(f"Adjusted frequency: {freq_mhz} MHz")
        
        print(f"Transmitting ULTRA SIMPLE PSK signal")
        print(f"IMPORTANT: Set URH to a very slow baud rate (around 1 symbol/sec)")
        print(f"IMPORTANT: Each symbol lasts for 1 second for clear visibility")
        
        # Create and start PSK transmitter
        tx = SimplePSKTransmitter(
            freq_mhz=freq_mhz, 
            baud_rate=args.baud,
            gain=args.gain,
            sample_rate=args.samplerate
        )
        
        # Redirect stdout to prevent binary data from being printed to terminal
        original_stdout = sys.stdout
        
        try:
            tx.start_transmission()
            
            # Use a clean status display instead of raw output
            print("Transmission running. Press Ctrl+C to stop.")
            print("=" * 50)
            print("PSK Transmission Status:")
            print(f"  Frequency: {freq_mhz} MHz")
            print(f"  Pattern: Simple alternating 0/1 (0° and 180° phases)")
            print(f"  Baud Rate: {args.baud} symbols/sec (EXTREMELY SLOW for visibility)")
            print(f"  Symbol Duration: {1/args.baud:.1f} seconds")
            print("=" * 50)
            print("URH SETTINGS:")
            print("  - Set Center to 0")
            print("  - Set Modulation to PSK")
            print("  - Set Bits per Symbol to 1 (for BPSK)")
            print(f"  - Set Samples/Symbol to {args.samplerate//args.baud}")
            print("=" * 50)
            print("Press Ctrl+C to stop transmission")
            
            # Prevent binary data from being printed to terminal
            sys.stdout = open(os.devnull, 'w')
            
            counter = 0
            try:
                while True:
                    time.sleep(1)
                    counter += 1
                    # Periodically restore stdout to show we're still running
                    if counter % 5 == 0:
                        sys.stdout = original_stdout
                        print(f"Still transmitting... (running for {counter} seconds)")
                        sys.stdout = open(os.devnull, 'w')
            except KeyboardInterrupt:
                # Restore stdout for clean exit messages
                sys.stdout = original_stdout
                print("\nTransmission interrupted by user")
        finally:
            # Always restore stdout
            sys.stdout = original_stdout
            tx.stop_transmission()
        
        print("Transmission completed successfully")
        return 0
        
    except Exception as e:
        print(f"ERROR: PSK transmission failed: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main()) 
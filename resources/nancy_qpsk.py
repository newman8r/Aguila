#!/usr/bin/env python3
"""
QPSK Transmitter for Aguila
Based on the PySDR example and adapted for HackRF transmission
Simplified for clear visualization in URH
"""

import sys
import os
import argparse
import time
import numpy as np
from gnuradio import gr, digital, blocks
import osmosdr

class NancyQPSKTransmitter(gr.top_block):
    def __init__(self, freq_mhz=433, baud_rate=100, gain=40, sample_rate=2000000):
        gr.top_block.__init__(self, "Nancy QPSK Transmitter")
        
        self.freq_mhz = freq_mhz
        self.baud_rate = baud_rate
        self.gain = gain
        self.sample_rate = sample_rate
        self.samples_per_symbol = int(self.sample_rate / self.baud_rate)
        
        self.setup_blocks()
        
    def setup_blocks(self):
        print(f"Initializing Nancy QPSK transmitter with parameters:")
        print(f"  - Frequency: {self.freq_mhz} MHz")
        print(f"  - Baud rate: {self.baud_rate} bps")
        print(f"  - Gain: {self.gain}")
        print(f"  - Sample rate: {self.sample_rate/1e3:.1f} kHz")
        print(f"  - Samples per symbol: {self.samples_per_symbol}")
        
        # Create data pattern
        self.data = self.create_qpsk_pattern()
        
        # Blocks
        # 1. Vector source for data
        self.source = blocks.vector_source_c(self.data, repeat=True)
        
        # 2. Repeat to match sample rate
        self.repeat = blocks.repeat(gr.sizeof_gr_complex, self.samples_per_symbol)
        
        # 3. Connect to HackRF
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
        self.connect(self.source, self.repeat, self.hackrf_sink)
        print("QPSK transmitter initialized successfully")
        
    def create_qpsk_pattern(self):
        """Create a QPSK pattern with clear phase transitions"""
        print(f"  - Creating QPSK pattern with clear phase transitions")
        
        # Number of symbols for each phase state
        symbols_per_state = 100
        
        # Create QPSK symbols directly using the PySDR approach
        # We'll create a sequence that cycles through all 4 QPSK symbols
        # with long durations for each symbol
        
        # QPSK symbols at 45, 135, 225, 315 degrees
        symbol_45 = np.cos(np.pi/4) + 1j*np.sin(np.pi/4)    # 45 degrees
        symbol_135 = np.cos(3*np.pi/4) + 1j*np.sin(3*np.pi/4)  # 135 degrees
        symbol_225 = np.cos(5*np.pi/4) + 1j*np.sin(5*np.pi/4)  # 225 degrees
        symbol_315 = np.cos(7*np.pi/4) + 1j*np.sin(7*np.pi/4)  # 315 degrees
        
        # Create a pattern that stays on each symbol for a long time
        pattern = []
        
        # Add each symbol for a long duration
        pattern.extend([symbol_45] * symbols_per_state)
        pattern.extend([symbol_135] * symbols_per_state)
        pattern.extend([symbol_225] * symbols_per_state)
        pattern.extend([symbol_315] * symbols_per_state)
        
        # Now add transitions between adjacent symbols
        # 45 -> 135 -> 225 -> 315 -> 45 (clockwise)
        for _ in range(2):  # Repeat the cycle twice
            pattern.extend([symbol_45] * (symbols_per_state // 2))
            pattern.extend([symbol_135] * (symbols_per_state // 2))
            pattern.extend([symbol_225] * (symbols_per_state // 2))
            pattern.extend([symbol_315] * (symbols_per_state // 2))
        
        # Add counter-clockwise transitions
        # 45 -> 315 -> 225 -> 135 -> 45
        for _ in range(2):  # Repeat the cycle twice
            pattern.extend([symbol_45] * (symbols_per_state // 2))
            pattern.extend([symbol_315] * (symbols_per_state // 2))
            pattern.extend([symbol_225] * (symbols_per_state // 2))
            pattern.extend([symbol_135] * (symbols_per_state // 2))
        
        print(f"  - Created pattern of {len(pattern)} symbols")
        return pattern
        
    def start_transmission(self):
        try:
            print(f"Starting QPSK transmission at {self.freq_mhz} MHz")
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
    parser = argparse.ArgumentParser(description='Nancy QPSK Transmitter')
    parser.add_argument('-f', '--freq', type=float, default=433,
                        help='Transmit frequency in MHz (default: 433)')
    parser.add_argument('-b', '--baud', type=int, default=100,
                        help='Baud rate in symbols/sec (default: 100)')
    parser.add_argument('-g', '--gain', type=int, default=40,
                        help='Transmit gain (default: 40)')
    parser.add_argument('-s', '--samplerate', type=int, default=2000000,
                        help='Sample rate (default: 2000000)')
    
    args = parser.parse_args()
    
    try:
        # Check if HackRF is available
        if not check_hackrf_available():
            print("ERROR: Unable to proceed without HackRF device")
            return 1
        
        freq_mhz = args.freq
        
        print(f"Transmitting Nancy QPSK test pattern")
        
        # Create and start QPSK transmitter
        tx = NancyQPSKTransmitter(
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
            print("QPSK Transmission Status:")
            print(f"  Frequency: {freq_mhz} MHz")
            print(f"  Pattern: Nancy QPSK pattern with clear phase transitions")
            print(f"  Baud Rate: {args.baud} symbols/sec")
            print(f"  Samples/Symbol: {int(args.samplerate/args.baud)}")
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
        print(f"ERROR: QPSK transmission failed: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main()) 
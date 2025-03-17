#!/usr/bin/env python3
"""
Basic QPSK Transmitter for Aguila
Inspired by ZipCPU's Verilog implementation
Stripped down to absolute basics for reliable operation
"""

import sys
import os
import argparse
import time
import numpy as np
from gnuradio import gr, digital, blocks
import osmosdr

class BasicQPSKTransmitter(gr.top_block):
    def __init__(self, freq_mhz=433, baud_rate=200, gain=40, sample_rate=2000000, message="Hello from QPSK!"):
        gr.top_block.__init__(self, "Basic QPSK Transmitter")
        
        self.freq_mhz = freq_mhz
        self.baud_rate = baud_rate
        self.gain = gain
        self.sample_rate = sample_rate
        self.samples_per_symbol = int(self.sample_rate / self.baud_rate)
        self.message = message
        
        self.setup_blocks()
        
    def setup_blocks(self):
        print(f"Initializing Basic QPSK transmitter with parameters:")
        print(f"  - Frequency: {self.freq_mhz} MHz")
        print(f"  - Baud rate: {self.baud_rate} bps")
        print(f"  - Gain: {self.gain}")
        print(f"  - Sample rate: {self.sample_rate/1e3:.1f} kHz")
        print(f"  - Samples per symbol: {self.samples_per_symbol}")
        print(f"  - Message: '{self.message}'")
        
        # Create super simple bit pattern
        self.data = self.create_simple_pattern()
        
        # Blocks
        # 1. Vector source for data
        self.source = blocks.vector_source_b(list(self.data), repeat=True)
        
        # 2. Bytes to bits (unpack)
        self.unpacker = blocks.unpack_k_bits_bb(8)
        
        # 3. Chunks to symbols (map bits to QPSK symbols)
        # QPSK uses 2 bits per symbol - using simpler constellation
        self.chunks_to_symbols = digital.chunks_to_symbols_bc(
            [1+0j, 0+1j, -1+0j, 0-1j],  # Simpler QPSK constellation
            2  # 2 bits per symbol
        )
        
        # 4. Repeat to match sample rate
        self.repeat = blocks.repeat(gr.sizeof_gr_complex, self.samples_per_symbol)
        
        # 5. Connect to HackRF
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
        self.connect(self.source, self.unpacker, self.chunks_to_symbols, 
                    self.repeat, self.hackrf_sink)
        print("QPSK transmitter initialized successfully")
        
    def create_simple_pattern(self):
        """Create an ultra-simple bit pattern for clear QPSK visualization"""
        print(f"  - Creating ultra-simple bit pattern for clear QPSK signal")
        
        # Create a very simple pattern that stays on each symbol for a long time
        # This will make phase transitions very obvious
        
        # 00000000 (symbol 00) for a long time
        pattern1 = bytes([0x00] * 32)
        
        # 01010101 (alternating symbols 00 and 01) for a long time
        pattern2 = bytes([0x55] * 32)
        
        # 10101010 (alternating symbols 10 and 11) for a long time
        pattern3 = bytes([0xAA] * 32)
        
        # 11111111 (symbol 11) for a long time
        pattern4 = bytes([0xFF] * 32)
        
        # Combine patterns
        full_data = pattern1 + pattern2 + pattern3 + pattern4
        
        print(f"  - Created pattern of {len(full_data)} bytes")
        print(f"  - Pattern includes long sequences of each QPSK symbol")
        
        return full_data
        
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
    parser = argparse.ArgumentParser(description='Basic QPSK Transmitter')
    parser.add_argument('-f', '--freq', type=float, default=433,
                        help='Transmit frequency in MHz (default: 433)')
    parser.add_argument('-b', '--baud', type=int, default=200,
                        help='Baud rate in symbols/sec (default: 200)')
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
        
        print(f"Transmitting basic QPSK test pattern")
        
        # Create and start QPSK transmitter
        tx = BasicQPSKTransmitter(
            freq_mhz=freq_mhz, 
            baud_rate=args.baud,
            gain=args.gain,
            sample_rate=args.samplerate,
            message="Test Pattern"
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
            print(f"  Pattern: Ultra-simple QPSK pattern")
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
#!/usr/bin/env python3
"""
QPSK Transmitter for Aguila based on Kevin Vivas' implementation
Adapted for HackRF transmission with both modulation and demodulation capabilities
Modified to make modulation VERY visible in URH/GQRX
"""

import sys
import os
import argparse
import time
import numpy as np
from math import sqrt
import random
from gnuradio import gr, blocks
import osmosdr

class KevinQPSKTransmitter(gr.top_block):
    def __init__(self, freq_mhz=433, baud_rate=10, gain=40, sample_rate=2000000, message=None):
        gr.top_block.__init__(self, "Kevin QPSK Transmitter")
        
        self.freq_mhz = freq_mhz
        self.baud_rate = baud_rate  # Slowed down significantly for visibility
        self.gain = gain
        self.sample_rate = sample_rate
        self.samples_per_symbol = int(self.sample_rate / self.baud_rate)
        self.message = message
        
        self.setup_blocks()
        
    def setup_blocks(self):
        print(f"Initializing Kevin QPSK transmitter with parameters:")
        print(f"  - Frequency: {self.freq_mhz} MHz")
        print(f"  - Baud rate: {self.baud_rate} bps (VERY SLOW for visibility)")
        print(f"  - Gain: {self.gain}")
        print(f"  - Sample rate: {self.sample_rate/1e3:.1f} kHz")
        print(f"  - Samples per symbol: {self.samples_per_symbol}")
        
        # Create QPSK modulated data
        self.data = self.create_qpsk_modulation()
        
        # Blocks
        # 1. Vector source for data
        self.source = blocks.vector_source_c(self.data, repeat=True)
        
        # 2. Repeat to match sample rate (not needed with our approach)
        # self.repeat = blocks.repeat(gr.sizeof_gr_complex, self.samples_per_symbol)
        
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
        self.connect(self.source, self.hackrf_sink)
        print("QPSK transmitter initialized successfully")
    
    def generate_binary_data(self, num_bits=16):
        """Generate binary data or convert message to binary"""
        if self.message:
            # Convert message to binary
            binary_data = []
            for char in self.message:
                # Convert each character to 8 bits
                bits = [int(b) for b in format(ord(char), '08b')]
                binary_data.extend(bits)
            
            # Ensure we have an even number of bits for QPSK
            if len(binary_data) % 2 != 0:
                binary_data.append(0)  # Padding
                
            print(f"  - Generated {len(binary_data)} bits from message: '{self.message}'")
            return binary_data
        else:
            # Generate a simple repeating pattern for clear visibility
            # Using 00, 01, 10, 11 sequence to cycle through all QPSK states
            pattern = [0, 0, 0, 1, 1, 0, 1, 1]
            binary_data = pattern * (num_bits // len(pattern) + 1)
            binary_data = binary_data[:num_bits]
            print(f"  - Generated {len(binary_data)} bits with pattern: {pattern}")
            return binary_data
    
    def create_qpsk_modulation(self):
        """Create QPSK modulated signal with VERY visible phase shifts"""
        print(f"  - Creating QPSK modulated signal with DRAMATIC phase shifts")
        
        # Generate binary data - use a small amount for testing
        binary_data = self.generate_binary_data(num_bits=32)  # Small for clear pattern
        
        # Number of samples per symbol - MUCH higher for visibility
        # This effectively creates a very slow baud rate
        samples_per_symbol = 200000  # 0.1 seconds per symbol at 2MHz sample rate
        
        # Create a complex signal array
        qpsk_signal = []
        
        # Process bits in pairs for QPSK
        for i in range(0, len(binary_data), 2):
            if i+1 >= len(binary_data):
                break  # Avoid index out of range
            
            # Get the I and Q bits
            I_bit = binary_data[i]
            Q_bit = binary_data[i+1]
            
            # Map bit pairs to QPSK phases (in radians)
            # 00 -> 225° (5π/4)
            # 01 -> 315° (7π/4)
            # 10 -> 135° (3π/4)
            # 11 -> 45°  (π/4)
            if I_bit == 0 and Q_bit == 0:
                phase = 5 * np.pi / 4  # 225 degrees
                symbol_desc = "00 -> 225°"
            elif I_bit == 0 and Q_bit == 1:
                phase = 7 * np.pi / 4  # 315 degrees
                symbol_desc = "01 -> 315°"
            elif I_bit == 1 and Q_bit == 0:
                phase = 3 * np.pi / 4  # 135 degrees
                symbol_desc = "10 -> 135°"
            else:  # I_bit == 1 and Q_bit == 1
                phase = np.pi / 4     # 45 degrees
                symbol_desc = "11 -> 45°"
            
            print(f"  - Symbol: {symbol_desc}")
            
            # Generate complex samples for this symbol
            # e^(jθ) = cos(θ) + j*sin(θ)
            symbol_samples = [np.exp(1j * phase)] * samples_per_symbol
            qpsk_signal.extend(symbol_samples)
        
        print(f"  - Created QPSK signal with {len(qpsk_signal)} samples")
        print(f"  - Each symbol lasts for {samples_per_symbol/self.sample_rate:.2f} seconds")
        print(f"  - Total signal duration: {len(qpsk_signal)/self.sample_rate:.2f} seconds")
        
        return qpsk_signal
    
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
    parser = argparse.ArgumentParser(description='Kevin QPSK Transmitter')
    parser.add_argument('-f', '--freq', type=float, default=433,
                        help='Transmit frequency in MHz (default: 433)')
    parser.add_argument('-b', '--baud', type=int, default=10,
                        help='Baud rate in symbols/sec (default: 10 - VERY SLOW for visibility)')
    parser.add_argument('-g', '--gain', type=int, default=40,
                        help='Transmit gain (default: 40)')
    parser.add_argument('-s', '--samplerate', type=int, default=2000000,
                        help='Sample rate (default: 2000000)')
    parser.add_argument('-m', '--message', type=str, default=None,
                        help='Text message to transmit (default: test pattern)')
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
        
        print(f"Transmitting SUPER-VISIBLE Kevin QPSK signal")
        print(f"IMPORTANT: Set URH to a very slow baud rate (around 10 symbols/sec)")
        print(f"IMPORTANT: Each symbol lasts 0.1 seconds for clear visibility")
        
        # Create and start QPSK transmitter
        tx = KevinQPSKTransmitter(
            freq_mhz=freq_mhz, 
            baud_rate=args.baud,
            gain=args.gain,
            sample_rate=args.samplerate,
            message=args.message
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
            if args.message:
                print(f"  Message: '{args.message}'")
            else:
                print(f"  Pattern: 00->01->10->11 (cycles through all QPSK states)")
            print(f"  Baud Rate: {args.baud} symbols/sec (VERY SLOW for visibility)")
            print(f"  Symbol Duration: {1/args.baud:.2f} seconds")
            print("=" * 50)
            print("URH SETTINGS:")
            print("  - Set Center to 0")
            print("  - Set Modulation to PSK")
            print("  - Set Bits per Symbol to 2 (for QPSK)")
            print("  - Set Samples/Symbol to 200000")
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
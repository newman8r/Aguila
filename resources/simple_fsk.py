#!/usr/bin/env python3
"""
SIMPLE FSK Transmitter for Aguila
Created from scratch to demonstrate basic FSK modulation
Designed to be clearly visible in URH/GQRX
Now with text message support!
"""

import sys
import os
import argparse
import time
import numpy as np
from gnuradio import gr, blocks, analog
import osmosdr

class SimpleFSKTransmitter(gr.top_block):
    def __init__(self, freq_mhz=433, baud_rate=50, freq_deviation=20000, gain=40, sample_rate=2000000, message=None):
        gr.top_block.__init__(self, "Simple FSK Transmitter")
        
        self.freq_mhz = freq_mhz
        self.baud_rate = baud_rate  # Slow for visibility
        self.freq_deviation = freq_deviation  # Hz - large deviation for visibility
        self.gain = gain
        self.sample_rate = sample_rate
        self.samples_per_symbol = int(self.sample_rate / self.baud_rate)
        self.message = message
        
        self.setup_blocks()
        
    def setup_blocks(self):
        print(f"Initializing SIMPLE FSK transmitter with parameters:")
        print(f"  - Frequency: {self.freq_mhz} MHz")
        print(f"  - Baud rate: {self.baud_rate} bps")
        print(f"  - Frequency deviation: {self.freq_deviation/1000:.1f} kHz")
        print(f"  - Gain: {self.gain}")
        print(f"  - Sample rate: {self.sample_rate/1e3:.1f} kHz")
        print(f"  - Samples per symbol: {self.samples_per_symbol}")
        if self.message:
            print(f"  - Message: '{self.message}'")
        
        # Create FSK modulated data
        self.data = self.create_fsk_modulation()
        
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
        print("FSK transmitter initialized successfully")
    
    def text_to_bits(self, text):
        """Convert text to binary bits"""
        bits = []
        for char in text:
            # Convert each character to 8 bits
            char_bits = [int(b) for b in format(ord(char), '08b')]
            bits.extend(char_bits)
        return bits
    
    def create_fsk_modulation(self):
        """Create FSK modulation with text message or pattern"""
        print(f"  - Creating FSK modulation")
        
        # Generate bits from message or use default pattern
        if self.message:
            # Add preamble for synchronization (alternating 10101010)
            preamble = [1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0]
            
            # Convert message to bits
            message_bits = self.text_to_bits(self.message)
            
            # Add postamble (alternating 10101010)
            postamble = [1, 0, 1, 0, 1, 0, 1, 0]
            
            # Combine all parts
            bits = preamble + message_bits + postamble
            
            print(f"  - Generated {len(bits)} bits from message:")
            print(f"    - Preamble: {len(preamble)} bits")
            print(f"    - Message: {len(message_bits)} bits ({len(self.message)} characters)")
            print(f"    - Postamble: {len(postamble)} bits")
        else:
            # Create a simple alternating bit pattern: 0101010101...
            bits = [0, 1, 0, 1, 0, 1, 0, 1, 0, 1]  # Simple alternating pattern
            print(f"  - Using default alternating pattern: {bits}")
        
        # Create the FSK signal
        fsk_signal = []
        
        # Time vector for one symbol
        t = np.arange(0, self.samples_per_symbol) / self.sample_rate
        
        # Keep track of phase to ensure continuity between symbols
        current_phase = 0
        
        for i, bit in enumerate(bits):
            # Map bit to frequency:
            # 0 -> -deviation
            # 1 -> +deviation
            freq = -self.freq_deviation if bit == 0 else self.freq_deviation
            
            # Generate complex samples for this symbol using frequency offset
            # e^(j2πft + φ) = cos(2πft + φ) + j*sin(2πft + φ)
            # Use continuous phase to avoid clicks/pops
            phase_increment = 2 * np.pi * freq * (1/self.sample_rate)
            symbol_phases = current_phase + np.cumsum(np.ones(self.samples_per_symbol) * phase_increment)
            current_phase = symbol_phases[-1]
            
            symbol_samples = np.exp(1j * symbol_phases)
            
            # Add to our signal
            fsk_signal.extend(symbol_samples)
            
            # Only print details for the first few bits to avoid flooding the console
            if i < 5 or i >= len(bits) - 5:
                print(f"  - Added symbol: bit={bit}, freq={freq/1000:.1f} kHz, duration={self.samples_per_symbol/self.sample_rate*1000:.2f} ms")
            elif i == 5:
                print(f"  - ... ({len(bits) - 10} more symbols) ...")
        
        print(f"  - Created FSK signal with {len(fsk_signal)} samples")
        print(f"  - Each symbol lasts for {self.samples_per_symbol/self.sample_rate*1000:.2f} ms")
        print(f"  - Total signal duration: {len(fsk_signal)/self.sample_rate:.2f} seconds")
        
        return fsk_signal
    
    def start_transmission(self):
        try:
            print(f"Starting FSK transmission at {self.freq_mhz} MHz")
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
    parser = argparse.ArgumentParser(description='Simple FSK Transmitter')
    parser.add_argument('-f', '--freq', type=float, default=433,
                        help='Transmit frequency in MHz (default: 433)')
    parser.add_argument('-b', '--baud', type=int, default=50,
                        help='Baud rate in symbols/sec (default: 50)')
    parser.add_argument('-d', '--deviation', type=int, default=20000,
                        help='Frequency deviation in Hz (default: 20000)')
    parser.add_argument('-g', '--gain', type=int, default=40,
                        help='Transmit gain (default: 40)')
    parser.add_argument('-s', '--samplerate', type=int, default=2000000,
                        help='Sample rate (default: 2000000)')
    parser.add_argument('-c', '--correction', type=int, default=0,
                        help='Frequency correction in ppm (default: 0)')
    parser.add_argument('-m', '--message', type=str, default=None,
                        help='Text message to transmit (default: alternating pattern)')
    
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
        
        print(f"Transmitting FSK signal")
        if args.message:
            print(f"Message: '{args.message}'")
        else:
            print(f"Pattern: Simple alternating 0/1")
        
        # Create and start FSK transmitter
        tx = SimpleFSKTransmitter(
            freq_mhz=freq_mhz, 
            baud_rate=args.baud,
            freq_deviation=args.deviation,
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
            print("FSK Transmission Status:")
            print(f"  Frequency: {freq_mhz} MHz")
            if args.message:
                print(f"  Message: '{args.message}'")
            else:
                print(f"  Pattern: Simple alternating 0/1")
            print(f"  Frequency deviation: +/- {args.deviation/1000:.1f} kHz")
            print(f"  Baud Rate: {args.baud} symbols/sec")
            print(f"  Symbol Duration: {1/args.baud*1000:.2f} ms")
            print(f"  Samples per Symbol: {int(args.samplerate/args.baud)}")
            print("=" * 50)
            print("URH SETTINGS:")
            print("  - Set Modulation to FSK")
            print("  - Set Bits per Symbol to 1")
            print(f"  - Set Samples/Symbol to {args.samplerate//args.baud}")
            if args.message:
                print("  - In Interpretation view, set format to ASCII to see the text")
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
        print(f"ERROR: FSK transmission failed: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main()) 
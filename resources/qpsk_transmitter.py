#!/usr/bin/env python3
"""
QPSK Transmitter for Aguila
Uses HackRF to transmit binary data using Quadrature Phase Shift Keying
Includes configurable settings and text message transmission
"""

import sys
import os
import argparse
import time
import numpy as np
from gnuradio import gr, digital, blocks, filter
import osmosdr

class QPSKTransmitter(gr.top_block):
    def __init__(self, freq_mhz=433, baud_rate=2000, excess_bw=0.35, gain=20, sample_rate=2400000, message="Hello from QPSK!"):
        gr.top_block.__init__(self, "QPSK Transmitter")
        
        self.freq_mhz = freq_mhz
        self.baud_rate = baud_rate
        self.excess_bw = excess_bw  # Roll-off factor for RRC filter
        self.gain = gain
        self.sample_rate = sample_rate
        self.samples_per_symbol = int(self.sample_rate / self.baud_rate)
        self.message = message
        
        self.setup_blocks()
        
    def setup_blocks(self):
        print(f"Initializing QPSK transmitter with parameters:")
        print(f"  - Frequency: {self.freq_mhz} MHz")
        print(f"  - Baud rate: {self.baud_rate} bps")
        print(f"  - Excess bandwidth: {self.excess_bw}")
        print(f"  - Gain: {self.gain}")
        print(f"  - Sample rate: {self.sample_rate/1e3:.1f} kHz")
        print(f"  - Samples per symbol: {self.samples_per_symbol}")
        print(f"  - Message: '{self.message}'")
        
        # Create data pattern
        self.data = self.create_data_pattern()
        
        # Blocks
        # 1. Vector source for data
        self.source = blocks.vector_source_b(list(self.data), repeat=True)
        
        # 2. Bytes to bits (unpack)
        self.unpacker = blocks.unpack_k_bits_bb(8)
        
        # 3. Chunks to symbols (map bits to QPSK symbols)
        # QPSK uses 2 bits per symbol
        self.chunks_to_symbols = digital.chunks_to_symbols_bc(
            [1+0j, 0+1j, -1+0j, 0-1j],  # Simpler QPSK constellation (Gray coded)
            2  # 2 bits per symbol
        )
        
        # 4. Add upsampler to match sample rate
        self.upsampler = blocks.repeat(gr.sizeof_gr_complex, self.samples_per_symbol)
        
        # 5. Root Raised Cosine filter for pulse shaping
        ntaps = 11 * self.samples_per_symbol
        self.rrc_filter = filter.firdes.root_raised_cosine(
            1.0,                   # Gain
            self.samples_per_symbol,  # Sampling rate
            1.0,                   # Symbol rate
            self.excess_bw,        # Roll-off factor
            ntaps                  # Number of taps
        )
        self.rrc_filter_block = filter.fir_filter_ccf(1, self.rrc_filter)
        
        # 6. Connect to HackRF
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
        self.hackrf_sink.set_gain(self.gain)  # Reduced RF gain
        self.hackrf_sink.set_if_gain(20)      # Reduced IF gain
        self.hackrf_sink.set_bb_gain(0)       # Reduced BB gain
        self.hackrf_sink.set_bandwidth(self.sample_rate/2)
        self.hackrf_sink.set_antenna("TX")
        
        # Connect the blocks
        print("Connecting GNU Radio blocks...")
        self.connect(self.source, self.unpacker, self.chunks_to_symbols, 
                    self.upsampler, self.rrc_filter_block, self.hackrf_sink)
        print("QPSK transmitter initialized successfully")
        
    def create_data_pattern(self):
        # Create a message with proper framing
        print(f"  - Creating message data pattern")
        
        # Convert message to bytes
        message_bytes = self.message.encode('utf-8')
        
        # Create a more distinct preamble for synchronization
        # Using 0xF0 (11110000) pattern which is more distinct than 0xAA
        preamble = bytes([0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0])
        
        # Create a distinct sync word to mark the start of the message
        sync_word = bytes([0x1A, 0xCF, 0xFC, 0x1D])  # Distinct pattern
        
        # Create postamble
        postamble = bytes([0xF0, 0xF0, 0xF0, 0xF0])
        
        # Add message length as a single byte (for messages up to 255 bytes)
        length_byte = bytes([len(message_bytes)])
        
        # Construct the full packet with framing
        data_packet = preamble + sync_word + length_byte + message_bytes + postamble
        
        # Add some padding zeros between repetitions (longer for better separation)
        padding = bytes([0x00] * 16)
        
        # Repeat the packet multiple times with padding
        full_data = data_packet + padding
        full_data = full_data * 5  # Repeat 5 times
        
        print(f"  - Packet structure: Preamble + Sync Word + Length + Message + Postamble")
        print(f"  - Total packet size: {len(data_packet)} bytes")
        print(f"  - Message content: '{self.message}'")
        print(f"  - Hex representation of first few bytes: {' '.join([f'{b:02X}' for b in data_packet[:20]])}")
        
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
    parser = argparse.ArgumentParser(description='QPSK Transmitter')
    parser.add_argument('-f', '--freq', type=float, default=433,
                        help='Transmit frequency in MHz (default: 433)')
    parser.add_argument('-b', '--baud', type=int, default=2000,
                        help='Baud rate in symbols/sec (default: 2000)')
    parser.add_argument('-e', '--excess_bw', type=float, default=0.35,
                        help='Excess bandwidth for RRC filter (default: 0.35)')
    parser.add_argument('-g', '--gain', type=int, default=20,
                        help='Transmit gain (default: 20)')
    parser.add_argument('-s', '--samplerate', type=int, default=2400000,
                        help='Sample rate (default: 2400000)')
    parser.add_argument('-c', '--correction', type=int, default=0,
                        help='Frequency correction in ppm (default: 0)')
    parser.add_argument('-m', '--message', type=str, default="HELLO STEVE! THIS IS A TEST MESSAGE FROM B. IF YOU CAN READ THIS, QPSK IS WORKING!",
                        help='Message to transmit')
    parser.add_argument("-d", "--debug", action="store_true",
                        help="Enable extra debug output")
    
    args = parser.parse_args()
    
    try:
        # Check if HackRF is available
        if not check_hackrf_available():
            print("ERROR: Unable to proceed without HackRF device")
            return 1
        
        # We're not applying frequency correction as the user is handling that
        freq_mhz = args.freq
        
        print(f"Transmitting QPSK message: '{args.message}'")
        
        # Create and start QPSK transmitter
        tx = QPSKTransmitter(
            freq_mhz=freq_mhz, 
            baud_rate=args.baud,
            excess_bw=args.excess_bw,
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
            print(f"  Message: '{args.message}'")
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
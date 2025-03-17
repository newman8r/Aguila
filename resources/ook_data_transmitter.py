#!/usr/bin/env python3
"""
OOK Data Transmitter
Transmits actual data using On-Off Keying (OOK) modulation.
Includes configurable baud rate, message content, and framing.
"""

import sys
import time
import struct
import numpy as np
import osmosdr
import argparse
from gnuradio import gr, blocks

class OOKTransmitter(gr.top_block):
    def __init__(self, freq_mhz, message, baud_rate=1200, gain=40, freq_correction_ppm=-69):
        gr.top_block.__init__(self, "OOK Data Transmitter")

        # Parameters
        self.freq_mhz = freq_mhz
        self.message = message
        self.baud_rate = baud_rate
        self.gain = gain
        self.freq_correction_ppm = freq_correction_ppm
        self.sample_rate = 2e6  # 2 MHz sample rate
        
        # Calculate samples per symbol based on sample rate and baud rate
        self.samples_per_symbol = int(self.sample_rate / self.baud_rate)
        
        # Calculate actual frequency with correction
        freq_hz = self.freq_mhz * 1e6
        freq_offset = freq_hz * (self.freq_correction_ppm / 1e6)
        self.corrected_freq = freq_hz + freq_offset
        
        # Prepare message bits with framing
        self.data = self.prepare_message()
        
        # Setup flowgraph
        self.setup_blocks()
        self.connect_blocks()
        
    def prepare_message(self):
        # Create preamble - alternating 1s and 0s for sync (0xAA = 10101010)
        preamble = bytes([0xAA] * 8)
        
        # Create message data
        if isinstance(self.message, str):
            # If string, encode to bytes
            message_bytes = self.message.encode('utf-8')
        else:
            # If already bytes or list, use as is
            message_bytes = bytes(self.message)
            
        # Create postamble - usually same as preamble
        postamble = bytes([0xAA] * 4)
        
        # Complete packet
        packet = preamble + message_bytes + postamble
        
        # Convert to unpacked bits (one byte per bit)
        bits = []
        for byte in packet:
            for i in range(8):
                bit = (byte >> (7 - i)) & 1
                bits.append(bit)
                
        return bytes(bits)
        
    def setup_blocks(self):
        # Vector source for data
        self.source = blocks.vector_source_b(list(self.data), repeat=True)
        
        # Repeat each bit to match sample rate
        self.repeat = blocks.repeat(gr.sizeof_char, self.samples_per_symbol)
        
        # Convert to float
        self.char_to_float = blocks.char_to_float(1, 1.0)
        
        # Convert to complex for transmission
        self.float_to_complex = blocks.float_to_complex(1)
        
        # HackRF sink
        self.sink = osmosdr.sink(args="hackrf=0")
        self.sink.set_sample_rate(self.sample_rate)
        self.sink.set_center_freq(self.corrected_freq)
        self.sink.set_gain(self.gain)               # Overall gain
        self.sink.set_if_gain(40)                   # IF gain
        self.sink.set_bb_gain(20)                   # Baseband gain
        self.sink.set_bandwidth(self.sample_rate/2) # Bandwidth
        self.sink.set_antenna("TX")
        
    def connect_blocks(self):
        self.connect(self.source, self.repeat, self.char_to_float, 
                    self.float_to_complex, self.sink)

def main():
    parser = argparse.ArgumentParser(description='OOK Data Transmitter')
    parser.add_argument('-f', '--frequency', type=float, default=434.6,
                        help='Center frequency in MHz (default: 434.6)')
    parser.add_argument('-m', '--message', type=str, default="Hello World!",
                        help='Message to transmit (default: "Hello World!")')
    parser.add_argument('-b', '--baud', type=int, default=1200,
                        help='Baud rate (default: 1200)')
    parser.add_argument('-g', '--gain', type=int, default=40,
                        help='Transmitter gain (default: 40)')
    parser.add_argument('-c', '--correction', type=int, default=-69,
                        help='Frequency correction in PPM (default: -69)')
    parser.add_argument('-p', '--pattern', action='store_true',
                        help='Use test pattern instead of message')
    
    args = parser.parse_args()
    
    # If pattern flag is set, use a test pattern
    if args.pattern:
        # Simple alternating pattern for easy recognition
        test_pattern = bytes([0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55])
        message = test_pattern
    else:
        message = args.message
    
    print(f"=== OOK Data Transmitter ===")
    print(f"Frequency: {args.frequency} MHz (corrected for {args.correction} PPM)")
    print(f"Baud Rate: {args.baud} bps")
    print(f"Gain: {args.gain}")
    if args.pattern:
        print(f"Transmitting test pattern: 0xAA 0x55 repeating")
    else:
        print(f"Transmitting message: '{args.message}'")
    print(f"Press Ctrl+C to stop transmission")
    
    # Create and run transmitter
    tb = OOKTransmitter(
        freq_mhz=args.frequency,
        message=message,
        baud_rate=args.baud,
        gain=args.gain,
        freq_correction_ppm=args.correction
    )
    
    try:
        tb.start()
        # Keep program running until interrupted
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nTransmission stopped by user")
    finally:
        # Clean shutdown
        tb.stop()
        tb.wait()
    
    return 0

if __name__ == "__main__":
    sys.exit(main()) 
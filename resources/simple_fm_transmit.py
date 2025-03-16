#!/usr/bin/env python3
"""
Ultra Simple FM Transmitter Script
Transmits a simple sine wave tone using HackRF
No audio file needed, just to test basic transmission
"""

import sys
import time
import numpy as np
from gnuradio import gr, analog, blocks
import osmosdr

class SimpleToneTransmitter(gr.top_block):
    def __init__(self, freq=88.0e6, tone_freq=1000, gain=14, sample_rate=2e6):
        gr.top_block.__init__(self, "Simple Tone Transmitter")
        
        # Create a signal source (sine wave)
        self.source = analog.sig_source_c(sample_rate, analog.GR_SIN_WAVE, tone_freq, 0.3, 0)
        
        # Create HackRF sink with minimal configuration
        self.sink = osmosdr.sink('hackrf=0')
        self.sink.set_sample_rate(sample_rate)
        self.sink.set_center_freq(freq)
        self.sink.set_gain(gain)
        self.sink.set_antenna("TX")
        
        # Connect blocks
        self.connect(self.source, self.sink)
        print(f"Set up tone transmission at {freq/1e6} MHz (tone: {tone_freq} Hz)")

def main():
    # Default parameters
    freq_mhz = 88.0
    tone_hz = 1000
    gain_db = 14
    
    # Allow frequency override from command line
    if len(sys.argv) > 1:
        try:
            freq_mhz = float(sys.argv[1])
        except:
            pass
    
    # Create flowgraph
    tb = SimpleToneTransmitter(freq=freq_mhz * 1e6, tone_freq=tone_hz, gain=gain_db)
    
    try:
        print(f"Starting transmission at {freq_mhz} MHz...")
        tb.start()
        
        # Run for 10 seconds
        print("Transmitting for 10 seconds...")
        time.sleep(10)
        
        # Clean up
        tb.stop()
        tb.wait()
        print("Transmission complete")
        return 0
        
    except KeyboardInterrupt:
        print("\nTransmission interrupted by user")
        tb.stop()
        tb.wait()
        return 0
        
    except Exception as e:
        print(f"ERROR: {str(e)}")
        return 1

if __name__ == "__main__":
    sys.exit(main()) 
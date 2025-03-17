#!/usr/bin/env python3
"""
Ultra-Simple OOK Transmitter
Just toggles the carrier on and off with long periods to make it very obvious
"""

import sys
import time
import osmosdr

def main():
    # Parameters
    freq_mhz = 434.6
    freq_correction_ppm = -69  # Based on previous testing
    
    # Calculate actual frequency with correction
    freq_hz = freq_mhz * 1e6
    freq_offset = freq_hz * (freq_correction_ppm / 1e6)
    corrected_freq = freq_hz + freq_offset
    
    print(f"Simple OOK carrier test")
    print(f"Base frequency: {freq_mhz} MHz")
    print(f"Corrected frequency: {corrected_freq/1e6:.3f} MHz")
    
    # Initialize HackRF
    print("Connecting to HackRF...")
    try:
        hackrf = osmosdr.sink('hackrf=0')
        print("HackRF connected successfully")
    except Exception as e:
        print(f"ERROR connecting to HackRF: {e}")
        return 1
    
    # Configure HackRF
    print("Configuring HackRF...")
    sample_rate = 2e6
    hackrf.set_sample_rate(sample_rate)
    hackrf.set_center_freq(corrected_freq)
    hackrf.set_gain(20)
    hackrf.set_if_gain(40)
    hackrf.set_bb_gain(20)
    hackrf.set_bandwidth(sample_rate/2)
    hackrf.set_antenna("TX")
    
    # Turn carrier on and off in simple pattern
    try:
        while True:
            # ON for 3 seconds
            print("Carrier ON")
            hackrf.set_gain(20)  # Turn on gain
            time.sleep(3)
            
            # OFF for 3 seconds
            print("Carrier OFF")
            hackrf.set_gain(0)   # Turn off gain
            time.sleep(3)
    except KeyboardInterrupt:
        print("Interrupted by user")
    finally:
        # Make sure to shut down transmitter
        print("Shutting down transmitter")
        hackrf.set_gain(0)
    
    return 0

if __name__ == "__main__":
    sys.exit(main()) 
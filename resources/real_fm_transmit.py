#!/usr/bin/env python3
"""
Real FM Transmitter for Aguila
Based on our working simple transmitter but with WAV file playback and FM modulation
"""

import sys
import time
import os
import argparse
import numpy as np
from gnuradio import gr, analog, blocks, filter
import osmosdr

class FMTransmitter(gr.top_block):
    def __init__(self, freq=88.0e6, audio_file=None, gain=14, sample_rate=2e6):
        gr.top_block.__init__(self, "FM Transmitter")
        
        # Parameters
        self.freq = freq
        self.gain = gain
        self.sample_rate = sample_rate
        self.audio_file = audio_file
        self.audio_rate = 48000  # Assuming 48kHz WAV file
        
        print(f"Initializing FM transmitter with parameters:")
        print(f"  - Frequency: {freq/1e6:.3f} MHz")
        print(f"  - Audio file: {audio_file}")
        print(f"  - Gain: {gain}")
        print(f"  - Sample rate: {sample_rate/1e6:.1f} MHz")
        
        # Blocks
        print("Creating audio source...")
        self.source = blocks.wavfile_source(audio_file, False)
        
        # If stereo WAV file, take just the left channel
        self.to_mono = blocks.multiply_const_ff(0.5)
        
        # FM modulation
        print("Setting up FM modulation...")
        self.fm_mod = analog.frequency_modulator_fc(
            self.audio_rate / 75e3)  # 75 kHz deviation
        
        # Interpolate audio to sample rate
        print("Creating interpolation filter...")
        self.interp = filter.rational_resampler_ccc(
            interpolation=int(self.sample_rate / self.audio_rate),
            decimation=1,
            taps=[],
            fractional_bw=0.0)
        
        # Connect to HackRF - using simple style that works
        print("Connecting to HackRF device...")
        self.hackrf_sink = osmosdr.sink('hackrf=0')
        
        # Configure HackRF parameters
        print("Configuring HackRF parameters...")
        self.hackrf_sink.set_sample_rate(self.sample_rate)
        self.hackrf_sink.set_center_freq(self.freq)
        self.hackrf_sink.set_gain(self.gain)
        self.hackrf_sink.set_bandwidth(self.sample_rate)
        self.hackrf_sink.set_antenna("TX")
        
        # Connect blocks
        print("Connecting GNU Radio blocks...")
        self.connect(self.source, self.to_mono, self.fm_mod, self.interp, self.hackrf_sink)
        print("FM transmitter initialized successfully")
    
    def start_transmission(self):
        print(f"Starting FM transmission at {self.freq/1e6:.3f} MHz")
        self.start()
        print("Transmission started successfully")
        
    def stop_transmission(self):
        print("Stopping transmission")
        self.stop()
        self.wait()
        print("Transmission stopped successfully")

def main():
    parser = argparse.ArgumentParser(description="Aguila FM Transmitter")
    parser.add_argument("-f", "--frequency", type=float, default=88.0,
                      help="Frequency in MHz (default: 88.0)")
    parser.add_argument("-a", "--audio-file", type=str, default="resources/audio/bgmusic.wav",
                      help="Path to WAV file to transmit (default: resources/audio/bgmusic.wav)")
    parser.add_argument("-g", "--gain", type=int, default=14,
                      help="RF gain (default: 14)")
    parser.add_argument("-d", "--debug", action="store_true",
                      help="Enable extra debug output")
    
    args = parser.parse_args()
    
    try:
        # Convert frequency from MHz to Hz
        freq_hz = args.frequency * 1e6
        
        # Get absolute path to the audio file
        script_dir = os.path.dirname(os.path.realpath(__file__))
        app_root = os.path.abspath(os.path.join(script_dir, ".."))
        audio_path = args.audio_file
        if not os.path.isabs(audio_path):
            audio_path = os.path.join(app_root, audio_path)
        
        print(f"Script directory: {script_dir}")
        print(f"App root directory: {app_root}")
        print(f"Full audio path: {audio_path}")
        
        if not os.path.exists(audio_path):
            print(f"ERROR: Audio file not found: {audio_path}")
            print(f"Current directory: {os.getcwd()}")
            return 1
        
        print(f"Transmitting {audio_path} at {args.frequency} MHz")
        
        # Create and start FM transmitter
        tx = FMTransmitter(freq=freq_hz, audio_file=audio_path, gain=args.gain)
        tx.start_transmission()
        
        try:
            # Run until WAV file finishes playing or Ctrl+C
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
        print(f"ERROR: FM transmission failed: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main()) 
#!/usr/bin/env python3
"""
FM Transmitter Tool for Aguila
Uses HackRF to transmit FM audio from a WAV file at the currently selected frequency
"""

import sys
import os
import argparse
import time
import traceback
import numpy as np
from gnuradio import gr, blocks, analog, filter
from gnuradio.eng_arg import eng_float
import osmosdr

class FMTransmitter(gr.top_block):
    def __init__(self, freq=100.0e6, audio_file=None, gain=14, sample_rate=2e6):
        try:
            gr.top_block.__init__(self, "FM Transmitter")
            
            # Parameters
            self.freq = freq
            self.gain = gain
            self.sample_rate = sample_rate
            self.audio_file = audio_file
            
            print(f"Initializing FM transmitter with parameters:")
            print(f"  - Frequency: {freq/1e6:.3f} MHz")
            print(f"  - Audio file: {audio_file}")
            print(f"  - Gain: {gain}")
            print(f"  - Sample rate: {sample_rate/1e6:.1f} MHz")
            
            # Blocks
            print("Creating audio source...")
            self.source = blocks.wavfile_source(audio_file, False)
            self.audio_rate = 48000  # Assuming 48kHz WAV file
            
            # If stereo WAV file, take just the left channel
            self.to_mono = blocks.multiply_const_ff(0.5)
            
            # FM modulation (deviation 75 kHz)
            print("Setting up FM modulation...")
            self.fm_mod = analog.frequency_modulator_fc(
                self.audio_rate / 75e3)
            
            # Interpolate audio to sample rate
            print("Creating interpolation filter...")
            self.interp = filter.rational_resampler_ccc(
                interpolation=int(self.sample_rate / self.audio_rate),
                decimation=1,
                taps=[],
                fractional_bw=0.0)
            
            # Connect to HackRF - using simple method that works
            print("Connecting to HackRF device...")
            try:
                # Use simple connection string that works reliably
                self.hackrf_sink = osmosdr.sink('hackrf=0')
                print(f"HackRF device connected successfully")
            except Exception as e:
                print(f"ERROR: Failed to connect to HackRF device: {e}")
                raise
                
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
            
        except Exception as e:
            print(f"ERROR during FM transmitter initialization: {e}")
            print("Traceback:")
            traceback.print_exc()
            raise
    
    def start_transmission(self):
        try:
            print(f"Starting FM transmission at {self.freq/1e6:.3f} MHz")
            self.start()
            print("Transmission started successfully")
        except Exception as e:
            print(f"ERROR starting transmission: {e}")
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
            traceback.print_exc()

def check_hackrf_available():
    """Check if HackRF device is available and connected"""
    try:
        print("Checking for HackRF device...")
        devices = osmosdr.device.find()
        if len(devices) == 0:
            print("ERROR: No SDR devices found")
            return False
            
        # Look for HackRF or any available device
        print("Available devices:")
        for i, dev in enumerate(devices):
            dev_str = str(dev)
            print(f"  {i}: {dev_str}")
            
        # If we get here, we have devices to try
        # Instead of checking the name (which doesn't show in the object's string repr),
        # we'll just attempt to create a sink with the right parameters later
        return True
    except Exception as e:
        print(f"ERROR checking SDR availability: {e}")
        traceback.print_exc()
        return False

def main():
    parser = argparse.ArgumentParser(description="Aguila FM Transmitter")
    parser.add_argument("-f", "--frequency", type=float, default=100.0,
                      help="Frequency in MHz (default: 100.0)")
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
        audio_path = os.path.join(app_root, args.audio_file)
        
        print(f"Script directory: {script_dir}")
        print(f"App root directory: {app_root}")
        print(f"Full audio path: {audio_path}")
        
        if not os.path.exists(audio_path):
            print(f"ERROR: Audio file not found: {audio_path}")
            print(f"Current directory: {os.getcwd()}")
            print(f"Directory contents: {os.listdir(os.path.dirname(audio_path))}")
            return 1
        
        # Check if HackRF is available
        if not check_hackrf_available():
            print("ERROR: Unable to proceed without HackRF device")
            return 1
        
        print(f"Transmitting {audio_path} at {args.frequency} MHz")
        
        # Create and start FM transmitter
        tx = FMTransmitter(freq=freq_hz, audio_file=audio_path, gain=args.gain)
        tx.start_transmission()
        
        try:
            # Run until WAV file finishes playing
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
        print("Traceback:")
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main()) 
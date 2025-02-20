#!/usr/bin/env python3
"""
GQRX Remote Control Test Script
Tests basic frequency tuning functionality via TCP interface
"""

import socket
import time
import sys

class GqrxClient:
    def __init__(self, host='127.0.0.1', port=7356):
        self.host = host
        self.port = port
        self.socket = None

    def connect(self):
        """Connect to GQRX's remote control interface"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.connect((self.host, self.port))
            print(f"✅ Connected to GQRX at {self.host}:{self.port}")
            return True
        except ConnectionRefusedError:
            print("❌ Connection failed. Is GQRX running with remote control enabled?")
            print("   Enable in GQRX: Tools -> Remote Control")
            return False
        except Exception as e:
            print(f"❌ Error connecting: {str(e)}")
            return False

    def close(self):
        """Close the connection"""
        if self.socket:
            self.socket.close()
            self.socket = None

    def send_command(self, command):
        """Send a command to GQRX and get the response"""
        try:
            self.socket.send((command + '\n').encode())
            response = self.socket.recv(1024).decode().strip()
            return response
        except Exception as e:
            print(f"❌ Error sending command: {str(e)}")
            return None

    def set_frequency(self, freq_hz):
        """Set frequency in Hz"""
        response = self.send_command(f"F {freq_hz}")
        if response == "RPRT 0":
            print(f"✅ Frequency set to {freq_hz} Hz")
            return True
        else:
            print(f"❌ Error setting frequency: {response}")
            return False

    def get_frequency(self):
        """Get current frequency in Hz"""
        response = self.send_command("f")
        try:
            return int(response)
        except:
            print(f"❌ Error getting frequency: {response}")
            return None

def test_frequency_control():
    """Test basic frequency control functions"""
    client = GqrxClient()
    
    if not client.connect():
        return False

    print("\n🧪 Testing frequency control...")
    
    # Test frequencies (in Hz)
    test_frequencies = [
        100_000_000,  # 100 MHz
        145_000_000,  # 145 MHz
        435_000_000,  # 435 MHz
    ]

    for freq in test_frequencies:
        print(f"\n📡 Testing frequency: {freq} Hz")
        
        # Set frequency
        if not client.set_frequency(freq):
            continue

        # Read back frequency
        time.sleep(0.5)  # Give GQRX time to tune
        current_freq = client.get_frequency()
        
        if current_freq is not None:
            if abs(current_freq - freq) < 1:  # Allow for minor rounding
                print(f"✅ Frequency verification successful")
            else:
                print(f"❌ Frequency mismatch: Set {freq} Hz, Got {current_freq} Hz")
        
        time.sleep(1)  # Pause between tests

    client.close()
    return True

if __name__ == "__main__":
    print("🚀 GQRX Remote Control Test Script")
    print("Make sure GQRX is running with remote control enabled")
    print("(Tools -> Remote Control -> Enable)")
    
    if test_frequency_control():
        print("\n✨ Test script completed")
    else:
        print("\n❌ Test script failed to complete")
        sys.exit(1) 
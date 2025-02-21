#!/usr/bin/env python3
"""
Simple test script to check available remote control commands
"""

import telnetlib
import time

def main():
    try:
        # Connect to Aguila
        print("Connecting to Aguila remote control...")
        tn = telnetlib.Telnet("localhost", 7356, timeout=2)
        
        # Test basic commands
        print("\nTesting basic commands:")
        
        # Get frequency
        print("\nGetting frequency:")
        tn.write(b"f\n")
        print("Response:", tn.read_until(b"\n", timeout=1).decode().strip())
        
        # Get available read commands
        print("\nGetting available read commands:")
        tn.write(b"l ?\n")
        print("Response:", tn.read_until(b"\n", timeout=1).decode().strip())
        
        # Get available write commands
        print("\nGetting available write commands:")
        tn.write(b"L ?\n")
        print("Response:", tn.read_until(b"\n", timeout=1).decode().strip())
        
        # Get signal strength
        print("\nGetting signal strength:")
        tn.write(b"l STRENGTH\n")
        print("Response:", tn.read_until(b"\n", timeout=1).decode().strip())
        
        # Get squelch level
        print("\nGetting squelch level:")
        tn.write(b"l SQL\n")
        print("Response:", tn.read_until(b"\n", timeout=1).decode().strip())
        
        # Close connection
        tn.write(b"q\n")
        tn.close()
        print("\nTest complete")
        
    except Exception as e:
        print(f"Error: {str(e)}")

if __name__ == "__main__":
    main() 
#!/usr/bin/env python3
"""
Wrapper for the FM Transmitter script
This script uses the system Python (which has GNU Radio installed) to run the fm_transmitter.py script
"""

import os
import sys
import subprocess

def main():
    # Get the directory of this script
    script_dir = os.path.dirname(os.path.realpath(__file__))
    
    # Build the path to the fm_transmitter.py script
    fm_script_path = os.path.join(script_dir, "fm_transmitter.py")
    
    # Make sure the FM transmitter script exists
    if not os.path.exists(fm_script_path):
        print(f"ERROR: FM transmitter script not found at: {fm_script_path}")
        print(f"Current directory: {os.getcwd()}")
        print(f"Script directory: {script_dir}")
        return 1
    
    # Get all args passed to this script
    args = sys.argv[1:]
    
    # Command to run the script with system python
    cmd = ["/usr/bin/python3", fm_script_path] + args
    
    print(f"Running: {' '.join(cmd)}")
    
    # Execute the command and forward all input/output
    return subprocess.call(cmd)

if __name__ == "__main__":
    sys.exit(main()) 
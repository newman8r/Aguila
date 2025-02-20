#!/usr/bin/env python3
"""
Waterfall Display Optimizer for Aguila SDR
Implements a comprehensive heuristic for waterfall display settings based on:
- Signal strength and noise floor
- LNA and VGA gain settings
- Current frequency band
"""

import telnetlib
import logging
import argparse

# Configure logging
logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger(__name__)

# Known good default values if heuristic fails
DEFAULT_MIN_DB = -100
DEFAULT_MAX_DB = -40

# Heuristic constants
SIGNAL_BUFFER = 5  # dB above peak signal
HIGH_GAIN_THRESHOLD = 40  # Total gain threshold
HIGH_GAIN_ADJUSTMENT = -5  # dB adjustment for high gain
SNR_BUFFER = 10  # dB buffer above SNR for range
MIN_RANGE = 30  # Minimum range in dB
MAX_RANGE = 48  # Maximum range (HackRF dynamic range)
ABSOLUTE_MIN_DB = -110  # Absolute minimum reference
ABSOLUTE_MAX_DB = -10   # Absolute maximum reference

class TelnetConnection:
    """Wrapper for telnet connection to Aguila's remote control interface"""
    
    def __init__(self, host: str = "localhost", port: int = 7356):
        self.host = host
        self.port = port
        self.tn = None
        
    def __enter__(self):
        try:
            self.tn = telnetlib.Telnet(self.host, self.port, timeout=2)
            logger.debug(f"Connected to {self.host}:{self.port}")
            
            # Get available commands
            logger.debug("Checking available read commands...")
            self.tn.write(b"l ?\n")
            readable = self.tn.read_until(b"\n", timeout=1).decode().strip()
            logger.debug(f"Available read commands: {readable}")
            
            logger.debug("Checking available write commands...")
            self.tn.write(b"L ?\n")
            writable = self.tn.read_until(b"\n", timeout=1).decode().strip()
            logger.debug(f"Available write commands: {writable}")
            
            return self
        except Exception as e:
            logger.error(f"Failed to connect: {str(e)}")
            raise
            
    def __exit__(self, exc_type, exc_val, exc_tb):
        if self.tn:
            self.tn.close()
            logger.debug("Connection closed")
            
    def get(self, command: str) -> float:
        """Send GET command and return response as float"""
        try:
            cmd = f"l {command}\n"
            logger.debug(f"Sending GET command: {cmd.strip()}")
            self.tn.write(cmd.encode())
            response = self.tn.read_until(b"\n", timeout=1).decode().strip()
            logger.debug(f"Raw response: {response}")
            
            if response.startswith("RPRT"):
                raise ValueError(f"Command failed: {response}")
            return float(response)
        except Exception as e:
            logger.error(f"Error getting {command}: {str(e)}")
            raise
            
    def set(self, command: str, value: float):
        """Send SET command and verify response"""
        try:
            cmd = f"L {command} {value}\n"
            logger.debug(f"Sending SET command: {cmd.strip()}")
            self.tn.write(cmd.encode())
            response = self.tn.read_until(b"\n", timeout=1).decode().strip()
            logger.debug(f"Raw response: {response}")
            
            if response != "RPRT 0":
                raise ValueError(f"Command failed: {response}")
        except Exception as e:
            logger.error(f"Error setting {command}: {str(e)}")
            raise

def calculate_optimal_settings(current_settings: dict) -> dict:
    """Calculate optimal waterfall settings using comprehensive heuristic"""
    logger.info("Calculating optimal settings")
    logger.debug(f"Current settings: {current_settings}")
    
    # Get signal strength and gains
    peak_signal = current_settings.get('strength', -50)  # Conservative default
    lna_gain = current_settings.get('lna_gain', 20)
    vga_gain = current_settings.get('vga_gain', 20)
    total_gain = lna_gain + vga_gain
    
    # Calculate reference level (max_db)
    reference = peak_signal + SIGNAL_BUFFER
    
    # Adjust for high gain
    if total_gain > HIGH_GAIN_THRESHOLD:
        reference += HIGH_GAIN_ADJUSTMENT
        logger.debug(f"Adjusting reference by {HIGH_GAIN_ADJUSTMENT} dB for high gain")
    
    # Estimate noise floor and SNR
    noise_floor = peak_signal - 30  # Conservative estimate
    snr = peak_signal - noise_floor
    logger.debug(f"Estimated SNR: {snr} dB")
    
    # Calculate range
    range_db = min(MAX_RANGE, max(MIN_RANGE, snr + SNR_BUFFER))
    logger.debug(f"Calculated range: {range_db} dB")
    
    # Calculate min_db from reference and range
    min_db = reference - range_db
    max_db = reference
    
    # Final bounds check
    if max_db > ABSOLUTE_MAX_DB:
        max_db = ABSOLUTE_MAX_DB
        min_db = max_db - range_db
    if min_db < ABSOLUTE_MIN_DB:
        min_db = ABSOLUTE_MIN_DB
        max_db = min_db + range_db
        
    settings = {
        'max_db': max_db,
        'min_db': min_db
    }
    
    logger.info(f"Calculated settings: {settings}")
    return settings

def optimize_waterfall(min_db: float = None, max_db: float = None) -> dict:
    """Set waterfall range using either provided values or calculated optimal settings"""
    logger.info("Starting waterfall optimization")
    
    try:
        with TelnetConnection() as tc:
            if min_db is not None and max_db is not None:
                # Use provided values
                settings = {'min_db': min_db, 'max_db': max_db}
            else:
                # Get current settings for heuristic
                current_settings = {}
                try:
                    current_settings['strength'] = tc.get('STRENGTH')
                    current_settings['lna_gain'] = tc.get('LNA_GAIN')
                    current_settings['vga_gain'] = tc.get('VGA_GAIN')
                    logger.info(f"Signal: {current_settings['strength']} dBFS, "
                              f"LNA: {current_settings['lna_gain']} dB, "
                              f"VGA: {current_settings['vga_gain']} dB")
                except Exception as e:
                    logger.warning(f"Could not get all settings: {str(e)}")
                    # Use defaults
                    settings = {'min_db': DEFAULT_MIN_DB, 'max_db': DEFAULT_MAX_DB}
                else:
                    # Calculate optimal settings
                    settings = calculate_optimal_settings(current_settings)
            
            # Apply settings
            tc.set('WF_MIN_DB', settings['min_db'])
            logger.info(f"Set waterfall min dB to {settings['min_db']}")
            
            tc.set('WF_MAX_DB', settings['max_db'])
            logger.info(f"Set waterfall max dB to {settings['max_db']}")
            
            return settings
            
    except Exception as e:
        logger.error(f"Failed to set waterfall range: {str(e)}")
        raise

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Optimize waterfall display range')
    parser.add_argument('--min-db', type=float, help='Minimum dB value (optional)')
    parser.add_argument('--max-db', type=float, help='Maximum dB value (optional)')
    args = parser.parse_args()

    try:
        if args.min_db is not None and args.max_db is not None:
            if args.max_db <= args.min_db:
                raise ValueError("max-db must be greater than min-db")
            if args.max_db - args.min_db < MIN_RANGE:
                raise ValueError(f"Range must be at least {MIN_RANGE} dB")
            new_settings = optimize_waterfall(args.min_db, args.max_db)
        else:
            new_settings = optimize_waterfall()
            
        print("\nWaterfall range set!")
        print(f"Range: {new_settings['min_db']} to {new_settings['max_db']} dB")
    except Exception as e:
        print(f"Error: {str(e)}") 
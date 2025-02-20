#!/usr/bin/env python3
"""
Waterfall Display Optimizer for Aguila SDR - Basic Testing Version
Tests TCP control and stepping functionality and captures waterfall screenshots.
"""

import os
import time
import logging
from typing import Dict, Tuple
import telnetlib
from datetime import datetime

# Configure logging
logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger(__name__)

# Constants
DEFAULT_MIN_DB = -85
DEFAULT_MAX_DB = -55
STEPS = 7
STEP_DELAY_MS = 1000  # Increased to 1 second to allow waterfall to fill with data
DB_STEP = 7
DB_RANGE = 30
SCREENSHOT_DIR = os.path.expanduser("~/.config/gqrx/screenshots")  # Use standard Gqrx screenshot dir
TARGET_WIDTH = 400  # Target width in pixels for the waterfall screenshot

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

    def send_command(self, command: str) -> str:
        """Send a raw command and return the response"""
        try:
            cmd = f"{command}\n"
            logger.debug(f"Sending command: {cmd.strip()}")
            self.tn.write(cmd.encode())
            response = self.tn.read_until(b"\n", timeout=1).decode().strip()
            logger.debug(f"Raw response: {response}")
            return response
        except Exception as e:
            logger.error(f"Error sending command {command}: {str(e)}")
            raise

class WaterfallOptimizer:
    """Tests waterfall display stepping functionality and captures screenshots"""
    
    def __init__(self):
        self.steps = STEPS
        self.step_delay_ms = STEP_DELAY_MS
        self.db_step = DB_STEP
        self.db_range = DB_RANGE
        
    def calculate_db_range(self, step: int) -> Tuple[float, float]:
        """Calculate min/max dB values for given step"""
        base_min = -70  # Starting point
        min_db = base_min - (step * self.db_step)  # Step down by subtracting
        max_db = min_db + self.db_range
        return min_db, max_db

    def capture_screenshot(self, output_path: str):
        """Capture screenshot using Gqrx's remote control interface"""
        try:
            with TelnetConnection() as tc:
                # Send screenshot command
                response = tc.send_command("SCREENSHOT")
                if response != "RPRT 0":
                    raise ValueError(f"Screenshot command failed: {response}")
                # Give it more time to save
                time.sleep(2.0)  # Increased from 0.5s to 2s
                logger.info(f"Screenshot triggered via remote control")
                
                # Verify the file exists
                if not os.path.exists(output_path):
                    logger.warning(f"Screenshot file not found at {output_path}")
                    # Wait a bit longer and check again
                    time.sleep(2.0)
                    if not os.path.exists(output_path):
                        raise ValueError(f"Screenshot file not found at {output_path}")
                    
                logger.info(f"Screenshot saved to {output_path}")
        except Exception as e:
            logger.error(f"Screenshot capture failed: {str(e)}")
            raise
        
    def test_stepping(self) -> Dict[str, float]:
        """Test waterfall stepping functionality and capture screenshot"""
        logger.info("Starting waterfall display test - stepping down from -70 dB")
        
        try:
            with TelnetConnection() as tc:
                # Start screenshot capture
                timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                screenshot_path = os.path.join(SCREENSHOT_DIR, f"waterfall_test_{timestamp}.png")
                
                # Step through dB ranges
                for step in range(self.steps):
                    min_db, max_db = self.calculate_db_range(step)
                    logger.info(f"Step {step + 1}/{self.steps}: Setting range {min_db:0.1f} to {max_db:0.1f} dB")
                    print(f"Step {step + 1}/{self.steps}: Waterfall range {min_db:0.1f} to {max_db:0.1f} dB")
                    
                    # Set dB range
                    tc.set('WF_MIN_DB', min_db)
                    tc.set('WF_MAX_DB', max_db)
                    
                    # Wait for waterfall to update
                    time.sleep(self.step_delay_ms / 1000.0)
                
                # Take screenshot after final step and delay
                self.capture_screenshot(screenshot_path)
                
                # Return to default range
                logger.info(f"Test complete, returning to default range ({DEFAULT_MIN_DB} to {DEFAULT_MAX_DB} dB)")
                tc.set('WF_MIN_DB', DEFAULT_MIN_DB)
                tc.set('WF_MAX_DB', DEFAULT_MAX_DB)
                
                return {
                    'min_db': DEFAULT_MIN_DB,
                    'max_db': DEFAULT_MAX_DB,
                    'screenshot': screenshot_path
                }
                
        except Exception as e:
            logger.error(f"Test failed: {str(e)}")
            # Return to safe defaults
            with TelnetConnection() as tc:
                tc.set('WF_MIN_DB', DEFAULT_MIN_DB)
                tc.set('WF_MAX_DB', DEFAULT_MAX_DB)
            return {
                'min_db': DEFAULT_MIN_DB,
                'max_db': DEFAULT_MAX_DB,
                'screenshot': None
            }

if __name__ == "__main__":
    # Create screenshots directory if it doesn't exist
    os.makedirs(SCREENSHOT_DIR, exist_ok=True)
    
    optimizer = WaterfallOptimizer()
    result = optimizer.test_stepping()
    print(f"\nTest complete!")
    print(f"Final range: {result['min_db']} to {result['max_db']} dB")
    if result['screenshot']:
        print(f"Screenshot saved to: {result['screenshot']}") 
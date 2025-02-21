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
import sys
import base64
import requests
import json
from dotenv import load_dotenv

# Configure logging first!
logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s][%(levelname)s]: %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)

# Get absolute path to .env file
script_dir = os.path.dirname(os.path.abspath(__file__))
env_path = os.path.join(os.path.dirname(script_dir), '.env')
logger.info(f"Looking for .env file at: {env_path}")
logger.info(f"Current working directory: {os.getcwd()}")

if not os.path.exists(env_path):
    logger.error(f"❌ .env file not found at {env_path}")
    sys.exit(1)

load_dotenv(env_path)

# Verify env vars were loaded
logger.info(f"ANTHROPIC_API_KEY loaded: {'ANTHROPIC_API_KEY' in os.environ}")
logger.info(f"AI_MODEL loaded: {os.getenv('AI_MODEL')}")

# Update logging level based on env
logging.getLogger().setLevel(
    logging.DEBUG if os.getenv('DEBUG_MODE', 'false').lower() == 'true' else logging.INFO
)

# Constants
DEFAULT_MIN_DB = -85
DEFAULT_MAX_DB = -55
STEPS = 7
STEP_DELAY_MS = 1000  # Increased to 1 second to allow waterfall to fill with data
DB_STEP = 7
DB_RANGE = 30
SCREENSHOT_DIR = os.path.expanduser("~/.config/gqrx/screenshots")  # Use standard Gqrx screenshot dir
TARGET_WIDTH = 400  # Target width in pixels for the waterfall screenshot
SCREENSHOT_TIMEOUT = 5.0  # Maximum time to wait for screenshot
MIN_SCREENSHOT_SIZE = 1024  # Minimum valid screenshot size in bytes

# Claude API settings
ANTHROPIC_API_URL = "https://api.anthropic.com/v1/messages"
ANTHROPIC_API_VERSION = "2023-06-01"  # Version used in docksigint.cpp
MAX_RETRIES = 3
RETRY_DELAY = 1.0

# Get model from .env
AI_MODEL = os.getenv('AI_MODEL', 'claude-3-opus-20240229')

CLAUDE_PROMPT = """You are an expert at analyzing SDR waterfall displays. I am showing you a waterfall display screenshot that contains 7 distinct horizontal slices of equal height, numbered from 1 to 7 starting from the top. Each slice represents a different dB range, with each subsequent slice stepping down by 7 dB from a starting point of -70 dB.

The slices are arranged as follows:
Slice 1 (top): -70 dB to -40 dB
Slice 2: -77 dB to -47 dB
Slice 3: -84 dB to -54 dB
Slice 4: -91 dB to -61 dB
Slice 5: -98 dB to -68 dB
Slice 6: -105 dB to -75 dB
Slice 7 (bottom): -112 dB to -82 dB

Please analyze the image and:
1. Identify which slice shows the clearest signal representation with the best contrast between signal and noise
2. Note if any slices appear too saturated (too bright) or too dark to be useful
3. Recommend which slice's dB range would be optimal for ongoing signal analysis
4. Explain your reasoning for the recommendation

Note that the bottom slices may appear very similar due to being below the noise floor, but they are still distinct ranges. Focus on finding the slice that shows the best signal-to-noise ratio while maintaining good visibility of signal details.

Your response should be in this format:
OPTIMAL_SLICE: [number 1-7]
ANALYSIS: [your detailed analysis]
RECOMMENDATION: [specific dB range recommendation based on the optimal slice]"""

def verify_screenshot(path: str, timeout: float = SCREENSHOT_TIMEOUT, min_size: int = MIN_SCREENSHOT_SIZE) -> bool:
    """Verify that screenshot exists and is valid by checking for new .png files"""
    start_time = time.time()
    screenshot_dir = os.path.dirname(path)
    
    # Get list of existing files before screenshot
    files_before = set(f for f in os.listdir(screenshot_dir) if f.endswith('.png'))
    
    while time.time() - start_time < timeout:
        # Get current files
        files_now = set(f for f in os.listdir(screenshot_dir) if f.endswith('.png'))
        
        # Look for new files
        new_files = files_now - files_before
        if new_files:
            newest_file = max(new_files, key=lambda f: os.path.getmtime(os.path.join(screenshot_dir, f)))
            newest_path = os.path.join(screenshot_dir, newest_file)
            
            if os.path.getsize(newest_path) >= min_size:
                logger.info(f"Found new screenshot: {newest_path}")
                # Update the path to the actual file
                global actual_screenshot_path
                actual_screenshot_path = newest_path
                return True
                
            logger.warning(f"⚠️ Screenshot file exists but may be corrupted (size: {os.path.getsize(newest_path)} bytes)")
        
        time.sleep(0.5)
    return False

def retry_with_backoff(func, max_retries: int = MAX_RETRIES, initial_delay: float = RETRY_DELAY):
    """Retry a function with exponential backoff"""
    for attempt in range(max_retries):
        try:
            return func()
        except Exception as e:
            if attempt == max_retries - 1:
                raise
            delay = initial_delay * (2 ** attempt)
            logger.warning(f"⚠️ Attempt {attempt + 1} failed, retrying in {delay:.1f}s: {str(e)}")
            time.sleep(delay)

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

def capture_waterfall_screenshot(tc: TelnetConnection, output_path: str, timeout: float = SCREENSHOT_TIMEOUT) -> str:
    """Capture waterfall screenshot using specific filename"""
    logger.info("📸 Capturing waterfall screenshot...")
    
    # Get current frequency using proper command format
    try:
        # Send frequency query and parse response
        response = tc.send_command("f")  # Just 'f' for frequency query
        if response.startswith("RPRT"):
            raise ValueError(f"Frequency query failed: {response}")
        freq_hz = float(response)
        freq_mhz = freq_hz / 1e6
        
        timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        filename = f"waterfall_{timestamp}_{freq_mhz:.3f}MHz.png"
        filepath = os.path.join(SCREENSHOT_DIR, filename)
        
        # Send screenshot command
        response = tc.send_command("SCREENSHOT")
        if response != "RPRT 0":
            raise ValueError(f"Screenshot command failed: {response}")
        
        # Wait for file to appear
        start_time = time.time()
        while time.time() - start_time < timeout:
            if os.path.exists(filepath):
                if os.path.getsize(filepath) >= MIN_SCREENSHOT_SIZE:
                    logger.info(f"✅ Waterfall screenshot saved to: {filepath}")
                    return filepath
            time.sleep(0.1)
        
        raise ValueError(f"❌ Failed to capture waterfall screenshot at {filepath}")
    except Exception as e:
        logger.error(f"❌ Screenshot capture error: {str(e)}")
        # Fallback to timestamp-only filename if frequency query fails
        timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        filename = f"waterfall_{timestamp}.png"
        filepath = os.path.join(SCREENSHOT_DIR, filename)
        
        # Still try to capture screenshot
        response = tc.send_command("SCREENSHOT")
        if response != "RPRT 0":
            raise ValueError(f"Screenshot command failed: {response}")
            
        # Wait for file with fallback name
        start_time = time.time()
        while time.time() - start_time < timeout:
            if os.path.exists(filepath):
                if os.path.getsize(filepath) >= MIN_SCREENSHOT_SIZE:
                    logger.info(f"✅ Waterfall screenshot saved to: {filepath}")
                    return filepath
            time.sleep(0.1)
        
        raise ValueError(f"❌ Failed to capture waterfall screenshot at {filepath}")

class WaterfallOptimizer:
    """Tests waterfall display stepping functionality and captures screenshots"""
    
    def __init__(self):
        self.steps = STEPS
        self.step_delay_ms = STEP_DELAY_MS
        self.db_step = DB_STEP
        self.db_range = DB_RANGE
        
        # Check for API key
        self.api_key = os.getenv('ANTHROPIC_API_KEY')
        if not self.api_key:
            logger.error("❌ ANTHROPIC_API_KEY environment variable not set")
            logger.error("Please set it with: export ANTHROPIC_API_KEY=your-key-here")
            sys.exit(1)

    def calculate_db_range(self, step: int) -> Tuple[float, float]:
        """Calculate min/max dB values for given step"""
        base_min = -70  # Starting point
        min_db = base_min - (step * self.db_step)  # Step down by subtracting
        max_db = min_db + self.db_range
        return min_db, max_db

    def capture_screenshot(self, output_path: str) -> str:
        """Capture screenshot using Gqrx's remote control interface"""
        try:
            with TelnetConnection() as tc:
                logger.info("📸 Capturing screenshot...")
                
                # Send screenshot command
                response = tc.send_command("SCREENSHOT")
                if response != "RPRT 0":
                    raise ValueError(f"Screenshot command failed: {response}")
                
                # Verify screenshot was created and is valid
                if not verify_screenshot(output_path):
                    raise ValueError("❌ Failed to capture valid screenshot")
                    
                # Return the actual path where Gqrx saved the file
                return actual_screenshot_path
                
        except Exception as e:
            logger.error(f"❌ Screenshot capture failed: {str(e)}")
            raise

    def analyze_screenshot(self, screenshot_path: str) -> Dict:
        """Send screenshot to Claude for analysis"""
        try:
            logger.info("🔍 Starting screenshot analysis...")
            
            if not os.path.exists(screenshot_path):
                raise ValueError(f"Screenshot file not found: {screenshot_path}")
            
            file_size = os.path.getsize(screenshot_path)
            if file_size < MIN_SCREENSHOT_SIZE:
                raise ValueError(f"Screenshot file too small ({file_size} bytes)")
            
            logger.info(f"📄 Reading screenshot from: {screenshot_path}")
            
            # Read the image file and convert to base64
            with open(screenshot_path, 'rb') as f:
                image_data = f.read()
            image_b64 = base64.b64encode(image_data).decode('utf-8')
            logger.info("✅ Image converted to base64")

            # Prepare the API request with correct version header
            headers = {
                'Content-Type': 'application/json',
                'x-api-key': self.api_key,
                'anthropic-version': ANTHROPIC_API_VERSION
            }

            data = {
                "model": AI_MODEL,
                "max_tokens": 1024,
                "messages": [{
                    "role": "user",
                    "content": [
                        {
                            "type": "text",
                            "text": CLAUDE_PROMPT
                        },
                        {
                            "type": "image",
                            "source": {
                                "type": "base64",
                                "media_type": "image/png",
                                "data": image_b64
                            }
                        }
                    ]
                }]
            }

            # Make the API request with retries
            def make_request():
                logger.info("🌐 Sending request to Claude API...")
                response = requests.post(ANTHROPIC_API_URL, headers=headers, json=data)
                
                if response.status_code != 200:
                    logger.error(f"❌ API request failed with status {response.status_code}")
                    logger.error(f"Response: {response.text}")
                    raise ValueError(f"API request failed: {response.text}")
                
                return response.json()

            # Use retry wrapper for API call
            result = retry_with_backoff(make_request)
            logger.info("✅ Received response from Claude API")

            try:
                # Parse the response - handle potential structure changes
                analysis = result.get('content', [{}])[0].get('text', '')
                if not analysis:
                    raise ValueError("Empty analysis received from API")
                
                logger.info("\n=== Claude's Analysis ===\n" + analysis + "\n==================")

                # Extract the optimal slice number
                optimal_slice = None
                for line in analysis.split('\n'):
                    if line.startswith('OPTIMAL_SLICE:'):
                        try:
                            slice_num = int(line.split(':')[1].strip())
                            if 1 <= slice_num <= 7:
                                optimal_slice = slice_num
                            else:
                                raise ValueError(f"Invalid slice number: {slice_num}")
                        except (ValueError, IndexError) as e:
                            raise ValueError(f"Failed to parse optimal slice: {str(e)}")

                if optimal_slice is None:
                    raise ValueError("Could not determine optimal slice from analysis")

                logger.info(f"✅ Optimal slice identified: {optimal_slice}")

                # Calculate the optimal dB range based on the slice number
                min_db, max_db = self.calculate_db_range(optimal_slice - 1)  # -1 because slices are 1-indexed
                logger.info(f"📊 Calculated optimal dB range: {min_db} to {max_db} dB")

                return {
                    'optimal_slice': optimal_slice,
                    'min_db': min_db,
                    'max_db': max_db,
                    'analysis': analysis
                }

            except Exception as e:
                logger.error(f"❌ Failed to parse API response: {str(e)}")
                logger.debug(f"Raw API response: {json.dumps(result, indent=2)}")
                raise

        except Exception as e:
            logger.error(f"❌ Screenshot analysis failed: {str(e)}")
            raise
        
    def test_stepping(self) -> Dict[str, float]:
        """Test waterfall stepping functionality and capture screenshot"""
        logger.info("\n=== Starting Waterfall Display Test ===")
        logger.info(f"📊 Starting from -70 dB, stepping down {self.db_step} dB {self.steps} times")
        
        try:
            with TelnetConnection() as tc:
                # Step through dB ranges
                for step in range(self.steps):
                    min_db, max_db = self.calculate_db_range(step)
                    logger.info(f"📈 Step {step + 1}/{self.steps}: Setting range {min_db:0.1f} to {max_db:0.1f} dB")
                    
                    # Set dB range
                    tc.set('WF_MIN_DB', min_db)
                    tc.set('WF_MAX_DB', max_db)
                    
                    # Wait for waterfall to update
                    time.sleep(self.step_delay_ms / 1000.0)
                
                # Take screenshot after final step and delay
                try:
                    screenshot_path = capture_waterfall_screenshot(tc, SCREENSHOT_DIR)
                    
                    # Analyze the screenshot
                    analysis = self.analyze_screenshot(screenshot_path)
                    logger.info("\n=== Analysis Results ===")
                    logger.info(f"✨ Optimal slice: {analysis['optimal_slice']}")
                    logger.info(f"📊 Recommended range: {analysis['min_db']} to {analysis['max_db']} dB")

                    # Set the optimal range
                    logger.info("\n=== Applying Optimal Settings ===")
                    tc.set('WF_MIN_DB', analysis['min_db'])
                    tc.set('WF_MAX_DB', analysis['max_db'])
                    logger.info("✅ Optimal settings applied")
                    
                    return {
                        'min_db': analysis['min_db'],
                        'max_db': analysis['max_db'],
                        'screenshot': screenshot_path,
                        'analysis': analysis['analysis']
                    }

                except Exception as e:
                    logger.error(f"❌ Screenshot analysis failed, using default range: {str(e)}")
                    # Return to default range
                    tc.set('WF_MIN_DB', DEFAULT_MIN_DB)
                    tc.set('WF_MAX_DB', DEFAULT_MAX_DB)
                    return {
                        'min_db': DEFAULT_MIN_DB,
                        'max_db': DEFAULT_MAX_DB,
                        'screenshot': screenshot_path if 'screenshot_path' in locals() else None,
                        'analysis': str(e)
                    }
                
        except Exception as e:
            logger.error(f"❌ Test failed: {str(e)}")
            # Return to safe defaults
            with TelnetConnection() as tc:
                tc.set('WF_MIN_DB', DEFAULT_MIN_DB)
                tc.set('WF_MAX_DB', DEFAULT_MAX_DB)
            return {
                'min_db': DEFAULT_MIN_DB,
                'max_db': DEFAULT_MAX_DB,
                'screenshot': None,
                'analysis': str(e)
            }

if __name__ == "__main__":
    # Create screenshots directory if it doesn't exist
    os.makedirs(SCREENSHOT_DIR, exist_ok=True)
    
    optimizer = WaterfallOptimizer()
    result = optimizer.test_stepping()
    
    print("\n=== Test Results ===")
    print(f"📊 Final range: {result['min_db']} to {result['max_db']} dB")
    if result['screenshot']:
        print(f"📸 Screenshot saved to: {result['screenshot']}")
    if result.get('analysis'):
        print(f"\n🔍 Analysis:\n{result['analysis']}") 
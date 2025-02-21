# Simple Waterfall Display Optimizer

**Status: SELECTED FOR IMPLEMENTATION**

## Overview
A simplified tool for optimizing waterfall display settings via TCP remote control. This tool will be available as an agent command to quickly adjust waterfall dB settings for better signal visibility. Using a direct heuristic approach rather than LLM-based analysis for reliable, consistent results.

## Core Concept
Instead of continuous monitoring or complex signal analysis, we'll create a simple tool that:
1. Gets current settings via TCP
2. Applies Grok's heuristic in a single pass
3. Sets new optimized values

## Implementation

### Step 1: TCP Connection Wrapper
```python
class TelnetConnection:
    def __init__(self, host="localhost", port=7356):
        self.host = host
        self.port = port
        self.tn = None

    def __enter__(self):
        self.tn = telnetlib.Telnet(self.host, self.port)
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if self.tn:
            self.tn.close()

    def get(self, command: str) -> float:
        self.tn.write(f"GET {command}\n".encode())
        response = self.tn.read_until(b"\n").decode().strip()
        return float(response)

    def set(self, command: str, value: float):
        self.tn.write(f"SET {command} {value}\n".encode())
        self.tn.read_until(b"RPRT 0\n")  # Wait for confirmation
```

### Step 2: Optimizer Implementation
```python
def optimize_waterfall():
    # 1. Connect and get current settings
    with TelnetConnection() as tc:
        current_settings = {
            'reference': tc.get('fft_dbreference'),
            'range': tc.get('fft_dbrange'),
            'fft_size': tc.get('fft_size'),
            'lna_gain': tc.get('lna_gain'),
            'vga_gain': tc.get('vga_gain')
        }
        
        # 2. Calculate new settings using Grok's heuristic
        new_settings = calculate_optimal_settings(current_settings)
            
        # 3. Apply new settings
        tc.set('fft_dbreference', new_settings['reference'])
        tc.set('fft_dbrange', new_settings['range'])
        
        return new_settings

def calculate_optimal_settings(current: dict) -> dict:
    # Start with recommended base values
    settings = {
        'reference': -5,  # Start at -5 dBFS
        'range': 48      # Use max range for HackRF
    }
    
    # Gain compensation
    total_gain = current['lna_gain'] + current['vga_gain']
    if total_gain > 50:  # High total gain
        settings['reference'] -= 10
    elif total_gain < 30:  # Low total gain
        settings['reference'] += 5
        
    # FFT size compensation
    if current['fft_size'] > 8192:
        settings['reference'] -= 5
        
    return settings
```

### Step 3: Agent Tool Integration
```python
@tool
def optimize_waterfall_display(self) -> str:
    """
    Optimize the waterfall display settings for better signal visibility.
    Uses Grok's heuristic to set optimal dB reference and range values.
    """
    try:
        new_settings = optimize_waterfall()
        return (f"Waterfall display optimized:\n"
                f"Reference: {new_settings['reference']} dBFS\n"
                f"Range: {new_settings['range']} dB")
    except Exception as e:
        return f"Error optimizing waterfall: {str(e)}"
```

## Implementation Timeline
1. **TCP Wrapper** (15 min)
   - Implement TelnetConnection class
   - Test basic GET/SET commands
   - Add error handling

2. **Optimizer** (15 min)
   - Implement calculate_optimal_settings
   - Test with different gain/FFT scenarios
   - Add safety bounds for values

3. **Tool Integration** (10 min)
   - Add tool decorator and documentation
   - Test tool invocation
   - Add error reporting

4. **Testing** (20 min)
   - Test with SDR connected
   - Verify settings changes
   - Check error handling

## Success Metrics
1. Settings are applied correctly via TCP
2. Waterfall display shows improved visibility
3. Tool returns clear success/failure messages
4. No interference with manual controls
5. Completes optimization in < 1 second

## Next Steps After Implementation
1. Add basic logging of before/after settings
2. Gather user feedback on effectiveness
3. Consider adding different optimization profiles
4. Document common usage scenarios

This is our selected approach for immediate implementation, chosen for its simplicity, reliability, and quick development time. 
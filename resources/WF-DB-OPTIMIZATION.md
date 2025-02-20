# Waterfall dB Level Optimization

## Overview
This document outlines the implementation plan for automatic optimization of waterfall dB levels in Aguila SDR. The goal is to maximize signal visibility while minimizing noise, using a heuristic approach based on signal processing principles.

## Core Requirements

### Functional Requirements
1. Automatically calculate optimal WF dB settings based on signal analysis
2. Preserve existing manual control functionality
3. Implement via TCP remote control interface
4. Support real-time adjustments based on signal conditions
5. Handle edge cases gracefully (no signal, very strong signals, etc.)

### Technical Constraints
- HackRF One 8-bit ADC (~48 dB dynamic range)
- Existing TCP control interface (port 7356)
- Must not interfere with current UI functionality
- Need to preserve user's ability to override automatic settings

## Implementation Phases

### Phase 1: Signal Analysis Infrastructure
1. Create SignalAnalyzer class
   ```python
   class SignalAnalyzer:
       def __init__(self):
           self.peak_signal = None
           self.noise_floor = None
           self.snr = None
           
       def analyze_fft_data(self, fft_data):
           # Analyze FFT data to determine signal characteristics
           pass
   ```

2. Implement core analysis methods:
   - Peak signal power detection
   - Noise floor estimation
   - SNR calculation
   - FFT data parsing from TCP interface

### Phase 2: WF dB Calculator
1. Create WaterfallOptimizer class
   ```python
   class WaterfallOptimizer:
       def __init__(self):
           self.analyzer = SignalAnalyzer()
           self.current_reference = None
           self.current_range = None
           
       def calculate_optimal_settings(self):
           # Apply heuristic to determine optimal settings
           pass
   ```

2. Implement heuristic calculation:
   - Reference = Peak Signal Power + 5 dB
   - Range = min(SNR + 10 dB, 48 dB)
   - Gain compensation logic
   - FFT size adjustment logic

### Phase 3: TCP Control Integration
1. Create RemoteController class
   ```python
   class RemoteController:
       def __init__(self, host="localhost", port=7356):
           self.host = host
           self.port = port
           
       def set_wf_settings(self, reference, range):
           # Apply settings via TCP
           pass
   ```

2. Implement TCP commands:
   - Connection management
   - Command formatting
   - Response parsing
   - Error handling

### Phase 4: Integration and Testing
1. Create WaterfallManager class to coordinate components
2. Implement test suite with various signal scenarios
3. Add logging and monitoring
4. Create configuration system for tuning parameters

## Testing Strategy

### Unit Tests
1. Signal analysis accuracy
2. Heuristic calculation correctness
3. TCP command formatting
4. Error handling

### Integration Tests
1. End-to-end optimization flow
2. Multiple signal scenarios
3. Edge cases
4. Performance testing

### Manual Verification
1. Visual inspection of waterfall display
2. Comparison with manual settings
3. Real-world signal testing

## Success Criteria
1. Improved signal visibility compared to default settings
2. Responsive to changing signal conditions
3. No interference with existing functionality
4. Reliable operation across different signal types
5. Graceful handling of edge cases

## Future Enhancements
1. UI integration for enabling/disabling automatic optimization
2. Custom optimization profiles for different signal types
3. Machine learning-based parameter tuning
4. Integration with gain control optimization

## Implementation Notes
- Start with basic TCP control implementation
- Add analysis capabilities incrementally
- Test thoroughly before adding UI elements
- Document all magic numbers and constants
- Keep optimization logic separate from UI code
- Use configuration files for tunable parameters

## Initial Development Focus
1. Basic TCP control implementation
2. Simple signal analysis
3. Core heuristic calculation
4. Basic testing framework

This approach allows for incremental development and testing while maintaining existing functionality. 
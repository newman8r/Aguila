# FSK Transmitter for HackRF

## Overview

This document explains the Frequency Shift Keying (FSK) transmitter implementation in `simple_fsk.py`. The transmitter uses GNU Radio and the HackRF One SDR to generate and transmit FSK modulated signals that can be decoded using software like URH (Universal Radio Hacker).

## What is FSK?

Frequency Shift Keying (FSK) is a digital modulation technique where binary data is represented by variations in the frequency of a carrier wave:

- A binary "0" is represented by shifting the frequency downward (by the frequency deviation)
- A binary "1" is represented by shifting the frequency upward (by the frequency deviation)

FSK is widely used in applications like:
- RFID systems
- Wireless keyboards/mice
- Garage door openers
- Some digital radio protocols

## How Our Implementation Works

### Architecture

The FSK transmitter consists of several components:

1. **Data Generation**: Converts text messages or patterns to binary bits
2. **FSK Modulation**: Maps bits to frequency shifts
3. **Signal Generation**: Creates complex samples with appropriate phase
4. **Transmission**: Sends the signal through the HackRF SDR

### Key Parameters

| Parameter | Description | Default | 
|-----------|-------------|---------|
| Frequency | Center frequency of transmission | 433 MHz |
| Sample Rate | Number of samples per second | 2,000,000 Hz |
| Baud Rate | Number of symbols per second | 50 baud |
| Frequency Deviation | How far the frequency shifts for 0/1 | 20 kHz |
| Gain | Transmission power | 40 dB |

The `samples_per_symbol` is automatically calculated as:
```
samples_per_symbol = sample_rate / baud_rate
```

### Signal Construction

1. **Binary Conversion**: If a message is provided, it's converted to binary with each character represented by 8 bits
2. **Framing**: A preamble (10101010...) is added before the message and a postamble after to help with synchronization
3. **Frequency Mapping**: Each bit maps to a frequency offset of +/- `frequency_deviation`
4. **Phase Continuity**: The phase is calculated continuously across symbol boundaries to avoid clicks/pops
5. **Complex Signal Generation**: Each symbol is represented by complex samples using `e^(j2πft + φ)`

### Continuous Phase FSK

We use a continuous phase implementation where:
- The phase increment per sample is calculated as `2π * freq * (1/sample_rate)`
- The current phase is accumulated across symbol boundaries
- This creates a smooth, continuous phase signal that reduces spectral splatter

## Usage

### Basic Usage

```bash
python3 resources/simple_fsk.py -f 434.6 -m "Hello World!"
```

### Full Parameter Control

```bash
python3 resources/simple_fsk.py \
    -f 434.6 \           # Frequency in MHz
    -b 500 \             # Baud rate (symbols/sec)
    -d 100000 \          # Frequency deviation in Hz
    -g 40 \              # Gain in dB
    -s 1000000 \         # Sample rate in Hz
    -c -69 \             # Frequency correction in ppm
    -m "Your message"    # Message to transmit
```

### Receiving in URH

To receive and decode the signal in URH:

1. Set the center frequency to match the transmitter
2. Set modulation to "FSK"
3. Set bits per symbol to "1" (for binary FSK)
4. Set samples/symbol to match `sample_rate/baud_rate`
5. In the interpretation view, set format to "ASCII" to see the text message

## Parameter Relationships

The relationship between parameters affects how the signal appears and how easily it can be decoded:

| Parameter Combination | Effect |
|-----------------------|--------|
| High deviation, low baud | Very clear signal, easy to decode |
| High deviation, high baud | Clear transitions but potentially wider bandwidth |
| Low deviation, low baud | Narrow bandwidth but may be harder to distinguish symbols |
| Low deviation, high baud | Challenging to decode, symbols may blur together |

## Samples per Symbol

The `samples_per_symbol` value is critical for decoding in URH. Here are some typical values:

| Sample Rate | Baud Rate | Samples/Symbol |
|-------------|-----------|----------------|
| 2,000,000 Hz | 50 | 40,000 |
| 1,000,000 Hz | 50 | 20,000 |
| 1,000,000 Hz | 200 | 5,000 |
| 1,000,000 Hz | 500 | 2,000 |
| 1,000,000 Hz | 1,000 | 1,000 |
| 1,000,000 Hz | 2,000 | 500 |

## Troubleshooting

If the signal isn't decoding properly:

1. **Ensure URH samples/symbol matches** - This is the most common issue
2. **Adjust the frequency deviation** - Larger deviations are easier to see
3. **Reduce the baud rate** - Slower transmissions are more reliable
4. **Check for interference** - Try different frequencies if there's noise
5. **Adjust the center frequency** - Make sure URH is tuned exactly to the transmission

## Code Structure

The transmitter is implemented as a GNU Radio flowgraph with these components:

```
┌────────────┐   ┌────────────┐   ┌────────────┐
│  Vector    │   │            │   │  HackRF    │
│  Source    ├──►│ (FSK Data) ├──►│  Sink      │
└────────────┘   └────────────┘   └────────────┘
```

Key methods in the implementation:

- `text_to_bits`: Converts text to binary
- `create_fsk_modulation`: Generates the FSK modulated signal
- `start_transmission`: Begins the GNU Radio flowgraph
- `stop_transmission`: Cleanly stops transmission
- `check_hackrf_available`: Verifies the HackRF is connected

## Example Applications

This FSK transmitter can be used for:

1. **Testing SDR setups** - Verify your SDR receiving capability
2. **Learning about digital modulation** - Visualize how FSK works
3. **Simple data transmission** - Send messages wirelessly
4. **Protocol development** - Base for building custom RF protocols

## Further Development

Potential improvements include:

1. **Error correction codes** - Add FEC for more reliable transmission
2. **Packet formatting** - Add headers, length fields, CRC checks
3. **Multiple FSK levels** - Implement M-FSK for higher data rates
4. **Adaptive parameters** - Automatically adjust parameters based on conditions

## References

- [GNU Radio Documentation](https://wiki.gnuradio.org/)
- [HackRF Documentation](https://github.com/greatscottgadgets/hackrf/wiki)
- [Universal Radio Hacker](https://github.com/jopohl/urh) 
Aguila Signal Analysis Platform
============================

[![CI](https://github.com/gqrx-sdr/gqrx/workflows/CI/badge.svg)](https://github.com/gqrx-sdr/gqrx/actions?query=workflow%3ACI+branch%3Amaster)
[![Build](https://github.com/gqrx-sdr/gqrx/workflows/Build/badge.svg)](https://github.com/gqrx-sdr/gqrx/actions?query=workflow%3ABuild+branch%3Amaster)

Aguila is an advanced signals analysis platform built by Steve Newman (AJ6KS) on top of Gqrx, combining traditional SDR capabilities with AI-powered signal analysis. It extends the core Gqrx functionality with intelligent waterfall optimization, automated signal classification, and an interactive AI assistant for signal analysis.

## Key Features

### AI-Powered Signal Analysis
- Intelligent waterfall display optimization using Claude AI
- Automated signal classification and modulation recognition
- Interactive AI assistant for signal analysis and SDR operation
- Real-time FFT optimization for optimal signal visibility

### Core SDR Capabilities
- Support for AM, FM, SSB, and raw I/Q modes
- Advanced FFT display and waterfall visualization
- Hardware support for RTL-SDR, HackRF, Airspy, BladeRF, and more
- Network control interface for external applications

## Requirements

### Core Dependencies
- GNU Radio 3.8, 3.9, or 3.10 with standard components
- Qt 5 or Qt 6 with Core, GUI, Network, Widgets, and Svg
- gr-osmosdr and device-specific drivers
- CMake >= 3.2.0

### Python Dependencies
```bash
pip install python-dotenv requests anthropic numpy scipy matplotlib
```

### Environment Setup
1. Create a `.env` file in the Aguila root directory:
```
ANTHROPIC_API_KEY=your_api_key_here
AI_MODEL=claude-3-opus-20240229
DEBUG_MODE=false
```

2. Install device-specific drivers:
- RTL-SDR: `rtl-sdr` package
- HackRF: `hackrf` package
- Airspy: `airspy` package
- Other devices as needed

## Installation

### From Source
```bash
git clone https://github.com/yourusername/aguila.git
cd aguila
mkdir build
cd build
cmake ..
make
```

### Device Management

#### Switching Between SDR Devices
To prevent configuration conflicts when switching devices:

1. Close Aguila completely
2. Run the device switch script:
```bash
# For HackRF:
./resources/sdr_switch.sh hackrf

# For RTL-SDR:
./resources/sdr_switch.sh rtlsdr
```
3. Launch Aguila

## Usage

### Basic Operation
1. Start Aguila
2. Configure your SDR device in the I/O Control panel
3. Click the power button to start DSP
4. Use the frequency display to tune to your signal of interest

### AI Features
- **AI Signal Analysis**: Click the "AI Signal Analysis" button or press Ctrl+P to capture and analyze the current signal
- **FFT Optimization**: Use "AI FFT Optimize" to automatically adjust waterfall display settings
- **Signal Assistant**: Use the chat interface to ask questions about signals or request analysis

### Keyboard Shortcuts
- Ctrl+P: Capture and analyze current signal
- Ctrl+G: Test spectrum capture
- F11: Toggle fullscreen
- Other standard Gqrx shortcuts remain unchanged

## Credits and License

Aguila is built on top of Gqrx, which is designed and written by Alexandru Csete OZ9AEC and licensed under the GNU General Public License. The AI features and additional functionality are developed by the Aguila team.

For a complete list of contributors and detailed license information, see the original Gqrx credits below.

[Original Gqrx credits and license information follows...]

Gqrx
====

[![CI](https://github.com/gqrx-sdr/gqrx/workflows/CI/badge.svg)](https://github.com/gqrx-sdr/gqrx/actions?query=workflow%3ACI+branch%3Amaster)
[![Build](https://github.com/gqrx-sdr/gqrx/workflows/Build/badge.svg)](https://github.com/gqrx-sdr/gqrx/actions?query=workflow%3ABuild+branch%3Amaster)

Aguila is a signals analysis platform build on top of Gqrx: open source software defined radio (SDR) receiver implemented using
[GNU Radio](https://gnuradio.org) and the [Qt GUI toolkit](https://www.qt.io/).
Currently it works on Linux and Mac with hardware supported by gr-osmosdr,
including Funcube Dongle, RTL-SDR, Airspy, HackRF, BladeRF, RFSpace, USRP and
SoapySDR.

Gqrx can operate as an AM/FM/SSB receiver with audio output or as an FFT-only
instrument. There are also various hooks for interacting with external
applications using network sockets.

![Screenshot of the main Gqrx window](resources/screenshots/gqrx-main.png)


Download
--------

Gqrx is distributed as a source code package and binaries for Linux and Mac.
Many Linux distributions provide gqrx in their package repositories.
Alternate Mac support is available through [MacPorts](https://ports.macports.org/port/gqrx/summary) and [Homebrew](https://formulae.brew.sh/cask/gqrx).
Windows support is available through [radioconda](https://github.com/ryanvolz/radioconda#radioconda).

* [Official releases](https://github.com/gqrx-sdr/gqrx/releases)
* [Pre-release builds](https://github.com/gqrx-sdr/gqrx/actions?query=workflow%3ABuild+branch%3Amaster)

Usage
-----

It is strongly recommended to run the `volk_profile` utility before
running gqrx. This will detect and enable processor-specific optimisations and
will in many cases give a significant performance boost.

The first time you start gqrx it will open a device configuration dialog.
Supported devices that are connected to the computer are discovered
automatically and you can select any of them in the drop-down list.

If you don't see your device listed in the drop-down list it could be because:
- The driver has not been included in a binary distribution
- The udev rule has not been properly configured
- Linux kernel driver is blocking access to the device

You can test your device using device specific tools, such as rtl_test,
airspy_rx, hackrf_transfer, qthid, etc.

Gqrx supports multiple configurations and sessions if you have several devices
or if you want to use the same device under different configurations. You can
load a configuration from the GUI or using the `-c` command line argument. See
`gqrx --help` for a complete list of command line arguments.

Tutorials and howtos are being written and published on the website
https://gqrx.dk/


Known problems
--------------

See the bug tracker on Github: https://github.com/gqrx-sdr/gqrx/issues


Getting help and reporting bugs
-------------------------------

There is a Google group for discussing anything related to Gqrx:
https://groups.google.com/g/gqrx
This includes getting help with installation and troubleshooting. Please
remember to provide detailed description of your problem, your setup, what
steps you followed, etc.

Please stick around and help others with their problems. Otherwise, if only
developers provide user support there will be no more time for further
development.


Installation from source
------------------------

The source code is hosted on Github: https://github.com/gqrx-sdr/gqrx

To compile gqrx from source you need the following dependencies:
- GNU Radio 3.8, 3.9, or 3.10 with the following components:
    - gnuradio-runtime
    - gnuradio-analog
    - gnuradio-audio
    - gnuradio-blocks
    - gnuradio-digital
    - gnuradio-fft
    - gnuradio-filter
    - gnuradio-network (GNU Radio 3.10 only)
    - gnuradio-pmt
- The gr-iqbalance library (optional)
- Drivers for the hardware you want to have support for:
    - Funcube Dongle Pro driver via gr-fcd
    - UHD driver via gr-uhd
    - Funcube Dongle Pro+ driver from https://github.com/dl1ksv/gr-fcdproplus
    - RTL-SDR driver from https://gitea.osmocom.org/sdr/rtl-sdr
    - HackRF driver from https://github.com/mossmann/hackrf
    - Airspy driver from https://github.com/airspy/airspyone_host
    - SoapySDR from https://github.com/pothosware/SoapySDR
    - RFSpace driver is built in
- gnuradio-osmosdr from https://gitea.osmocom.org/sdr/gr-osmosdr
- pulseaudio or portaudio (Linux-only and optional)
- Qt 5 or Qt 6 with the following components:
    - Core
    - GUI
    - Network
    - Widgets
    - Svg (runtime-only)
- cmake version >= 3.2.0

Gqrx can be compiled from within Qt Creator or in a terminal:

For command line builds:
<pre>
$ git clone https://github.com/gqrx-sdr/gqrx.git gqrx.git
$ cd gqrx.git
$ mkdir build
$ cd build
$ cmake ..
$ make
</pre>
On some systems, the default cmake release builds are "over-optimized" and
perform poorly. In that case try forcing -O2 using
<pre>
export CXXFLAGS=-O2
</pre>
before the cmake step.

For Qt Creator builds:
<pre>
$ git clone https://github.com/gqrx-sdr/gqrx.git gqrx.git
$ cd gqrx.git
$ mkdir build
Start Qt Creator
Open gqrx.git/CMakeLists.txt file
At the dialog asking for build location, select gqrx.git/build
click continue
If asked to choose cmake executable, do so
click continue
click the run cmake button
click done
optionally, on the Projects page, under Build Steps/Make/Additional arguments,
	enter -j4 (replacing 4 with the number of cores in your CPU).
Use Qt Creator as before
</pre>


Debugging
---------

Debug logging can be enabled by setting the `QT_LOGGING_RULES` environment
variable:

```
QT_LOGGING_RULES="*.debug=true;plotter.debug=false;qt.*.debug=false" gqrx
```

To turn on plotter debugging as well, use the following command:

```
QT_LOGGING_RULES="*.debug=true;qt.*.debug=false" gqrx
```


Credits and License
-------------------

Gqrx is designed and written by Alexandru Csete OZ9AEC, and it is licensed
under the GNU General Public License.

Some of the source files were adapted from Cutesdr by Moe Weatley and these
come with a Simplified BSD license.

The following people and organisations have contributed to gqrx:

* Alex Grinkov
* Alexander Fasching
* Andrea Merello
* Andrea Montefusco, IW0HDV
* Andy Sloane
* Anthony Willard
* Anton Blanchard
* AsciiWolf
* Bastian Bloessl
* Ben Reese
* Bob McGwier, N4HY
* Brandonn Etheve
* charlylima
* Chris Kuethe
* Christian Lindner, DL2VCL
* Clayton Smith, VE3IRR
* Dallas Epperson
* Daniil Cherednik
* Darin Franklin
* Davide Gerhard
* Dominic Chen
* Doron Behar
* Doug Hammond
* Edouard Lafargue
* Elias Önal
* Federico Fuga
* Frank Brickle, AB2KT
* Frank Werner-Krippendorf, HB9FXQ
* Ganael Laplanche
* Gisle Vanem
* Göran Weinholt, SA6CJK
* Grigory Shipunov
* Gwenhael Goavec-Merou
* Herman Semenov
* James Yuzawa
* Jaroslav Škarvada
* Jeff Long
* Jiawei Chen
* Jiří Pinkava
* Joachim Schueth, DL2KCD
* Jon Bergli Heier
* Josh Blum
* Kate Adams
* Kenji Rikitake, JJ1BDX
* Kitware Inc.
* Konrad Beckmann
* Luna Gräfje
* luzpaz
* Marco Savelli
* Markus Kolb
* Michael Dickens
* Michael Lass
* Michael Tatarinov
* Moe Weatley
* Nadeem Hasan
* Nate Temple
* Nick Robinson, KE5YWP
* Nokia
* Oliver Grossmann, DH2WQ
* Pavel Milanes, CO7WT
* Pavel Stano
* Phil Vachon
* Radoslav Gerganov
* Rob Frohne
* Ron Economos, W6RZ
* Ruslan Migirov
* Russell Dwarshuis, KB8U
* Ryan Volz
* Shuyuan Liu
* Stefano Leucci
* Sultan Qasim Khan
* Sylvain Munaut
* Tarmo Tanilsoo
* Tomasz Lemiech
* Timothy Reaves
* Valentin Ochs
* Vesa Solonen
* Vincent Pelletier
* Vladisslav P
* Will Scales
* Wolfgang Fritz, DK7OB
* Youssef Touil
* Zero_Chaos

Some of the icons are from:
- The GNOME icon theme CC-SA 3.0 by GNOME icon artists
- Tango icon theme, Public Domain by The people from the Tango! project
- Mint-X icon theme, GPL by Clement Lefebvre

Also thanks to Volker Schroer and Alexey Bazhin for bringing Funcube Dongle
Pro+ support to GNU Radio and Gqrx.

Let me know if somebody is missing from the list.

Alex OZ9AEC

# Aguila SDR Platform

## SDR Device Management

### Switching Between SDR Devices
To prevent configuration conflicts when switching between different SDR devices (e.g., HackRF and RTL-SDR), use the provided `sdr_switch.sh` script:

1. Close GQRX completely if it's running
2. Run the script with your desired device:
   ```bash
   # For HackRF:
   ./resources/sdr_switch.sh hackrf

   # For RTL-SDR:
   ./resources/sdr_switch.sh rtlsdr
   ```
3. Launch GQRX normally

The script will:
- Back up your current GQRX configuration
- Restore device-specific settings if available
- Create a fresh configuration if needed
- Automatically save working configurations for each device

This prevents configuration conflicts and segmentation faults that can occur when switching between different SDR devices.

### Troubleshooting Device Switching
If you encounter issues:
1. Close GQRX completely
2. Run the switch script for your desired device
3. Launch GQRX fresh

Your device-specific configurations will be saved automatically when GQRX exits normally.

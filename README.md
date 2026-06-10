# ENGR 357 Pico SDR Receiver

A custom-built, high-performance Software Defined Radio (SDR) receiver powered by the Raspberry Pi Pico (RP2040). This project combines a classic hardware Tayloe detector with modern digital signal processing, streaming 24-bit I/Q audio and CAT control over a single USB cable directly to PC-based SDR software like **Quisk**.

---

## Overview

Traditional homebrew SDR receivers often rely on routing analog audio cables into a PC soundcard, which can introduce noise and ground loops. This project solves that by digitizing the baseband I/Q signals directly on the board using a dedicated 24-bit ADC, and utilizing the RP2040's hardware PIO and TinyUSB stack to act as a **Composite CDC-UAC1 USB Device**. 

When plugged into a computer, this radio appears simultaneously as:
1. **USB Audio Class 2.0 (UAC2) Input:** Streaming ultra-clean 24-bit stereo I/Q audio to the PC.
2. **USB CDC Serial Port:** Receiving standard CAT tuning commands from the SDR software.

## Key Features

ADD FEATURES

---

## Hardware
### Overview
* **MCU:** Raspberry Pi Pico (RP2040) utilizing PIO and DMA for zero-jitter data handling.
* **Clock Generator:** Si5351A providing precise LO generation and the 12.288 MHz Master Clock (MCLK) for the ADC.
* **Quadrature Generator:** SN74AC74 Johnson counter for exact 90-degree I/Q phase shifting.
* **Mixer (Tayloe Detector):** SN74CBT3253 Quadrature Sampling Detector.
* **Baseband Amplification:** OPA2350 high-speed, low-noise operational amplifiers.
* **Digitization:** PCM1808 24-bit Stereo I2S ADC.
* **User Interface:** * Newhaven 1x8 Character LCD (ST7066U / 3.3V)
  * Quadrature Rotary Encoder with push-button for VFO tuning
  * 4x SMD Push Buttons for menu/mode navigation (not currently implemented in software)

### Pinout Configuration
The following table outlines the physical connections between the Raspberry Pi Pico and your hardware components.

| Pin | Function | Connected | Implemented |
| :--- | :--- | :---: | :---: |
| **GPIO0** | BCK | ✅ | ❌ |
| **GPIO1** | WS | ✅ | ❌ |
| **GPIO2** | DOUT | ✅ | ❌ |
| **GPIO11** | SD | ✅ | ❌ |
| **GPIO3** | LCD_Bit0 | ✅ | ✅ |
| **GPIO4** | LCD_Bit1 | ✅ | ✅ |
| **GPIO5** | LCD_Bit2 | ✅ | ✅ |
| **GPIO6** | LCD_Bit3 | ✅ | ✅ |
| **GPIO7** | LCD_Bit4 | ✅ | ✅ |
| **GPIO8** | LCD_Bit5 | ✅ | ✅ |
| **GPIO9** | LCD_Bit6 | ✅ | ✅ |
| **GPIO10** | LCD_Bit7 | ✅ | ✅ |
| **GPIO12** | I2C Bus SDA | ✅ | ✅ |
| **GPIO13** | I2C Bus SCL | ✅ | ✅ |
| **GPIO14** | LCD_RegSel | ✅ | ✅ |
| **GPIO15** | LCD_Ena | ✅ | ✅ |
| **GPIO16** | Up_Btn | ✅ | ❌ |
| **GPIO17** | Left_Btn | ✅ | ❌ |
| **GPIO18** | Select_Btn | ✅ | ❌ |
| **GPIO19** | Right_Btn | ✅ | ❌ |
| **GPIO20** | Encoder_A | ✅ | ✅ |
| **GPIO21** | Encoder_B | ✅ | ✅ |
| **GPIO22** | Encoder_Button| ✅ | ❌ |
| **GPIO26** | ADC0 (I_OUT) | ❌ | ❌ |
| **GPIO27** | ADC1 (Q_OUT) | ❌ | ❌ |
| **AGND** | GND | ✅ | ✅ |
| **VBUS** | +5v Bus | ✅ | ✅ |
| **3v3** | +3.3v Bus | ✅ | ✅ |

---

## Software
### Overview
* **Framework:** C/C++ via the Raspberry Pi Pico SDK.
* **USB Stack:** TinyUSB configured for a unidirectional composite UAC1 Capture + CDC interface.
* **DSP Pipeline:** Custom PIO state machines reading I2S data, fed via DMA directly to USB buffers without bottlenecking the CPU.

### Function Reference

#### LCD Driver (`lcd_driver.c`)
Manages the 8-bit parallel interface for the ST7066U controller.

| Function | Description |
| :--- | :--- |
| `void lcd_init()` | [cite_start]Executes the power-on initialization routine, including the 3-step "Wake up" sequence required for the ST7066U controller to stabilize[cite: 259]. |
| `void lcd_command(uint8_t cmd)` | Sets the `RS` (Register Select) pin LOW for command mode, drives the 8-bit bus, and toggles the `E` (Enable) pin to latch data. |
| `void lcd_clear()` | [cite_start]Sends the `0x01` instruction to clear the display RAM and reset the cursor[cite: 178]. |
| `void lcd_print(const char* str)` | Sets `RS` HIGH for data mode, iterates through the string, and writes character bytes to the LCD data bus. |

#### Clock Synthesizer (`si5351.c`)
Interfaces with the Si5351 via I2C for frequency generation.

| Function | Description |
| :--- | :--- |
| `bool si5351_init(i2c_inst_t *i2c)` | [cite_start]Initializes I2C, verifies device status, and loads factory crystal load settings[cite: 172]. |
| `uint32_t si5351_set_frequency(...)` | Calculates PLL and MultiSynth divisors, programs the register blocks, resets the PLL to achieve frequency lock, and enables the output clock. |

#### Command & Control (`cdc_app.c` & `main.c`)
Handles communication and system state.

| Function | Description |
| :--- | :--- |
| `void cdc_task()` | Parses incoming serial data for Quisk-compatible commands (e.g., `FREQ`, `VER`, `XTAL`), enabling remote control of the SDR. |
| `static void process_command(const char *line)` | Dispatcher function that maps string commands (like `FREQ`) to specific hardware actions. |
| `void audio_task()` | Handles the audio buffer from the DMA/FIFO into the USB streaming interface. |
| `void encoder_callback(...)` | ISR that tracks encoder rotation to update the `current_hz` variable, which is then processed by the main loop. |

### Build Instructions
1. **Prerequisites**: Ensure the `Pico SDK` abd `TinyUSB` libraries are installed
2. **Setup**:
   ```bash
   cd /path/to/sdr-firmware
   rm -rf build && mkdir build && cd build

---

## Repository Structure

This repository contains the hardware design files, the C/C++ firmware, Python unit tests, and relevant research documentation.

ENGR-357-SDR/
├── Documents/              # Research notes, datasheets, and design requirements
├── KiCad Designs/          # PCB schematics, layout files, and BOMs
│   ├── v1 Switch Mixer/
│   ├── v2 Tayloe Detector/
│   └── v3 Tayloe Detector FINAL/
├── LT Spice Simulations/   # RF demodulator analysis and circuit models
│   ├── LTSpice project files/
│   └── Simulation Results/ # Images of simulation results
└── sdr-firmware/           # RP2040 Firmware (Pico SDK)
    ├── build/              # Compiled binaries and artifacts
    ├── PythonUnitTests/    # Verification and testing scripts
    └── src/                # C firmware source files
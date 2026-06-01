# ENGR 356 Pico SDR Receiver

A custom-built, high-performance Software Defined Radio (SDR) receiver powered by the Raspberry Pi Pico (RP2040). This project combines a classic hardware Tayloe detector with modern digital signal processing, streaming 24-bit I/Q audio and CAT control over a single USB cable directly to PC-based SDR software like **Quisk**.

## 📻 Overview

Traditional homebrew SDR receivers often rely on routing analog audio cables into a PC soundcard, which can introduce noise and ground loops. This project solves that by digitizing the baseband I/Q signals directly on the board using a dedicated 24-bit ADC, and utilizing the RP2040's hardware PIO and TinyUSB stack to act as a **Composite USB Device**. 

When plugged into a computer, this radio appears simultaneously as:
1. **USB Audio Class 2.0 (UAC2) Input:** Streaming ultra-clean 24-bit stereo I/Q audio to the PC.
2. **USB CDC Serial Port:** Receiving standard CAT tuning commands from the SDR software.

## ✨ Key Features

### Hardware
* **MCU:** Raspberry Pi Pico W (RP2040) utilizing PIO and DMA for zero-jitter data handling.
* **Clock Generator:** Si5351A providing precise LO generation and the 12.288 MHz Master Clock (MCLK) for the ADC.
* **Quadrature Generator:** SN74AC74 Johnson counter for exact 90-degree I/Q phase shifting.
* **Mixer (Tayloe Detector):** SN74CBT3253 Quadrature Sampling Detector.
* **Baseband Amplification:** OPA2350 high-speed, low-noise operational amplifiers.
* **Digitization:** PCM1808 24-bit Stereo I2S ADC.
* **User Interface:** * Newhaven 1x8 Character LCD (ST7066U / 3.3V)
  * Quadrature Rotary Encoder with push-button for VFO tuning
  * 4x SMD Push Buttons for menu/mode navigation

### Software
* **Framework:** C/C++ via the Raspberry Pi Pico SDK.
* **USB Stack:** TinyUSB configured for a unidirectional composite UAC2 Capture + CDC interface.
* **DSP Pipeline:** Custom PIO state machines reading I2S data, fed via DMA directly to USB buffers without bottlenecking the CPU.

---

## 📂 Repository Structure

This repository contains the hardware design files, the C/C++ firmware, and relevant research documentation.

```text
├── hardware/                 # KiCad schematics, PCB layouts, and BOM
│   └── SDR-v3-TayloDetector-FINAL.kicad_sch
├── software/                 # RP2040 Firmware (Pico SDK)
│   ├── CMakeLists.txt
│   ├── tusb_config.h         # TinyUSB configuration (CDC + UAC2 Audio)
│   ├── usb_descriptors.c     # USB configuration describing the board to the host PC
│   ├── src/
│   │   ├── main.c            # Core execution loop
│   │   ├── hw_drivers/       # Si5351, LCD, Encoder, and Button drivers
│   │   ├── dsp_audio/        # I2S PIO assembly and DMA buffer management
│   │   └── application/      # Quisk CAT parser and VFO UI state logic
│   └── tests/                # Standalone bring-up tests for hardware verification
└── docs/                     # Research notes, datasheets, and architectural planning
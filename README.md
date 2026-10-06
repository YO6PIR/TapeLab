# TapeLAB

<p align="center">
  <img width="1200" alt="TapeLAB overview" src="https://github.com/user-attachments/assets/2354e5d9-07b7-447e-b999-ee16c3b69f11" />
</p>

A standalone cassette deck measurement and calibration instrument for analog audio systems.

TapeLAB is a custom-built firmware and hardware platform designed to measure, analyze, and calibrate cassette tape decks and analog audio chains using a touchscreen interface, high-speed ADC acquisition, and DSP-based signal processing.

It is built around an STM32 platform and is intended for practical use in audio servicing, tape deck alignment, Dolby testing, and signal analysis.

## Why TapeLAB?

Tape decks are analog devices, and accurate setup requires more than a simple level check. TapeLAB helps with:

- frequency response measurement
- cassette EQ analysis
- bias optimization
- Dolby evaluation
- channel matching
- noise and high-frequency behavior checks
- measurement history and comparison

The goal is not just to display a waveform, but to provide meaningful and repeatable measurements that support real calibration work.

---

## Key Features

- Audio level and RMS measurement
- Frequency response analysis
- 256-bin FFT spectrum analyzer
- Zoom and re-centering for spectral inspection
- Tape EQ measurement for 2-head and 3-head configurations
- Bias measurement and validation
- Automatic Tape Calibration (ATC)
- Dolby testing for OFF / B / C modes
- Noise measurement and HF analysis
- Left / right channel comparison
- Touchscreen graphical interface
- Measurement history browser

---

## Hardware Platform

TapeLAB is based on a compact embedded architecture optimized for audio measurement.

| Component | Selection |
|---|---|
| Microcontroller | STM32F401RCT6 / STM32F401RCTx |
| Framework | Arduino STM32 Core |
| Display | ILI9341, 320 × 240 TFT |
| Touch Controller | XPT2046 |
| ADC | STM32 internal ADC |
| Audio Front-End | Custom analog front-end |
| DSP | ARM CMSIS-DSP |
| Storage | EEPROM I²C |
| Signal Generator | AD9833 |

<p align="center">
  <img width="1800" alt="TapeLAB hardware" src="https://github.com/user-attachments/assets/198cf70d-5662-4f33-b095-30f39558df11" />
</p>

---

## Signal Acquisition

The acquisition chain uses timer-triggered ADC conversion with DMA for stable and low-overhead capture of audio signals.

```text
Audio Input
     │
     ▼
Analog Front-End
     │
     ▼
STM32 ADC
     │
     ▼
DMA
     │
     ▼
RAM Buffer
     │
     ▼
DSP Processing
     ├── Level Measurement
     ├── Frequency Analysis
     ├── FFT
     ├── Noise Analysis
     └── Calibration
```

---

## Measurement Capabilities

### FFT Spectrum Analyzer

TapeLAB includes a real-time FFT analyzer based on ARM CMSIS-DSP.

It performs analysis with a 256-bin spectrum and supports wide-band and zoomed inspection. This is useful for evaluating:

- tape frequency response
- harmonic distortion content
- noise components
- spurious signals
- high-frequency performance

<p align="center">
  <img width="600" alt="FFT analyzer" src="https://github.com/user-attachments/assets/d1e9b9eb-a94c-44ba-85dc-b05a2f49f396" />
</p>

### Tape Equalization

TapeLAB measures tape EQ across multiple frequencies and displays both left and right channel responses, as well as a combined L + R view.

<p align="center">
  <img width="600" alt="Tape EQ" src="https://github.com/user-attachments/assets/ab9caf25-c70a-44c6-89d1-bfe6bbaa34d4" />
</p>

### Bias Calibration

Bias calibration is a core task for accurate cassette alignment. The system evaluates bias conditions and checks whether the left and right channels are correctly matched.

Typical result states can include:

- BIAS MATCHED
- EXCELLENT
- BIAS ACCEPTABLE

<p align="center">
  <img width="600" alt="Bias calibration" src="https://github.com/user-attachments/assets/751990d3-e85a-4f9e-bdc7-31ff11b43be0" />
</p>

### Automatic Tape Calibration (ATC)

ATC combines several measurement stages into a controlled sequence to help standardize calibration work.

The system evaluates:

- reference level
- bias
- tape equalization
- left/right matching
- frequency response

This makes calibration more repeatable than purely manual adjustment.

<p align="center">
  <img width="600" alt="ATC" src="https://github.com/user-attachments/assets/7e7e4e95-037a-4bab-a0b3-ff2f7f88a375" />
</p>

### Dolby Testing

TapeLAB supports Dolby test workflows for both 2-head and 3-head cassette decks using a series of reference frequencies.

```text
1000 Hz
1600 Hz
2500 Hz
4000 Hz
6300 Hz
8000 Hz
10000 Hz
12500 Hz
15000 Hz
```

<p align="center">
  <img width="600" alt="Dolby test" src="https://github.com/user-attachments/assets/80e43869-6c83-4e26-af88-9a8cc594161d" />
</p>

### Noise Measurement

TapeLAB provides dedicated noise measurements, including HF noise analysis in a constrained frequency region to better represent the effective tape and deck performance.

<p align="center">
  <img width="600" alt="Noise measurement" src="https://github.com/user-attachments/assets/8f8a5e9a-cfeb-471c-aeb9-25c4c7747e5f" />
</p>

### Test History

Measurements are organized into categories such as:

```text
EQ TESTS 2H
EQ TESTS 3H
DOLBY 2H
DOLBY 3H
```

The built-in browser allows stored results to be reviewed directly from the instrument.

---

## Software Architecture

TapeLAB is developed in C/C++ using the Arduino ecosystem for STM32.

Major software areas include:

- STM32duino
- ARM CMSIS-DSP
- TFT_eSPI
- custom ADC/DMA acquisition
- custom DSP processing
- custom measurement algorithms
- custom touchscreen interface

At a high level, the firmware is organized into these layers:

```text
┌─────────────────────────────┐
│       Touchscreen UI        │
├─────────────────────────────┤
│ Measurements / Calibration  │
├─────────────────────────────┤
│        DSP / FFT            │
├─────────────────────────────┤
│       ADC / DMA             │
├─────────────────────────────┤
│     Analog Front-End        │
└─────────────────────────────┘
```

---

## Development Progress

TapeLAB has evolved through several firmware stages:

- initial ADC/DMA acquisition
- CMSIS-DSP FFT integration
- spectrum analyzer implementation
- spectrum zoom and re-centering
- tape EQ measurements
- Dolby testing
- noise measurement
- Automatic Tape Calibration
- test history and browsing

Current development focuses on accuracy, usability, and expanded calibration workflows.

---

## Project Status

Current firmware version: `v1.0.4`

TapeLAB is a functional experimental measurement instrument and is actively under development.

It is primarily intended for:

- DIY electronics
- audio experimentation
- cassette deck servicing
- analog measurement and calibration

---

## Roadmap

| Area | Status |
|---|---|
| Core / Display / Touch | Functional |
| Audio Engine | Functional |
| Playback Analyzer | Functional |
| Spectrum Analyzer | Functional / refining |
| Transport Analysis | Functional / refining |
| Tape EQ | Functional |
| Dolby Check | Functional / hardware validation |
| 2HEAD / 3HEAD | In development and validation |
| Generator + loopback calibration | Functional / validation |
| ATC | In development |
| Waveform / Oscilloscope | Redesign in progress |
| Record Calibration | Planned |
| Azimuth Assistant | Planned |
| Frequency Response | In development |
| THD | Planned |
| Save / Recall Profiles | In development |

---

## Project Principles

- validated functions are not changed without measurable reason
- independent modules are not modified unnecessarily
- measurements are stored numerically
- hardware is validated separately from software changes
- major changes are followed by build and memory validation
- correction is controlled and verified on real hardware

---

## Gallery

<p align="center">
  <img width="2800" alt="TapeLAB hardware board" src="https://github.com/user-attachments/assets/a859504d-50df-451c-a6f6-7fcb03fbd02c" />
</p>

<p align="center">
  <img width="2800" alt="TapeLAB hardware board detail" src="https://github.com/user-attachments/assets/1915a7a4-e386-4843-80a5-9ecf546d6ce1" />
</p>

---

## Repository Notes

This repository contains the firmware and supporting project material for TapeLAB.

Additional documentation will be added over time, covering:

- hardware design
- analog front-end
- ADC/DMA acquisition
- DSP and FFT processing
- measurement methodology
- tape calibration procedures
- Dolby testing
- ATC operation
- firmware architecture

---

## License

This project is provided for educational, experimental, and personal development use.

See the repository license for the applicable terms.

---

## Project Credits

Developed by Ovidiu — YO6PIR.

"TapeLAB" is a practical measurement platform designed to make analog cassette calibration more transparent, repeatable, and useful for real-world servicing work.

# TapeLAB

### A standalone audio measurement and cassette tape calibration instrument

TapeLAB is a dedicated measurement and calibration system designed for working with analog cassette tape recorders and magnetic tape.

It combines a custom analog audio front-end, high-speed ADC acquisition, DSP-based signal analysis and a graphical touchscreen interface into a compact standalone instrument.

<img width="1272" height="770" alt="image" src="https://github.com/user-attachments/assets/2354e5d9-07b7-447e-b999-ee16c3b69f11" />

## Overview

TapeLAB was developed as a practical laboratory instrument for testing, measuring and calibrating analog cassette tape decks.

The purpose of the project is not simply to display an audio waveform, but to provide meaningful measurements that can be used when servicing and calibrating cassette recorders.

TapeLAB can measure and analyze:

- audio level
- frequency response
- tape equalization
- bias response
- channel balance
- noise
- Dolby response
- high-frequency performance

The measurement results are presented through a dedicated touchscreen user interface.

---

## Key Features

- **Audio Level Measurement**
- **Frequency Response Measurement**
- **256-bin FFT Spectrum Analyzer**
- **Spectrum Zoom and Frequency Re-centering**
- **Automatic Tape Calibration (ATC)**
- **Tape EQ Measurement**
- **Bias Measurement and Calibration**
- **Dolby Calibration and Testing**
- **Noise Measurement**
- **High-Frequency Noise Analysis**
- **Left / Right Channel Analysis**
- **Touchscreen Graphical User Interface**
- **Measurement Test History**

---

## Hardware

TapeLAB is based on an STM32 microcontroller and a custom analog audio front-end.

| Component | Description |
|---|---|
| Microcontroller | STM32F401RCT6 |
| Display | ILI9341 320 × 240 SPI TFT |
| Touch Controller | XPT2046 |
| ADC | STM32 internal ADC |
| Audio Front-End | Custom analog circuitry |
| DSP | ARM CMSIS-DSP |

<img width="1833" height="576" alt="image" src="https://github.com/user-attachments/assets/198cf70d-5662-4f33-b095-30f39558df11" />

## Signal Acquisition

The audio acquisition chain uses timer-triggered ADC conversion combined with DMA.

This allows continuous audio capture into RAM while minimizing CPU overhead.

```text
Audio Input
     │
     ▼
Analog Audio Front-End
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
     │
     ├── Level Measurement
     ├── Frequency Analysis
     ├── FFT
     ├── Noise Analysis
     └── Calibration
```

## FFT Spectrum Analyzer

TapeLAB includes a real-time FFT spectrum analyzer based on **ARM CMSIS-DSP**.

The analyzer uses 256 spectrum bins and provides both a wide-band view and a zoomed view for detailed inspection of selected frequency regions.

The zoom function allows the user to re-center the spectrum around a selected frequency range.

---

<img width="600" height="446" alt="image" src="https://github.com/user-attachments/assets/d1e9b9eb-a94c-44ba-85dc-b05a2f49f396" />

The FFT analyzer is useful for examining:

- tape frequency response
- harmonic content
- noise components
- unwanted spurious signals
- high-frequency behavior

---

## Tape Equalization

TapeLAB provides dedicated tape EQ measurements for cassette decks.

Measurements are performed across multiple test frequencies and can be displayed for the left and right channels.

The final result can also be viewed as a combined **L + R** response.

---
<img width="600" height="444" alt="image" src="https://github.com/user-attachments/assets/ab9caf25-c70a-44c6-89d1-bfe6bbaa34d4" />

## Bias Calibration

Bias calibration is an important part of cassette tape alignment.

TapeLAB measures the response at different bias conditions and evaluates the difference between the left and right channels.

The calibration procedure uses defined acceptance limits to determine whether the bias is matched correctly.

Typical result categories include:

- **BIAS MATCHED**
- **EXCELLENT**
- **BIAS ACCEPTABLE**

---
<img width="600" height="441" alt="image" src="https://github.com/user-attachments/assets/751990d3-e85a-4f9e-bdc7-31ff11b43be0" />

## Automatic Tape Calibration

TapeLAB includes an **Automatic Tape Calibration (ATC)** system.

The calibration procedure combines several measurement stages into a controlled sequence.

The system can evaluate:

- reference level
- bias
- tape equalization
- left/right matching
- frequency response

The objective is to provide a repeatable calibration procedure instead of relying entirely on manual measurements.

---

<img width="600" height="445" alt="image" src="https://github.com/user-attachments/assets/7e7e4e95-037a-4bab-a0b3-ff2f7f88a375" />

## Dolby Testing

TapeLAB includes dedicated Dolby test procedures for both **2-head and 3-head cassette decks**.

The test sequence uses a series of reference frequencies:

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

The Dolby test system evaluates the playback response and provides a graphical representation of the measured results.

---

<img width="600" height="454" alt="image" src="https://github.com/user-attachments/assets/80e43869-6c83-4e26-af88-9a8cc594161d" />

## Noise Measurement

TapeLAB also provides dedicated noise measurements.

The high-frequency noise analysis uses a restricted frequency region in order to avoid known unwanted components outside the useful measurement range.

This makes the measurement more representative of the actual tape and deck performance.

---

<img width="600" height="451" alt="image" src="https://github.com/user-attachments/assets/8f8a5e9a-cfeb-471c-aeb9-25c4c7747e5f" />

## Test History

TapeLAB includes a test browser for reviewing stored measurement results.

Tests are organized into separate categories:

```text
EQ TESTS 2H
EQ TESTS 3H
DOLBY 2H
DOLBY 3H
```

The browser allows the user to review individual measurement sequences directly from the instrument.

## Software

TapeLAB is developed in **C/C++** using the Arduino ecosystem for STM32.

Major software components include:

- STM32duino
- ARM CMSIS-DSP
- TFT_eSPI
- Custom ADC/DMA acquisition
- Custom DSP processing
- Custom measurement algorithms
- Custom touchscreen user interface

The firmware is organized into separate modules for acquisition, signal processing, measurements, calibration and user interface.

---

## Project Architecture

At a high level, the firmware can be viewed as several cooperating layers:

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

## Development History

TapeLAB has been developed incrementally through several firmware phases.

Major development steps included:

- initial ADC/DMA acquisition
- CMSIS-DSP FFT integration
- spectrum analyzer
- spectrum zoom
- tape EQ measurement
- Dolby testing
- noise measurement
- Automatic Tape Calibration
- test history and browsing

Current firmware development continues to focus on measurement accuracy, usability and calibration workflows.

---

## Project Status

**Current firmware version: `v1.0.4`**

TapeLAB is a functional experimental measurement instrument and is under continued development.

The project is primarily intended for **DIY electronics, audio experimentation, cassette deck servicing and measurement**.

---

## Documentation

Additional technical documentation will be added to the repository.

Planned documentation includes:

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

<img width="2806" height="1984" alt="image" src="https://github.com/user-attachments/assets/a859504d-50df-451c-a6f6-7fcb03fbd02c" />

<img width="2806" height="1984" alt="image" src="https://github.com/user-attachments/assets/1915a7a4-e386-4843-80a5-9ecf546d6ce1" />

---

## Gallery

A selection of TapeLAB photographs and screenshots will be maintained in the `images/` directory.

Suggested image set:

```text
images/
├── tapelab.jpg
├── tapelab-inside.jpg
├── block-diagram.png
├── fft-analyzer.jpg
├── tape-eq.jpg
├── bias-calibration.jpg
├── atc.jpg
├── dolby-test.jpg
├── noise-test.jpg
└── test-browser.jpg
```

---

## License

This project is provided for educational, experimental and personal development purposes.

See the repository license for the applicable terms.

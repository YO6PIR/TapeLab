# TapeLAB

### A standalone audio measurement and cassette tape calibration instrument

TapeLAB is a dedicated measurement and calibration system designed for working with analog cassette tape recorders and magnetic tape.

It combines a custom analog audio front-end, high-speed ADC acquisition, DSP-based signal analysis and a graphical touchscreen interface into a compact standalone instrument.

---

<img width="1272" height="770" alt="image" src="https://github.com/user-attachments/assets/2354e5d9-07b7-447e-b999-ee16c3b69f11" />


*Recommended image: A clean photograph showing the complete TapeLAB instrument, preferably powered on with the display visible.*

---

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

---

**[PLACEHOLDER: IMAGINE TAPELAB – INTERIOR]**

*Recommended image: Photograph of the inside of the instrument showing the STM32 board, analog front-end, power supply and wiring.*

This image is useful because it shows how the project was actually built rather than only presenting the finished enclosure.

---

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

---

**[PLACEHOLDER: IMAGINE TAPELAB – SCHEMA BLOC]**

*Recommended image: A clean graphical block diagram showing the complete signal path from audio input to ADC, DMA, DSP and display.*

*A simplified block diagram is preferable here to a full electronic schematic.*

---

## FFT Spectrum Analyzer

TapeLAB includes a real-time FFT spectrum analyzer based on **ARM CMSIS-DSP**.

The analyzer uses 256 spectrum bins and provides both a wide-band view and a zoomed view for detailed inspection of selected frequency regions.

The zoom function allows the user to re-center the spectrum around a selected frequency range.

---

**[PLACEHOLDER: IMAGINE TAPELAB – FFT ANALYZER]**

*Recommended image: Screenshot of the TapeLAB FFT Analyzer showing the spectrum curve, frequency scale and PEAK measurement.*

---

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

**[PLACEHOLDER: IMAGINE TAPELAB – TAPE EQ]**

*Recommended image: Screenshot of the final Tape EQ measurement screen with the response graph visible.*

---

## Bias Calibration

Bias calibration is an important part of cassette tape alignment.

TapeLAB measures the response at different bias conditions and evaluates the difference between the left and right channels.

The calibration procedure uses defined acceptance limits to determine whether the bias is matched correctly.

Typical result categories include:

- **BIAS MATCHED**
- **EXCELLENT**
- **BIAS ACCEPTABLE**

---

**[PLACEHOLDER: IMAGINE TAPELAB – BIAS CALIBRATION]**

*Recommended image: Screenshot of the Bias Calibration / ATC screen showing the measurement progress or final result.*

---

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

**[PLACEHOLDER: IMAGINE TAPELAB – ATC]**

*Recommended image: Screenshot showing the ATC procedure in progress or the completed calibration result.*

---

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

**[PLACEHOLDER: IMAGINE TAPELAB – DOLBY TEST]**

*Recommended image: Screenshot of the final Dolby test result screen.*

---

## Noise Measurement

TapeLAB also provides dedicated noise measurements.

The high-frequency noise analysis uses a restricted frequency region in order to avoid known unwanted components outside the useful measurement range.

This makes the measurement more representative of the actual tape and deck performance.

---

**[PLACEHOLDER: IMAGINE TAPELAB – NOISE MEASUREMENT]**

*Recommended image: Screenshot of the Noise or Noise HF measurement screen.*

---

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

---

**[PLACEHOLDER: IMAGINE TAPELAB – TEST BROWSER]**

*Recommended image: Screenshot of the TESTS browser showing the available test categories.*

---

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

---

**[PLACEHOLDER: IMAGINE TAPELAB – ARHITECTURA SOFTWARE]**

*Optional image: A polished diagram showing the relationship between the hardware acquisition layer, DSP, measurement modules and user interface.*

*This image is optional because the text diagram above already explains the architecture.*

---

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

**[PLACEHOLDER: IMAGINE TAPELAB – SCHEMA ELECTRONICĂ]**

*Recommended image or PDF: The complete electronic schematic of the TapeLAB hardware.*

*Unlike the earlier block diagram, this should be the actual circuit schematic.*

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

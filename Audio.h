#pragma once

#include <Arduino.h>
#include "FFT.h"

class Audio
{
public:
    bool begin();
    bool calibrate();

    uint16_t readLeft();
    uint16_t readRight();
    int16_t readLeftCentered();
    int16_t readRightCentered();

    // Captures one uniformly timed block from PA0 using TIM3 -> ADC1 -> DMA.
    bool captureLeftBlock(
        int16_t *buffer,
        uint16_t sampleCount,
        uint32_t sampleRateHz);
    void stopCapture();
    bool captureActive() const;

    void update();
    void calculateFFT();
    bool calculateFFTChannel(bool rightChannel, bool &clipped);

    float levelLeft();
    float levelRight();
    float frequency();

    uint8_t getBand(uint8_t index);

private:
    bool configureAdc(bool hardwareTriggered);
    bool configureCaptureChannel(bool rightChannel);
    bool configureCaptureTimer(uint32_t sampleRateHz);
    bool captureBlock(
        int16_t *buffer,
        uint16_t sampleCount,
        uint32_t sampleRateHz,
        bool rightChannel);

    int16_t biasL = 2048;
    int16_t biasR = 2048;

    float levelL = -40.0f;
    float levelR = -40.0f;
    float freqL = 0.0f;

    uint8_t spectrumBands[FFT_BANDS] = {0};
};

extern Audio audio;

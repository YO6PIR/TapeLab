#pragma once
// definește FFTAnalyzer, dimensiunea FFT și interfața rezultatelor.
#include <Arduino.h>

constexpr uint8_t FFT_BANDS = 16;

class FFTAnalyzer
{
public:
    static constexpr uint16_t SAMPLES = 512;
    static constexpr uint16_t BIN_COUNT = SAMPLES / 2;
    static constexpr uint32_t SAMPLE_RATE_HZ = 40000;
    static constexpr float BIN_WIDTH_HZ =
        static_cast<float>(SAMPLE_RATE_HZ) / SAMPLES;

    void calculate(const int16_t samples[SAMPLES],
                   uint8_t spectrumBands[FFT_BANDS]);

    // Linear magnitudes for bins 0..255, ready for the Phase 3 renderer.
    const float *binMagnitudes() const;

    // Integrated RMS for an inclusive range of the most recent FFT bins,
    // using the same Hamming normalization as the spectrum renderer.
    float integratedRms(uint16_t firstBin, uint16_t lastBin) const;

private:
    bool initialize();
    void updatePeak(uint8_t band, uint8_t level);

    bool initialized = false;
    float fftBins[BIN_COUNT] = {0.0f};
    uint16_t barDecayCounter[FFT_BANDS] = {0};
    uint16_t peakHoldCounter[FFT_BANDS] = {0};
    uint16_t peakDecayCounter[FFT_BANDS] = {0};
};

extern FFTAnalyzer fftAnalyzer;
extern uint8_t peakBands[FFT_BANDS];

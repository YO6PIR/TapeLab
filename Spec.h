#pragma once
#include "Arduino.h"
#include "Audio.h"

class Spectrum
{
public:
    struct FFTGraphTouch
    {
        uint16_t x;
        uint16_t y;
    };

    void begin();
    void run();
    void drawScreen();
    void drawGrid();
    void drawScale();
    void updateDisplay();
    bool updateDiagnosticPeak(float &frequencyHz, float &levelDb);
    void drawFFTBackground();
    void drawBarBackground();
    void drawBars(const uint8_t barValues[FFT_BANDS], const uint8_t peakValues[FFT_BANDS]);
    bool takeFFTGraphTouch(FFTGraphTouch &coordinates);

private:
    enum class View : uint8_t
    {
        Spectrum,
        FFT
    };

    const char *activeTitle() const;
    bool processTouch();
    bool isTitleTouch(uint16_t x, uint16_t y) const;
    bool isBackTouch(uint16_t y) const;
    bool isFFTGraphTouch(uint16_t x, uint16_t y) const;
    void captureFFTGraphTouch(uint16_t x, uint16_t y);
    void toggleView();
    void resetFFTView();
    void recenterFFTView(uint16_t graphX);
    uint32_t fftColumnToFrequency(uint16_t column) const;
    uint8_t fftLevelForColumn(uint16_t column) const;
    uint32_t fftTickFrequency(uint8_t tick) const;

    bool createSpectrumSprite();
    void releaseSpectrumSprite();
    void resetSpectrumLevels();
    void updateSpectrumLevels(const float *magnitudes);
    void resetPeakMeasurement();
    void updatePeakMeasurement(const float *magnitudes);
    void buildDisplayMagnitudes(const float *rawMagnitudes);
    void drawPeakMeasurement();
    void drawHighResolutionSpectrum();
    void drawSpectrumGridToSprite();
    void drawSpectrumAxes();
    uint8_t magnitudeToLevel(
        float magnitude,
        float rmsReference) const;

    int gridY(uint8_t line);
    void drawFrequencyScale();
    void drawBarsBackground();
    void clearFFT();

    uint8_t smoothedLevels[FFTAnalyzer::BIN_COUNT] = {0};
    // Display-only copy of the raw FFT magnitudes. Never written back to the
    // analyzer; used by the View::FFT renderer and peak readout.
    float displayMagnitudes[FFTAnalyzer::BIN_COUNT] = {0.0f};
    FFTGraphTouch pendingFFTGraphTouch = {0, 0};
    bool fftGraphTouchPending = false;
    bool fftZoomActive = false;
    uint32_t fftViewStartHz = 0;
    uint32_t fftViewStopHz = FFTAnalyzer::SAMPLE_RATE_HZ / 2;
    uint32_t fftCenterHz = FFTAnalyzer::SAMPLE_RATE_HZ / 4;
    float peakFrequencyHz = 0.0f;
    float peakLevelDb = 0.0f;
    bool peakMeasurementValid = false;
    bool peakMeasurementInitialized = false;
    char displayedPeakText[40] = {0};
    bool spectrumSpriteReady = false;
    View activeView = View::Spectrum;
};

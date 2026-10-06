#pragma once

#include <Arduino.h>

class Waveform
{
public:
    static constexpr uint16_t MAX_SAMPLE_COUNT = 800;
    static constexpr uint16_t MAX_CAPTURE_SAMPLE_COUNT =
        MAX_SAMPLE_COUNT * 2;

    void begin();
    void run();

private:
    enum class ScopeControl : uint8_t
    {
        None,
        Vertical,
        Horizontal
    };

    void captureLeftSignal();
    uint16_t findAutoTrigger(uint16_t maximumStartIndex) const;
    void resetMeasurements();
    void updateMeasurements();

    bool processTouch();
    bool isVerticalScaleTouch(uint16_t x, uint16_t y) const;
    bool isHorizontalScaleTouch(uint16_t x, uint16_t y) const;
    void stepVerticalScale();
    void stepTimebase();
    void reverseControlDirection(ScopeControl control);
    void highlightControl(ScopeControl control);
    bool isControlHighlighted(ScopeControl control) const;

    void drawHorizontalScaleLabel();
    void drawVerticalScaleLabel();
    void drawMeasurements();
    void drawScreen();
    void drawGrid();
    void drawWaveform();
    void drawScopeControls();
    void drawScopeControlsToSprite();

    // Desenarea fără flicker în sprite
    bool createWaveSprite();
    void drawGridToSprite();
    void drawTraceToSprite(
        const int16_t *samples,
        uint16_t sampleCount);

    int sampleToY(int16_t sample) const;
    int sampleToSpriteY(int16_t sample) const;
    float nativeVoltsPerDivision() const;
    float selectedVoltsPerDivision() const;
    float verticalVisualGain() const;

    uint8_t timebaseIndex = 1;
    // Index 1 is the calibrated native ~0.21 V/div range.
    uint8_t scopeVoltsPerDivIndex = 1;
    bool verticalArrowPointsUp = true;
    bool horizontalArrowPointsRight = false;
    ScopeControl pressedScopeControl = ScopeControl::None;
    ScopeControl highlightedScopeControl = ScopeControl::None;
    uint32_t scopeControlPressStartedMs = 0;
    uint32_t scopeControlHighlightUntilMs = 0;
    bool scopeControlLongPressHandled = false;
    uint16_t waveformSampleCount = 400;
    uint32_t waveformSampleRateHz = 40000;
    float signalFrequencyHz = 0.0f;
    float signalPeriodUs = 0.0f;
    float signalVppVolts = 0.0f;
    float signalVrmsAcVolts = 0.0f;
    float signalVmaxVolts = 0.0f;
    float signalVminVolts = 0.0f;
    float signalVdcVolts = 0.0f;
    bool frequencyMeasurementValid = false;
    bool amplitudeMeasurementValid = false;
    bool frequencyMeasurementInitialized = false;
    bool amplitudeMeasurementInitialized = false;

    int16_t captureSamples[MAX_CAPTURE_SAMPLE_COUNT] = {0};
    uint16_t displayedSampleOffset = 0;
    bool captureFrameValid = false;

    char displayedVerticalScaleText[24] = {0};
    char displayedHorizontalScaleText[24] = {0};
    char displayedMeasurementsText[80] = {0};
    bool waveSpriteReady = false;
};

extern Waveform waveform;

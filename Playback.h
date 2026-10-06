//======== Palyback.h ======
#pragma once
#include "Arduino.h"
constexpr uint16_t VU_SAMPLE_COUNT = 1024;
#define AUDIO_PIN_L PA0
#define AUDIO_PIN_R PA1

class Playback
{
public:
    void begin();
    void run();
    // void drawVUmeter(int x, int y, int barPixels, int peakPixels, bool isOver);
    void drawVUmeter(
        int x,
        int y,
        int barPixels,
        int peakPixels,
        bool isOver,
        bool showRecordTarget = false);
    void runAlignment();
    // Alignment UI helpers
    void drawAlignmentScreen();
    void updateAlignmentDisplay();

private:
    void updateLevels();
    void updateDisplay();
    void processTouch();
    void drawScreen();
    void drawFrequencyLabel();
    void drawChannelDifferenceValue();
    bool alignmentMode = false;
    bool running = false;

    // Audio measurements
    float levelL = 0.0f;
    float levelR = 0.0f;
    float newLevelL = 0.0f;
    float newLevelR = 0.0f;

    // Peak
    float peakL = -40.0f;
    float peakR = -40.0f;
    int peakWL = 0;
    int peakWR = 0;
    uint32_t peakTimerL = 0;
    uint32_t peakTimerR = 0;
    int lastBarWL = 0;
    int lastBarWR = 0;
    int zeroDbPos;

    // Display
    int barWL = 0;
    int barWR = 0;
    uint16_t signalFrequencyHz = 0;
    uint16_t displayedFrequencyHz = 0;
    bool frequencyDisplayInitialized = false;
    uint32_t frequencyLastRefreshMs = 0;
    int16_t leftSamples[VU_SAMPLE_COUNT] = {0};

    // Filters
    float smoothedLevelL = -40.0f;
    float smoothedLevelR = -40.0f;
    // Balance
    float balance = 0.0f;
    int balancePos = 0;

    // Variabile OVER
    bool overL = false;
    bool overR = false;

    uint32_t overTimerL = 0;
    uint32_t overTimerR = 0;
    const uint16_t OVER_HOLD = 1500; // 1.5 secunde OVER-HOLD

    int16_t rightSamples[VU_SAMPLE_COUNT] = {0};

    float channelDifferenceDb = 0.0f;
    float azimuthPhaseDegrees = 0.0f;
    bool azimuthPhaseValid = false;
    // Alignment UI state
    bool alignmentDialDrawn = false;
    float needleVisualAngleDeg = 0.0f; // smoothed visual angle for needle
    int needlePrevX = -1;
    int needlePrevY = -1;
    const float alignmentDeadbandDeg = 0.3f;
    const float alignmentSmoothingAlpha = 0.25f;
    // Geometry
    int alignPivotX = 160;
    int alignPivotY = 202;
    int alignRadius = 80; // visual radius for arc
    int alignNeedleLength = 72;

    int alignFreqValueX = 0;
    int alignFreqValueWidth = 0;

    int alignPhaseValueX = 0;
    int alignPhaseValueWidth = 0;

    uint16_t displayedAlignmentFrequency = 0xFFFF;
    int16_t displayedAlignmentPhaseTenths = 32767;
    bool displayedAlignmentPhaseValid = false;
};

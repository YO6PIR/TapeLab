#pragma once

#include <Arduino.h>

class Transport
{
public:
    void run();

private:
    static constexpr uint16_t FREQUENCY_SAMPLE_COUNT = 1024;
    static constexpr uint16_t MAX_DEVIATION_POINTS = 16;
    float deviationPoints[MAX_DEVIATION_POINTS] = {0.0f};
    uint16_t deviationPointCount = 0;

    void drawScreen();
    void redrawUi();
    void captureMeasuredFrequency();
    void drawMeasuredFrequency(bool force = false);
    void drawSpeed(bool force = false);
    void drawError(bool force = false);

    // Graficul abaterii vitezei
    void drawDeviationGraphFrame();
    void drawDeviationPoint(float deviationPercent);
    void drawGraphHistory();
    int deviationToY(float deviationPercent) const;
    void restoreGraphColumn(int x);

    int16_t frequencySamples[FREQUENCY_SAMPLE_COUNT] = {0};

    float measuredFrequencyHz = 0.0f;
    // calcul frecventa interpolare
    float interpolatedFrequencyHz = 0.0f;

    float speedCmPerSecond = 0.0f;
    float errorPercent = 0.0f;

    int32_t displayedMeasuredTenths = -1;
    int32_t displayedSpeedHundredths = -1;
    int32_t displayedErrorThousandths = -2147483647L;

    uint32_t lastSpeedErrorRefreshMs = 0;

    // Starea graficului
    int graphWriteX = 0;
    int previousGraphX = 0;
    int previousGraphY = 0;
    bool hasPreviousGraphPoint = false;
    static constexpr uint16_t GRAPH_HISTORY_SIZE = 280;
    float graphHistory[GRAPH_HISTORY_SIZE] = {0.0f};
    bool graphHistoryValid[GRAPH_HISTORY_SIZE] = {false};
    bool graphHistoryBreak[GRAPH_HISTORY_SIZE] = {false};

    // W&F RMS neponderat
    float wfPreviousInput = 0.0f;
    float wfHighPassOutput = 0.0f;
    float wfMeanSquare = 0.0f;

    float wowFlutterRmsPercent = 0.0f;

    float correctedWowFlutterRmsPercent = 0.0f;

    bool wfFilterInitialized = false;

    int32_t displayedWfThousandths = -1;
    uint32_t lastWfDisplayRefreshMs = 0;

    void updateWowFlutterRms(float deviationPercent);
    void drawWowFlutterRms(bool force = false);
    void resetWowFlutterRms();
    void advanceEmptyGraphPoint();
};

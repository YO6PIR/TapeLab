#pragma once

#include <Arduino.h>

namespace SignalFrequency
{
    constexpr uint32_t SAMPLE_RATE_HZ = 40000;
    constexpr uint32_t SAMPLE_PERIOD_US = 1000000UL / SAMPLE_RATE_HZ;
    constexpr int16_t CROSSING_THRESHOLD = 30;

    uint16_t measure(const int16_t *samples, uint16_t sampleCount);
    float latestMeasurementHz();

    // Măsurare cu interpolarea poziției trecerilor prin zero.
    float measureInterpolated(
        const int16_t *samples,
        uint16_t sampleCount);

    float measureInterpolated(
        const int16_t *samples,
        uint16_t sampleCount,
        uint32_t sampleRateHz);

    uint16_t extractDeviationPoints(
        const int16_t *samples,
        uint16_t sampleCount,
        float referenceFrequencyHz,
        float *outputDeviations,
        uint16_t maximumPoints,
        uint8_t periodsPerPoint = 8);

    void setCalibrationPpm(uint32_t calibrationPpm);
    uint32_t calibrationPpm();

    float measureRaw(
        const int16_t *samples,
        uint16_t sampleCount);
    
    float measureInterpolatedRaw(
    const int16_t *samples,
    uint16_t sampleCount);

}

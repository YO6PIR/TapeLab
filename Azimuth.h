#pragma once

#include <Arduino.h>

class Azimuth
{
public:
    void process(
        const int16_t *leftSamples,
        const int16_t *rightSamples,
        uint16_t sampleCount,
        float sampleRateHz);

    bool valid() const;
    float phaseDegrees() const;

private:
    bool measurementValid = false;
    float measuredPhaseDegrees = 0.0f;

    static bool findAverageRisingCrossingOffset(
        const int16_t *leftSamples,
        const int16_t *rightSamples,
        uint16_t sampleCount,
        float expectedPeriodSamples,
        float &offsetSamples);
};

extern Azimuth azimuth;
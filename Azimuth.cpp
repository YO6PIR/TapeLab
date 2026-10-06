#include "Azimuth.h"
#include "SignalFrequency.h"
#include <math.h>

Azimuth azimuth;

namespace
{
    constexpr float MIN_VALID_FREQUENCY_HZ = 7000.0f;
    constexpr float MAX_VALID_FREQUENCY_HZ = 15000.0f;

    constexpr uint16_t MAX_CROSSINGS = 256;
    constexpr float PHASE_SMOOTHING = 0.25f;
    //constanta de compensare azimut citit succesiv L→R
    constexpr float PHASE_OFFSET_DEGREES = -13.9f;

    float interpolateRisingCrossing(
        int16_t previousSample,
        int16_t currentSample,
        uint16_t currentIndex)
    {
        const float previous =
            static_cast<float>(previousSample);

        const float current =
            static_cast<float>(currentSample);

        const float difference =
            current - previous;

        if (difference <= 0.0f)
            return static_cast<float>(currentIndex);

        const float fraction =
            -previous / difference;

        return static_cast<float>(currentIndex - 1) +
               fraction;
    }

    uint16_t collectRisingCrossings(
        const int16_t *samples,
        uint16_t sampleCount,
        float *crossings,
        uint16_t maximumCrossings)
    {
        if (samples == nullptr ||
            crossings == nullptr ||
            sampleCount < 2)
        {
            return 0;
        }

        bool armed = false;
        uint16_t crossingCount = 0;

        for (uint16_t i = 1;
             i < sampleCount &&
             crossingCount < maximumCrossings;
             ++i)
        {
            if (samples[i] <=
                -SignalFrequency::CROSSING_THRESHOLD)
            {
                armed = true;
            }

            if (armed &&
                samples[i - 1] < 0 &&
                samples[i] >= 0)
            {
                crossings[crossingCount++] =
                    interpolateRisingCrossing(
                        samples[i - 1],
                        samples[i],
                        i);

                armed = false;
            }
        }

        return crossingCount;
    }

    float wrapSampleOffset(
        float offsetSamples,
        float periodSamples)
    {
        const float halfPeriod =
            periodSamples * 0.5f;

        while (offsetSamples > halfPeriod)
            offsetSamples -= periodSamples;

        while (offsetSamples < -halfPeriod)
            offsetSamples += periodSamples;

        return offsetSamples;
    }
}

void Azimuth::process(
    const int16_t *leftSamples,
    const int16_t *rightSamples,
    uint16_t sampleCount,
    float sampleRateHz)
{
    measurementValid = false;

    if (leftSamples == nullptr ||
        rightSamples == nullptr ||
        sampleCount < 16 ||
        sampleRateHz <= 0.0f)
    {
        return;
    }

    const float frequencyHz =
        SignalFrequency::measureInterpolated(
            leftSamples,
            sampleCount);

    if (frequencyHz < MIN_VALID_FREQUENCY_HZ ||
        frequencyHz > MAX_VALID_FREQUENCY_HZ)
    {
        return;
    }

    const float periodSamples =
        sampleRateHz / frequencyHz;

    float offsetSamples = 0.0f;

    if (!findAverageRisingCrossingOffset(
            leftSamples,
            rightSamples,
            sampleCount,
            periodSamples,
            offsetSamples))
    {
        return;
    }

    const float rawPhaseDegrees =
        offsetSamples *
        360.0f /
        periodSamples;

    const float correctedPhaseDegrees =
    rawPhaseDegrees -
    PHASE_OFFSET_DEGREES;    

    measuredPhaseDegrees =
        PHASE_SMOOTHING * correctedPhaseDegrees +
        (1.0f - PHASE_SMOOTHING) *
            measuredPhaseDegrees;

    measurementValid = true;
}

bool Azimuth::findAverageRisingCrossingOffset(
    const int16_t *leftSamples,
    const int16_t *rightSamples,
    uint16_t sampleCount,
    float expectedPeriodSamples,
    float &offsetSamples)
{
    static float leftCrossings[MAX_CROSSINGS];
    static float rightCrossings[MAX_CROSSINGS];

    const uint16_t leftCount =
        collectRisingCrossings(
            leftSamples,
            sampleCount,
            leftCrossings,
            MAX_CROSSINGS);

    const uint16_t rightCount =
        collectRisingCrossings(
            rightSamples,
            sampleCount,
            rightCrossings,
            MAX_CROSSINGS);

    if (leftCount < 4 || rightCount < 4)
        return false;

    uint16_t leftIndex = 0;
    uint16_t rightIndex = 0;
    uint16_t pairCount = 0;
    float offsetSum = 0.0f;

    const float maximumPairDistance =
        expectedPeriodSamples * 0.5f;

    while (leftIndex < leftCount &&
           rightIndex < rightCount)
    {
        float difference =
            rightCrossings[rightIndex] -
            leftCrossings[leftIndex];

        difference =
            wrapSampleOffset(
                difference,
                expectedPeriodSamples);

        if (fabsf(difference) <=
            maximumPairDistance)
        {
            offsetSum += difference;
            pairCount++;

            leftIndex++;
            rightIndex++;
        }
        else if (
            rightCrossings[rightIndex] <
            leftCrossings[leftIndex])
        {
            rightIndex++;
        }
        else
        {
            leftIndex++;
        }
    }

    if (pairCount < 4)
        return false;

    offsetSamples =
        offsetSum /
        static_cast<float>(pairCount);

    return true;
}

bool Azimuth::valid() const
{
    return measurementValid;
}

float Azimuth::phaseDegrees() const
{
    return measuredPhaseDegrees;
}
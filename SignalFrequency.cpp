#include "SignalFrequency.h"

namespace
{
    float latestFrequencyHz = 0.0f;
    uint32_t currentCalibrationPpm = 1000000;

    float applyFrequencyCalibration(float rawHz)
    {
        return rawHz *
               static_cast<float>(
                   currentCalibrationPpm) /
               1000000.0f;
    }
}

// Measures the average period between rising threshold crossings. Using a
// complete period instead of the raw number of zero crossings makes the result
// independent of the capture-window boundaries.
uint16_t SignalFrequency::measure(
    const int16_t *samples,
    uint16_t sampleCount)
{
    const float rawHz =
        measureRaw(samples, sampleCount);

    if (rawHz <= 0.0f)
    {
        latestFrequencyHz = 0.0f;
        return 0;
    }

    latestFrequencyHz =
        applyFrequencyCalibration(rawHz);

    return static_cast<uint16_t>(
        latestFrequencyHz + 0.5f);
}

float SignalFrequency::latestMeasurementHz()
{
    return latestFrequencyHz;
}

float SignalFrequency::measureInterpolatedRaw(
    const int16_t *samples,
    uint16_t sampleCount)
{
    if (samples == nullptr || sampleCount < 2)
        return 0.0f;

    bool sawNegative = false;
    bool hasPreviousCrossing = false;

    float previousCrossing = 0.0f;
    float periodSum = 0.0f;
    uint16_t periodCount = 0;

    for (uint16_t i = 1; i < sampleCount; ++i)
    {
        if (samples[i] <= -CROSSING_THRESHOLD)
            sawNegative = true;

        if (sawNegative &&
            samples[i - 1] < 0 &&
            samples[i] >= 0)
        {
            const float previousSample =
                static_cast<float>(samples[i - 1]);

            const float currentSample =
                static_cast<float>(samples[i]);

            const float difference =
                currentSample - previousSample;

            if (difference > 0.0f)
            {
                const float fraction =
                    -previousSample / difference;

                const float crossingPosition =
                    static_cast<float>(i - 1) +
                    fraction;

                if (hasPreviousCrossing)
                {
                    const float period =
                        crossingPosition -
                        previousCrossing;
                    /*
                           Domeniu general pentru Playback și Waveform.

                           La 40 kHz:
                           2.5 samples  ≈ 16 kHz
                           400 samples  ≈ 100 Hz
                        */
                    if (period >= 2.6f &&
                        period <= 400.0f)
                    {
                        periodSum += period;
                        periodCount++;
                    }
                }

                previousCrossing = crossingPosition;
                hasPreviousCrossing = true;
                sawNegative = false;
            }
        }
    }

    if (periodCount == 0 || periodSum <= 0.0f)
        return 0.0f;

    return static_cast<float>(SAMPLE_RATE_HZ) *
           static_cast<float>(periodCount) /
           periodSum;
}

float SignalFrequency::measureInterpolated(
    const int16_t *samples,
    uint16_t sampleCount)
{
    return measureInterpolated(
        samples,
        sampleCount,
        SAMPLE_RATE_HZ);
}

float SignalFrequency::measureInterpolated(
    const int16_t *samples,
    uint16_t sampleCount,
    uint32_t sampleRateHz)
{
    if (sampleRateHz == 0)
        return 0.0f;

    const float rawAtDefaultRateHz =
        measureInterpolatedRaw(
            samples,
            sampleCount);

    if (rawAtDefaultRateHz <= 0.0f)
        return 0.0f;

    const float rawHz =
        rawAtDefaultRateHz *
        static_cast<float>(sampleRateHz) /
        static_cast<float>(SAMPLE_RATE_HZ);

    return applyFrequencyCalibration(rawHz);
}

uint16_t SignalFrequency::extractDeviationPoints(
    const int16_t *samples,
    uint16_t sampleCount,
    float referenceFrequencyHz,
    float *outputDeviations,
    uint16_t maximumPoints,
    uint8_t periodsPerPoint)
{
    if (samples == nullptr ||
        outputDeviations == nullptr ||
        sampleCount < 2 ||
        maximumPoints == 0 ||
        periodsPerPoint == 0 ||
        referenceFrequencyHz <= 0.0f)
    {
        return 0;
    }

    bool sawNegative = false;
    bool hasPreviousCrossing = false;

    float previousCrossing = 0.0f;

    float groupPeriodSum = 0.0f;
    uint8_t groupPeriodCount = 0;

    uint16_t outputCount = 0;

    for (uint16_t i = 1; i < sampleCount; i++)
    {
        if (samples[i] <= -CROSSING_THRESHOLD)
        {
            sawNegative = true;
        }

        if (sawNegative &&
            samples[i - 1] < 0 &&
            samples[i] >= 0)
        {
            const float previousSample =
                static_cast<float>(samples[i - 1]);

            const float currentSample =
                static_cast<float>(samples[i]);

            const float difference =
                currentSample - previousSample;

            if (difference > 0.0f)
            {
                const float fraction =
                    -previousSample / difference;

                const float crossingPosition =
                    static_cast<float>(i - 1) + fraction;

                if (hasPreviousCrossing)
                {
                    const float periodSamples =
                        crossingPosition - previousCrossing;

                    /*
                       Pentru 3150 Hz la 40 kHz:
                       perioada normală este aproximativ 12,7 samples.

                       Intervalul 8...20 respinge trecerile false.
                    */
                    if (periodSamples >= 8.0f &&
                        periodSamples <= 20.0f)
                    {
                        groupPeriodSum += periodSamples;
                        groupPeriodCount++;

                        if (groupPeriodCount >= periodsPerPoint)
                        {
                            const float averagePeriodSamples =
                                groupPeriodSum /
                                static_cast<float>(groupPeriodCount);

                            const float rawFrequencyHz =
                                static_cast<float>(SAMPLE_RATE_HZ) /
                                averagePeriodSamples;

                            const float frequencyHz =
                                applyFrequencyCalibration(
                                    rawFrequencyHz);

                            const float deviationPercent =
                                ((frequencyHz -
                                  referenceFrequencyHz) /
                                 referenceFrequencyHz) *
                                100.0f;

                            outputDeviations[outputCount] =
                                deviationPercent;

                            outputCount++;

                            groupPeriodSum = 0.0f;
                            groupPeriodCount = 0;

                            if (outputCount >= maximumPoints)
                            {
                                return outputCount;
                            }
                        }
                    }
                    else
                    {
                        /*
                           Dacă apare o perioadă evident invalidă,
                           abandonăm grupul curent.
                        */
                        groupPeriodSum = 0.0f;
                        groupPeriodCount = 0;
                    }
                }

                previousCrossing = crossingPosition;
                hasPreviousCrossing = true;
                sawNegative = false;
            }
        }
    }

    return outputCount;
}

void SignalFrequency::setCalibrationPpm(
    uint32_t calibrationPpm)
{
    // Limită defensivă: ±2 %
    if (calibrationPpm >= 950000 &&
        calibrationPpm <= 1050000)
    {
        currentCalibrationPpm =
            calibrationPpm;
    }
}

uint32_t SignalFrequency::calibrationPpm()
{
    return currentCalibrationPpm;
}

float SignalFrequency::measureRaw(
    const int16_t *samples,
    uint16_t sampleCount)
{
    bool sawNegative = false;
    bool hasPreviousCrossing = false;
    uint16_t previousCrossing = 0;
    uint32_t periodSum = 0;
    uint16_t periodCount = 0;

    for (uint16_t i = 0; i < sampleCount; i++)
    {
        if (samples[i] <= -CROSSING_THRESHOLD)
        {
            sawNegative = true;
        }
        else if (sawNegative &&
                 samples[i] >= CROSSING_THRESHOLD)
        {
            if (hasPreviousCrossing)
            {
                periodSum += i - previousCrossing;
                periodCount++;
            }

            previousCrossing = i;
            hasPreviousCrossing = true;
            sawNegative = false;
        }
    }

    if (periodCount == 0 || periodSum == 0)
        return 0.0f;

    return static_cast<float>(SAMPLE_RATE_HZ) *
           static_cast<float>(periodCount) /
           static_cast<float>(periodSum);
}

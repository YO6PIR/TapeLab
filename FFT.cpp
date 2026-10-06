/*conține fereastra Hamming, transformata FFT,
gruparea logaritmică 150 Hz–18 kHz,
conversia RMS/dB, smoothing și peak hold.*/

#include "FFT.h"
#include "Config.h"
#include "SetAudio.h"

#include <CMSIS_DSP.h>
#include <math.h>

namespace
{
    static_assert(
        SPECTRUM_BAR_DECAY_FRAMES > 0,
        "SPECTRUM_BAR_DECAY_FRAMES must be at least 1");
    static_assert(
        SPECTRUM_BAR_DECAY_BRICKS > 0,
        "SPECTRUM_BAR_DECAY_BRICKS must be at least 1");
    static_assert(
        SPECTRUM_PEAK_DECAY_FRAMES > 0,
        "SPECTRUM_PEAK_DECAY_FRAMES must be at least 1");
    static_assert(
        SPECTRUM_PEAK_DECAY_BRICKS > 0,
        "SPECTRUM_PEAK_DECAY_BRICKS must be at least 1");

    constexpr float SPECTRUM_MIN_HZ = 150.0f;
    constexpr float SPECTRUM_MAX_HZ = 18000.0f;
    constexpr uint8_t MAX_BRICKS = 32;
    constexpr uint8_t UNUSED_BAND = 0xFF;
    constexpr float MIN_DB = -30.0f;
    constexpr float MAX_DB = 10.0f;
    constexpr float HAMMING_RMS_GAIN = 0.62979f;
    constexpr float FFT_TO_RMS = 1.41421356f /
                                 (FFTAnalyzer::SAMPLES * HAMMING_RMS_GAIN);

    arm_rfft_fast_instance_f32 rfft;
    float32_t fftInput[FFTAnalyzer::SAMPLES];
    float32_t fftOutput[FFTAnalyzer::SAMPLES];
    float32_t hammingWindow[FFTAnalyzer::SAMPLES];
    uint8_t bandForBin[FFTAnalyzer::BIN_COUNT];
}

FFTAnalyzer fftAnalyzer;
uint8_t peakBands[FFT_BANDS] = {0};
extern SetAudio setAudio;

bool FFTAnalyzer::initialize()
{
    if (initialized)
        return true;

    if (arm_rfft_fast_init_512_f32(&rfft) != ARM_MATH_SUCCESS)
        return false;

    constexpr float HAMMING_TWO_PI = 6.28318530717958647692f;
    for (uint16_t i = 0; i < SAMPLES; ++i)
    {
        hammingWindow[i] =
            0.54f -
            0.46f * cosf(
                        HAMMING_TWO_PI * static_cast<float>(i) /
                        static_cast<float>(SAMPLES - 1));
    }

    const float bandRatio =
        powf(
            SPECTRUM_MAX_HZ / SPECTRUM_MIN_HZ,
            1.0f / FFT_BANDS);
    const float inverseLogBandRatio = 1.0f / logf(bandRatio);

    for (uint16_t bin = 0; bin < BIN_COUNT; ++bin)
    {
        bandForBin[bin] = UNUSED_BAND;
        const float frequency = bin * BIN_WIDTH_HZ;
        if (frequency < SPECTRUM_MIN_HZ ||
            frequency > SPECTRUM_MAX_HZ)
            continue;

        uint8_t band = static_cast<uint8_t>(
            logf(frequency / SPECTRUM_MIN_HZ) *
            inverseLogBandRatio);
        if (band >= FFT_BANDS)
            band = FFT_BANDS - 1;

        bandForBin[bin] = band;
    }

    initialized = true;
    return true;
}

void FFTAnalyzer::calculate(const int16_t samples[SAMPLES],
                            uint8_t spectrumBands[FFT_BANDS])
{
    if (samples == nullptr ||
        spectrumBands == nullptr ||
        !initialize())
        return;

    int32_t sampleSum = 0;
    for (uint16_t i = 0; i < SAMPLES; i++)
        sampleSum += samples[i];

    const float32_t blockMean =
        static_cast<float32_t>(sampleSum) /
        static_cast<float32_t>(SAMPLES);

    for (uint16_t i = 0; i < SAMPLES; ++i)
    {
        fftInput[i] =
            (static_cast<float32_t>(samples[i]) - blockMean) *
            hammingWindow[i];
    }

    arm_rfft_fast_f32(&rfft, fftInput, fftOutput, 0);

    // CMSIS packs DC in output[0] and Nyquist in output[1]. Bins 1..255
    // follow as interleaved real/imaginary pairs.
    fftBins[0] = fabsf(fftOutput[0]);
    arm_cmplx_mag_f32(
        &fftOutput[2],
        &fftBins[1],
        BIN_COUNT - 1);

    float32_t bandPower[FFT_BANDS] = {0.0f};

    for (uint16_t bin = 1; bin < BIN_COUNT; ++bin)
    {
        const uint8_t band = bandForBin[bin];
        if (band == UNUSED_BAND)
            continue;

        const float32_t magnitude = fftBins[bin];
        bandPower[band] += magnitude * magnitude;
    }

    const float rmsReference = setAudio.rmsReference();
    for (uint8_t band = 0; band < FFT_BANDS; band++)
    {
        float32_t magnitude = 0.0f;
        arm_sqrt_f32(bandPower[band], &magnitude);
        const float rms = magnitude * FFT_TO_RMS;

        float db = MIN_DB;
        if (rms > 0.0001f &&
            rmsReference > 0.0f &&
            isfinite(rms) &&
            isfinite(rmsReference))
            db = 20.0f * log10f(rms / rmsReference);

        if (!isfinite(db))
            db = MIN_DB;

        if (db < MIN_DB)
            db = MIN_DB;
        if (db > MAX_DB)
            db = MAX_DB;

        const uint8_t mappedLevel = (uint8_t)(((db - MIN_DB) / (MAX_DB - MIN_DB)) * MAX_BRICKS + 0.5f);

        if (mappedLevel > spectrumBands[band])
        {
            spectrumBands[band] = mappedLevel;
            barDecayCounter[band] = 0;
        }
        else if (mappedLevel < spectrumBands[band] &&
                 spectrumBands[band] > 0)
        {
            barDecayCounter[band]++;
            if (barDecayCounter[band] >= SPECTRUM_BAR_DECAY_FRAMES)
            {
                const uint8_t levelDifference =
                    spectrumBands[band] - mappedLevel;
                const uint8_t decayAmount =
                    levelDifference < SPECTRUM_BAR_DECAY_BRICKS
                        ? levelDifference
                        : SPECTRUM_BAR_DECAY_BRICKS;
                spectrumBands[band] -= decayAmount;
                barDecayCounter[band] = 0;
            }
        }
        else
        {
            barDecayCounter[band] = 0;
        }

        if (spectrumBands[band] > MAX_BRICKS)
            spectrumBands[band] = MAX_BRICKS;

        updatePeak(band, spectrumBands[band]);
    }
}

const float *FFTAnalyzer::binMagnitudes() const
{
    return fftBins;
}

float FFTAnalyzer::integratedRms(
    uint16_t firstBin,
    uint16_t lastBin) const
{
    if (firstBin >= BIN_COUNT ||
        lastBin >= BIN_COUNT ||
        firstBin > lastBin)
        return NAN;

    float32_t power = 0.0f;
    for (uint16_t bin = firstBin; bin <= lastBin; ++bin)
    {
        const float32_t magnitude = fftBins[bin];
        power += magnitude * magnitude;
    }

    float32_t magnitude = 0.0f;
    if (arm_sqrt_f32(power, &magnitude) != ARM_MATH_SUCCESS)
        return NAN;
    return magnitude * FFT_TO_RMS;
}

void FFTAnalyzer::updatePeak(uint8_t band, uint8_t level)
{
    if (level >= peakBands[band])
    {
        peakBands[band] = level;
        peakHoldCounter[band] = SPECTRUM_PEAK_HOLD_FRAMES;
        peakDecayCounter[band] = 0;
    }
    else if (peakHoldCounter[band] > 0)
    {
        peakHoldCounter[band]--;
    }
    else if (peakBands[band] > 0)
    {
        peakDecayCounter[band]++;
        if (peakDecayCounter[band] >= SPECTRUM_PEAK_DECAY_FRAMES)
        {
            const uint8_t levelDifference = peakBands[band] - level;
            const uint8_t decayAmount =
                levelDifference < SPECTRUM_PEAK_DECAY_BRICKS
                    ? levelDifference
                    : SPECTRUM_PEAK_DECAY_BRICKS;
            peakBands[band] -= decayAmount;
            peakDecayCounter[band] = 0;
        }
    }

    if (peakBands[band] > MAX_BRICKS)
        peakBands[band] = MAX_BRICKS;
}

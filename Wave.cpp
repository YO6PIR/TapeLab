#include "Wave.h"

#include "Audio.h"
#include "Config.h"
#include "Display.h"
#include "SetAudio.h"
#include "SignalFrequency.h"
#include "Touch.h"
#include "Buzzer.h"

#include <TFT_eSPI.h>
#include <math.h>
#include <string.h>

extern TFT_eSPI tft;
extern Audio audio;
extern Display display;
extern SetAudio setAudio;
extern Touch touch;
extern Buzzer buzzer;
static TFT_eSprite waveSprite(&tft);
static TFT_eSprite scopeVerticalScaleSprite(&tft);
static TFT_eSprite scopeMeasurementsSprite(&tft);

namespace
{
    constexpr int WAVE_X = 8;
    constexpr int WAVE_Y = 38;
    constexpr int WAVE_W = 304;
    constexpr int WAVE_H = 156;
    constexpr uint16_t WAVE_BACKGROUND_COLOR = TFT_BLACK;
    // Same RGB565 color used by Spectrum's inactive cyan bars.
    constexpr uint16_t COL_CYAN_OFF = 0x00A5;

    constexpr int WAVE_SPRITE_W = WAVE_W - 2;
    constexpr int WAVE_SPRITE_H = WAVE_H - 2;

    constexpr uint8_t SPRITE_COLOR_BACKGROUND = 0;
    constexpr uint8_t SPRITE_COLOR_GRID = 1;
    constexpr uint8_t SPRITE_COLOR_TRACE = 2;
    constexpr uint8_t SPRITE_COLOR_WHITE = 3;
    constexpr uint8_t SPRITE_COLOR_CYAN_OFF = 4;

    constexpr uint8_t TEXT_COLOR_BACKGROUND = 0;
    constexpr uint8_t TEXT_COLOR_CYAN = 1;
    constexpr uint8_t TEXT_COLOR_ORANGE = 2;
    constexpr uint8_t TEXT_COLOR_GREEN = 3;
    constexpr uint8_t TEXT_COLOR_MAGENTA = 4;
    constexpr uint8_t TEXT_COLOR_YELLOW = 5;

    uint16_t wavePalette[16] = {
        TFT_BLACK,    // 0 - fundal
        TFT_DARKGREY, // 1 - grilă
        TFT_GREEN,    // 2 - trasă
        TFT_WHITE,    // 3 - gradații
        COL_CYAN_OFF,
        TFT_BLACK,
        TFT_BLACK,
        TFT_BLACK,
        TFT_BLACK,
        TFT_BLACK,
        TFT_BLACK,
        TFT_BLACK,
        TFT_BLACK,
        TFT_BLACK,
        TFT_BLACK,
        TFT_BLACK};

    constexpr int GRID_COLUMNS = 10;
    constexpr int GRID_ROWS = 8;
    constexpr int Y_CONTROL_TOUCH_X_MIN = WAVE_X;
    constexpr int Y_CONTROL_TOUCH_X_MAX =
        WAVE_X + WAVE_W / GRID_COLUMNS;
    constexpr int Y_CONTROL_TOUCH_Y_MIN = WAVE_Y;
    constexpr int Y_CONTROL_TOUCH_Y_MAX =
        WAVE_Y + WAVE_H / GRID_ROWS;
    // The arrow stays centred in the bottom-right grid division.  Its touch
    // target is deliberately a little wider and taller to absorb the small
    // residual offset of the resistive touch calibration at that corner.
    constexpr int X_CONTROL_ARROW_X =
        WAVE_X + (WAVE_W * (2 * GRID_COLUMNS - 1)) /
                     (2 * GRID_COLUMNS);
    constexpr int X_CONTROL_ARROW_Y =
        WAVE_Y + (WAVE_H * (2 * GRID_ROWS - 1)) /
                     (2 * GRID_ROWS);
    constexpr int X_CONTROL_TOUCH_X_MIN = X_CONTROL_ARROW_X - 24;
    constexpr int X_CONTROL_TOUCH_X_MAX = X_CONTROL_ARROW_X + 19;
    constexpr int X_CONTROL_TOUCH_Y_MIN = X_CONTROL_ARROW_Y - 18;
    constexpr int X_CONTROL_TOUCH_Y_MAX = X_CONTROL_ARROW_Y + 12;
    constexpr int ZERO_DB_PEAK_ADC = 382;
    constexpr int CENTER_GRID_ROW = GRID_ROWS / 2;
    constexpr int TOP_ZERO_DB_GRID_ROW = 1;
    constexpr int BOTTOM_ZERO_DB_GRID_ROW = GRID_ROWS - 1;
    constexpr int POSITIVE_ZERO_DB_DIVISIONS =
        CENTER_GRID_ROW - TOP_ZERO_DB_GRID_ROW;
    constexpr int NEGATIVE_ZERO_DB_DIVISIONS =
        BOTTOM_ZERO_DB_GRID_ROW - CENTER_GRID_ROW;
    constexpr int CENTER_Y = WAVE_Y + WAVE_H / 2;
    constexpr int TOP_ZERO_DB_Y = WAVE_Y + WAVE_H / GRID_ROWS;
    constexpr int BOTTOM_ZERO_DB_Y =
        WAVE_Y + (WAVE_H * (GRID_ROWS - 1)) / GRID_ROWS;
    constexpr int POSITIVE_ZERO_DB_AMPLITUDE = CENTER_Y - TOP_ZERO_DB_Y;
    constexpr int NEGATIVE_ZERO_DB_AMPLITUDE = BOTTOM_ZERO_DB_Y - CENTER_Y;
    struct ScopeTimebase
    {
        uint16_t sampleCount;
        uint32_t sampleRateHz;
    };

    constexpr ScopeTimebase TIMEBASES[] = {
        {800, 40000},
        {400, 40000},
        {500, 100000},
        {400, 200000},
        {400, 400000},
        {200, 400000}};
    constexpr uint8_t TIMEBASE_COUNT =
        sizeof(TIMEBASES) / sizeof(TIMEBASES[0]);
    constexpr uint16_t WAVE_REFRESH_INTERVAL_MS = 50;
    constexpr int MEASUREMENTS_Y = WAVE_Y + WAVE_H + 5;
    constexpr int MEASUREMENTS_H = 9;
    constexpr int MEASUREMENTS_HORIZONTAL_PADDING = 10;
    constexpr int HORIZONTAL_SCALE_LABEL_W = 96;
    constexpr int VERTICAL_SCALE_LABEL_W = 96;
    constexpr int VERTICAL_SCALE_LABEL_H = 10;
    // Ordered from the coarsest visual range to the finest.  The calibrated
    // native range remains a sentinel so its value continues to come from the
    // existing ADC voltage calibration rather than a hard-coded 0.21 V/div.
    constexpr uint8_t NATIVE_VERTICAL_SCALE_INDEX = 1;
    constexpr float VERTICAL_SCALE_VOLTS_PER_DIV[] = {
        0.50f,
        0.0f,
        0.10f,
        0.05f,
        0.025f};
    constexpr uint8_t VERTICAL_SCALE_COUNT =
        sizeof(VERTICAL_SCALE_VOLTS_PER_DIV) /
        sizeof(VERTICAL_SCALE_VOLTS_PER_DIV[0]);
    constexpr uint32_t SCOPE_LONG_PRESS_MS = 600;
    constexpr uint32_t SCOPE_ARROW_HIGHLIGHT_MS = 125;
    constexpr float ZERO_DB_RMS_VOLTS = 0.35355339f;
    constexpr float MEASUREMENT_SMOOTHING_ALPHA = 0.25f;
    constexpr int16_t MINIMUM_MEASURABLE_VPP_COUNTS =
        SignalFrequency::CROSSING_THRESHOLD * 2;

    static_assert(
        POSITIVE_ZERO_DB_DIVISIONS == NEGATIVE_ZERO_DB_DIVISIONS,
        "Scope vertical renderer must be symmetric in grid divisions");

    bool formatUnsignedFixed(
        float value,
        uint8_t decimalPlaces,
        char *text,
        size_t textSize)
    {
        if (text == nullptr || textSize == 0 ||
            !isfinite(value) || value < 0.0f ||
            decimalPlaces > 3)
        {
            return false;
        }

        uint32_t scale = 1;
        for (uint8_t i = 0; i < decimalPlaces; ++i)
            scale *= 10;

        const uint32_t scaled = static_cast<uint32_t>(
            value * static_cast<float>(scale) + 0.5f);
        const uint32_t whole = scaled / scale;
        const uint32_t fraction = scaled % scale;

        switch (decimalPlaces)
        {
        case 0:
            snprintf(text, textSize, "%lu",
                     static_cast<unsigned long>(whole));
            break;
        case 1:
            snprintf(text, textSize, "%lu.%01lu",
                     static_cast<unsigned long>(whole),
                     static_cast<unsigned long>(fraction));
            break;
        case 2:
            snprintf(text, textSize, "%lu.%02lu",
                     static_cast<unsigned long>(whole),
                     static_cast<unsigned long>(fraction));
            break;
        default:
            snprintf(text, textSize, "%lu.%03lu",
                     static_cast<unsigned long>(whole),
                     static_cast<unsigned long>(fraction));
            break;
        }

        return true;
    }

    void formatCompactFrequency(
        float frequencyHz,
        char *valueText,
        size_t valueTextSize,
        const char *&unitText)
    {
        if (!isfinite(frequencyHz) || frequencyHz <= 0.0f)
        {
            snprintf(valueText, valueTextSize, "---");
            unitText = "Hz";
        }
        else if (frequencyHz >= 1000.0f)
        {
            if (formatUnsignedFixed(
                    frequencyHz / 1000.0f,
                    3,
                    valueText,
                    valueTextSize))
                unitText = "kHz";
            else
            {
                snprintf(valueText, valueTextSize, "---");
                unitText = "Hz";
            }
        }
        else
        {
            snprintf(
                valueText,
                valueTextSize,
                "%lu",
                static_cast<unsigned long>(frequencyHz + 0.5f));
            unitText = "Hz";
        }
    }

    void formatCompactPeriod(
        float periodUs,
        char *valueText,
        size_t valueTextSize,
        const char *&unitText)
    {
        if (!isfinite(periodUs) || periodUs <= 0.0f)
        {
            snprintf(valueText, valueTextSize, "---");
            unitText = "ms";
        }
        else if (periodUs >= 1000.0f)
        {
            if (formatUnsignedFixed(
                    periodUs / 1000.0f,
                    3,
                    valueText,
                    valueTextSize))
                unitText = "ms";
            else
            {
                snprintf(valueText, valueTextSize, "---");
                unitText = "ms";
            }
        }
        else
        {
            if (formatUnsignedFixed(
                    periodUs,
                    1,
                    valueText,
                    valueTextSize))
                unitText = "us";
            else
            {
                snprintf(valueText, valueTextSize, "---");
                unitText = "us";
            }
        }
    }

    void updateScopeTextPalette(TFT_eSprite &sprite)
    {
        sprite.setPaletteColor(TEXT_COLOR_BACKGROUND, COL_BG);
        sprite.setPaletteColor(TEXT_COLOR_CYAN, TFT_CYAN);
        sprite.setPaletteColor(TEXT_COLOR_ORANGE, TFT_ORANGE);
        sprite.setPaletteColor(TEXT_COLOR_GREEN, TFT_GREEN);
        sprite.setPaletteColor(TEXT_COLOR_MAGENTA, TFT_MAGENTA);
        sprite.setPaletteColor(TEXT_COLOR_YELLOW, TFT_YELLOW);
    }

    bool prepareScopeTextSprite(
        TFT_eSprite &sprite,
        int width,
        int height)
    {
        if (!sprite.created())
        {
            sprite.setColorDepth(4);
            if (sprite.createSprite(width, height) == nullptr)
                return false;
        }

        updateScopeTextPalette(sprite);
        sprite.setTextFont(1);
        sprite.setTextDatum(TL_DATUM);
        sprite.fillSprite(TEXT_COLOR_BACKGROUND);
        return true;
    }

    void prepareScopeTextSprites()
    {
        prepareScopeTextSprite(
            scopeVerticalScaleSprite,
            VERTICAL_SCALE_LABEL_W,
            VERTICAL_SCALE_LABEL_H);
        prepareScopeTextSprite(
            scopeMeasurementsSprite,
            WAVE_W,
            MEASUREMENTS_H);
    }

    void releaseScopeTextSprites()
    {
        if (scopeVerticalScaleSprite.created())
            scopeVerticalScaleSprite.deleteSprite();
        if (scopeMeasurementsSprite.created())
            scopeMeasurementsSprite.deleteSprite();
    }

    uint16_t scopeTextColor(uint8_t colorIndex)
    {
        switch (colorIndex)
        {
        case TEXT_COLOR_ORANGE:
            return TFT_ORANGE;
        case TEXT_COLOR_GREEN:
            return TFT_GREEN;
        case TEXT_COLOR_MAGENTA:
            return TFT_MAGENTA;
        case TEXT_COLOR_YELLOW:
            return TFT_YELLOW;
        default:
            return TFT_CYAN;
        }
    }

    template <typename Canvas>
    void drawHorizontalArrow(
        Canvas &canvas,
        int centerX,
        int centerY,
        bool pointsRight,
        uint32_t color)
    {
        constexpr int HALF_LENGTH = 7;
        constexpr int HEAD_SIZE = 4;
        canvas.drawFastHLine(
            centerX - HALF_LENGTH,
            centerY,
            HALF_LENGTH * 2 + 1,
            color);

        const int tipX =
            pointsRight
                ? centerX + HALF_LENGTH
                : centerX - HALF_LENGTH;
        const int headBaseX =
            pointsRight
                ? tipX - HEAD_SIZE
                : tipX + HEAD_SIZE;
        canvas.drawLine(tipX, centerY, headBaseX, centerY - HEAD_SIZE, color);
        canvas.drawLine(tipX, centerY, headBaseX, centerY + HEAD_SIZE, color);
    }

    template <typename Canvas>
    void drawVerticalArrow(
        Canvas &canvas,
        int centerX,
        int centerY,
        bool pointsUp,
        uint32_t color)
    {
        constexpr int HALF_LENGTH = 6;
        constexpr int HEAD_SIZE = 4;
        canvas.drawFastVLine(
            centerX,
            centerY - HALF_LENGTH,
            HALF_LENGTH * 2 + 1,
            color);

        const int tipY =
            pointsUp
                ? centerY - HALF_LENGTH
                : centerY + HALF_LENGTH;
        const int headBaseY =
            pointsUp
                ? tipY + HEAD_SIZE
                : tipY - HEAD_SIZE;
        canvas.drawLine(centerX, tipY, centerX - HEAD_SIZE, headBaseY, color);
        canvas.drawLine(centerX, tipY, centerX + HEAD_SIZE, headBaseY, color);
    }

    void fillControlCells()
    {
        const int cellWidth = WAVE_W / GRID_COLUMNS - 1;
        const int cellHeight = WAVE_H / GRID_ROWS - 1;
        const int bottomCellX =
            WAVE_X + (WAVE_W * (GRID_COLUMNS - 1)) / GRID_COLUMNS + 1;
        const int bottomCellY =
            WAVE_Y + (WAVE_H * (GRID_ROWS - 1)) / GRID_ROWS + 1;

        tft.fillRect(
            WAVE_X + 1,
            WAVE_Y + 1,
            cellWidth,
            cellHeight,
            COL_CYAN_OFF);
        tft.fillRect(
            bottomCellX,
            bottomCellY,
            cellWidth,
            cellHeight,
            COL_CYAN_OFF);
    }

    void fillControlCellsToSprite()
    {
        const int cellWidth = WAVE_W / GRID_COLUMNS - 1;
        const int cellHeight = WAVE_H / GRID_ROWS - 1;
        const int bottomCellX =
            (WAVE_W * (GRID_COLUMNS - 1)) / GRID_COLUMNS;
        const int bottomCellY =
            (WAVE_H * (GRID_ROWS - 1)) / GRID_ROWS;

        waveSprite.fillRect(
            0,
            0,
            cellWidth,
            cellHeight,
            SPRITE_COLOR_CYAN_OFF);
        waveSprite.fillRect(
            bottomCellX,
            bottomCellY,
            cellWidth,
            cellHeight,
            SPRITE_COLOR_CYAN_OFF);
    }
}

void Waveform::begin()
{
    waveformSampleCount =
        TIMEBASES[timebaseIndex].sampleCount;
    waveformSampleRateHz =
        TIMEBASES[timebaseIndex].sampleRateHz;
    resetMeasurements();
    displayedHorizontalScaleText[0] = '\0';
    pressedScopeControl = ScopeControl::None;
    highlightedScopeControl = ScopeControl::None;
    scopeControlPressStartedMs = 0;
    scopeControlHighlightUntilMs = 0;
    scopeControlLongPressHandled = false;

    waveSpriteReady = createWaveSprite();
    prepareScopeTextSprites();
}

// Opens the waveform analyzer and returns to the menu after a touch.
void Waveform::run()
{
    audio.stopCapture();
    begin();
    drawScreen();

    while (true)
    {
        const uint32_t frameStart = millis();
        buzzer.update();
        captureLeftSignal();
        drawWaveform();
        display.updateClock();

        if (processTouch())
            break;

        const uint32_t frameElapsed = millis() - frameStart;
        if (frameElapsed < WAVE_REFRESH_INTERVAL_MS)
            delay(WAVE_REFRESH_INTERVAL_MS - frameElapsed);
    }

    audio.stopCapture();
    releaseScopeTextSprites();
}

// Captures two windows from the Left ADC and points the renderer at the
// auto-triggered view inside that same buffer.
void Waveform::captureLeftSignal()
{
    const uint16_t captureSampleCount = waveformSampleCount * 2;

    if (!audio.captureLeftBlock(
            captureSamples,
            captureSampleCount,
            waveformSampleRateHz))
    {
        captureFrameValid = false;
        return;
    }

    const uint16_t triggerIndex = findAutoTrigger(waveformSampleCount);
    displayedSampleOffset = triggerIndex;
    captureFrameValid = true;

    updateMeasurements();
}

// Finds a rising zero-crossing; falls back to the steepest rising edge.
uint16_t Waveform::findAutoTrigger(uint16_t maximumStartIndex) const
{
    constexpr int16_t TRIGGER_THRESHOLD = 30;
    bool sawNegative = false;
    int32_t largestRise = -2147483647L - 1L;
    uint16_t fallbackIndex = 0;

    for (uint16_t i = 1; i <= maximumStartIndex; i++)
    {
        const int32_t rise =
            (int32_t)captureSamples[i] - captureSamples[i - 1];
        if (rise > largestRise)
        {
            largestRise = rise;
            fallbackIndex = i;
        }

        if (captureSamples[i] <= -TRIGGER_THRESHOLD)
        {
            sawNegative = true;
        }
        else if (sawNegative && captureSamples[i] >= TRIGGER_THRESHOLD)
        {
            return i;
        }
    }

    return fallbackIndex;
}

void Waveform::resetMeasurements()
{
    signalFrequencyHz = 0.0f;
    signalPeriodUs = 0.0f;
    signalVppVolts = 0.0f;
    signalVrmsAcVolts = 0.0f;
    signalVmaxVolts = 0.0f;
    signalVminVolts = 0.0f;
    signalVdcVolts = 0.0f;
    frequencyMeasurementValid = false;
    amplitudeMeasurementValid = false;
    frequencyMeasurementInitialized = false;
    amplitudeMeasurementInitialized = false;
    displayedVerticalScaleText[0] = '\0';
    displayedMeasurementsText[0] = '\0';
}

// Derives all automatic measurements from the DMA block already captured.
void Waveform::updateMeasurements()
{
    const uint16_t captureSampleCount = waveformSampleCount * 2;
    const float measuredHz =
        SignalFrequency::measureInterpolated(
            captureSamples,
            captureSampleCount,
            waveformSampleRateHz);

    if (measuredHz > 0.0f && isfinite(measuredHz))
    {
        if (frequencyMeasurementInitialized)
        {
            signalFrequencyHz +=
                MEASUREMENT_SMOOTHING_ALPHA *
                (measuredHz - signalFrequencyHz);
        }
        else
        {
            signalFrequencyHz = measuredHz;
            frequencyMeasurementInitialized = true;
        }

        signalPeriodUs = 1000000.0f / signalFrequencyHz;
        frequencyMeasurementValid =
            isfinite(signalPeriodUs) && signalPeriodUs > 0.0f;
    }
    else
    {
        frequencyMeasurementValid = false;
        frequencyMeasurementInitialized = false;
    }

    int16_t minimumSample = captureSamples[0];
    int16_t maximumSample = captureSamples[0];
    int64_t sampleSum = 0;
    int64_t sampleSquareSum = 0;

    for (uint16_t i = 0; i < captureSampleCount; ++i)
    {
        const int32_t sample = captureSamples[i];
        if (sample < minimumSample)
            minimumSample = static_cast<int16_t>(sample);
        if (sample > maximumSample)
            maximumSample = static_cast<int16_t>(sample);

        sampleSum += sample;
        sampleSquareSum += sample * sample;
    }

    const int32_t peakToPeakCounts =
        static_cast<int32_t>(maximumSample) - minimumSample;
    const float rmsReferenceCounts = setAudio.rmsReference();

    if (peakToPeakCounts < MINIMUM_MEASURABLE_VPP_COUNTS ||
        rmsReferenceCounts <= 0.0f ||
        !isfinite(rmsReferenceCounts))
    {
        amplitudeMeasurementValid = false;
        amplitudeMeasurementInitialized = false;
        return;
    }

    const float sampleCount = static_cast<float>(captureSampleCount);
    const float averageCounts =
        static_cast<float>(sampleSum) / sampleCount;
    float varianceCounts =
        static_cast<float>(sampleSquareSum) / sampleCount -
        averageCounts * averageCounts;
    if (varianceCounts < 0.0f)
        varianceCounts = 0.0f;

    const float voltsPerCount =
        ZERO_DB_RMS_VOLTS / rmsReferenceCounts;
    const float measuredVpp = peakToPeakCounts * voltsPerCount;
    const float measuredVrmsAc = sqrtf(varianceCounts) * voltsPerCount;
    const float measuredVmax = maximumSample * voltsPerCount;
    const float measuredVmin = minimumSample * voltsPerCount;
    const float measuredVdc = averageCounts * voltsPerCount;

    if (amplitudeMeasurementInitialized)
    {
        signalVppVolts += MEASUREMENT_SMOOTHING_ALPHA *
                          (measuredVpp - signalVppVolts);
        signalVrmsAcVolts += MEASUREMENT_SMOOTHING_ALPHA *
                             (measuredVrmsAc - signalVrmsAcVolts);
        signalVmaxVolts += MEASUREMENT_SMOOTHING_ALPHA *
                           (measuredVmax - signalVmaxVolts);
        signalVminVolts += MEASUREMENT_SMOOTHING_ALPHA *
                           (measuredVmin - signalVminVolts);
        signalVdcVolts += MEASUREMENT_SMOOTHING_ALPHA *
                          (measuredVdc - signalVdcVolts);
    }
    else
    {
        signalVppVolts = measuredVpp;
        signalVrmsAcVolts = measuredVrmsAc;
        signalVmaxVolts = measuredVmax;
        signalVminVolts = measuredVmin;
        signalVdcVolts = measuredVdc;
        amplitudeMeasurementInitialized = true;
    }

    amplitudeMeasurementValid =
        isfinite(signalVppVolts) &&
        isfinite(signalVrmsAcVolts) &&
        isfinite(signalVmaxVolts) &&
        isfinite(signalVminVolts) &&
        isfinite(signalVdcVolts);
}

// The Y-scale label itself is the cyclic vertical-gain control.
bool Waveform::isVerticalScaleTouch(uint16_t x, uint16_t y) const
{
    return x >= Y_CONTROL_TOUCH_X_MIN &&
           x <= Y_CONTROL_TOUCH_X_MAX &&
           y >= Y_CONTROL_TOUCH_Y_MIN &&
           y <= Y_CONTROL_TOUCH_Y_MAX;
}

bool Waveform::isHorizontalScaleTouch(uint16_t x, uint16_t y) const
{
    return x >= X_CONTROL_TOUCH_X_MIN &&
           x <= X_CONTROL_TOUCH_X_MAX &&
           y >= X_CONTROL_TOUCH_Y_MIN &&
           y <= X_CONTROL_TOUCH_Y_MAX;
}

void Waveform::reverseControlDirection(ScopeControl control)
{
    if (control == ScopeControl::Vertical)
        verticalArrowPointsUp = !verticalArrowPointsUp;
    else if (control == ScopeControl::Horizontal)
        horizontalArrowPointsRight = !horizontalArrowPointsRight;
}

void Waveform::highlightControl(ScopeControl control)
{
    highlightedScopeControl = control;
    scopeControlHighlightUntilMs = millis() + SCOPE_ARROW_HIGHLIGHT_MS;
}

bool Waveform::isControlHighlighted(ScopeControl control) const
{
    return highlightedScopeControl == control &&
           static_cast<int32_t>(
               scopeControlHighlightUntilMs - millis()) > 0;
}

// Moves one Y-scale step in the arrow direction without wrapping.
void Waveform::stepVerticalScale()
{
    const int direction = verticalArrowPointsUp ? 1 : -1;
    const int nextIndex =
        static_cast<int>(scopeVoltsPerDivIndex) + direction;

    if (nextIndex < 0 || nextIndex >= VERTICAL_SCALE_COUNT)
    {
        verticalArrowPointsUp = !verticalArrowPointsUp;
        return;
    }
    scopeVoltsPerDivIndex = static_cast<uint8_t>(nextIndex);
    if (scopeVoltsPerDivIndex == 0)
        verticalArrowPointsUp = true;
    else if (scopeVoltsPerDivIndex == VERTICAL_SCALE_COUNT - 1)
        verticalArrowPointsUp = false;

    displayedVerticalScaleText[0] = '\0';
}
// Moves one real timebase step in the arrow direction without wrapping.
void Waveform::stepTimebase()
{
    const int direction = horizontalArrowPointsRight ? -1 : 1;
    const int nextIndex = static_cast<int>(timebaseIndex) + direction;

    if (nextIndex < 0 || nextIndex >= TIMEBASE_COUNT)
    {
        horizontalArrowPointsRight = !horizontalArrowPointsRight;
        return;
    }

    timebaseIndex = static_cast<uint8_t>(nextIndex);
    if (timebaseIndex == 0)
        horizontalArrowPointsRight = false;
    else if (timebaseIndex == TIMEBASE_COUNT - 1)
        horizontalArrowPointsRight = true;

    waveformSampleCount =
        TIMEBASES[timebaseIndex].sampleCount;
    waveformSampleRateHz =
        TIMEBASES[timebaseIndex].sampleRateHz;
    resetMeasurements();

    drawVerticalScaleLabel();
    displayedHorizontalScaleText[0] = '\0';
    drawHorizontalScaleLabel();
    captureLeftSignal();
}

bool Waveform::processTouch()
{
    const uint32_t now = millis();

    if (pressedScopeControl != ScopeControl::None)
    {
        if (touch.checkCalibrationRequest())
        {
            if (!scopeControlLongPressHandled &&
                now - scopeControlPressStartedMs >= SCOPE_LONG_PRESS_MS)
            {
                reverseControlDirection(pressedScopeControl);
                scopeControlLongPressHandled = true;
                highlightControl(pressedScopeControl);
                buzzer.play(BeepPattern::Touch);
                drawWaveform();
            }
            return false;
        }

        touch.waitAnyTouch();
        const ScopeControl releasedControl = pressedScopeControl;
        pressedScopeControl = ScopeControl::None;

        if (!scopeControlLongPressHandled)
        {
            if (releasedControl == ScopeControl::Vertical)
                stepVerticalScale();
            else
                stepTimebase();

            highlightControl(releasedControl);
            drawWaveform();
        }

        return false;
    }

    if (!touch.pressed())
        return false;

    const uint16_t x = touch.getX();
    const uint16_t y = touch.getY();
    if (isVerticalScaleTouch(x, y))
        pressedScopeControl = ScopeControl::Vertical;
    else if (isHorizontalScaleTouch(x, y))
        pressedScopeControl = ScopeControl::Horizontal;
    else
    {
        touch.waitAnyTouch();
        return true;
    }

    scopeControlPressStartedMs = now;
    scopeControlLongPressHandled = false;
    highlightControl(pressedScopeControl);
    drawWaveform();
    return false;
}

// Draws the current timebase at the upper-right side of the waveform frame.
void Waveform::drawHorizontalScaleLabel()
{
    const float timePerDivisionUs =
        (static_cast<float>(waveformSampleCount) * 1000000.0f) /
        (static_cast<float>(waveformSampleRateHz) * GRID_COLUMNS);
    char value[16];
    const char *unit = "ms";
    bool formatted = false;

    if (isfinite(timePerDivisionUs) && timePerDivisionUs > 0.0f)
    {
        if (timePerDivisionUs >= 10.0f)
        {
            formatted = formatUnsignedFixed(
                timePerDivisionUs / 1000.0f,
                2,
                value,
                sizeof(value));
        }
        else
        {
            unit = "us";
            formatted = formatUnsignedFixed(
                timePerDivisionUs,
                1,
                value,
                sizeof(value));
        }
    }

    if (!formatted)
        snprintf(value, sizeof(value), "---");

    char unitText[12];
    snprintf(unitText, sizeof(unitText), " %s/div", unit);

    char label[24];
    snprintf(label, sizeof(label), "X = %s%s", value, unitText);
    if (strcmp(label, displayedHorizontalScaleText) == 0)
        return;

    tft.fillRect(
        WAVE_X + WAVE_W - HORIZONTAL_SCALE_LABEL_W,
        WAVE_Y - 13,
        HORIZONTAL_SCALE_LABEL_W,
        10,
        COL_BG);
    const int labelWidth =
        tft.textWidth("X = ", 1) +
        tft.textWidth(value, 1) +
        tft.textWidth(unitText, 1);
    int x = WAVE_X + WAVE_W - labelWidth;

    tft.setTextColor(TFT_CYAN, COL_BG);
    tft.drawString("X = ", x, WAVE_Y - 11, 1);
    x += tft.textWidth("X = ", 1);

    tft.setTextColor(TFT_YELLOW, COL_BG);
    tft.drawString(value, x, WAVE_Y - 11, 1);
    x += tft.textWidth(value, 1);

    tft.setTextColor(TFT_CYAN, COL_BG);
    tft.drawString(unitText, x, WAVE_Y - 11, 1);
    snprintf(
        displayedHorizontalScaleText,
        sizeof(displayedHorizontalScaleText),
        "%s",
        label);
}

float Waveform::nativeVoltsPerDivision() const
{
    const float rmsReferenceCounts = setAudio.rmsReference();
    if (rmsReferenceCounts <= 0.0f || !isfinite(rmsReferenceCounts))
        return NAN;

    const float renderedCountsPerDivision =
        0.5f * ZERO_DB_PEAK_ADC *
        (1.0f / POSITIVE_ZERO_DB_DIVISIONS +
         1.0f / NEGATIVE_ZERO_DB_DIVISIONS);

    return renderedCountsPerDivision *
           (ZERO_DB_RMS_VOLTS / rmsReferenceCounts);
}

float Waveform::selectedVoltsPerDivision() const
{
    if (scopeVoltsPerDivIndex == NATIVE_VERTICAL_SCALE_INDEX ||
        scopeVoltsPerDivIndex >= VERTICAL_SCALE_COUNT)
        return nativeVoltsPerDivision();

    return VERTICAL_SCALE_VOLTS_PER_DIV[scopeVoltsPerDivIndex];
}

float Waveform::verticalVisualGain() const
{
    if (scopeVoltsPerDivIndex == NATIVE_VERTICAL_SCALE_INDEX ||
        scopeVoltsPerDivIndex >= VERTICAL_SCALE_COUNT)
        return 1.0f;

    const float nativeScale = nativeVoltsPerDivision();
    const float selectedScale =
        VERTICAL_SCALE_VOLTS_PER_DIV[scopeVoltsPerDivIndex];
    if (!isfinite(nativeScale) || nativeScale <= 0.0f ||
        !isfinite(selectedScale) || selectedScale <= 0.0f)
        return 1.0f;

    return nativeScale / selectedScale;
}

// Shows the calibrated voltage represented by one horizontal grid division.
void Waveform::drawVerticalScaleLabel()
{
    char scaleValue[16];
    const float voltsPerDivision = selectedVoltsPerDivision();
    const uint8_t decimalPlaces =
        scopeVoltsPerDivIndex == VERTICAL_SCALE_COUNT - 1
            ? 3
            : 2;

    if (!formatUnsignedFixed(
            voltsPerDivision,
            decimalPlaces,
            scaleValue,
            sizeof(scaleValue)))
        snprintf(scaleValue, sizeof(scaleValue), "---");

    char scaleText[24];
    snprintf(
        scaleText,
        sizeof(scaleText),
        "Y = %s V/div",
        scaleValue);

    if (strcmp(scaleText, displayedVerticalScaleText) == 0)
        return;

    if (scopeVerticalScaleSprite.created())
    {
        scopeVerticalScaleSprite.fillSprite(TEXT_COLOR_BACKGROUND);
        scopeVerticalScaleSprite.setTextColor(
            TEXT_COLOR_CYAN,
            TEXT_COLOR_BACKGROUND);
        scopeVerticalScaleSprite.drawString("Y = ", 0, 2, 1);

        int x = tft.textWidth("Y = ", 1);
        scopeVerticalScaleSprite.setTextColor(
            TEXT_COLOR_ORANGE,
            TEXT_COLOR_BACKGROUND);
        scopeVerticalScaleSprite.drawString(scaleValue, x, 2, 1);

        x += tft.textWidth(scaleValue, 1);
        scopeVerticalScaleSprite.setTextColor(
            TEXT_COLOR_CYAN,
            TEXT_COLOR_BACKGROUND);
        scopeVerticalScaleSprite.drawString(" V/div", x, 2, 1);
        scopeVerticalScaleSprite.pushSprite(WAVE_X, WAVE_Y - 13);
    }
    else
    {
        tft.fillRect(
            WAVE_X,
            WAVE_Y - 13,
            VERTICAL_SCALE_LABEL_W,
            VERTICAL_SCALE_LABEL_H,
            COL_BG);
        tft.setTextColor(TFT_CYAN, COL_BG);
        tft.drawString("Y = ", WAVE_X, WAVE_Y - 11, 1);

        const int scaleX =
            WAVE_X + tft.textWidth("Y = ", 1);
        tft.setTextColor(TFT_ORANGE, COL_BG);
        tft.drawString(scaleValue, scaleX, WAVE_Y - 11, 1);

        const int unitX =
            scaleX + tft.textWidth(scaleValue, 1);
        tft.setTextColor(TFT_CYAN, COL_BG);
        tft.drawString(" V/div", unitX, WAVE_Y - 11, 1);
    }

    snprintf(
        displayedVerticalScaleText,
        sizeof(displayedVerticalScaleText),
        "%s",
        scaleText);
}

void Waveform::drawMeasurements()
{
    char frequencyValue[16];
    char periodValue[16];
    char vppValue[12];
    char rmsValue[12];
    const char *frequencyUnit = "Hz";
    const char *periodUnit = "ms";

    formatCompactFrequency(
        frequencyMeasurementValid ? signalFrequencyHz : 0.0f,
        frequencyValue,
        sizeof(frequencyValue),
        frequencyUnit);
    formatCompactPeriod(
        frequencyMeasurementValid ? signalPeriodUs : 0.0f,
        periodValue,
        sizeof(periodValue),
        periodUnit);

    if (amplitudeMeasurementValid)
    {
        char value[16];
        if (formatUnsignedFixed(
                signalVppVolts,
                2,
                value,
                sizeof(value)))
            snprintf(vppValue, sizeof(vppValue), "%s", value);
        else
            snprintf(vppValue, sizeof(vppValue), "---");

        if (formatUnsignedFixed(
                signalVrmsAcVolts,
                3,
                value,
                sizeof(value)))
            snprintf(rmsValue, sizeof(rmsValue), "%s", value);
        else
            snprintf(rmsValue, sizeof(rmsValue), "---");
    }
    else
    {
        snprintf(vppValue, sizeof(vppValue), "---");
        snprintf(rmsValue, sizeof(rmsValue), "---");
    }

    char measurementKey[80];
    snprintf(
        measurementKey,
        sizeof(measurementKey),
        "F:%s%s Vpp:%sV RMS:%sV T:%s%s",
        frequencyValue,
        frequencyUnit,
        vppValue,
        rmsValue,
        periodValue,
        periodUnit);

    if (strcmp(measurementKey, displayedMeasurementsText) == 0)
        return;

    struct ColoredTextPart
    {
        const char *text;
        uint8_t color;
        bool endsGroup;
    };

    const ColoredTextPart parts[] = {
        {"F:", TEXT_COLOR_CYAN, false},
        {frequencyValue, TEXT_COLOR_ORANGE, false},
        {frequencyUnit, TEXT_COLOR_CYAN, true},
        {"Vpp:", TEXT_COLOR_CYAN, false},
        {vppValue, TEXT_COLOR_GREEN, false},
        {"V", TEXT_COLOR_CYAN, true},
        {"RMS:", TEXT_COLOR_CYAN, false},
        {rmsValue, TEXT_COLOR_MAGENTA, false},
        {"V", TEXT_COLOR_CYAN, true},
        {"T:", TEXT_COLOR_CYAN, false},
        {periodValue, TEXT_COLOR_YELLOW, false},
        {periodUnit, TEXT_COLOR_CYAN, true}};

    int totalTextWidth = 0;
    for (uint8_t i = 0; i < sizeof(parts) / sizeof(parts[0]); ++i)
        totalTextWidth += tft.textWidth(parts[i].text, 1);

    constexpr uint8_t MEASUREMENT_GROUP_COUNT = 4;
    constexpr uint8_t MEASUREMENT_GAP_COUNT =
        MEASUREMENT_GROUP_COUNT - 1;
    const int availableWidth =
        WAVE_W - (2 * MEASUREMENTS_HORIZONTAL_PADDING);
    const int remainingSpace = availableWidth - totalTextWidth;
    const int groupGap =
        remainingSpace > 0
            ? remainingSpace / MEASUREMENT_GAP_COUNT
            : 0;
    const uint8_t extraGapPixels =
        remainingSpace > 0
            ? static_cast<uint8_t>(
                  remainingSpace % MEASUREMENT_GAP_COUNT)
            : 0;

    if (scopeMeasurementsSprite.created())
    {
        scopeMeasurementsSprite.fillSprite(TEXT_COLOR_BACKGROUND);
        int x = MEASUREMENTS_HORIZONTAL_PADDING;
        uint8_t completedGroups = 0;
        for (uint8_t i = 0; i < sizeof(parts) / sizeof(parts[0]); ++i)
        {
            scopeMeasurementsSprite.setTextColor(
                parts[i].color,
                TEXT_COLOR_BACKGROUND);
            scopeMeasurementsSprite.drawString(
                parts[i].text,
                x,
                0,
                1);
            x += tft.textWidth(parts[i].text, 1);
            if (parts[i].endsGroup &&
                completedGroups < MEASUREMENT_GAP_COUNT)
            {
                x += groupGap;
                if (completedGroups < extraGapPixels)
                    ++x;
                ++completedGroups;
            }
        }
        scopeMeasurementsSprite.pushSprite(WAVE_X, MEASUREMENTS_Y);
    }
    else
    {
        tft.fillRect(
            WAVE_X,
            MEASUREMENTS_Y,
            WAVE_W,
            MEASUREMENTS_H,
            COL_BG);
        int x = WAVE_X + MEASUREMENTS_HORIZONTAL_PADDING;
        uint8_t completedGroups = 0;
        for (uint8_t i = 0; i < sizeof(parts) / sizeof(parts[0]); ++i)
        {
            tft.setTextColor(
                scopeTextColor(parts[i].color),
                COL_BG);
            tft.drawString(
                parts[i].text,
                x,
                MEASUREMENTS_Y,
                1);
            x += tft.textWidth(parts[i].text, 1);
            if (parts[i].endsGroup &&
                completedGroups < MEASUREMENT_GAP_COUNT)
            {
                x += groupGap;
                if (completedGroups < extraGapPixels)
                    ++x;
                ++completedGroups;
            }
        }
    }

    snprintf(
        displayedMeasurementsText,
        sizeof(displayedMeasurementsText),
        "%s",
        measurementKey);
}

// Draws the waveform-analyzer frame and its static grid.
void Waveform::drawScreen()
{
    display.openApp("WAVEFORM ANALYZER", "TOUCH TO BACK");

    // Chenarul rămâne static pe TFT.
    tft.drawRect(
        WAVE_X,
        WAVE_Y,
        WAVE_W,
        WAVE_H,
        COL_FRAME);

    drawVerticalScaleLabel();
    drawHorizontalScaleLabel();
    drawMeasurements();

    tft.setTextColor(
        TFT_GREEN,
        COL_BG);

    tft.drawCentreString(
        "AUTO TRIG",
        WAVE_X + WAVE_W / 2,
        WAVE_Y - 11,
        1);

    if (waveSpriteReady)
    {
        waveSprite.fillSprite(
            SPRITE_COLOR_BACKGROUND);

        drawGridToSprite();
        drawScopeControlsToSprite();

        waveSprite.pushSprite(
            WAVE_X + 1,
            WAVE_Y + 1);
    }
    else
    {
        // Fallback numai dacă memoria sprite-ului nu poate fi alocată.
        tft.fillRect(
            WAVE_X + 1,
            WAVE_Y + 1,
            WAVE_W - 2,
            WAVE_H - 2,
            WAVE_BACKGROUND_COLOR);

        drawGrid();
        drawScopeControls();
    }
}

// Draws the oscilloscope-style grid for the waveform display area.
void Waveform::drawGrid()
{
    tft.drawRect(WAVE_X, WAVE_Y, WAVE_W, WAVE_H, COL_FRAME);
    fillControlCells();

    for (int column = 0; column <= GRID_COLUMNS; column++)
    {
        const int x = WAVE_X + (WAVE_W * column) / GRID_COLUMNS;

        if (column > 0 && column < GRID_COLUMNS)
        {
            for (int y = WAVE_Y + 2; y < WAVE_Y + WAVE_H - 2; y += 4)
                tft.drawPixel(x, y, TFT_DARKGREY);
        }

        // Time graduations corresponding to every vertical dotted division.
        tft.drawFastVLine(x, WAVE_Y, 4, TFT_WHITE);
        tft.drawFastVLine(x, WAVE_Y + WAVE_H - 4, 4, TFT_WHITE);
    }

    for (int row = 0; row <= GRID_ROWS; row++)
    {
        const int y = WAVE_Y + (WAVE_H * row) / GRID_ROWS;

        if (row > 0 && row < GRID_ROWS)
        {
            for (int x = WAVE_X + 2; x < WAVE_X + WAVE_W - 2; x += 4)
                tft.drawPixel(x, y, TFT_DARKGREY);
        }

        // Amplitude graduations corresponding to every horizontal dotted division.
        tft.drawFastHLine(WAVE_X, y, 4, TFT_WHITE);
        tft.drawFastHLine(WAVE_X + WAVE_W - 4, y, 4, TFT_WHITE);
    }

    const int centerY = WAVE_Y + WAVE_H / 2;
    constexpr int DASH_WIDTH = 4;
    constexpr int MARKER_WIDTH = 4;

    // Alternanță repetată: un segment orizontal urmat de un marcaj „+”.
    for (int x = WAVE_X + 2; x < WAVE_X + WAVE_W - 2;
         x += DASH_WIDTH + MARKER_WIDTH)
    {
        tft.drawFastHLine(x, centerY, DASH_WIDTH, TFT_DARKGREY);

        const int markerX = x + DASH_WIDTH + MARKER_WIDTH / 2;
        tft.drawFastHLine(markerX - 1, centerY, 3, TFT_DARKGREY);
        tft.drawFastVLine(markerX, centerY - 2, 5, TFT_DARKGREY);
    }
}

void Waveform::drawScopeControls()
{
    const int yCenterX =
        (Y_CONTROL_TOUCH_X_MIN + Y_CONTROL_TOUCH_X_MAX) / 2;
    const int yCenterY =
        (Y_CONTROL_TOUCH_Y_MIN + Y_CONTROL_TOUCH_Y_MAX) / 2;
    const int xCenterX = X_CONTROL_ARROW_X;
    const int xCenterY = X_CONTROL_ARROW_Y;

    drawVerticalArrow(
        tft,
        yCenterX,
        yCenterY,
        verticalArrowPointsUp,
        isControlHighlighted(ScopeControl::Vertical)
            ? TFT_WHITE
            : TFT_DARKGREY);
    drawHorizontalArrow(
        tft,
        xCenterX,
        xCenterY,
        horizontalArrowPointsRight,
        isControlHighlighted(ScopeControl::Horizontal)
            ? TFT_WHITE
            : TFT_DARKGREY);
}

void Waveform::drawScopeControlsToSprite()
{
    constexpr int SPRITE_ORIGIN_X = WAVE_X + 1;
    constexpr int SPRITE_ORIGIN_Y = WAVE_Y + 1;
    const int yCenterX =
        (Y_CONTROL_TOUCH_X_MIN + Y_CONTROL_TOUCH_X_MAX) / 2 -
        SPRITE_ORIGIN_X;
    const int yCenterY =
        (Y_CONTROL_TOUCH_Y_MIN + Y_CONTROL_TOUCH_Y_MAX) / 2 -
        SPRITE_ORIGIN_Y;
    const int xCenterX = X_CONTROL_ARROW_X - SPRITE_ORIGIN_X;
    const int xCenterY = X_CONTROL_ARROW_Y - SPRITE_ORIGIN_Y;

    drawVerticalArrow(
        waveSprite,
        yCenterX,
        yCenterY,
        verticalArrowPointsUp,
        isControlHighlighted(ScopeControl::Vertical)
            ? SPRITE_COLOR_WHITE
            : SPRITE_COLOR_GRID);
    drawHorizontalArrow(
        waveSprite,
        xCenterX,
        xCenterY,
        horizontalArrowPointsRight,
        isControlHighlighted(ScopeControl::Horizontal)
            ? SPRITE_COLOR_WHITE
            : SPRITE_COLOR_GRID);
}

// Converts a centered ADC sample into a clamped vertical screen position.
int Waveform::sampleToY(int16_t sample) const
{
    const int amplitude = (sample >= 0) ? POSITIVE_ZERO_DB_AMPLITUDE
                                        : NEGATIVE_ZERO_DB_AMPLITUDE;
    const float scaledOffset =
        (static_cast<float>(sample) * amplitude *
         verticalVisualGain()) /
        ZERO_DB_PEAK_ADC;
    int y = CENTER_Y - static_cast<int>(scaledOffset);

    if (y < WAVE_Y + 1)
        y = WAVE_Y + 1;
    if (y > WAVE_Y + WAVE_H - 2)
        y = WAVE_Y + WAVE_H - 2;

    return y;
}

int Waveform::sampleToSpriteY(
    int16_t sample) const
{
    int y =
        sampleToY(sample) -
        (WAVE_Y + 1);

    if (y < 0)
        y = 0;

    if (y >= WAVE_SPRITE_H)
        y = WAVE_SPRITE_H - 1;

    return y;
}

// Updates only the trace, preserving the grid between frames.
void Waveform::drawWaveform()
{
    // Keep the last fully rendered frame on screen if DMA capture failed.
    if (!captureFrameValid)
        return;

    if (!waveSpriteReady)
    {
        // Fallback în caz că sprite-ul nu a putut fi creat.
        tft.fillRect(
            WAVE_X + 1,
            WAVE_Y + 1,
            WAVE_W - 2,
            WAVE_H - 2,
            WAVE_BACKGROUND_COLOR);

        drawGrid();
        drawScopeControls();
        drawVerticalScaleLabel();
        drawMeasurements();
        return;
    }

    // Cadrul complet este construit în RAM.
    drawGridToSprite();

    drawTraceToSprite(
        captureSamples + displayedSampleOffset,
        waveformSampleCount);
    drawScopeControlsToSprite();

    // Transfer unic către TFT.
    waveSprite.pushSprite(
        WAVE_X + 1,
        WAVE_Y + 1);

    drawVerticalScaleLabel();
    drawMeasurements();
}

// Draws the sampled signal as a continuous trace across the grid.
void Waveform::drawTraceToSprite(
    const int16_t *samples,
    uint16_t sampleCount)
{
    if (samples == nullptr ||
        sampleCount < 2)
    {
        return;
    }

    int previousX = 0;
    int previousY =
        sampleToSpriteY(samples[0]);

    for (uint16_t i = 1;
         i < sampleCount;
         i++)
    {
        const int x =
            ((WAVE_SPRITE_W - 1) * i) /
            (sampleCount - 1);

        const int y =
            sampleToSpriteY(samples[i]);

        waveSprite.drawLine(
            previousX,
            previousY,
            x,
            y,
            SPRITE_COLOR_TRACE);

        if (previousY + 1 <
                WAVE_SPRITE_H &&
            y + 1 <
                WAVE_SPRITE_H)
        {
            waveSprite.drawLine(
                previousX,
                previousY + 1,
                x,
                y + 1,
                SPRITE_COLOR_TRACE);
        }

        previousX = x;
        previousY = y;
    }
}

bool Waveform::createWaveSprite()
{
    if (waveSprite.created())
        return true;

    waveSprite.setColorDepth(4);

    if (waveSprite.createSprite(
            WAVE_SPRITE_W,
            WAVE_SPRITE_H) == nullptr)
    {
        return false;
    }

    waveSprite.createPalette(
        wavePalette,
        16);

    waveSprite.fillSprite(
        SPRITE_COLOR_BACKGROUND);

    return true;
}

void Waveform::drawGridToSprite()
{
    waveSprite.fillSprite(
        SPRITE_COLOR_BACKGROUND);
    fillControlCellsToSprite();

    //--------------------------------------------------
    // LINII VERTICALE ȘI GRADAȚII
    //--------------------------------------------------
    for (int column = 0;
         column <= GRID_COLUMNS;
         column++)
    {
        const int screenX =
            (WAVE_W * column) /
            GRID_COLUMNS;

        const int x = screenX - 1;

        if (column > 0 &&
            column < GRID_COLUMNS &&
            x >= 0 &&
            x < WAVE_SPRITE_W)
        {
            for (int y = 1;
                 y < WAVE_SPRITE_H - 1;
                 y += 4)
            {
                waveSprite.drawPixel(
                    x,
                    y,
                    SPRITE_COLOR_GRID);
            }
        }

        if (x >= 0 &&
            x < WAVE_SPRITE_W)
        {
            waveSprite.drawFastVLine(
                x,
                0,
                3,
                SPRITE_COLOR_WHITE);

            waveSprite.drawFastVLine(
                x,
                WAVE_SPRITE_H - 3,
                3,
                SPRITE_COLOR_WHITE);
        }
    }

    //--------------------------------------------------
    // LINII ORIZONTALE ȘI GRADAȚII
    //--------------------------------------------------
    for (int row = 0;
         row <= GRID_ROWS;
         row++)
    {
        const int screenY =
            (WAVE_H * row) /
            GRID_ROWS;

        const int y = screenY - 1;

        if (row > 0 &&
            row < GRID_ROWS &&
            y >= 0 &&
            y < WAVE_SPRITE_H)
        {
            for (int x = 1;
                 x < WAVE_SPRITE_W - 1;
                 x += 4)
            {
                waveSprite.drawPixel(
                    x,
                    y,
                    SPRITE_COLOR_GRID);
            }
        }

        if (y >= 0 &&
            y < WAVE_SPRITE_H)
        {
            waveSprite.drawFastHLine(
                0,
                y,
                3,
                SPRITE_COLOR_WHITE);

            waveSprite.drawFastHLine(
                WAVE_SPRITE_W - 3,
                y,
                3,
                SPRITE_COLOR_WHITE);
        }
    }

    //--------------------------------------------------
    // LINIA CENTRALĂ
    //--------------------------------------------------
    const int centerY =
        WAVE_SPRITE_H / 2;

    constexpr int DASH_WIDTH = 4;
    constexpr int MARKER_WIDTH = 4;

    for (int x = 1;
         x < WAVE_SPRITE_W - 1;
         x += DASH_WIDTH + MARKER_WIDTH)
    {
        waveSprite.drawFastHLine(
            x,
            centerY,
            DASH_WIDTH,
            SPRITE_COLOR_GRID);

        const int markerX =
            x + DASH_WIDTH +
            MARKER_WIDTH / 2;

        if (markerX <
            WAVE_SPRITE_W - 1)
        {
            waveSprite.drawFastHLine(
                markerX - 1,
                centerY,
                3,
                SPRITE_COLOR_GRID);

            waveSprite.drawFastVLine(
                markerX,
                centerY - 2,
                5,
                SPRITE_COLOR_GRID);
        }
    }
}

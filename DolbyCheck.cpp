#include "DolbyCheck.h"

#include "Config.h"
#include "Display.h"
#include "Audio.h"
#include "FFT.h"
#include "Record.h"
#include "Touch.h"
#include "ProgressBar.h"

#include <TFT_eSPI.h>
#include <math.h>

extern Display display;
extern Record record;
extern TFT_eSPI tft;
extern Touch touch;

DolbyCheck dolbyCheck;

namespace
{
    constexpr uint16_t DOLBY_TEST_FREQUENCIES_HZ[DOLBY_CHECK_FREQUENCY_COUNT] = {
        1000, 1600, 2500, 4000, 6300, 8000, 10000, 12500, 15000};
    constexpr float DOLBY_TEST_LEVEL_DB = -10.0f;
    constexpr float DOLBY_PLAYBACK_MIN_FREQUENCY_HZ = 950.0f;
    constexpr float DOLBY_PLAYBACK_MAX_FREQUENCY_HZ = 1050.0f;
    constexpr float DOLBY_PLAYBACK_MIN_LEVEL_DB = -13.0f;
    constexpr float DOLBY_PLAYBACK_MAX_LEVEL_DB = -7.0f;
    constexpr uint16_t DOLBY_PLAYBACK_REQUIRED_TONE_WINDOWS = 3;
    constexpr uint32_t DOLBY_PLAYBACK_OK_DISPLAY_MS = 350;
    constexpr float DOLBY_RUNNING_MIN_LEVEL_DB = -30.0f;
    constexpr float DOLBY_RUNNING_MAX_LEVEL_DB = 0.0f;
    constexpr float DOLBY_RUNNING_FREQUENCY_TOLERANCE = 0.05f;
    constexpr uint16_t DOLBY_MAX_RELIABLE_FREQUENCY_CHECK_HZ = 12500;
    constexpr uint8_t DOLBY_HF_FIRST_INDEX = 4;
    constexpr float TRACKING_GOOD_AVERAGE_ERROR_DB = 0.75f;
    constexpr float TRACKING_GOOD_MAXIMUM_ERROR_DB = 1.5f;
    constexpr float TRACKING_FAIR_AVERAGE_ERROR_DB = 1.5f;
    constexpr float TRACKING_FAIR_MAXIMUM_ERROR_DB = 3.0f;
    constexpr uint16_t NOISE_AVERAGE_WINDOW_COUNT = 32;
    constexpr uint32_t NOISE_SETTLE_MS = 500;
    constexpr float NOISE_MINIMUM_PLAUSIBLE_DB = -90.0f;
    constexpr float NOISE_MAXIMUM_PLAUSIBLE_DB = -3.0f;
    constexpr float NOISE_MAXIMUM_WINDOW_SPREAD_DB = 6.0f;
    constexpr float NOISE_MAXIMUM_CHANNEL_DIFFERENCE_DB = 12.0f;
    constexpr uint16_t HF_NOISE_FIRST_BIN = 51;
    constexpr uint16_t HF_NOISE_LAST_BIN = 192;
    constexpr uint8_t HF_NOISE_CAPTURE_COUNT = 16;
    constexpr uint16_t DOLBY_2H_SYNC_HZ = 2000;
    constexpr uint32_t DOLBY_2H_SYNC_MS = 2000;
    constexpr uint32_t DOLBY_2H_GAP_MS = 500;
    constexpr uint32_t DOLBY_2H_TONE_MS = 2000;
    constexpr uint32_t DOLBY_2H_TONE_SETTLE_MS = 500;
    constexpr uint32_t DOLBY_2H_TONE_MEASURE_MS = 800;
    constexpr uint32_t DOLBY_2H_SYNC_TIMEOUT_MS = 60000;
    constexpr uint32_t DOLBY_2H_END_SYNC_TIMEOUT_MS = 10000;
    constexpr uint8_t DOLBY_2H_SYNC_CONFIRMATIONS = 3;
    static_assert(
        HF_NOISE_LAST_BIN < FFTAnalyzer::BIN_COUNT,
        "HF noise bins must fit the FFT result");

    constexpr int GRAPH_LEFT = 34;
    constexpr int GRAPH_RIGHT = CONTENT_RIGHT - 10;
    constexpr int GRAPH_TOP = 70;
    constexpr int GRAPH_BOTTOM = 166;
    constexpr int GRAPH_WIDTH = GRAPH_RIGHT - GRAPH_LEFT;
    constexpr int GRAPH_HEIGHT = GRAPH_BOTTOM - GRAPH_TOP;
    constexpr int ITEM_FONT = 2;
    constexpr int TRACKING_SUMMARY_X = CONTENT_LEFT + 13;
    constexpr int TRACKING_SUMMARY_Y = 27;
    constexpr int TRACKING_SUMMARY_W = CONTENT_RIGHT - TRACKING_SUMMARY_X + 1;
    constexpr int TRACKING_SUMMARY_H = 40;
    constexpr int GRAPH_LABEL_Y = GRAPH_BOTTOM + 4;
    constexpr int SNR_SUMMARY_Y = GRAPH_LABEL_Y + 11;
    constexpr int SNR_SUMMARY_H = FOOTER_Y - SNR_SUMMARY_Y - 2;
    constexpr int SNR_LINE_HEIGHT = 16;

    enum class TrackingGrade : uint8_t
    {
        Good,
        Fair,
        Poor
    };

    int graphBandX(uint8_t bandIndex)
    {
        return GRAPH_LEFT +
               (static_cast<int>(bandIndex) * GRAPH_WIDTH +
                (DOLBY_CHECK_FREQUENCY_COUNT - 1) / 2) /
                   (DOLBY_CHECK_FREQUENCY_COUNT - 1);
    }

    void formatFrequency(char *buffer, size_t size, uint16_t frequencyHz)
    {
        const unsigned whole = frequencyHz / 1000;
        const unsigned tenth = (frequencyHz % 1000) / 100;
        if (tenth == 0)
            snprintf(buffer, size, "%uk", whole);
        else
            snprintf(buffer, size, "%u.%uk", whole, tenth);
    }

    bool isExpectedPlaybackFrequency(float frequencyHz)
    {
        return isfinite(frequencyHz) &&
               frequencyHz >= DOLBY_PLAYBACK_MIN_FREQUENCY_HZ &&
               frequencyHz <= DOLBY_PLAYBACK_MAX_FREQUENCY_HZ;
    }

    bool isExpectedSweepFrequency(float frequencyHz, uint16_t expectedHz)
    {
        const float toleranceHz =
            expectedHz * DOLBY_RUNNING_FREQUENCY_TOLERANCE;
        return isfinite(frequencyHz) &&
               frequencyHz >= expectedHz - toleranceHz &&
               frequencyHz <= expectedHz + toleranceHz;
    }

    bool isPlausibleSweepLevel(float levelDb)
    {
        return isfinite(levelDb) &&
               levelDb >= DOLBY_RUNNING_MIN_LEVEL_DB &&
               levelDb <= DOLBY_RUNNING_MAX_LEVEL_DB;
    }

    void formatTrackingError(
        float errorDb,
        char *text,
        size_t textSize)
    {
        if (!isfinite(errorDb) || errorDb < 0.0f)
        {
            snprintf(text, textSize, "--");
            return;
        }

        const uint16_t tenths = static_cast<uint16_t>(
            errorDb * 10.0f + 0.5f);
        snprintf(
            text,
            textSize,
            "%u.%u",
            tenths / 10U,
            tenths % 10U);
    }

    TrackingGrade classifyTracking(
        float averageErrorDb,
        float maximumErrorDb)
    {
        if (averageErrorDb <= TRACKING_GOOD_AVERAGE_ERROR_DB &&
            maximumErrorDb <= TRACKING_GOOD_MAXIMUM_ERROR_DB)
            return TrackingGrade::Good;
        if (averageErrorDb <= TRACKING_FAIR_AVERAGE_ERROR_DB &&
            maximumErrorDb <= TRACKING_FAIR_MAXIMUM_ERROR_DB)
            return TrackingGrade::Fair;
        return TrackingGrade::Poor;
    }

    const char *trackingGradeText(TrackingGrade grade)
    {
        switch (grade)
        {
        case TrackingGrade::Good:
            return "GOOD";
        case TrackingGrade::Fair:
            return "FAIR";
        default:
            return "POOR";
        }
    }

    uint16_t trackingGradeColor(TrackingGrade grade)
    {
        switch (grade)
        {
        case TrackingGrade::Good:
            return TFT_GREEN;
        case TrackingGrade::Fair:
            return TFT_YELLOW;
        default:
            return TFT_RED;
        }
    }

    void formatSnr(
        float signalDb,
        float noiseDb,
        char *text,
        size_t textSize)
    {
        const float snrDb = signalDb - noiseDb;
        if (!isfinite(signalDb) || !isfinite(noiseDb) ||
            !isfinite(snrDb))
        {
            snprintf(text, textSize, "--");
            return;
        }

        const bool negative = snrDb < 0.0f;
        const float magnitude = negative ? -snrDb : snrDb;
        const unsigned long tenths = static_cast<unsigned long>(
            magnitude * 10.0f + 0.5f);
        snprintf(
            text,
            textSize,
            "%s%lu.%lu",
            negative ? "-" : "",
            tenths / 10UL,
            tenths % 10UL);
    }

    void drawSnrLine(
        int y,
        const char *prefix,
        const char *offValue,
        const char *bValue,
        const char *cValue)
    {
        const char *const offLabel = "OFF=";
        const char *const bLabel = " B=";
        const char *const cLabel = " C=";
        const char *const suffix = " dB";
        const int width =
            tft.textWidth(prefix, ITEM_FONT) +
            tft.textWidth(offLabel, ITEM_FONT) +
            tft.textWidth(offValue, ITEM_FONT) +
            tft.textWidth(bLabel, ITEM_FONT) +
            tft.textWidth(bValue, ITEM_FONT) +
            tft.textWidth(cLabel, ITEM_FONT) +
            tft.textWidth(cValue, ITEM_FONT) +
            tft.textWidth(suffix, ITEM_FONT);
        int x = (LCD_WIDTH - width) / 2;
        if (x < CONTENT_LEFT)
            x = CONTENT_LEFT;

        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(COL_TEXT, COL_BG);
        tft.drawString(prefix, x, y, ITEM_FONT);
        x += tft.textWidth(prefix, ITEM_FONT);
        tft.setTextColor(TFT_YELLOW, COL_BG);
        tft.drawString(offLabel, x, y, ITEM_FONT);
        x += tft.textWidth(offLabel, ITEM_FONT);
        tft.setTextColor(COL_TEXT, COL_BG);
        tft.drawString(offValue, x, y, ITEM_FONT);
        x += tft.textWidth(offValue, ITEM_FONT);
        tft.setTextColor(TFT_GREEN, COL_BG);
        tft.drawString(bLabel, x, y, ITEM_FONT);
        x += tft.textWidth(bLabel, ITEM_FONT);
        tft.setTextColor(COL_TEXT, COL_BG);
        tft.drawString(bValue, x, y, ITEM_FONT);
        x += tft.textWidth(bValue, ITEM_FONT);
        tft.setTextColor(TFT_ORANGE, COL_BG);
        tft.drawString(cLabel, x, y, ITEM_FONT);
        x += tft.textWidth(cLabel, ITEM_FONT);
        tft.setTextColor(COL_TEXT, COL_BG);
        tft.drawString(cValue, x, y, ITEM_FONT);
        x += tft.textWidth(cValue, ITEM_FONT);
        tft.drawString(suffix, x, y, ITEM_FONT);
    }
}

void DolbyCheck::run(DeckMode deckMode)
{
    activeDeckMode = deckMode;

    activeCurve = SweepCurve::C;
    resetActiveSweep();
    activeCurve = SweepCurve::B;
    resetActiveSweep();
    activeCurve = SweepCurve::Off;
    resetActiveSweep();
    if (deckMode == DeckMode::TwoHead)
        resetTwoHeadWorkflow();
    drawScreen(deckMode);

    while (true)
    {
        if (deckMode == DeckMode::ThreeHead)
        {
            if (noiseState == NoiseState::Settling ||
                noiseState == NoiseState::Measuring ||
                noiseState == NoiseState::MeasuringHf)
            {
                updateNoiseMeasurement();
            }
            else
            {
                updateOffSweep();
            }
        }
        else
        {
            updateTwoHeadWorkflow();
        }

        display.updateClock();
        if (!touch.pressed())
        {
            delay(5);
            continue;
        }

        const uint16_t touchX = touch.getX();
        const uint16_t touchY = touch.getY();
        if (touchY < FOOTER_Y)
        {
            if (deckMode == DeckMode::ThreeHead &&
                (noiseState == NoiseState::Prompt ||
                 noiseState == NoiseState::Error) &&
                isStartTouch(touchX, touchY))
            {
                startNoisePrecheck();
                continue;
            }

            if (deckMode == DeckMode::TwoHead &&
                isStartTouch(touchX, touchY))
            {
                handleTwoHeadAction();
                continue;
            }

            SweepCurve startCurve = SweepCurve::Off;
            const bool startAvailable =
                selectStartCurve(startCurve);
            if (deckMode == DeckMode::ThreeHead &&
                startAvailable &&
                isStartTouch(touchX, touchY))
            {
                drawStartButton(true, startCurve);
                while (touch.pressed())
                {
                    display.updateClock();
                    delay(5);
                }

                clearStartButton();
                startPlaybackPrecheck(startCurve);
                continue;
            }

            delay(5);
            continue;
        }

        record.stopCalibratedTestTone();

        while (touch.pressed())
        {
            display.updateClock();
            delay(5);
        }

        if (graphResultComplete() && askSaveGraphResult())
        {
            const CurvePlaceholder &off = curveData(SweepCurve::Off);
            const CurvePlaceholder &b = curveData(SweepCurve::B);
            const CurvePlaceholder &c = curveData(SweepCurve::C);
            if (!graphMemorySaveDolby(
                    off.relativeDb, b.relativeDb, c.relativeDb))
                showMemoryFull();
        }

        return;
    }
}

void DolbyCheck::showMemoryResult(const GraphMemorySlot &slot)
{
    if (slot.type != GraphMemoryType::Dolby)
        return;

    activeDeckMode = DeckMode::ThreeHead;
    CurvePlaceholder *curves[] = {
        &threeHeadState.dolbyOff,
        &threeHeadState.dolbyB,
        &threeHeadState.dolbyC};
    for (uint8_t curve = 0; curve < 3; ++curve)
    {
        for (uint8_t i = 0; i < DOLBY_CHECK_FREQUENCY_COUNT; ++i)
        {
            curves[curve]->relativeDb[i] = slot.points.dolby[curve][i];
            curves[curve]->captured[i] = isfinite(slot.points.dolby[curve][i]);
        }
    }

    drawScreen(activeDeckMode);
    drawStoredCurves();
    while (true)
    {
        display.updateClock();
        if (!touch.pressed())
        {
            delay(5);
            continue;
        }
        const uint16_t y = touch.getY();
        while (touch.pressed())
            delay(5);
        if (y >= FOOTER_Y)
            break;
    }
}

bool DolbyCheck::graphResultComplete() const
{
    const SweepCurve curveIds[] = {SweepCurve::Off, SweepCurve::B, SweepCurve::C};
    for (uint8_t curve = 0; curve < 3; ++curve)
    {
        const CurvePlaceholder &data = curveData(curveIds[curve]);
        for (uint8_t i = 0; i < DOLBY_CHECK_FREQUENCY_COUNT; ++i)
            if (!data.captured[i] || !isfinite(data.relativeDb[i]))
                return false;
    }
    return true;
}

bool DolbyCheck::askSaveGraphResult()
{
    display.openApp("SAVE RESULT?", "TOUCH TO CHOOSE");
    const int buttonY[] = {88, 136};
    const char *labels[] = {"SAVE", "NOT SAVE"};
    for (uint8_t i = 0; i < 2; ++i)
    {
        tft.drawRoundRect(52, buttonY[i], 216, 36, 4, TFT_CYAN);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COL_TEXT, COL_BG);
        tft.drawString(labels[i], 160, buttonY[i] + 18, ITEM_FONT);
    }
    tft.setTextDatum(TL_DATUM);
    while (true)
    {
        display.updateClock();
        if (!touch.pressed())
        {
            delay(5);
            continue;
        }
        const uint16_t x = touch.getX();
        const uint16_t y = touch.getY();
        while (touch.pressed())
            delay(5);
        if (x < 52 || x >= 268)
            continue;
        if (y >= buttonY[0] && y < buttonY[0] + 36)
            return true;
        if (y >= buttonY[1] && y < buttonY[1] + 36)
            return false;
    }
}

void DolbyCheck::showMemoryFull()
{
    display.openApp("MEMORY FULL", "TOUCH TO CONTINUE");
    tft.drawRoundRect(82, 136, 156, 36, 4, TFT_CYAN);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("OK", 160, 154, ITEM_FONT);
    tft.setTextDatum(TL_DATUM);
    while (!touch.pressed())
    {
        display.updateClock();
        delay(5);
    }
    while (touch.pressed())
        delay(5);
}

void DolbyCheck::drawScreen(DeckMode deckMode)
{
    display.openApp("DOLBY CHECK", "TOUCH TO BACK");
    tft.setTextDatum(TL_DATUM);

    if (deckMode == DeckMode::ThreeHead)
    {
        tft.setTextColor(TFT_YELLOW, COL_BG);
        tft.drawString(
            "SET MONITOR TO TAPE",
            CONTENT_LEFT + 13,
            30,
            ITEM_FONT);
    }
    else
    {
        tft.setTextColor(TFT_CYAN, COL_BG);
        tft.drawString(
            "MODE: 2 HEAD REC/REW/PLAY",
            CONTENT_LEFT + 13,
            30,
            ITEM_FONT);
    }

    drawStatus(
        deckMode == DeckMode::TwoHead
            ? "TOUCH GRID TO START RECORD"
            : "TOUCH GRID TO START OFF",
        curveColor(SweepCurve::Off));
    drawLegend();
    drawGraphGrid();
    if (deckMode == DeckMode::ThreeHead)
        drawStartButton();
    else
        drawTwoHeadControl("START", "RECORD");
}

uint16_t DolbyCheck::curveColor(SweepCurve curve) const
{
    switch (curve)
    {
    case SweepCurve::Off:
        return TFT_YELLOW;
    case SweepCurve::B:
        return TFT_GREEN;
    case SweepCurve::C:
        return TFT_ORANGE;
    }
    return TFT_LIGHTGREY;
}

void DolbyCheck::drawLegend(bool focusActiveCurve)
{
    constexpr int LEGEND_Y = 48;
    constexpr const char *PREFIX = "DOLBY ";
    constexpr const char *OFF = "OFF";
    constexpr const char *SEPARATOR = "/";
    constexpr const char *B = "B";
    constexpr const char *C = "C";
    const int totalWidth =
        tft.textWidth(PREFIX, ITEM_FONT) +
        tft.textWidth(OFF, ITEM_FONT) +
        2 * tft.textWidth(SEPARATOR, ITEM_FONT) +
        tft.textWidth(B, ITEM_FONT) +
        tft.textWidth(C, ITEM_FONT);
    int x = CONTENT_RIGHT - totalWidth-8;

    tft.fillRect(x, LEGEND_Y, totalWidth, 18, COL_BG);

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_CYAN, COL_BG);
    tft.drawString(PREFIX, x, LEGEND_Y, ITEM_FONT);
    x += tft.textWidth(PREFIX, ITEM_FONT);
    tft.setTextColor(
        focusActiveCurve && activeCurve != SweepCurve::Off
            ? TFT_DARKGREY
            : TFT_YELLOW,
        COL_BG);
    tft.drawString(OFF, x, LEGEND_Y, ITEM_FONT);
    x += tft.textWidth(OFF, ITEM_FONT);
    tft.setTextColor(TFT_CYAN, COL_BG);
    tft.drawString(SEPARATOR, x, LEGEND_Y, ITEM_FONT);
    x += tft.textWidth(SEPARATOR, ITEM_FONT);
    tft.setTextColor(
        focusActiveCurve && activeCurve != SweepCurve::B
            ? TFT_DARKGREY
            : TFT_GREEN,
        COL_BG);
    tft.drawString(B, x, LEGEND_Y, ITEM_FONT);
    x += tft.textWidth(B, ITEM_FONT);
    tft.setTextColor(TFT_CYAN, COL_BG);
    tft.drawString(SEPARATOR, x, LEGEND_Y, ITEM_FONT);
    x += tft.textWidth(SEPARATOR, ITEM_FONT);
    tft.setTextColor(
        focusActiveCurve && activeCurve != SweepCurve::C
            ? TFT_DARKGREY
            : TFT_ORANGE,
        COL_BG);
    tft.drawString(C, x, LEGEND_Y, ITEM_FONT);
}

void DolbyCheck::drawGraphGrid()
{
    tft.drawRect(
        GRAPH_LEFT,
        GRAPH_TOP,
        GRAPH_WIDTH + 1,
        GRAPH_HEIGHT + 1,
        COL_FRAME);

    tft.setTextDatum(TR_DATUM);
    for (int8_t db = 6; db >= -6; db -= 2)
    {
        const int y = GRAPH_TOP + (6 - db) * GRAPH_HEIGHT / 12;
        const uint16_t color = TFT_DARKGREY;
        tft.drawFastHLine(GRAPH_LEFT, y, GRAPH_WIDTH + 1, color);

        char label[5];
        snprintf(label, sizeof(label), "%+d", db);
        tft.setTextColor(db == 0 ? TFT_CYAN : color, COL_BG);
        tft.drawString(label, GRAPH_LEFT - 3, y, 1);
    }

    for (uint8_t i = 0; i < DOLBY_CHECK_FREQUENCY_COUNT; ++i)
    {
        const int x = graphBandX(i);
        tft.drawFastVLine(x, GRAPH_TOP, GRAPH_HEIGHT + 1, COL_FRAME);
    }

    static const char *const labels[DOLBY_CHECK_FREQUENCY_COUNT] = {
        "1k", "1k6", "2k5", "4k", "6k3", "8k", "10k", "12k5", "15k"};
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COL_TEXT, COL_BG);
    for (uint8_t i = 0; i < DOLBY_CHECK_FREQUENCY_COUNT; ++i)
    {
        tft.drawString(
            labels[i],
            graphBandX(i) + (i == 0 ? 5 : 0),
            GRAPH_LABEL_Y,
            1);
    }

    // Keep the future measurement-frequency table linked into this UI layer.
    static_assert(
        sizeof(DOLBY_TEST_FREQUENCIES_HZ) /
                sizeof(DOLBY_TEST_FREQUENCIES_HZ[0]) ==
            DOLBY_CHECK_FREQUENCY_COUNT,
        "Dolby graph and frequency table must have matching band counts");
    tft.setTextDatum(TL_DATUM);
}

void DolbyCheck::drawStatus(const char *text, uint16_t color)
{
    constexpr int STATUS_X = CONTENT_LEFT;
    constexpr int STATUS_Y = GRAPH_LABEL_Y + 8;
    constexpr int STATUS_WIDTH = CONTENT_WIDTH;
    constexpr int STATUS_HEIGHT = 18;

    tft.fillRect(STATUS_X, STATUS_Y, STATUS_WIDTH, STATUS_HEIGHT, COL_BG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(color, COL_BG);
    tft.drawString(
        text,
        STATUS_X + STATUS_WIDTH / 2,
        STATUS_Y + STATUS_HEIGHT / 2,
        ITEM_FONT);
    tft.setTextDatum(TL_DATUM);
}

void DolbyCheck::drawTrackingSummary()
{
    float offAverageErrorDb = 0.0f;
    float offMaximumErrorDb = 0.0f;
    float bAverageErrorDb = 0.0f;
    float bMaximumErrorDb = 0.0f;
    float cAverageErrorDb = 0.0f;
    float cMaximumErrorDb = 0.0f;
    if (!calculateTrackingErrors(
            curveData(SweepCurve::Off),
            offAverageErrorDb,
            offMaximumErrorDb) ||
        !calculateTrackingErrors(
            curveData(SweepCurve::B),
            bAverageErrorDb,
            bMaximumErrorDb) ||
        !calculateTrackingErrors(
            curveData(SweepCurve::C),
            cAverageErrorDb,
            cMaximumErrorDb))
        return;

    char offAverage[6];
    char offMaximum[6];
    char bAverage[6];
    char bMaximum[6];
    char cAverage[6];
    char cMaximum[6];
    formatTrackingError(offAverageErrorDb, offAverage, sizeof(offAverage));
    formatTrackingError(offMaximumErrorDb, offMaximum, sizeof(offMaximum));
    formatTrackingError(bAverageErrorDb, bAverage, sizeof(bAverage));
    formatTrackingError(bMaximumErrorDb, bMaximum, sizeof(bMaximum));
    formatTrackingError(cAverageErrorDb, cAverage, sizeof(cAverage));
    formatTrackingError(cMaximumErrorDb, cMaximum, sizeof(cMaximum));

    const TrackingGrade bGrade =
        classifyTracking(bAverageErrorDb, bMaximumErrorDb);
    const TrackingGrade cGrade =
        classifyTracking(cAverageErrorDb, cMaximumErrorDb);

    tft.fillRect(
        TRACKING_SUMMARY_X,
        TRACKING_SUMMARY_Y,
        TRACKING_SUMMARY_W,
        TRACKING_SUMMARY_H,
        COL_BG);
    const char *const trackingPrefix = "HF AVG/MAX  B=";
    const char *const trackingSeparator = "  C=";
    const char *const bGradeText = trackingGradeText(bGrade);
    const char *const cGradeText = trackingGradeText(cGrade);
    const int trackingWidth =
        tft.textWidth(trackingPrefix, ITEM_FONT) +
        tft.textWidth(bGradeText, ITEM_FONT) +
        tft.textWidth(trackingSeparator, ITEM_FONT) +
        tft.textWidth(cGradeText, ITEM_FONT);
    int x = (LCD_WIDTH - trackingWidth) / 2;
    if (x < TRACKING_SUMMARY_X)
        x = TRACKING_SUMMARY_X;

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_CYAN, COL_BG);
    tft.drawString(trackingPrefix, x, TRACKING_SUMMARY_Y, ITEM_FONT);
    x += tft.textWidth(trackingPrefix, ITEM_FONT);
    tft.setTextColor(trackingGradeColor(bGrade), COL_BG);
    tft.drawString(bGradeText, x, TRACKING_SUMMARY_Y, ITEM_FONT);
    x += tft.textWidth(bGradeText, ITEM_FONT);
    tft.setTextColor(TFT_CYAN, COL_BG);
    tft.drawString(
        trackingSeparator,
        x,
        TRACKING_SUMMARY_Y,
        ITEM_FONT);
    x += tft.textWidth(trackingSeparator, ITEM_FONT);
    tft.setTextColor(trackingGradeColor(cGrade), COL_BG);
    tft.drawString(cGradeText, x, TRACKING_SUMMARY_Y, ITEM_FONT);

    char offMetrics[12];
    char bMetrics[12];
    char cMetrics[12];
    snprintf(
        offMetrics,
        sizeof(offMetrics),
        "%s/%s",
        offAverage,
        offMaximum);
    snprintf(
        bMetrics,
        sizeof(bMetrics),
        "%s/%s",
        bAverage,
        bMaximum);
    snprintf(
        cMetrics,
        sizeof(cMetrics),
        "%s/%s",
        cAverage,
        cMaximum);
    const char *offLabel = "OFF ";
    const char *bLabel = " B ";
    const char *cLabel = " C ";
    int metricsWidth =
        tft.textWidth(offLabel, ITEM_FONT) +
        tft.textWidth(offMetrics, ITEM_FONT) +
        tft.textWidth(bLabel, ITEM_FONT) +
        tft.textWidth(bMetrics, ITEM_FONT) +
        tft.textWidth(cLabel, ITEM_FONT) +
        tft.textWidth(cMetrics, ITEM_FONT);
    if (metricsWidth > TRACKING_SUMMARY_W)
    {
        offLabel = "O ";
        metricsWidth =
            tft.textWidth(offLabel, ITEM_FONT) +
            tft.textWidth(offMetrics, ITEM_FONT) +
            tft.textWidth(bLabel, ITEM_FONT) +
            tft.textWidth(bMetrics, ITEM_FONT) +
            tft.textWidth(cLabel, ITEM_FONT) +
            tft.textWidth(cMetrics, ITEM_FONT);
    }
    x = (LCD_WIDTH - metricsWidth) / 2;
    if (x < TRACKING_SUMMARY_X)
        x = TRACKING_SUMMARY_X;

    tft.setTextColor(TFT_YELLOW, COL_BG);
    tft.drawString(offLabel, x, TRACKING_SUMMARY_Y + 21, ITEM_FONT);
    x += tft.textWidth(offLabel, ITEM_FONT);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(offMetrics, x, TRACKING_SUMMARY_Y + 21, ITEM_FONT);
    x += tft.textWidth(offMetrics, ITEM_FONT);
    tft.setTextColor(TFT_GREEN, COL_BG);
    tft.drawString(bLabel, x, TRACKING_SUMMARY_Y + 21, ITEM_FONT);
    x += tft.textWidth(bLabel, ITEM_FONT);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(bMetrics, x, TRACKING_SUMMARY_Y + 21, ITEM_FONT);
    x += tft.textWidth(bMetrics, ITEM_FONT);
    tft.setTextColor(TFT_ORANGE, COL_BG);
    tft.drawString(cLabel, x, TRACKING_SUMMARY_Y + 21, ITEM_FONT);
    x += tft.textWidth(cLabel, ITEM_FONT);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(cMetrics, x, TRACKING_SUMMARY_Y + 21, ITEM_FONT);
    tft.setTextDatum(TL_DATUM);
}

void DolbyCheck::drawSnrSummary()
{
    char offSnr[10];
    char bSnr[10];
    char cSnr[10];
    char offHfSnr[10];
    char bHfSnr[10];
    char cHfSnr[10];
    formatSnr(
        curveData(SweepCurve::Off).measuredDb[0],
        noiseDb[curveIndex(SweepCurve::Off)],
        offSnr,
        sizeof(offSnr));
    formatSnr(
        curveData(SweepCurve::B).measuredDb[0],
        noiseDb[curveIndex(SweepCurve::B)],
        bSnr,
        sizeof(bSnr));
    formatSnr(
        curveData(SweepCurve::C).measuredDb[0],
        noiseDb[curveIndex(SweepCurve::C)],
        cSnr,
        sizeof(cSnr));
    formatSnr(
        curveData(SweepCurve::Off).measuredDb[0],
        hfNoiseDb[curveIndex(SweepCurve::Off)],
        offHfSnr,
        sizeof(offHfSnr));
    formatSnr(
        curveData(SweepCurve::B).measuredDb[0],
        hfNoiseDb[curveIndex(SweepCurve::B)],
        bHfSnr,
        sizeof(bHfSnr));
    formatSnr(
        curveData(SweepCurve::C).measuredDb[0],
        hfNoiseDb[curveIndex(SweepCurve::C)],
        cHfSnr,
        sizeof(cHfSnr));

    tft.fillRect(
        CONTENT_LEFT,
        SNR_SUMMARY_Y,
        CONTENT_WIDTH,
        SNR_SUMMARY_H,
        COL_BG);
    drawSnrLine(
        SNR_SUMMARY_Y,
        "S/N ",
        offSnr,
        bSnr,
        cSnr);
    drawSnrLine(
        SNR_SUMMARY_Y + SNR_LINE_HEIGHT,
        "HF ",
        offHfSnr,
        bHfSnr,
        cHfSnr);
    tft.setTextDatum(TL_DATUM);
}

void DolbyCheck::drawNoiseControl(
    const char *topText,
    const char *bottomText,
    bool pressed)
{
    (void)pressed;
    char message[48];
    const bool touchPrompt =
        strcmp(bottomText, "START") == 0 ||
        strcmp(bottomText, "NEXT") == 0 ||
        strcmp(bottomText, "RETRY") == 0;
    if (touchPrompt)
        snprintf(
            message,
            sizeof(message),
            strcmp(bottomText, "RETRY") == 0
                ? "TOUCH GRID TO RETRY %s"
                : "TOUCH GRID TO START %s",
            topText);
    else if (strcmp(topText, "CHECK 1kHz") == 0)
        snprintf(message, sizeof(message), "CHECK PLAYBACK 1kHz");
    else
        snprintf(message, sizeof(message), "%s: %s", topText, bottomText);

    drawStatus(
        message,
        strcmp(topText, "PATH OK") == 0
            ? TFT_GREEN
            : (touchPrompt || strncmp(topText, "NOISE", 5) == 0
                   ? curveColor(noiseCurve)
                   : TFT_CYAN));
}

void DolbyCheck::drawTwoHeadControl(
    const char *topText,
    const char *bottomText,
    bool pressed)
{
    (void)topText;
    (void)bottomText;
    (void)pressed;
}

void DolbyCheck::drawStartButton(bool pressed, SweepCurve curve)
{
    (void)pressed;
    (void)curve;
}

bool DolbyCheck::isStartTouch(uint16_t x, uint16_t y) const
{
    return x >= GRAPH_LEFT && x <= GRAPH_RIGHT &&
           y >= GRAPH_TOP && y <= GRAPH_BOTTOM;
}

bool DolbyCheck::isDolbyBStartAvailable() const
{
    const bool offComplete =
        curveData(SweepCurve::Off).captured[0] &&
        ((activeCurve == SweepCurve::Off &&
          offSweepState == OffSweepState::Complete) ||
         (activeCurve == SweepCurve::B &&
          offSweepState == OffSweepState::Error));
    return offComplete;
}

bool DolbyCheck::isDolbyCStartAvailable() const
{
    const bool bComplete =
        curveData(SweepCurve::B).captured[0] &&
        ((activeCurve == SweepCurve::B &&
          offSweepState == OffSweepState::Complete) ||
         (activeCurve == SweepCurve::C &&
          offSweepState == OffSweepState::Error));
    return bComplete;
}

bool DolbyCheck::selectStartCurve(SweepCurve &curve) const
{
    if (activeCurve == SweepCurve::Off &&
        (offSweepState == OffSweepState::Ready ||
         offSweepState == OffSweepState::Error))
    {
        curve = SweepCurve::Off;
        return true;
    }

    if (isDolbyBStartAvailable())
    {
        curve = SweepCurve::B;
        return true;
    }

    if (isDolbyCStartAvailable())
    {
        curve = SweepCurve::C;
        return true;
    }

    return false;
}

void DolbyCheck::clearStartButton()
{
    tft.fillRect(
        GRAPH_LEFT,
        GRAPH_TOP,
        GRAPH_WIDTH + 1,
        GRAPH_HEIGHT + 1,
        COL_BG);
    drawGraphGrid();
    drawStoredCurves();
}

DolbyCheck::CurvePlaceholder &DolbyCheck::activeCurveData()
{
    DeckPlaceholderState &deckState =
        activeDeckMode == DeckMode::TwoHead
            ? twoHeadState
            : threeHeadState;
    switch (activeCurve)
    {
    case SweepCurve::B:
        return deckState.dolbyB;
    case SweepCurve::C:
        return deckState.dolbyC;
    default:
        return deckState.dolbyOff;
    }
}

const DolbyCheck::CurvePlaceholder &DolbyCheck::curveData(
    SweepCurve curve) const
{
    const DeckPlaceholderState &deckState =
        activeDeckMode == DeckMode::TwoHead
            ? twoHeadState
            : threeHeadState;
    switch (curve)
    {
    case SweepCurve::B:
        return deckState.dolbyB;
    case SweepCurve::C:
        return deckState.dolbyC;
    default:
        return deckState.dolbyOff;
    }
}

void DolbyCheck::resetActiveSweep()
{
    record.stopCalibratedTestTone();
    noisePrecheckActive = false;
    CurvePlaceholder &curve = activeCurveData();
    for (uint8_t i = 0; i < DOLBY_CHECK_FREQUENCY_COUNT; ++i)
    {
        curve.measuredDb[i] = NAN;
        curve.relativeDb[i] = NAN;
        curve.captured[i] = false;
    }

    offSweepState = OffSweepState::Ready;
    offBandIndex = 0;
    offPointStartedMs = 0;
    offSumLeftRms = 0.0f;
    offSumRightRms = 0.0f;
    offLeftWindowCount = 0;
    offRightWindowCount = 0;
    offPointClipped = false;
    resetPlaybackPrecheck();
}

void DolbyCheck::resetPlaybackPrecheck()
{
    precheckStartedMs = 0;
    precheckSumLeftRms = 0.0f;
    precheckSumRightRms = 0.0f;
    precheckLeftWindowCount = 0;
    precheckRightWindowCount = 0;
    precheckLeftToneWindowCount = 0;
    precheckRightToneWindowCount = 0;
    precheckClipped = false;
}

void DolbyCheck::startPlaybackPrecheck(SweepCurve curve, bool forNoise)
{
    activeCurve = curve;
    if (forNoise)
    {
        record.stopCalibratedTestTone();
        resetPlaybackPrecheck();
    }
    else
    {
        resetActiveSweep();
    }
    noisePrecheckActive = forNoise;
    if (!forNoise && activeDeckMode == DeckMode::ThreeHead)
    {
        drawThreeHeadSweepProgress(true);
        drawGraphGrid();
        drawStoredCurves();
    }
    if (!record.setCalibratedTestTone(
            DOLBY_TEST_FREQUENCIES_HZ[0],
            DOLBY_TEST_LEVEL_DB))
    {
        failPlaybackPrecheck(forNoise ? "NOISE ERROR" : "OFF ERROR");
        return;
    }

    precheckStartedMs = millis();
    offSweepState = OffSweepState::PrecheckSettling;
    if (noisePrecheckActive)
        drawNoiseControl("CHECK 1kHz", "WAIT");
    else
        drawStatus("CHECK 1kHz", TFT_CYAN);
}

void DolbyCheck::updatePlaybackPrecheck(uint32_t now)
{
    if (offSweepState == OffSweepState::PrecheckSettling)
    {
        if (now - precheckStartedMs < record.tapeResponsePointSettleMs(true))
            return;

        precheckStartedMs = now;
        offSweepState = OffSweepState::PrecheckMeasuring;
        return;
    }

    if (offSweepState == OffSweepState::PrecheckPassed)
    {
        if (now - precheckStartedMs >= DOLBY_PLAYBACK_OK_DISPLAY_MS)
        {
            if (noisePrecheckActive)
                startNoiseMeasurement();
            else
                startActiveSweep();
        }
        return;
    }

    if (now - precheckStartedMs >= record.tapeResponsePointMeasureMs())
    {
        finishPlaybackPrecheck();
        return;
    }

    float leftRms = 0.0f;
    float rightRms = 0.0f;
    float leftFrequencyHz = 0.0f;
    float rightFrequencyHz = 0.0f;
    bool leftValid = false;
    bool rightValid = false;
    bool clipped = false;
    record.measureTapeResponseToneWindow(
        leftRms,
        rightRms,
        leftFrequencyHz,
        rightFrequencyHz,
        leftValid,
        rightValid,
        clipped);

    precheckClipped = precheckClipped || clipped;
    if (leftValid)
    {
        precheckSumLeftRms += leftRms;
        ++precheckLeftWindowCount;
        if (isExpectedPlaybackFrequency(leftFrequencyHz))
            ++precheckLeftToneWindowCount;
    }
    if (rightValid)
    {
        precheckSumRightRms += rightRms;
        ++precheckRightWindowCount;
        if (isExpectedPlaybackFrequency(rightFrequencyHz))
            ++precheckRightToneWindowCount;
    }
}

void DolbyCheck::finishPlaybackPrecheck()
{
    if (precheckClipped)
    {
        failPlaybackPrecheck("PLAYBACK LEVEL HIGH");
        return;
    }

    if (precheckLeftToneWindowCount < DOLBY_PLAYBACK_REQUIRED_TONE_WINDOWS ||
        precheckRightToneWindowCount < DOLBY_PLAYBACK_REQUIRED_TONE_WINDOWS)
    {
        failPlaybackPrecheck("NO 1kHz PLAYBACK");
        return;
    }

    if (precheckLeftWindowCount == 0 || precheckRightWindowCount == 0)
    {
        failPlaybackPrecheck("PLAYBACK LEVEL LOW");
        return;
    }

    const float averageLeftRms =
        precheckSumLeftRms / precheckLeftWindowCount;
    const float averageRightRms =
        precheckSumRightRms / precheckRightWindowCount;
    const float combinedRms = sqrtf(
        0.5f *
        (averageLeftRms * averageLeftRms +
         averageRightRms * averageRightRms));
    const float combinedDb = record.rmsToReferenceDb(combinedRms);
    if (!isfinite(combinedDb) || combinedDb < DOLBY_PLAYBACK_MIN_LEVEL_DB)
    {
        failPlaybackPrecheck("PLAYBACK LEVEL LOW");
        return;
    }
    if (combinedDb > DOLBY_PLAYBACK_MAX_LEVEL_DB)
    {
        failPlaybackPrecheck("PLAYBACK LEVEL HIGH");
        return;
    }

    precheckStartedMs = millis();
    offSweepState = OffSweepState::PrecheckPassed;
    if (noisePrecheckActive)
        drawNoiseControl("PATH OK", "MUTING");
    else
        drawStatus("LEVEL OK", TFT_GREEN);
}

void DolbyCheck::failPlaybackPrecheck(const char *status)
{
    if (noisePrecheckActive)
        failNoiseMeasurement(status);
    else
        failOffSweep(status);
}

void DolbyCheck::startActiveSweep()
{
    resetActiveSweep();
    threeHeadProgressStep = 0;
    threeHeadProgressBricks = 0xFF;
    drawThreeHeadSweepProgress(true);
    startOffPoint();
}

void DolbyCheck::startOffPoint()
{
    if (offBandIndex >= DOLBY_CHECK_FREQUENCY_COUNT ||
        !record.setCalibratedTestTone(
            DOLBY_TEST_FREQUENCIES_HZ[offBandIndex],
            DOLBY_TEST_LEVEL_DB))
    {
        failOffSweep();
        return;
    }

    offSumLeftRms = 0.0f;
    offSumRightRms = 0.0f;
    offLeftWindowCount = 0;
    offRightWindowCount = 0;
    offPointClipped = false;
    offPointStartedMs = millis();
    offSweepState = OffSweepState::Settling;
    if (activeDeckMode == DeckMode::ThreeHead)
        drawThreeHeadSweepProgress();

    char status[24];
    const uint16_t frequencyHz = DOLBY_TEST_FREQUENCIES_HZ[offBandIndex];
    if (activeCurve != SweepCurve::Off)
    {
        snprintf(
            status,
            sizeof(status),
            "%c %u.%u kHz",
            activeCurve == SweepCurve::B ? 'B' : 'C',
            frequencyHz / 1000,
            (frequencyHz % 1000) / 100);
    }
    else
    {
        char frequency[12];
        formatFrequency(frequency, sizeof(frequency), frequencyHz);
        snprintf(status, sizeof(status), "OFF TEST %s", frequency);
    }
    drawStatus(status, curveColor(activeCurve));
}

void DolbyCheck::updateOffSweep()
{
    const uint32_t now = millis();
    if (offSweepState == OffSweepState::PrecheckSettling ||
        offSweepState == OffSweepState::PrecheckMeasuring ||
        offSweepState == OffSweepState::PrecheckPassed)
    {
        updatePlaybackPrecheck(now);
        return;
    }

    if (offSweepState != OffSweepState::Settling &&
        offSweepState != OffSweepState::Measuring)
        return;

    if (offSweepState == OffSweepState::Settling)
    {
        const uint32_t settleMs =
            record.tapeResponsePointSettleMs(offBandIndex == 0);
        if (now - offPointStartedMs < settleMs)
            return;

        offPointStartedMs = now;
        offSweepState = OffSweepState::Measuring;
        return;
    }

    if (now - offPointStartedMs < record.tapeResponsePointMeasureMs())
    {
        float leftRms = 0.0f;
        float rightRms = 0.0f;
        float leftFrequencyHz = 0.0f;
        float rightFrequencyHz = 0.0f;
        bool leftValid = false;
        bool rightValid = false;
        bool clipped = false;
        record.measureTapeResponseToneWindow(
            leftRms,
            rightRms,
            leftFrequencyHz,
            rightFrequencyHz,
            leftValid,
            rightValid,
            clipped);

        offPointClipped = offPointClipped || clipped;
        const uint16_t expectedHz =
            DOLBY_TEST_FREQUENCIES_HZ[offBandIndex];
        const float leftDb = record.rmsToReferenceDb(leftRms);
        const float rightDb = record.rmsToReferenceDb(rightRms);
        // At 15 kHz the 40 kS/s validation capture has only 2.67 samples per
        // period, too close to the lower limit of the zero-crossing estimator
        // for a reliable per-window gate.  Up to 12.5 kHz, a match on either
        // playback channel confirms the generated tone.  At 15 kHz, retain
        // the strict stereo level/window validation so noise floor still
        // cannot become a measured point.
        const bool expectedToneDetected =
            expectedHz > DOLBY_MAX_RELIABLE_FREQUENCY_CHECK_HZ ||
            isExpectedSweepFrequency(leftFrequencyHz, expectedHz) ||
            isExpectedSweepFrequency(rightFrequencyHz, expectedHz);
        if (leftValid && isPlausibleSweepLevel(leftDb) &&
            expectedToneDetected)
        {
            offSumLeftRms += leftRms;
            ++offLeftWindowCount;
        }
        if (rightValid && isPlausibleSweepLevel(rightDb) &&
            expectedToneDetected)
        {
            offSumRightRms += rightRms;
            ++offRightWindowCount;
        }
        return;
    }

    finishOffPoint();
}

void DolbyCheck::finishOffPoint()
{
    if (offPointClipped)
    {
        failOffSweep();
        return;
    }
    if (offLeftWindowCount < DOLBY_PLAYBACK_REQUIRED_TONE_WINDOWS ||
        offRightWindowCount < DOLBY_PLAYBACK_REQUIRED_TONE_WINDOWS)
    {
        failOffSweep("SIGNAL LOST");
        return;
    }

    const float averageLeftRms =
        offSumLeftRms / offLeftWindowCount;
    const float averageRightRms =
        offSumRightRms / offRightWindowCount;
    const float combinedRms = sqrtf(
        0.5f *
        (averageLeftRms * averageLeftRms +
         averageRightRms * averageRightRms));
    const float measuredDb = record.rmsToReferenceDb(combinedRms);
    if (!isfinite(measuredDb))
    {
        failOffSweep();
        return;
    }

    CurvePlaceholder &curve = activeCurveData();
    curve.measuredDb[offBandIndex] = measuredDb;
    curve.captured[offBandIndex] = true;
    curve.relativeDb[offBandIndex] = offBandIndex == 0
                                         ? 0.0f
                                         : measuredDb - curve.measuredDb[0];
    drawStoredCurves();

    ++offBandIndex;
    if (offBandIndex >= DOLBY_CHECK_FREQUENCY_COUNT)
    {
        record.stopCalibratedTestTone();
        if (!normalizeActiveCurve())
        {
            failOffSweep();
            return;
        }

        offSweepState = OffSweepState::Complete;
        if (activeDeckMode == DeckMode::ThreeHead)
            drawThreeHeadSweepProgress(true);
        drawStoredCurves();
        if (activeCurve == SweepCurve::C)
        {
            startNoiseWorkflow();
        }
        else if (activeCurve == SweepCurve::B)
        {
            drawStatus("TOUCH GRID TO START DOLBY C", curveColor(SweepCurve::C));
            drawStartButton(false, SweepCurve::C);
        }
        else
        {
            drawStatus("TOUCH GRID TO START DOLBY B", curveColor(SweepCurve::B));
            drawStartButton(false, SweepCurve::B);
        }
        return;
    }

    startOffPoint();
}

bool DolbyCheck::normalizeActiveCurve()
{
    CurvePlaceholder &curve = activeCurveData();
    const float referenceDb = curve.measuredDb[0];
    if (!curve.captured[0] || !isfinite(referenceDb))
        return false;

    for (uint8_t i = 0; i < DOLBY_CHECK_FREQUENCY_COUNT; ++i)
    {
        if (!curve.captured[i] || !isfinite(curve.measuredDb[i]))
            return false;
        curve.relativeDb[i] = curve.measuredDb[i] - referenceDb;
    }
    curve.relativeDb[0] = 0.0f;
    return true;
}

void DolbyCheck::drawCurve(const CurvePlaceholder &curve, uint16_t color)
{
    bool previousValid = false;
    int previousX = 0;
    int previousY = 0;

    for (uint8_t i = 0; i < DOLBY_CHECK_FREQUENCY_COUNT; ++i)
    {
        const float relativeDb = curve.relativeDb[i];
        if (!curve.captured[i] || !isfinite(relativeDb))
        {
            previousValid = false;
            continue;
        }

        const float clippedDb = constrain(relativeDb, -6.0f, 6.0f);
        const int x = graphBandX(i);
        const int y = GRAPH_TOP +
                      static_cast<int>(
                          (6.0f - clippedDb) * GRAPH_HEIGHT / 12.0f);
        if (previousValid)
            tft.drawLine(previousX, previousY, x, y, color);
        tft.fillCircle(x, y, 2, color);
        previousX = x;
        previousY = y;
        previousValid = true;
    }
}

void DolbyCheck::drawStoredCurves()
{
    drawCurve(curveData(SweepCurve::Off), TFT_YELLOW);
    drawCurve(curveData(SweepCurve::B), TFT_GREEN);
    drawCurve(curveData(SweepCurve::C), TFT_ORANGE);
}

bool DolbyCheck::calculateTrackingErrors(
    const CurvePlaceholder &curve,
    float &averageErrorDb,
    float &maximumErrorDb) const
{
    float sumErrorDb = 0.0f;
    maximumErrorDb = 0.0f;
    for (uint8_t i = DOLBY_HF_FIRST_INDEX;
         i < DOLBY_CHECK_FREQUENCY_COUNT;
         ++i)
    {
        if (!curve.captured[i] || !isfinite(curve.relativeDb[i]))
            return false;

        const float errorDb = fabsf(curve.relativeDb[i]);
        sumErrorDb += errorDb;
        if (errorDb > maximumErrorDb)
            maximumErrorDb = errorDb;
    }

    averageErrorDb = sumErrorDb /
                     (DOLBY_CHECK_FREQUENCY_COUNT - DOLBY_HF_FIRST_INDEX);
    return true;
}

uint8_t DolbyCheck::curveIndex(SweepCurve curve) const
{
    switch (curve)
    {
    case SweepCurve::B:
        return 1;
    case SweepCurve::C:
        return 2;
    default:
        return 0;
    }
}

void DolbyCheck::resetTwoHeadWorkflow()
{
    record.stopCalibratedTestTone();
    activeCurve = SweepCurve::Off;
    noiseState = NoiseState::Inactive;
    noiseCurve = SweepCurve::Off;
    for (uint8_t i = 0; i < 3; ++i)
    {
        noiseDb[i] = NAN;
        hfNoiseDb[i] = NAN;
    }
    twoHeadWorkflowState = TwoHeadWorkflowState::ReadyToRecord;
    twoHeadSequencePhase = TwoHeadSequencePhase::Sync;
    twoHeadSegmentStartedMs = 0;
    twoHeadPlaybackStartedMs = 0;
    twoHeadSyncDetections = 0;
    twoHeadSyncExitDetections = 0;
    twoHeadSyncFound = false;
    offBandIndex = 0;
}

void DolbyCheck::prepareTwoHeadMode(SweepCurve curve)
{
    activeCurve = curve;
    resetActiveSweep();
    twoHeadWorkflowState = TwoHeadWorkflowState::ReadyToRecord;
    clearStartButton();

    drawStatus("TOUCH GRID TO START RECORD", curveColor(curve));
    drawLegend();
    drawTwoHeadControl(
        curve == SweepCurve::Off ? "START" :
        (curve == SweepCurve::B ? "START B" : "START C"),
        "RECORD");
}

void DolbyCheck::handleTwoHeadAction()
{
    const bool recordAction =
        twoHeadWorkflowState == TwoHeadWorkflowState::ReadyToRecord ||
        twoHeadWorkflowState == TwoHeadWorkflowState::RecordError;
    const bool playbackAction =
        twoHeadWorkflowState == TwoHeadWorkflowState::Rewind ||
        twoHeadWorkflowState == TwoHeadWorkflowState::PlaybackError;
    if (!recordAction && !playbackAction)
        return;

    drawTwoHeadControl(
        "START",
        playbackAction ? "PLAYBACK" : "RECORD",
        true);
    while (touch.pressed())
    {
        display.updateClock();
        delay(5);
    }

    clearStartButton();
    if (recordAction)
        startTwoHeadRecording();
    else
        startTwoHeadPlayback();
}

void DolbyCheck::startTwoHeadRecording()
{
    resetActiveSweep();
    offBandIndex = 0;
    twoHeadSequencePhase = TwoHeadSequencePhase::Sync;
    twoHeadRecordProgressStep = 0;
    twoHeadRecordProgressBricks = 0xFF;
    if (!record.setCalibratedTestTone(
            DOLBY_2H_SYNC_HZ,
            DOLBY_TEST_LEVEL_DB))
    {
        failTwoHead("GENERATOR ERROR", false);
        return;
    }

    twoHeadSegmentStartedMs = millis();
    twoHeadWorkflowState = TwoHeadWorkflowState::Recording;
    drawStatus("RECORDING", curveColor(activeCurve));
    drawTwoHeadRecordProgress();
    drawLegend(true);
    drawTwoHeadControl("RECORDING", "SYNC");
}

void DolbyCheck::updateTwoHeadWorkflow()
{
    if (noiseState == NoiseState::Settling ||
        noiseState == NoiseState::Measuring ||
        noiseState == NoiseState::MeasuringHf)
    {
        updateNoiseMeasurement();
        return;
    }

    const uint32_t now = millis();
    switch (twoHeadWorkflowState)
    {
    case TwoHeadWorkflowState::Recording:
        updateTwoHeadRecording(now);
        break;
    case TwoHeadWorkflowState::WaitingSync:
        updateTwoHeadSync(now);
        break;
    case TwoHeadWorkflowState::Analyzing:
        updateTwoHeadAnalysis(now);
        break;
    case TwoHeadWorkflowState::WaitingEndSync:
        updateTwoHeadEndSync(now);
        break;
    default:
        break;
    }
}

void DolbyCheck::updateTwoHeadRecording(uint32_t now)
{
    drawTwoHeadRecordProgress();
    const uint32_t elapsed = now - twoHeadSegmentStartedMs;
    if (twoHeadSequencePhase == TwoHeadSequencePhase::Sync)
    {
        if (elapsed < DOLBY_2H_SYNC_MS)
            return;
        record.stopCalibratedTestTone();
        twoHeadSequencePhase = TwoHeadSequencePhase::Gap;
        twoHeadSegmentStartedMs = now;
        drawTwoHeadControl("RECORDING", "WAIT");
        return;
    }

    if (twoHeadSequencePhase == TwoHeadSequencePhase::Gap)
    {
        if (elapsed < DOLBY_2H_GAP_MS)
            return;
        if (offBandIndex >= DOLBY_CHECK_FREQUENCY_COUNT)
        {
            if (!record.setCalibratedTestTone(
                    DOLBY_2H_SYNC_HZ,
                    DOLBY_TEST_LEVEL_DB))
            {
                failTwoHead("GENERATOR ERROR", false);
                return;
            }
            twoHeadSequencePhase = TwoHeadSequencePhase::EndSync;
            twoHeadSegmentStartedMs = now;
            drawStatus("RECORDING", TFT_CYAN);
            drawTwoHeadControl("RECORDING", "END SYNC");
            return;
        }

        if (!record.setCalibratedTestTone(
                DOLBY_TEST_FREQUENCIES_HZ[offBandIndex],
                DOLBY_TEST_LEVEL_DB))
        {
            failTwoHead("GENERATOR ERROR", false);
            return;
        }
        twoHeadSequencePhase = TwoHeadSequencePhase::Tone;
        twoHeadSegmentStartedMs = now;
        char frequency[10];
        formatFrequency(
            frequency,
            sizeof(frequency),
            DOLBY_TEST_FREQUENCIES_HZ[offBandIndex]);
        drawStatus("RECORDING", TFT_CYAN);
        drawTwoHeadControl("RECORDING", frequency);
        return;
    }

    if (twoHeadSequencePhase == TwoHeadSequencePhase::Tone)
    {
        if (elapsed < DOLBY_2H_TONE_MS)
            return;
        record.stopCalibratedTestTone();
        ++offBandIndex;
        twoHeadSequencePhase =
            offBandIndex >= DOLBY_CHECK_FREQUENCY_COUNT
                ? TwoHeadSequencePhase::Noise
                : TwoHeadSequencePhase::Gap;
        twoHeadSegmentStartedMs = now;
        drawTwoHeadControl(
            "RECORDING",
            twoHeadSequencePhase == TwoHeadSequencePhase::Noise
                ? "NOISE"
                : "WAIT");
        return;
    }

    if (twoHeadSequencePhase == TwoHeadSequencePhase::Noise)
    {
        if (elapsed < DOLBY_2H_TONE_MS)
            return;
        if (!record.setCalibratedTestTone(
                DOLBY_2H_SYNC_HZ,
                DOLBY_TEST_LEVEL_DB))
        {
            failTwoHead("GENERATOR ERROR", false);
            return;
        }
        twoHeadSequencePhase = TwoHeadSequencePhase::EndSync;
        twoHeadSegmentStartedMs = now;
        drawStatus("RECORDING", TFT_CYAN);
        drawTwoHeadControl("RECORDING", "END SYNC");
        return;
    }

    if (elapsed < DOLBY_2H_SYNC_MS)
        return;

    record.stopCalibratedTestTone();
    twoHeadWorkflowState = TwoHeadWorkflowState::Rewind;
    clearStartButton();
    drawStatus("TOUCH GRID TO START PLAYBACK", TFT_YELLOW);
    drawTwoHeadControl("START", "PLAYBACK");
}

void DolbyCheck::drawTwoHeadRecordProgress(bool clear)
{
    const uint16_t nextStep = static_cast<uint16_t>(offBandIndex) + 1;
    const uint8_t step = static_cast<uint8_t>(
        nextStep < DOLBY_CHECK_FREQUENCY_COUNT + 1
            ? nextStep
            : DOLBY_CHECK_FREQUENCY_COUNT + 1);
    drawDolbyProgress(
        step,
        DOLBY_CHECK_FREQUENCY_COUNT + 1,
        clear,
        twoHeadRecordProgressStep,
        twoHeadRecordProgressBricks);
}

void DolbyCheck::drawThreeHeadSweepProgress(bool clear)
{
    const uint16_t nextStep = static_cast<uint16_t>(offBandIndex) + 1;
    const uint8_t step = static_cast<uint8_t>(
        nextStep < DOLBY_CHECK_FREQUENCY_COUNT
            ? nextStep
            : DOLBY_CHECK_FREQUENCY_COUNT);
    drawDolbyProgress(
        step,
        DOLBY_CHECK_FREQUENCY_COUNT,
        clear,
        threeHeadProgressStep,
        threeHeadProgressBricks);
}

void DolbyCheck::drawDolbyProgress(
    uint8_t step,
    uint8_t totalSteps,
    bool clear,
    uint8_t &previousStep,
    uint8_t &previousBricks)
{
    constexpr int FONT = 2;
    constexpr int LABEL_GAP = 8;
    constexpr int BAR_Y = FOOTER_Y - PROGRESS_BAR_BRICK_HEIGHT - 6;
    constexpr int LABEL_Y = BAR_Y - 3;
    char maxLabel[8];
    snprintf(
        maxLabel,
        sizeof(maxLabel),
        "%u/%u",
        static_cast<unsigned>(totalSteps),
        static_cast<unsigned>(totalSteps));
    const int labelWidth = tft.textWidth(maxLabel, FONT);
    const int totalWidth = labelWidth + LABEL_GAP + PROGRESS_BAR_WIDTH;
    const int groupX = (LCD_WIDTH - totalWidth) / 2;
    const int barX = groupX + labelWidth + LABEL_GAP;

    if (clear)
    {
        tft.fillRect(
            groupX,
            LABEL_Y,
            totalWidth,
            PROGRESS_BAR_BRICK_HEIGHT + 6,
            COL_BG);
        previousStep = 0;
        previousBricks = 0xFF;
        return;
    }

    if (step == previousStep)
        return;

    char label[8];
    snprintf(
        label,
        sizeof(label),
        "%u/%u",
        static_cast<unsigned>(step),
        static_cast<unsigned>(totalSteps));

    tft.fillRect(
        groupX,
        BAR_Y - 6,
        labelWidth,
        PROGRESS_BAR_BRICK_HEIGHT + 6,
        COL_BG);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_CYAN, COL_BG);
    tft.drawString(label, groupX, BAR_Y - 6, FONT);

    const uint8_t bricks = static_cast<uint8_t>(
        (step * PROGRESS_BAR_BRICK_COUNT + totalSteps - 1) / totalSteps);
    drawCalibrationProgressBar(
        tft,
        barX,
        BAR_Y,
        bricks,
        previousBricks);
    previousStep = step;
    previousBricks = bricks;
}

void DolbyCheck::drawProgressMessage(const char *text, uint16_t color)
{
    constexpr int FONT = 2;
    constexpr int BAR_Y = FOOTER_Y - PROGRESS_BAR_BRICK_HEIGHT - 6;
    constexpr int LABEL_Y = BAR_Y - 3;
    constexpr int ROW_HEIGHT = PROGRESS_BAR_BRICK_HEIGHT + 6;
    tft.fillRect(
        CONTENT_LEFT,
        LABEL_Y,
        CONTENT_WIDTH,
        ROW_HEIGHT,
        COL_BG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(color, COL_BG);
    tft.drawString(
        text,
        CONTENT_LEFT + CONTENT_WIDTH / 2,
        LABEL_Y + ROW_HEIGHT / 2,
        FONT);
    tft.setTextDatum(TL_DATUM);
}

void DolbyCheck::startTwoHeadPlayback()
{
    record.stopCalibratedTestTone();
    resetActiveSweep();
    offBandIndex = 0;
    twoHeadSyncDetections = 0;
    twoHeadSyncExitDetections = 0;
    twoHeadSyncFound = false;
    twoHeadPlaybackStartedMs = millis();
    twoHeadWorkflowState = TwoHeadWorkflowState::WaitingSync;
    drawStatus("WAITING FOR SYNC", TFT_CYAN);
    drawTwoHeadControl("PLAYBACK", "SYNC");
}

bool DolbyCheck::measureTwoHeadSyncWindow(bool &clipped)
{
    float leftRms = 0.0f;
    float rightRms = 0.0f;
    float leftFrequencyHz = 0.0f;
    float rightFrequencyHz = 0.0f;
    bool leftValid = false;
    bool rightValid = false;
    clipped = false;
    record.measureTapeResponseToneWindow(
        leftRms,
        rightRms,
        leftFrequencyHz,
        rightFrequencyHz,
        leftValid,
        rightValid,
        clipped);
    if (clipped || !leftValid || !rightValid ||
        !isExpectedSweepFrequency(leftFrequencyHz, DOLBY_2H_SYNC_HZ) ||
        !isExpectedSweepFrequency(rightFrequencyHz, DOLBY_2H_SYNC_HZ))
        return false;

    const float leftDb = record.rmsToReferenceDb(leftRms);
    const float rightDb = record.rmsToReferenceDb(rightRms);
    const float combinedRms = sqrtf(
        0.5f * (leftRms * leftRms + rightRms * rightRms));
    const float combinedDb = record.rmsToReferenceDb(combinedRms);
    return isPlausibleSweepLevel(leftDb) &&
           isPlausibleSweepLevel(rightDb) &&
           isfinite(combinedDb) &&
           combinedDb >= DOLBY_PLAYBACK_MIN_LEVEL_DB &&
           combinedDb <= DOLBY_PLAYBACK_MAX_LEVEL_DB;
}

void DolbyCheck::updateTwoHeadSync(uint32_t now)
{
    bool clipped = false;
    const bool syncValid = measureTwoHeadSyncWindow(clipped);
    if (clipped)
    {
        failTwoHead("PLAYBACK LEVEL HIGH", true);
        return;
    }

    if (!twoHeadSyncFound)
    {
        if (syncValid)
        {
            if (twoHeadSyncDetections < DOLBY_2H_SYNC_CONFIRMATIONS)
                ++twoHeadSyncDetections;
        }
        else if (twoHeadSyncDetections > 0)
        {
            --twoHeadSyncDetections;
        }

        if (twoHeadSyncDetections >= DOLBY_2H_SYNC_CONFIRMATIONS)
        {
            twoHeadSyncFound = true;
            twoHeadSyncExitDetections = 0;
            drawStatus("SYNC FOUND", TFT_GREEN);
        }
    }
    else
    {
        if (syncValid)
        {
            twoHeadSyncExitDetections = 0;
        }
        else if (twoHeadSyncExitDetections < DOLBY_2H_SYNC_CONFIRMATIONS)
        {
            ++twoHeadSyncExitDetections;
        }

        if (twoHeadSyncExitDetections >= DOLBY_2H_SYNC_CONFIRMATIONS)
        {
            startTwoHeadAnalysis(now);
            return;
        }
    }

    if (now - twoHeadPlaybackStartedMs >= DOLBY_2H_SYNC_TIMEOUT_MS)
        failTwoHead("NO SYNC / NO SIGNAL", true);
}

void DolbyCheck::startTwoHeadAnalysis(uint32_t now)
{
    drawTwoHeadRecordProgress(true);
    offBandIndex = 0;
    twoHeadSequencePhase = TwoHeadSequencePhase::Gap;
    twoHeadSegmentStartedMs = now;
    twoHeadWorkflowState = TwoHeadWorkflowState::Analyzing;
    drawGraphGrid();
    drawStoredCurves();
    drawStatus("ANALYZING", TFT_CYAN);
    drawTwoHeadControl("ANALYZING", "WAIT");
}

void DolbyCheck::updateTwoHeadAnalysis(uint32_t now)
{
    const uint32_t elapsed = now - twoHeadSegmentStartedMs;
    if (twoHeadSequencePhase == TwoHeadSequencePhase::Gap)
    {
        if (elapsed < DOLBY_2H_GAP_MS)
            return;
        offSumLeftRms = 0.0f;
        offSumRightRms = 0.0f;
        offLeftWindowCount = 0;
        offRightWindowCount = 0;
        offPointClipped = false;
        twoHeadSequencePhase = TwoHeadSequencePhase::Tone;
        twoHeadSegmentStartedMs = now;
        char frequency[10];
        formatFrequency(
            frequency,
            sizeof(frequency),
            DOLBY_TEST_FREQUENCIES_HZ[offBandIndex]);
        drawStatus("ANALYZING", TFT_CYAN);
        drawTwoHeadControl("ANALYZING", frequency);
        return;
    }

    if (twoHeadSequencePhase != TwoHeadSequencePhase::Tone)
        return;

    if (elapsed >= DOLBY_2H_TONE_SETTLE_MS &&
        elapsed < DOLBY_2H_TONE_SETTLE_MS +
                      DOLBY_2H_TONE_MEASURE_MS)
    {
        float leftRms = 0.0f;
        float rightRms = 0.0f;
        float leftFrequencyHz = 0.0f;
        float rightFrequencyHz = 0.0f;
        bool leftValid = false;
        bool rightValid = false;
        bool clipped = false;
        record.measureTapeResponseToneWindow(
            leftRms,
            rightRms,
            leftFrequencyHz,
            rightFrequencyHz,
            leftValid,
            rightValid,
            clipped);
        offPointClipped = offPointClipped || clipped;
        const uint16_t expectedHz =
            DOLBY_TEST_FREQUENCIES_HZ[offBandIndex];
        const float leftDb = record.rmsToReferenceDb(leftRms);
        const float rightDb = record.rmsToReferenceDb(rightRms);
        const bool expectedToneDetected =
            expectedHz > DOLBY_MAX_RELIABLE_FREQUENCY_CHECK_HZ ||
            isExpectedSweepFrequency(leftFrequencyHz, expectedHz) ||
            isExpectedSweepFrequency(rightFrequencyHz, expectedHz);
        if (leftValid && isPlausibleSweepLevel(leftDb) &&
            expectedToneDetected)
        {
            offSumLeftRms += leftRms;
            ++offLeftWindowCount;
        }
        if (rightValid && isPlausibleSweepLevel(rightDb) &&
            expectedToneDetected)
        {
            offSumRightRms += rightRms;
            ++offRightWindowCount;
        }
    }

    if (elapsed >= DOLBY_2H_TONE_MS)
        finishTwoHeadBand(now);
}

void DolbyCheck::finishTwoHeadBand(uint32_t now)
{
    if (offPointClipped ||
        offLeftWindowCount < DOLBY_PLAYBACK_REQUIRED_TONE_WINDOWS ||
        offRightWindowCount < DOLBY_PLAYBACK_REQUIRED_TONE_WINDOWS)
    {
        failTwoHead("SIGNAL LOST", true);
        return;
    }

    const float averageLeftRms = offSumLeftRms / offLeftWindowCount;
    const float averageRightRms = offSumRightRms / offRightWindowCount;
    const float combinedRms = sqrtf(
        0.5f * (averageLeftRms * averageLeftRms +
                averageRightRms * averageRightRms));
    const float measuredDb = record.rmsToReferenceDb(combinedRms);
    if (!isfinite(measuredDb))
    {
        failTwoHead("SIGNAL INVALID", true);
        return;
    }
    if (offBandIndex == 0 && measuredDb < DOLBY_PLAYBACK_MIN_LEVEL_DB)
    {
        failTwoHead("PLAYBACK LEVEL LOW", true);
        return;
    }
    if (offBandIndex == 0 && measuredDb > DOLBY_PLAYBACK_MAX_LEVEL_DB)
    {
        failTwoHead("PLAYBACK LEVEL HIGH", true);
        return;
    }

    CurvePlaceholder &curve = activeCurveData();
    curve.measuredDb[offBandIndex] = measuredDb;
    curve.captured[offBandIndex] = true;
    curve.relativeDb[offBandIndex] = offBandIndex == 0
                                         ? 0.0f
                                         : measuredDb - curve.measuredDb[0];
    drawStoredCurves();
    ++offBandIndex;
    if (offBandIndex >= DOLBY_CHECK_FREQUENCY_COUNT &&
        !normalizeActiveCurve())
    {
        failTwoHead("REFERENCE INVALID", true);
        return;
    }

    if (offBandIndex >= DOLBY_CHECK_FREQUENCY_COUNT)
    {
        noiseCurve = activeCurve;
        startNoiseMeasurement();
        return;
    }

    twoHeadSequencePhase = TwoHeadSequencePhase::Gap;
    twoHeadSegmentStartedMs = now;
}

void DolbyCheck::updateTwoHeadEndSync(uint32_t now)
{
    bool clipped = false;
    if (measureTwoHeadSyncWindow(clipped))
    {
        if (twoHeadSyncDetections < DOLBY_2H_SYNC_CONFIRMATIONS)
            ++twoHeadSyncDetections;
        if (twoHeadSyncDetections >= DOLBY_2H_SYNC_CONFIRMATIONS)
        {
            completeTwoHeadMode();
            return;
        }
    }
    else
    {
        twoHeadSyncDetections = 0;
        if (clipped)
        {
            failTwoHead("PLAYBACK LEVEL HIGH", true);
            return;
        }
    }

    if (now - twoHeadPlaybackStartedMs >= DOLBY_2H_END_SYNC_TIMEOUT_MS)
        failTwoHead("END SYNC LOST", true);
}

void DolbyCheck::completeTwoHeadMode()
{
    clearStartButton();
    drawStoredCurves();
    if (activeCurve == SweepCurve::Off)
    {
        prepareTwoHeadMode(SweepCurve::B);
    }
    else if (activeCurve == SweepCurve::B)
    {
        prepareTwoHeadMode(SweepCurve::C);
    }
    else
    {
        twoHeadWorkflowState = TwoHeadWorkflowState::Complete;
        drawTrackingSummary();
        drawSnrSummary();
    }
}

void DolbyCheck::failTwoHead(const char *status, bool playbackError)
{
    record.stopCalibratedTestTone();
    twoHeadWorkflowState = playbackError
                               ? TwoHeadWorkflowState::PlaybackError
                               : TwoHeadWorkflowState::RecordError;
    clearStartButton();
    drawStatus(status, TFT_RED);
    drawTwoHeadControl(
        playbackError ? "REWIND" : "RETRY",
        playbackError ? "RETRY" : "RECORD");
}

void DolbyCheck::startNoiseWorkflow()
{
    record.stopCalibratedTestTone();
    noisePrecheckActive = false;
    noiseCurve = SweepCurve::Off;
    noiseState = NoiseState::Prompt;
    drawThreeHeadSweepProgress(true);
    for (uint8_t i = 0; i < 3; ++i)
    {
        noiseDb[i] = NAN;
        hfNoiseDb[i] = NAN;
    }
    tft.fillRect(
        CONTENT_LEFT,
        SNR_SUMMARY_Y,
        CONTENT_WIDTH,
        SNR_SUMMARY_H,
        COL_BG);
    drawNoiseControl("NOISE OFF", "START");
}

void DolbyCheck::startNoisePrecheck()
{
    noiseState = NoiseState::Precheck;
    startPlaybackPrecheck(noiseCurve, true);
}

void DolbyCheck::startNoiseMeasurement()
{
    record.stopCalibratedTestTone();
    noisePrecheckActive = false;
    offSweepState = OffSweepState::Complete;
    noiseStartedMs = millis();
    noiseSumLeftMeanSquare = 0.0f;
    noiseSumRightMeanSquare = 0.0f;
    noiseMinimumWindowDb = NAN;
    noiseMaximumWindowDb = NAN;
    noiseWindowCount = 0;
    noiseClipped = false;
    noiseState = NoiseState::Settling;

    const char *const label =
        noiseCurve == SweepCurve::Off
            ? "NOISE OFF"
            : (noiseCurve == SweepCurve::B ? "NOISE B" : "NOISE C");
    drawNoiseControl(label, "MUTING");
}

void DolbyCheck::updateNoiseMeasurement()
{
    if (noiseState == NoiseState::MeasuringHf)
    {
        updateHfNoiseMeasurement();
        return;
    }

    if (noiseState == NoiseState::Settling)
    {
        if (millis() - noiseStartedMs < NOISE_SETTLE_MS)
            return;

        noiseState = NoiseState::Measuring;
        const char *const label =
            noiseCurve == SweepCurve::Off
                ? "NOISE OFF"
                : (noiseCurve == SweepCurve::B ? "NOISE B" : "NOISE C");
        drawNoiseControl(label, "MEASURE");
    }

    if (noiseState != NoiseState::Measuring)
        return;

    float leftRms = 0.0f;
    float rightRms = 0.0f;
    bool leftValid = false;
    bool rightValid = false;
    bool clipped = false;
    record.measureTapeResponseWindow(
        leftRms,
        rightRms,
        leftValid,
        rightValid,
        clipped);
    noiseClipped = noiseClipped || clipped;
    if (clipped || !isfinite(leftRms) || !isfinite(rightRms) ||
        leftRms < 0.0f || rightRms < 0.0f)
    {
        failNoiseMeasurement(clipped ? "NOISE CLIPPED" : "NOISE INVALID");
        return;
    }

    // Noise can legitimately be below the normal tone-valid threshold, so
    // leftValid/rightValid are intentionally not used here.  Connectivity was
    // established by the immediately preceding 1 kHz pre-check.
    noiseSumLeftMeanSquare += leftRms * leftRms;
    noiseSumRightMeanSquare += rightRms * rightRms;
    const float windowCombinedRms = sqrtf(
        0.5f * (leftRms * leftRms + rightRms * rightRms));
    const float windowDb = record.rmsToReferenceDb(windowCombinedRms);
    if (!isfinite(windowDb))
    {
        failNoiseMeasurement("NOISE INVALID");
        return;
    }

    if (noiseWindowCount == 0)
    {
        noiseMinimumWindowDb = windowDb;
        noiseMaximumWindowDb = windowDb;
    }
    else
    {
        if (windowDb < noiseMinimumWindowDb)
            noiseMinimumWindowDb = windowDb;
        if (windowDb > noiseMaximumWindowDb)
            noiseMaximumWindowDb = windowDb;
    }

    ++noiseWindowCount;
    if (noiseWindowCount >= NOISE_AVERAGE_WINDOW_COUNT)
        finishNoiseMeasurement();
}

void DolbyCheck::finishNoiseMeasurement()
{
    if (noiseClipped || noiseWindowCount != NOISE_AVERAGE_WINDOW_COUNT)
    {
        failNoiseMeasurement("NOISE INVALID");
        return;
    }

    const float leftRms = sqrtf(
        noiseSumLeftMeanSquare / noiseWindowCount);
    const float rightRms = sqrtf(
        noiseSumRightMeanSquare / noiseWindowCount);
    const float combinedRms = sqrtf(
        0.5f * (leftRms * leftRms + rightRms * rightRms));
    const float leftDb = record.rmsToReferenceDb(leftRms);
    const float rightDb = record.rmsToReferenceDb(rightRms);
    const float combinedDb = record.rmsToReferenceDb(combinedRms);
    const bool plausible =
        isfinite(leftDb) && isfinite(rightDb) && isfinite(combinedDb) &&
        combinedDb >= NOISE_MINIMUM_PLAUSIBLE_DB &&
        combinedDb <= NOISE_MAXIMUM_PLAUSIBLE_DB &&
        fabsf(leftDb - rightDb) <= NOISE_MAXIMUM_CHANNEL_DIFFERENCE_DB &&
        isfinite(noiseMinimumWindowDb) &&
        isfinite(noiseMaximumWindowDb) &&
        noiseMaximumWindowDb - noiseMinimumWindowDb <=
            NOISE_MAXIMUM_WINDOW_SPREAD_DB;
    if (!plausible)
    {
        failNoiseMeasurement("NOISE UNSTABLE");
        return;
    }

    noiseDb[curveIndex(noiseCurve)] = combinedDb;
    startHfNoiseMeasurement();
}

void DolbyCheck::startHfNoiseMeasurement()
{
    hfNoiseSumLeftPower = 0.0f;
    hfNoiseSumRightPower = 0.0f;
    hfNoiseMinimumCaptureDb = NAN;
    hfNoiseMaximumCaptureDb = NAN;
    hfNoiseCaptureCount = 0;
    hfNoiseClipped = false;
    noiseState = NoiseState::MeasuringHf;

    const char *const label =
        noiseCurve == SweepCurve::Off
            ? "NOISE OFF"
            : (noiseCurve == SweepCurve::B ? "NOISE B" : "NOISE C");
    drawNoiseControl(label, "HF NOISE");
}

void DolbyCheck::updateHfNoiseMeasurement()
{
    bool leftClipped = false;
    if (!audio.calculateFFTChannel(false, leftClipped))
    {
        failNoiseMeasurement("HF FFT ERROR");
        return;
    }
    const float leftRms = fftAnalyzer.integratedRms(
        HF_NOISE_FIRST_BIN,
        HF_NOISE_LAST_BIN);

    bool rightClipped = false;
    if (!audio.calculateFFTChannel(true, rightClipped))
    {
        failNoiseMeasurement("HF FFT ERROR");
        return;
    }
    const float rightRms = fftAnalyzer.integratedRms(
        HF_NOISE_FIRST_BIN,
        HF_NOISE_LAST_BIN);

    hfNoiseClipped =
        hfNoiseClipped || leftClipped || rightClipped;
    if (hfNoiseClipped ||
        !isfinite(leftRms) || !isfinite(rightRms) ||
        leftRms < 0.0f || rightRms < 0.0f)
    {
        failNoiseMeasurement(
            hfNoiseClipped ? "HF NOISE CLIPPED" : "HF NOISE INVALID");
        return;
    }

    const float leftPower = leftRms * leftRms;
    const float rightPower = rightRms * rightRms;
    hfNoiseSumLeftPower += leftPower;
    hfNoiseSumRightPower += rightPower;

    const float captureCombinedRms = sqrtf(
        0.5f * (leftPower + rightPower));
    const float captureDb = record.rmsToReferenceDb(captureCombinedRms);
    if (!isfinite(captureDb))
    {
        failNoiseMeasurement("HF NOISE INVALID");
        return;
    }

    if (hfNoiseCaptureCount == 0)
    {
        hfNoiseMinimumCaptureDb = captureDb;
        hfNoiseMaximumCaptureDb = captureDb;
    }
    else
    {
        if (captureDb < hfNoiseMinimumCaptureDb)
            hfNoiseMinimumCaptureDb = captureDb;
        if (captureDb > hfNoiseMaximumCaptureDb)
            hfNoiseMaximumCaptureDb = captureDb;
    }

    ++hfNoiseCaptureCount;
    if (hfNoiseCaptureCount >= HF_NOISE_CAPTURE_COUNT)
        finishHfNoiseMeasurement();
}

void DolbyCheck::finishHfNoiseMeasurement()
{
    if (hfNoiseClipped || hfNoiseCaptureCount != HF_NOISE_CAPTURE_COUNT)
    {
        failNoiseMeasurement("HF NOISE INVALID");
        return;
    }

    const float averageLeftPower =
        hfNoiseSumLeftPower / hfNoiseCaptureCount;
    const float averageRightPower =
        hfNoiseSumRightPower / hfNoiseCaptureCount;
    const float leftRms = sqrtf(averageLeftPower);
    const float rightRms = sqrtf(averageRightPower);
    const float combinedRms = sqrtf(
        0.5f * (averageLeftPower + averageRightPower));
    const float leftDb = record.rmsToReferenceDb(leftRms);
    const float rightDb = record.rmsToReferenceDb(rightRms);
    const float combinedDb = record.rmsToReferenceDb(combinedRms);
    const bool plausible =
        isfinite(leftDb) && isfinite(rightDb) && isfinite(combinedDb) &&
        combinedDb >= NOISE_MINIMUM_PLAUSIBLE_DB &&
        combinedDb <= NOISE_MAXIMUM_PLAUSIBLE_DB &&
        fabsf(leftDb - rightDb) <= NOISE_MAXIMUM_CHANNEL_DIFFERENCE_DB &&
        isfinite(hfNoiseMinimumCaptureDb) &&
        isfinite(hfNoiseMaximumCaptureDb) &&
        hfNoiseMaximumCaptureDb - hfNoiseMinimumCaptureDb <=
            NOISE_MAXIMUM_WINDOW_SPREAD_DB;
    if (!plausible)
    {
        failNoiseMeasurement("HF NOISE UNSTABLE");
        return;
    }

    hfNoiseDb[curveIndex(noiseCurve)] = combinedDb;
    clearStartButton();
    if (activeDeckMode == DeckMode::TwoHead)
    {
        noiseState = NoiseState::Inactive;
        twoHeadWorkflowState = TwoHeadWorkflowState::WaitingEndSync;
        twoHeadPlaybackStartedMs = millis();
        twoHeadSyncDetections = 0;
        drawStatus("WAIT END SYNC", TFT_CYAN);
        drawTwoHeadControl("VERIFY", "END SYNC");
        return;
    }

    drawTrackingSummary();
    if (noiseCurve == SweepCurve::Off)
    {
        noiseCurve = SweepCurve::B;
        noiseState = NoiseState::Prompt;
        drawNoiseControl("NOISE B", "NEXT");
    }
    else if (noiseCurve == SweepCurve::B)
    {
        noiseCurve = SweepCurve::C;
        noiseState = NoiseState::Prompt;
        drawNoiseControl("NOISE C", "NEXT");
    }
    else
    {
        noiseState = NoiseState::Complete;
        drawSnrSummary();
    }
}

void DolbyCheck::failNoiseMeasurement(const char *status)
{
    if (activeDeckMode == DeckMode::TwoHead)
    {
        failTwoHead(status, true);
        return;
    }

    record.stopCalibratedTestTone();
    noisePrecheckActive = false;
    offSweepState = OffSweepState::Complete;
    noiseState = NoiseState::Error;
    clearStartButton();
    drawStatus(status, TFT_RED);
    drawProgressMessage("TOUCH GRID TO RETRY", TFT_YELLOW);
}

void DolbyCheck::failOffSweep(const char *status)
{
    record.stopCalibratedTestTone();
    offSweepState = OffSweepState::Error;
    if (activeDeckMode == DeckMode::ThreeHead)
        drawThreeHeadSweepProgress(true);
    drawStatus(status, TFT_RED);
    if (activeDeckMode == DeckMode::ThreeHead)
        drawProgressMessage("TOUCH GRID TO RETRY", TFT_YELLOW);
    if (activeCurve == SweepCurve::B &&
        curveData(SweepCurve::Off).captured[0])
        drawStartButton(false, SweepCurve::B);
    else if (activeCurve == SweepCurve::C &&
             curveData(SweepCurve::B).captured[0])
        drawStartButton(false, SweepCurve::C);
}

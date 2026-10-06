#include "Transport.h"

#include "Audio.h"
#include "Config.h"
#include "Display.h"
#include "Help.h"
#include "SetAudio.h"
#include "SignalFrequency.h"
#include "Touch.h"
#include <math.h>
#include <Arduino.h>
#include <TFT_eSPI.h>

extern Display display;
extern TFT_eSPI tft;
extern Touch touch;
extern Audio audio;
extern SetAudio setAudio;

namespace
{
    constexpr uint8_t LABEL_FONT = 1;
    constexpr uint8_t VALUE_FONT = 2;

    constexpr int METRICS_X = CONTENT_LEFT + 5;
    constexpr int METRICS_Y = 34;
    constexpr int METRICS_W = CONTENT_RIGHT - METRICS_X - 4;
    constexpr int METRICS_H = 76;
    constexpr int METRICS_DIVIDER_X = 160;
    constexpr int METRICS_ROW_DIVIDER_Y = 71;

    constexpr int FREQUENCY_LABEL_X = METRICS_X + 7;
    constexpr int FREQUENCY_VALUE_X = 80;
    constexpr int TRANSPORT_LABEL_X = METRICS_DIVIDER_X + 7;
    constexpr int TRANSPORT_VALUE_X = 211;

    constexpr int REFERENCE_Y = 48;
    constexpr int MEASURED_Y = 80;
    constexpr int SPEED_Y = REFERENCE_Y;
    constexpr int ERROR_Y = MEASURED_Y;
    constexpr float REFERENCE_FREQUENCY_HZ = 3150.0f;
    constexpr float MIN_VALID_FREQUENCY_HZ = 3000.0f;
    constexpr float MAX_VALID_FREQUENCY_HZ = 4000.0f;
    constexpr float NOMINAL_SPEED_CM_PER_SECOND = 4.76f;
    constexpr uint32_t SPEED_ERROR_REFRESH_MS = 500;

    constexpr int GRAPH_X = 20;
    constexpr int GRAPH_Y = 145;
    constexpr int GRAPH_W = 280;
    constexpr int GRAPH_H = 55;
    constexpr int HELP_Y = 8;

    constexpr char TRANSPORT_HELP_TITLE[] = "TRANSPORT ANALYSIS";
    const char *const TRANSPORT_HELP_TEXT[] =
        {
            "REFERENCE is the nominal 3150 Hz tone.",
            "",
            "MEASURED is the detected frequency.",
            "",
            "SPEED is calculated from the measured",
            "frequency.",
            "",
            "ERROR is the speed deviation from nominal.",
            "",
            "W&F RMS shows short-term speed variation",
            "after the configured noise correction."};

    constexpr float GRAPH_MIN_PERCENT = -1.0f;
    constexpr float GRAPH_MAX_PERCENT = 1.0f;

    constexpr uint16_t GRAPH_COLOR = TFT_GREEN;
    constexpr uint16_t GRAPH_GRID_COLOR = TFT_DARKGREY;
    constexpr uint16_t GRAPH_BACKGROUND_COLOR = TFT_BLACK;

    constexpr uint8_t DEVIATION_PERIODS_PER_POINT = 16;
    // Timpul reprezentat de un punct al graficului.
    constexpr float DEVIATION_POINT_INTERVAL_SECONDS =
        (float)DEVIATION_PERIODS_PER_POINT /
        REFERENCE_FREQUENCY_HZ;

    // Filtru trece-sus preliminar la aproximativ 0,5 Hz.
    // Elimină eroarea constantă de viteză și variațiile foarte lente.
    constexpr float WF_HIGH_PASS_CUTOFF_HZ = 0.5f;

    constexpr float WF_HIGH_PASS_RC =
        1.0f /
        (2.0f * PI * WF_HIGH_PASS_CUTOFF_HZ);

    constexpr float WF_HIGH_PASS_ALPHA =
        WF_HIGH_PASS_RC /
        (WF_HIGH_PASS_RC +
         DEVIATION_POINT_INTERVAL_SECONDS);

    // Constanta de timp pentru RMS.
    constexpr float WF_RMS_TIME_SECONDS = 2.0f;

    constexpr float WF_RMS_ALPHA =
        DEVIATION_POINT_INTERVAL_SECONDS /
        (WF_RMS_TIME_SECONDS +
         DEVIATION_POINT_INTERVAL_SECONDS);

    constexpr uint32_t WF_DISPLAY_REFRESH_MS = 250;

    void drawMeasurement(
        const char *label,
        const char *value,
        const char *unit,
        int labelX,
        int valueX,
        int y,
        uint16_t valueColor)
    {
        tft.setTextColor(COL_TEXT, COL_BG);
        tft.drawString(label, labelX, y + 3, LABEL_FONT);

        tft.setTextColor(valueColor, COL_BG);
        tft.drawString(value, valueX, y, VALUE_FONT);

        const int unitX = valueX + tft.textWidth(value, VALUE_FONT);
        tft.setTextColor(COL_TEXT, COL_BG);
        tft.drawString(unit, unitX, y, VALUE_FONT);
    }

    int getTransportHelpX()
    {
        return 12 +
               tft.textWidth("TRANSPORT ANALYSIS", 2) + 4;
    }
}

void Transport::run()
{
    drawScreen();

    // Ignore the press that opened this menu.
    while (touch.pressed())
    {
        display.updateClock();
        delay(5);
    }

    while (true)
    {
        captureMeasuredFrequency();

        if (deviationPointCount > 0)
        {
            for (uint16_t i = 0;
                 i < deviationPointCount;
                 i++)
            {
                const float deviation =
                    deviationPoints[i];

                updateWowFlutterRms(deviation);
                drawDeviationPoint(deviation);
            }
        }
        else
        {
            /*
            Nu există ton detectat.
            Continuăm sweep-ul și ștergem treptat urma veche. */
            constexpr uint8_t EMPTY_SWEEP_STEPS = 5;

            for (uint8_t i = 0;
                 i < EMPTY_SWEEP_STEPS;
                 i++)
            {
                advanceEmptyGraphPoint();
            }
        }

        const uint32_t wfNow = millis();

        if (wfNow - lastWfDisplayRefreshMs >=
            WF_DISPLAY_REFRESH_MS)
        {
            drawWowFlutterRms();
            lastWfDisplayRefreshMs = wfNow;
        }

        drawMeasuredFrequency();

        const uint32_t now = millis();
        if (now - lastSpeedErrorRefreshMs >= SPEED_ERROR_REFRESH_MS)
        {
            drawSpeed();
            drawError();
            lastSpeedErrorRefreshMs = now;
        }

        display.updateClock();

        if (touch.pressed())
        {
            const int helpX = getTransportHelpX();
            if (help.hitTest(touch.getX(), touch.getY(), helpX, HELP_Y))
            {
                help.drawButton(helpX, HELP_Y, true);

                while (touch.pressed())
                {
                    display.updateClock();
                    delay(5);
                }

                help.showModal(
                    TRANSPORT_HELP_TITLE,
                    TRANSPORT_HELP_TEXT,
                    sizeof(TRANSPORT_HELP_TEXT) /
                        sizeof(TRANSPORT_HELP_TEXT[0]));
                redrawUi();
                continue;
            }

            while (touch.pressed())
            {
                display.updateClock();
                delay(5);
            }
            break;
        }

        delay(20);
    }
}

void Transport::drawScreen()
{
    displayedMeasuredTenths = -1;
    displayedSpeedHundredths = -1;
    displayedErrorThousandths = -2147483647L;
    measuredFrequencyHz = 0.0f;
    speedCmPerSecond = 0.0f;
    errorPercent = 0.0f;
    lastSpeedErrorRefreshMs = millis() - SPEED_ERROR_REFRESH_MS;

    graphWriteX = 0;
    previousGraphX = 0;
    previousGraphY = 0;
    hasPreviousGraphPoint = false;

    for (uint16_t i = 0; i < GRAPH_HISTORY_SIZE; ++i)
    {
        graphHistoryValid[i] = false;
        graphHistoryBreak[i] = false;
    }

    resetWowFlutterRms();

    redrawUi();
}

void Transport::redrawUi()
{
    display.openApp("TRANSPORT ANALYSIS", "TOUCH TO BACK");
    tft.setTextDatum(TL_DATUM);

    // Grupăm valorile conexe într-un singur panou compact.
    tft.drawRect(
        METRICS_X,
        METRICS_Y,
        METRICS_W,
        METRICS_H,
        COL_FRAME);
    tft.drawFastVLine(
        METRICS_DIVIDER_X,
        METRICS_Y + 1,
        METRICS_H - 2,
        COL_FRAME);
    tft.drawFastHLine(
        METRICS_X + 1,
        METRICS_ROW_DIVIDER_Y,
        METRICS_W - 2,
        COL_FRAME);

    drawMeasurement(
        "REFERENCE",
        "3150.0",
        " Hz",
        FREQUENCY_LABEL_X,
        FREQUENCY_VALUE_X,
        REFERENCE_Y,
        COL_TEXT);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(
        "MEASURED",
        FREQUENCY_LABEL_X,
        MEASURED_Y + 3,
        LABEL_FONT);
    drawMeasuredFrequency(true);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(
        "SPEED",
        TRANSPORT_LABEL_X,
        SPEED_Y + 3,
        LABEL_FONT);
    drawSpeed(true);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(
        "ERROR",
        TRANSPORT_LABEL_X,
        ERROR_Y + 3,
        LABEL_FONT);
    drawError(true);

    drawDeviationGraphFrame();
    drawGraphHistory();
    drawWowFlutterRms(true);
    help.drawButton(getTransportHelpX(), HELP_Y);
}

void Transport::captureMeasuredFrequency()
{
    for (uint16_t i = 0; i < FREQUENCY_SAMPLE_COUNT; i++)
    {
        const uint32_t sampleStart = micros();
        frequencySamples[i] = audio.readLeftCentered();

        while (micros() - sampleStart < SignalFrequency::SAMPLE_PERIOD_US)
            ;
    }

    // Respinge capturile din afara domeniului util înainte ca ele
    // să ajungă în calculele de viteză, eroare sau W&F.
    interpolatedFrequencyHz =
        SignalFrequency::measureInterpolated(
            frequencySamples,
            FREQUENCY_SAMPLE_COUNT);

    measuredFrequencyHz =
        interpolatedFrequencyHz;

    // Validarea se face DUPĂ măsurare.
    if (measuredFrequencyHz < MIN_VALID_FREQUENCY_HZ ||
        measuredFrequencyHz > MAX_VALID_FREQUENCY_HZ)
    {
        measuredFrequencyHz = 0.0f;
        interpolatedFrequencyHz = 0.0f;
        deviationPointCount = 0;
        speedCmPerSecond = 0.0f;
        errorPercent = 0.0f;
        return;
    }

    deviationPointCount =
        SignalFrequency::extractDeviationPoints(
            frequencySamples,
            FREQUENCY_SAMPLE_COUNT,
            REFERENCE_FREQUENCY_HZ,
            deviationPoints,
            MAX_DEVIATION_POINTS,
            DEVIATION_PERIODS_PER_POINT);

    speedCmPerSecond =
        measuredFrequencyHz > 0.0f
            ? (NOMINAL_SPEED_CM_PER_SECOND * measuredFrequencyHz) /
                  REFERENCE_FREQUENCY_HZ
            : 0.0f;
    errorPercent =
        interpolatedFrequencyHz > 0.0f
            ? ((interpolatedFrequencyHz -
                REFERENCE_FREQUENCY_HZ) /
               REFERENCE_FREQUENCY_HZ) *
                  100.0f
            : 0.0f;
}

void Transport::drawMeasuredFrequency(bool force)
{
    const int32_t measuredTenths =
        measuredFrequencyHz > 0.0f
            ? (int32_t)(measuredFrequencyHz * 10.0f + 0.5f)
            : 0;

    if (!force && measuredTenths == displayedMeasuredTenths)
        return;

    char value[12];
    if (measuredTenths > 0)
    {
        snprintf(
            value,
            sizeof(value),
            "%ld.%ld",
            (long)(measuredTenths / 10),
            (long)(measuredTenths % 10));
    }
    else
    {
        snprintf(value, sizeof(value), "---.-");
    }

    const int valueWidth = tft.textWidth("0000.0", VALUE_FONT);
    const int unitX = FREQUENCY_VALUE_X + valueWidth;

    tft.setTextColor(TFT_YELLOW, COL_BG);
    tft.setTextPadding(valueWidth);
    tft.drawString(
        value,
        FREQUENCY_VALUE_X,
        MEASURED_Y,
        VALUE_FONT);
    tft.setTextPadding(0);

    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(" Hz", unitX, MEASURED_Y, VALUE_FONT);

    displayedMeasuredTenths = measuredTenths;
}

void Transport::drawSpeed(bool force)
{
    constexpr float SPEED_MIN_CM_PER_SECOND = 4.75f;
    constexpr float SPEED_MAX_CM_PER_SECOND = 4.77f;
    const int32_t speedHundredths =
        speedCmPerSecond > 0.0f
            ? (int32_t)(speedCmPerSecond * 100.0f + 0.5f)
            : 0;
    const bool speedInRange =
        speedCmPerSecond >= SPEED_MIN_CM_PER_SECOND &&
        speedCmPerSecond <= SPEED_MAX_CM_PER_SECOND;

    if (!force && speedHundredths == displayedSpeedHundredths)
        return;

    char value[12];
    if (speedHundredths > 0)
    {
        snprintf(
            value,
            sizeof(value),
            "%ld,%02ld",
            (long)(speedHundredths / 100),
            (long)(speedHundredths % 100));
    }
    else
    {
        snprintf(value, sizeof(value), "-,--");
    }

    const int valueWidth = tft.textWidth("0,00", VALUE_FONT);
    const int unitX = TRANSPORT_VALUE_X + valueWidth;

    tft.setTextColor(
        speedInRange ? TFT_GREEN : TFT_ORANGE,
        COL_BG);
    tft.setTextPadding(valueWidth);
    tft.drawString(
        value,
        TRANSPORT_VALUE_X,
        SPEED_Y,
        VALUE_FONT);
    tft.setTextPadding(0);

    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(" cm/s", unitX, SPEED_Y, VALUE_FONT);

    displayedSpeedHundredths = speedHundredths;
}

void Transport::drawError(bool force)
{
    constexpr int32_t INVALID_ERROR = 2147483647L;
    const bool valid = measuredFrequencyHz > 0.0f;
    const int32_t errorThousandths =
        valid
            ? (int32_t)(errorPercent * 1000.0f +
                        (errorPercent >= 0.0f ? 0.5f : -0.5f))
            : INVALID_ERROR;

    if (!force && errorThousandths == displayedErrorThousandths)
        return;

    char value[14];
    if (valid)
    {
        const bool negative = errorThousandths < 0;
        const int32_t absoluteError =
            negative ? -errorThousandths : errorThousandths;
        snprintf(
            value,
            sizeof(value),
            "%s%ld.%03ld",
            negative ? "-" : "",
            (long)(absoluteError / 1000),
            (long)(absoluteError % 1000));
    }
    else
    {
        snprintf(value, sizeof(value), "-.---");
    }

    const int valueWidth = tft.textWidth("-00.000", VALUE_FONT);
    const int unitX = TRANSPORT_VALUE_X + valueWidth;

    tft.setTextColor(TFT_YELLOW, COL_BG);
    tft.setTextPadding(valueWidth);
    tft.drawString(
        value,
        TRANSPORT_VALUE_X,
        ERROR_Y,
        VALUE_FONT);
    tft.setTextPadding(0);

    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(" %", unitX, ERROR_Y, VALUE_FONT);

    displayedErrorThousandths = errorThousandths;
}

void Transport::drawDeviationGraphFrame()
{
    // Titlul graficului
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(
        "Instantaneous deviation",
        GRAPH_X - 5,
        GRAPH_Y - 20,
        1);

    // Interiorul graficului
    tft.fillRect(
        GRAPH_X,
        GRAPH_Y,
        GRAPH_W,
        GRAPH_H,
        GRAPH_BACKGROUND_COLOR);

    // Cadrul exterior
    tft.drawRect(
        GRAPH_X - 1,
        GRAPH_Y - 1,
        GRAPH_W + 2,
        GRAPH_H + 2,
        COL_FRAME);

    const int centerY = deviationToY(0.0f);
    const int upperY = deviationToY(0.5f);
    const int lowerY = deviationToY(-0.5f);

    // Linia centrală 0 %
    tft.drawFastHLine(
        GRAPH_X,
        centerY,
        GRAPH_W,
        GRAPH_GRID_COLOR);

    // Liniile punctate la +0,5 % și -0,5 %
    for (int x = GRAPH_X; x < GRAPH_X + GRAPH_W; x += 6)
    {
        tft.drawPixel(x, upperY, GRAPH_GRID_COLOR);
        tft.drawPixel(x, lowerY, GRAPH_GRID_COLOR);
    }

    // Diviziuni verticale
    for (int x = GRAPH_X + 40;
         x < GRAPH_X + GRAPH_W;
         x += 40)
    {
        for (int y = GRAPH_Y; y < GRAPH_Y + GRAPH_H; y += 5)
        {
            tft.drawPixel(x, y, GRAPH_GRID_COLOR);
        }
    }
}

int Transport::deviationToY(float deviationPercent) const
{
    if (deviationPercent > GRAPH_MAX_PERCENT)
        deviationPercent = GRAPH_MAX_PERCENT;

    if (deviationPercent < GRAPH_MIN_PERCENT)
        deviationPercent = GRAPH_MIN_PERCENT;

    const float normalized =
        (GRAPH_MAX_PERCENT - deviationPercent) /
        (GRAPH_MAX_PERCENT - GRAPH_MIN_PERCENT);

    return GRAPH_Y +
           (int)(normalized * (float)(GRAPH_H - 1));
}

void Transport::restoreGraphColumn(int x)
{
    // Ștergem vechea urmă din coloana respectivă
    tft.drawFastVLine(
        x,
        GRAPH_Y,
        GRAPH_H,
        GRAPH_BACKGROUND_COLOR);

    const int centerY = deviationToY(0.0f);
    const int upperY = deviationToY(0.5f);
    const int lowerY = deviationToY(-0.5f);

    // Restaurăm linia centrală
    tft.drawPixel(
        x,
        centerY,
        GRAPH_GRID_COLOR);

    // Restaurăm liniile punctate
    if (((x - GRAPH_X) % 6) == 0)
    {
        tft.drawPixel(
            x,
            upperY,
            GRAPH_GRID_COLOR);

        tft.drawPixel(
            x,
            lowerY,
            GRAPH_GRID_COLOR);
    }

    // Restaurăm diviziunile verticale punctate
    if (((x - GRAPH_X) % 40) == 0 &&
        x != GRAPH_X)
    {
        for (int y = GRAPH_Y;
             y < GRAPH_Y + GRAPH_H;
             y += 5)
        {
            tft.drawPixel(
                x,
                y,
                GRAPH_GRID_COLOR);
        }
    }
}

void Transport::drawDeviationPoint(float deviationPercent)
{
    const int currentX =
        GRAPH_X + graphWriteX;

    const int currentY =
        deviationToY(deviationPercent);
    graphHistory[graphWriteX] = deviationPercent;
    graphHistoryValid[graphWriteX] = true;
    graphHistoryBreak[graphWriteX] = !hasPreviousGraphPoint;

    restoreGraphColumn(currentX);

    if (currentX + 1 < GRAPH_X + GRAPH_W)
    {
        restoreGraphColumn(currentX + 1);
    }

    if (hasPreviousGraphPoint &&
        currentX > previousGraphX)
    {
        tft.drawLine(
            previousGraphX,
            previousGraphY,
            currentX,
            currentY,
            GRAPH_COLOR);
    }
    else
    {
        tft.drawPixel(
            currentX,
            currentY,
            GRAPH_COLOR);
    }

    previousGraphX = currentX;
    previousGraphY = currentY;
    hasPreviousGraphPoint = true;

    graphWriteX++;

    if (graphWriteX >= GRAPH_W)
    {
        graphWriteX = 0;
        hasPreviousGraphPoint = false;
    }

    if (graphHistoryValid[graphWriteX])
        graphHistoryBreak[graphWriteX] = true;
}

void Transport::drawGraphHistory()
{
    int previousX = 0;
    int previousY = 0;
    bool hasPreviousPoint = false;

    for (uint16_t i = 0; i < GRAPH_HISTORY_SIZE; ++i)
    {
        if (!graphHistoryValid[i])
        {
            hasPreviousPoint = false;
            continue;
        }

        const int x = GRAPH_X + i;
        const int y = deviationToY(graphHistory[i]);

        if (hasPreviousPoint && !graphHistoryBreak[i])
            tft.drawLine(previousX, previousY, x, y, GRAPH_COLOR);
        else
            tft.drawPixel(x, y, GRAPH_COLOR);

        previousX = x;
        previousY = y;
        hasPreviousPoint = true;
    }
}

void Transport::resetWowFlutterRms()
{
    correctedWowFlutterRmsPercent = 0.0f;

    wfPreviousInput = 0.0f;
    wfHighPassOutput = 0.0f;
    wfMeanSquare = 0.0f;
    wowFlutterRmsPercent = 0.0f;

    wfFilterInitialized = false;

    displayedWfThousandths = -1;
    lastWfDisplayRefreshMs =
        millis() - WF_DISPLAY_REFRESH_MS;
}

void Transport::updateWowFlutterRms(
    float deviationPercent)
{

    if (deviationPercent < -2.0f ||
        deviationPercent > 2.0f)
    {
        return;
    }

    if (!wfFilterInitialized)
    {
        wfPreviousInput = deviationPercent;
        wfHighPassOutput = 0.0f;
        wfMeanSquare = 0.0f;
        wowFlutterRmsPercent = 0.0f;

        wfFilterInitialized = true;
        return;
    }

    wfHighPassOutput =
        WF_HIGH_PASS_ALPHA *
        (wfHighPassOutput +
         deviationPercent -
         wfPreviousInput);

    wfPreviousInput = deviationPercent;

    const float squared =
        wfHighPassOutput *
        wfHighPassOutput;

    wfMeanSquare +=
        WF_RMS_ALPHA *
        (squared - wfMeanSquare);

    if (wfMeanSquare > 0.0f)
    {
        wowFlutterRmsPercent =
            sqrtf(wfMeanSquare);

        const float wfNoiseFloorPercent =
            setAudio.wfNoiseFloorPercent();
        const float correctedSquared =
            wowFlutterRmsPercent * wowFlutterRmsPercent -
            wfNoiseFloorPercent * wfNoiseFloorPercent;

        correctedWowFlutterRmsPercent =
            correctedSquared > 0.0f
                ? sqrtf(correctedSquared)
                : 0.0f;
    }
    else
    {
        wowFlutterRmsPercent = 0.0f;
    }
}

void Transport::drawWowFlutterRms(bool force)
{
    const int32_t wfThousandths =
        wfFilterInitialized
            ? (int32_t)(correctedWowFlutterRmsPercent *
                            1000.0f +
                        0.5f)
            : -1;

    if (!force &&
        wfThousandths ==
            displayedWfThousandths)
    {
        return;
    }

    constexpr int LABEL_X = 178;
    constexpr int VALUE_X_WF = 248;
    constexpr int DISPLAY_Y = GRAPH_Y - 15;
    constexpr int VALUE_Y = DISPLAY_Y - 10;
    constexpr uint8_t FONT = 2;

    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(
        "W&F RMS:",
        LABEL_X,
        DISPLAY_Y - 10,
        FONT);

    char value[12];

    if (wfThousandths >= 0)
    {
        snprintf(
            value,
            sizeof(value),
            "%ld.%03ld",
            (long)(wfThousandths / 1000),
            (long)(wfThousandths % 1000));
    }
    else
    {
        snprintf(
            value,
            sizeof(value),
            "-.---");
    }

    const int valueWidth =
        tft.textWidth("0.000", VALUE_FONT);

    tft.setTextColor(
        TFT_GREEN,
        COL_BG);

    tft.setTextPadding(valueWidth);

    tft.drawString(
        value,
        VALUE_X_WF,
        VALUE_Y,
        VALUE_FONT);

    tft.setTextPadding(0);

    const int unitX =
        VALUE_X_WF + valueWidth;

    tft.setTextColor(
        COL_TEXT,
        COL_BG);

    tft.drawString(
        "%",
        unitX + 2,
        VALUE_Y,
        VALUE_FONT);

    displayedWfThousandths =
        wfThousandths;
}

void Transport::advanceEmptyGraphPoint()
{
    const int currentX = GRAPH_X + graphWriteX;

    graphHistoryValid[graphWriteX] = false;
    graphHistoryBreak[graphWriteX] = true;

    restoreGraphColumn(currentX);

    if (currentX + 1 < GRAPH_X + GRAPH_W)
    {
        restoreGraphColumn(currentX + 1);
    }

    hasPreviousGraphPoint = false;

    graphWriteX++;

    if (graphWriteX >= GRAPH_W)
    {
        graphWriteX = 0;
    }

    if (graphHistoryValid[graphWriteX])
        graphHistoryBreak[graphWriteX] = true;
}

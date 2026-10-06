#include "Atc.h"

#include "Buzzer.h"
#include "Config.h"
#include "Display.h"
#include "ProgressBar.h"
#include "Record.h"
#include "Touch.h"

#include <TFT_eSPI.h>
#include <math.h>
#include <string.h>

extern Buzzer buzzer;
extern Display display;
extern Record record;
extern TFT_eSPI tft;
extern Touch touch;

Atc atc;

namespace
{
    constexpr uint16_t SYNC_HZ = 2000;
    constexpr float SYNC_LEVEL_DB = -10.0f;
    constexpr uint32_t SYNC_MS = 2000;
    constexpr uint32_t TONE_MS = 4000;
    constexpr uint32_t GAP_MS = 500;
    constexpr uint32_t TONE_SETTLE_MS = 500;
    constexpr uint32_t TONE_MEASURE_MS = 900;
    constexpr uint32_t START_SYNC_TIMEOUT_MS = 60000;
    constexpr uint32_t END_SYNC_TIMEOUT_MS = 10000;
    constexpr uint8_t SYNC_CONFIRMATIONS = 3;
    constexpr uint8_t MIN_TONE_SAMPLES = 3;
    constexpr float FREQUENCY_TOLERANCE = 0.05f;
    constexpr int CONTROL_W = 72;
    constexpr int CONTROL_H = 72;
    constexpr int MESSAGE_H = 34;
    constexpr int MESSAGE_X = CONTENT_LEFT + 10;
    constexpr int CONTROL_X = LCD_WIDTH - FRAME_MARGIN - CONTROL_W - 10;
    constexpr int CONTROL_Y = (LCD_HEIGHT - CONTROL_H) / 2;
    constexpr int MESSAGE_Y = (LCD_HEIGHT - MESSAGE_H) / 2;
    constexpr int TEXT_X = CONTENT_LEFT + 13;
    constexpr uint8_t CONTROL_TOP_FONT = 1;
    constexpr uint8_t FONT = 2;
    constexpr uint8_t RESULT_FONT = 4;
    constexpr uint16_t ATC_TOUCH_BEEP_MS = 8;

    bool frequencyMatches(float frequency, uint16_t expectedHz)
    {
        const float tolerance = expectedHz * FREQUENCY_TOLERANCE;
        return isfinite(frequency) && frequency >= expectedHz - tolerance &&
               frequency <= expectedHz + tolerance;
    }

    void formatDb(char *buffer, size_t size, float value)
    {
        if (!isfinite(value))
        {
            snprintf(buffer, size, "---.- dB");
            return;
        }
        const bool negative = value < 0.0f;
        const int tenths = static_cast<int>(fabsf(value) * 10.0f + 0.5f);
        snprintf(
            buffer,
            size,
            "%s%d.%d dB",
            negative ? "-" : "+",
            tenths / 10,
            tenths % 10);
    }

    void formatNominalGeneratorLevel(
        char *buffer,
        size_t size,
        float levelDb)
    {
        const bool negative = levelDb < 0.0f;
        const int tenths = static_cast<int>(fabsf(levelDb) * 10.0f + 0.5f);
        snprintf(
            buffer,
            size,
            "GEN %s%d.%d dB NOMINAL",
            negative ? "-" : "",
            tenths / 10,
            tenths % 10);
    }
}

void Atc::run()
{
    stage = Stage::Bias;
    resetStage();
    drawScreen();

    while (touch.isDown())
        delay(5);

    while (true)
    {
        const uint32_t now = millis();
        drawPhaseProgress(now);
        if (state == State::Recording)
            updateRecording(now);
        else if (state == State::WaitingStartSync ||
                 state == State::MeasuringFirst ||
                 state == State::MeasuringGap ||
                 state == State::MeasuringSecond ||
                 state == State::WaitingEndSync)
            updatePlayback(now);
        buzzer.update();
        display.updateClock();
        // ATC provides its own feedback per action, after release.
        if (!touch.pressed(false))
        {
            delay(5);
            continue;
        }

        const uint16_t x = touch.getX();
        const uint16_t y = touch.getY();
        if (y >= FOOTER_Y)
        {
            record.stopCalibratedTestTone();
            while (touch.isDown())
                delay(5);
            buzzer.beepNow(ATC_TOUCH_BEEP_MS);
            return;
        }

        if (!controlHit(x, y))
        {
            while (touch.isDown())
                delay(5);
            buzzer.beepNow(ATC_TOUCH_BEEP_MS);
            delay(5);
            continue;
        }

        if (stage == Stage::Complete)
        {
            drawCompleteControl(true);
            while (touch.isDown())
                delay(5);
            buzzer.play(BeepPattern::Success);
            record.stopCalibratedTestTone();
            return;
        }
        else if (state == State::WaitingNext)
        {
            drawControl("NEXT", "GO", true);
            while (touch.isDown())
                delay(5);
            buzzer.beepNow(ATC_TOUCH_BEEP_MS);
            if (stage == Stage::Level)
            {
                // Only advance to Complete if all 3 stages are done
                if (completedStages == 0x07) // 0b111 = Bias + EQ + Level
                {
                    stage = Stage::Complete;
                }
                else
                {
                    // Stay on Level stage, show message that other stages needed
                    // The result screen will show again with current results
                    drawResultScreen();
                    return;
                }
            }
            else
            {
                stage = stage == Stage::Bias ? Stage::Eq : Stage::Level;
            }
            resetStage();
            drawScreen();
        }
        else if (state == State::ReadyToRecord || state == State::Result)
        {
            drawControl("START", state == State::Result ? "REPEAT" : "RECORD", true);
            while (touch.isDown())
                delay(5);
            buzzer.beepNow(ATC_TOUCH_BEEP_MS);
            startRecording();
        }
        else if (state == State::Rewind)
        {
            drawControl("REWIND", "DONE", true);
            while (touch.isDown())
                delay(5);
            buzzer.beepNow(ATC_TOUCH_BEEP_MS);
            state = State::ReadyToPlay;
            drawCycleScreen();
        }
        else if (state == State::ReadyToPlay)
        {
            drawControl("START", "PLAY", true);
            while (touch.isDown())
                delay(5);
            buzzer.beepNow(ATC_TOUCH_BEEP_MS);
            startPlayback();
        }
    }
}

void Atc::resetStage()
{
    state = stage == Stage::Complete ? State::Result : State::ReadyToRecord;
    segmentStartedMs = 0;
    playbackStartedMs = 0;
    syncDetections = 0;
    syncExitDetections = 0;
    syncFound = false;
    recordPhase = 0;
    firstLeftRmsSum = firstRightRmsSum = 0.0f;
    secondLeftRmsSum = secondRightRmsSum = 0.0f;
    firstLeftSampleCount = firstRightSampleCount = 0;
    secondLeftSampleCount = secondRightSampleCount = 0;
    clipped = false;
    firstLeftDb = firstRightDb = NAN;
    secondLeftDb = secondRightDb = NAN;
    leftErrorDb = rightErrorDb = NAN;
    resultOk = false;
    errorMessage = nullptr;
    recommendation = nullptr;
    // Clear completed flag for current stage when restarting
    completedStages &= ~(1 << static_cast<uint8_t>(stage));
}

const char *Atc::stageName() const
{
    return stage == Stage::Bias ? "BIAS" : stage == Stage::Eq ? "EQ"
                                                              : "LEVEL";
}

uint16_t Atc::firstFrequency() const
{
    return stage == Stage::Bias ? ATC_BIAS_FREQ_LOW
           : stage == Stage::Eq ? ATC_EQ_FREQ_LOW
                                : ATC_LEVEL_FREQ;
}

uint16_t Atc::secondFrequency() const
{
    return stage == Stage::Bias ? ATC_BIAS_FREQ_HIGH : ATC_EQ_FREQ_HIGH;
}

bool Atc::stageHasSecondTone() const
{
    return stage != Stage::Level;
}

float Atc::stageLevelDb() const
{
    return stage == Stage::Bias ? ATC_BIAS_LEVEL_DB
           : stage == Stage::Eq ? ATC_EQ_LEVEL_DB
                                : ATC_LEVEL_DB;
}

float Atc::stageToleranceDb() const
{
    return stage == Stage::Bias ? ATC_BIAS_TOLERANCE_DB
           : stage == Stage::Eq ? ATC_EQ_TOLERANCE_DB
                                : ATC_LEVEL_TOLERANCE_DB;
}

void Atc::drawScreen()
{
    if (stage == Stage::Complete)
    {
        drawCompleteScreen();
        return;
    }
    display.openApp("AUTO TAPE CALIBRATION", "TOUCH TO BACK");
    drawCycleScreen();
}

void Atc::drawControl(const char *top, const char *bottom, bool pressed)
{
    const uint16_t color = pressed ? TFT_WHITE : TFT_CYAN;
    const uint16_t fill = pressed ? TFT_DARKCYAN : COL_BG;
    tft.fillRect(CONTROL_X, CONTROL_Y, CONTROL_W, CONTROL_H, fill);
    tft.drawRect(CONTROL_X, CONTROL_Y, CONTROL_W, CONTROL_H, color);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(color, fill);
    const int textTopY = CONTROL_Y + (CONTROL_H - 34) / 2;
    tft.drawString(top, CONTROL_X + CONTROL_W / 2, textTopY, FONT);
    tft.drawString(bottom, CONTROL_X + CONTROL_W / 2, textTopY + 18, FONT);
    tft.setTextDatum(TL_DATUM);
}

void Atc::drawCompleteControl(bool pressed)
{
    const uint16_t color = pressed ? TFT_WHITE : TFT_GREEN;
    const uint16_t fill = pressed ? TFT_DARKGREEN : COL_BG;
    tft.fillRect(CONTROL_X, CONTROL_Y, CONTROL_W, CONTROL_H, fill);
    tft.drawRect(CONTROL_X, CONTROL_Y, CONTROL_W, CONTROL_H, color);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(color, fill);
    const int textTopY = CONTROL_Y + (CONTROL_H - 34) / 2;
    tft.drawString("ATC", CONTROL_X + CONTROL_W / 2, textTopY, FONT);
    tft.drawString("COMPLETE", CONTROL_X + CONTROL_W / 2, textTopY + 18, FONT);
    tft.setTextDatum(TL_DATUM);
}

void Atc::drawMessageBox(const char *top, const char *bottom, uint16_t color)
{
    tft.fillRect(MESSAGE_X, MESSAGE_Y, CONTROL_W, MESSAGE_H, COL_BG);
    tft.drawRect(MESSAGE_X, MESSAGE_Y, CONTROL_W, MESSAGE_H, color);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(color, COL_BG);
    tft.drawString(top, MESSAGE_X + 6, MESSAGE_Y + 3, CONTROL_TOP_FONT);
    tft.drawString(bottom, MESSAGE_X + 6, MESSAGE_Y + 14, FONT);
    tft.setTextDatum(TL_DATUM);
}

void Atc::drawBottomMessage(const char *top, const char *bottom, uint16_t color)
{
    constexpr int MESSAGE_TOP = FOOTER_Y - 34;
    constexpr int MESSAGE_HEIGHT = 31;
    tft.fillRect(CONTENT_LEFT, MESSAGE_TOP, CONTENT_WIDTH, MESSAGE_HEIGHT, COL_BG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(color, COL_BG);
    tft.drawString(top, LCD_WIDTH / 2, FOOTER_Y - 25, CONTROL_TOP_FONT);
    tft.drawString(bottom, LCD_WIDTH / 2, FOOTER_Y - 11, FONT);
    tft.setTextDatum(TL_DATUM);
}

void Atc::drawBottomSingleLine(const char *text, uint16_t color, uint8_t font)
{
    const int messageTop = font == RESULT_FONT ? FOOTER_Y - 34 : FOOTER_Y - 27;
    const int messageHeight = font == RESULT_FONT ? 31 : 24;
    const int messageY = font == RESULT_FONT ? FOOTER_Y - 18 : FOOTER_Y - 14;
    tft.fillRect(CONTENT_LEFT, messageTop, CONTENT_WIDTH, messageHeight, COL_BG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(color, COL_BG);
    tft.drawString(text, LCD_WIDTH / 2, messageY, font);
    tft.setTextDatum(TL_DATUM);
}

bool Atc::controlHit(uint16_t x, uint16_t y) const
{
    return x >= CONTROL_X && x < CONTROL_X + CONTROL_W &&
           y >= CONTROL_Y && y < CONTROL_Y + CONTROL_H;
}

void Atc::drawCycleScreen()
{
    progressBarPhaseActive = false;
    progressBarValue = 0xFF;
    progressBarElapsedSecond = UINT32_MAX;
    progressBarOperationKey = UINT16_MAX;
    tft.fillRect(
        CONTENT_LEFT,
        CONTENT_TOP + 1,
        CONTENT_WIDTH,
        CONTENT_HEIGHT - 3,
        COL_BG);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_TEXT, COL_BG);
    char line[40];
    snprintf(line, sizeof(line), "%s  (%s)", stageName(),
             stage == Stage::Bias ? "1/3" : stage == Stage::Eq ? "2/3"
                                                               : "3/3");
    tft.drawString(line, TEXT_X, 34, FONT);

    if (state == State::ReadyToRecord || state == State::Recording)
    {
        snprintf(line, sizeof(line), "REC: SYNC, %u Hz%s", firstFrequency(),
                 stageHasSecondTone() ? ", second tone" : "");
        tft.drawString(line, TEXT_X, 60, FONT);
        formatNominalGeneratorLevel(line, sizeof(line), stageLevelDb());
        tft.drawString(line, TEXT_X, 82, FONT);
        if (state == State::Recording)
        {
            tft.setTextColor(TFT_CYAN, COL_BG);
            tft.drawString("RECORDING", TEXT_X, 110, FONT);
            const char *segment = "SYNC";
            if (state == State::Recording)
            {
                // The generator state is expressed by the current control label.
                segment = "SEQUENCE RUNNING";
            }
            tft.drawString(segment, TEXT_X, 132, FONT);
            drawControl("RECORD", "ACTIVE");
        }
        else
            drawControl("START", "RECORD");
        return;
    }

    if (state == State::Rewind)
    {
        tft.setTextColor(TFT_YELLOW, COL_BG);
        tft.drawString("RECORD COMPLETE", TEXT_X, 62, FONT);
        tft.drawString("REWIND TAPE", TEXT_X, 88, FONT);
        tft.drawString("Then touch PLAY", TEXT_X, 114, FONT);
        drawControl("REWIND", "DONE");
        return;
    }

    if (state == State::ReadyToPlay)
    {
        tft.drawString("START TAPE PLAYBACK", TEXT_X, 62, FONT);
        drawControl("START", "PLAY");
        return;
    }

    tft.setTextColor(TFT_CYAN, COL_BG);
    tft.drawString("PLAY / MEASURE", TEXT_X, 62, FONT);
    const char *message = state == State::WaitingStartSync  ? "WAITING FOR SYNC"
                          : state == State::MeasuringFirst  ? "MEASURING FIRST TONE"
                          : state == State::MeasuringGap    ? "WAIT"
                          : state == State::MeasuringSecond ? "MEASURING SECOND TONE"
                                                            : "VERIFYING END TONE";
    tft.drawString(message, TEXT_X, 88, FONT);
    drawControl("MEASURE", "WAIT");
}

void Atc::drawResultScreen()
{
    tft.fillRect(
        CONTENT_LEFT,
        CONTENT_TOP + 1,
        CONTENT_WIDTH,
        CONTENT_HEIGHT - 3,
        COL_BG);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(stageName(), TEXT_X, 32, FONT);
    char value[18];
    char label[16];
    if (stage == Stage::Level)
    {
        tft.drawString("L INPUT", TEXT_X, 48, FONT);
        tft.drawString("0.0 dB", 132, 48, FONT);
        tft.drawString("L PLAY", TEXT_X, 66, FONT);
        formatDb(value, sizeof(value), firstLeftDb);
        tft.drawString(value, 132, 66, FONT);
        tft.drawString("L ERROR", TEXT_X, 84, FONT);
        formatDb(value, sizeof(value), leftErrorDb);
        tft.drawString(value, 132, 84, FONT);
        tft.drawString("R INPUT", TEXT_X, 106, FONT);
        tft.drawString("0.0 dB", 132, 106, FONT);
        tft.drawString("R PLAY", TEXT_X, 124, FONT);
        formatDb(value, sizeof(value), firstRightDb);
        tft.drawString(value, 132, 124, FONT);
        tft.drawString("R ERROR", TEXT_X, 142, FONT);
        formatDb(value, sizeof(value), rightErrorDb);
        tft.drawString(value, 132, 142, FONT);
    }
    else
    {
        snprintf(label, sizeof(label), "L %u Hz", firstFrequency());
        tft.drawString(label, TEXT_X, 48, FONT);
        formatDb(value, sizeof(value), firstLeftDb);
        tft.drawString(value, 132, 48, FONT);
        snprintf(label, sizeof(label), "L %u Hz", secondFrequency());
        tft.drawString(label, TEXT_X, 66, FONT);
        formatDb(value, sizeof(value), secondLeftDb);
        tft.drawString(value, 132, 66, FONT);
        tft.drawString("L DIFF", TEXT_X, 84, FONT);
        formatDb(value, sizeof(value), leftErrorDb);
        tft.drawString(value, 132, 84, FONT);
        snprintf(label, sizeof(label), "R %u Hz", firstFrequency());
        tft.drawString(label, TEXT_X, 106, FONT);
        formatDb(value, sizeof(value), firstRightDb);
        tft.drawString(value, 132, 106, FONT);
        snprintf(label, sizeof(label), "R %u Hz", secondFrequency());
        tft.drawString(label, TEXT_X, 124, FONT);
        formatDb(value, sizeof(value), secondRightDb);
        tft.drawString(value, 132, 124, FONT);
        tft.drawString("R DIFF", TEXT_X, 142, FONT);
        formatDb(value, sizeof(value), rightErrorDb);
        tft.drawString(value, 132, 142, FONT);
    }

    if (errorMessage != nullptr)
    {
        const char *errorTop = "TEST";
        const char *errorBottom = "ERROR";
        if (strcmp(errorMessage, "GENERATOR ERROR") == 0)
        {
            errorTop = "GENERATOR";
        }
        else if (strcmp(errorMessage, "PLAYBACK CLIPPED") == 0)
        {
            errorTop = "PLAYBACK";
            errorBottom = "CLIPPED";
        }
        else if (strcmp(errorMessage, "NO SYNC / SIGNAL") == 0)
        {
            errorTop = "NO SYNC";
            errorBottom = "SIGNAL";
        }
        else if (strcmp(errorMessage, "SIGNAL INVALID") == 0)
        {
            errorTop = "SIGNAL";
            errorBottom = "INVALID";
        }
        else if (strcmp(errorMessage, "MEASURE ERROR") == 0)
        {
            errorTop = "MEASURE";
        }
        drawBottomMessage(errorTop, errorBottom, TFT_RED);
        drawControl("START", "REPEAT");
        return;
    }

    char resultMessage[24];
    if (resultOk)
    {
        if (stage == Stage::Level && completedStages != 0x07)
        {
            // Level passed but other stages not done
            snprintf(resultMessage, sizeof(resultMessage), "%s OK - COMPLETE OTHERS", stageName());
            drawBottomSingleLine(resultMessage, TFT_YELLOW, FONT);
            drawControl("NEXT", "GO");
        }
        else
        {
            snprintf(resultMessage, sizeof(resultMessage), "%s OK", stageName());
            drawBottomSingleLine(resultMessage, TFT_GREEN, RESULT_FONT);
            drawControl("NEXT", "GO");
        }
    }
    else if (recommendation != nullptr)
    {
        snprintf(resultMessage, sizeof(resultMessage), "%s %s",
                 recommendation, stageName());
        drawBottomSingleLine(resultMessage, TFT_YELLOW, FONT);
        drawControl("START", "REPEAT");
    }
    else
    {
        snprintf(resultMessage, sizeof(resultMessage), "%s ADJUST", stageName());
        drawBottomSingleLine(resultMessage, TFT_YELLOW, FONT);
        drawControl("START", "REPEAT");
    }
}

void Atc::drawCompleteScreen()
{
    display.openApp("ATC", "TOUCH TO BACK");
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("BIAS", TEXT_X, 48, RESULT_FONT);
    tft.drawString("EQ", TEXT_X, 82, RESULT_FONT);
    tft.drawString("LEVEL", TEXT_X, 116, RESULT_FONT);
    tft.setTextColor(TFT_GREEN, COL_BG);
    tft.drawString((completedStages & 0x01) ? "OK" : "--", 150, 48, RESULT_FONT);
    tft.drawString((completedStages & 0x02) ? "OK" : "--", 150, 82, RESULT_FONT);
    tft.drawString((completedStages & 0x04) ? "OK" : "--", 150, 116, RESULT_FONT);
    drawCompleteControl();
}

uint32_t Atc::activeCounterDurationMs() const
{
    switch (state)
    {
    case State::Recording:
        if (!syncFound)
            return SYNC_MS;
        if (recordPhase == 0 || recordPhase == 2)
            return TONE_MS;
        if (recordPhase == 1)
            return GAP_MS;
        return SYNC_MS;
    case State::WaitingStartSync:
        return START_SYNC_TIMEOUT_MS;
    case State::MeasuringFirst:
        return TONE_MS;
    case State::MeasuringGap:
        return GAP_MS;
    case State::MeasuringSecond:
        return TONE_MS;
    case State::WaitingEndSync:
        return END_SYNC_TIMEOUT_MS;
    default:
        return 0;
    }
}

void Atc::drawPhaseProgress(uint32_t now)
{
    const uint32_t durationMs = activeCounterDurationMs();
    if (durationMs == 0)
    {
        progressBarPhaseActive = false;
        progressBarValue = 0xFF;
        return;
    }

    const bool syncWait = state == State::WaitingStartSync ||
                          state == State::WaitingEndSync;
    const uint32_t anchorMs = syncWait ? playbackStartedMs : segmentStartedMs;
    if (!progressBarPhaseActive || progressBarAnchorMs != anchorMs ||
        progressBarDurationMs != durationMs)
    {
        progressBarAnchorMs = anchorMs;
        progressBarDurationMs = durationMs;
        progressBarValue = 0xFF;
        progressBarElapsedSecond = UINT32_MAX;
        progressBarOperationKey = UINT16_MAX;
    }

    const uint32_t elapsedMs = now - anchorMs;
    const uint8_t value = elapsedMs >= durationMs
                              ? PROGRESS_BAR_BRICK_COUNT
                              : static_cast<uint8_t>(
                                    (elapsedMs * PROGRESS_BAR_BRICK_COUNT) / durationMs);
    const uint32_t totalSeconds = (durationMs + 999) / 1000;
    const uint32_t elapsedSeconds = elapsedMs >= durationMs
                                        ? totalSeconds
                                        : min(elapsedMs / 1000, totalSeconds);
    char progressLabel[16];
    char maxWidthLabel[16];
    snprintf(progressLabel, sizeof(progressLabel), "%lu/%lu",
             static_cast<unsigned long>(elapsedSeconds),
             static_cast<unsigned long>(totalSeconds));
    snprintf(maxWidthLabel, sizeof(maxWidthLabel), "%lu/%lu",
             static_cast<unsigned long>(totalSeconds),
             static_cast<unsigned long>(totalSeconds));
    constexpr int LABEL_GAP = 8;
    const int labelWidth = tft.textWidth(maxWidthLabel, FONT);
    constexpr int BAR_RIGHT = LCD_WIDTH - 1 - 10;
    const int barX = BAR_RIGHT - PROGRESS_BAR_WIDTH + 1;
    const int labelX = barX - LABEL_GAP - labelWidth;
    constexpr int BAR_Y = FOOTER_Y - PROGRESS_BAR_BRICK_HEIGHT - 10;

    if (progressBarElapsedSecond != elapsedSeconds)
    {
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_CYAN, COL_BG);
        tft.setTextPadding(labelWidth);
        tft.drawString(progressLabel, labelX, BAR_Y - 3, FONT);
        tft.setTextPadding(0);
        progressBarElapsedSecond = elapsedSeconds;
    }

    char operation[16] = "WAIT";
    uint16_t operationKey =
        (static_cast<uint16_t>(state) << 8) |
        (syncFound ? 0x80 : 0) | recordPhase;
    const auto formatToneOperation = [&operation](uint16_t frequencyHz)
    {
        if (frequencyHz >= 1000 && frequencyHz % 1000 == 0)
        {
            snprintf(operation, sizeof(operation), "TONE %uKHz",
                     static_cast<unsigned>(frequencyHz / 1000));
        }
        else
        {
            snprintf(operation, sizeof(operation), "TONE %uHz",
                     static_cast<unsigned>(frequencyHz));
        }
    };

    if (state == State::Recording)
    {
        if (!syncFound || recordPhase == 3)
            snprintf(operation, sizeof(operation), "TONE SYNC");
        else if (recordPhase == 0)
            formatToneOperation(firstFrequency());
        else if (recordPhase == 1)
            snprintf(operation, sizeof(operation), "WAIT");
        else
            formatToneOperation(secondFrequency());
    }
    else if (state == State::WaitingStartSync ||
             state == State::WaitingEndSync)
    {
        snprintf(operation, sizeof(operation), "WAIT SYNC");
    }
    else if (state == State::MeasuringFirst)
    {
        formatToneOperation(firstFrequency());
    }
    else if (state == State::MeasuringGap)
    {
        snprintf(operation, sizeof(operation), "WAIT");
    }
    else if (state == State::MeasuringSecond)
    {
        formatToneOperation(secondFrequency());
    }

    if (progressBarOperationKey != operationKey)
    {
        constexpr int OPERATION_X = 10;
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_YELLOW, COL_BG);
        tft.setTextPadding(labelX - OPERATION_X - LABEL_GAP);
        tft.drawString(operation, OPERATION_X, BAR_Y - 3, FONT);
        tft.setTextPadding(0);
        progressBarOperationKey = operationKey;
    }

    drawCalibrationProgressBar(tft, barX, BAR_Y, value, progressBarValue);
    progressBarValue = value;
    progressBarPhaseActive = true;
}

void Atc::startRecording()
{
    resetStage();
    if (!record.setCalibratedTestTone(SYNC_HZ, SYNC_LEVEL_DB))
    {
        fail("GENERATOR ERROR");
        return;
    }
    segmentStartedMs = millis();
    state = State::Recording;
    drawCycleScreen();
}

void Atc::updateRecording(uint32_t now)
{
    const uint32_t elapsed = now - segmentStartedMs;
    if (syncFound == false)
    {
        if (elapsed < SYNC_MS)
            return;
        syncFound = true; // Recording phase marker: first test tone follows SYNC.
        if (!record.setCalibratedTestTone(firstFrequency(), stageLevelDb()))
        {
            fail("GENERATOR ERROR");
            return;
        }
        segmentStartedMs = now;
        drawCycleScreen();
        return;
    }

    // During recording syncFound is a phase marker, not a playback detection.
    if (recordPhase == 0)
    {
        if (elapsed < TONE_MS)
            return;
        recordPhase = 1;
        if (!stageHasSecondTone())
        {
            if (!record.setCalibratedTestTone(SYNC_HZ, SYNC_LEVEL_DB))
            {
                fail("GENERATOR ERROR");
                return;
            }
            segmentStartedMs = now;
            recordPhase = 3;
            return;
        }
        record.stopCalibratedTestTone();
        segmentStartedMs = now;
        return;
    }

    if (recordPhase == 1 && stageHasSecondTone())
    {
        if (elapsed < GAP_MS)
            return;
        if (!record.setCalibratedTestTone(secondFrequency(), stageLevelDb()))
        {
            fail("GENERATOR ERROR");
            return;
        }
        recordPhase = 2;
        segmentStartedMs = now;
        return;
    }

    if (recordPhase == 2 && stageHasSecondTone())
    {
        if (elapsed < TONE_MS)
            return;
        if (!record.setCalibratedTestTone(SYNC_HZ, SYNC_LEVEL_DB))
        {
            fail("GENERATOR ERROR");
            return;
        }
        recordPhase = 3;
        segmentStartedMs = now;
        return;
    }

    if (elapsed < SYNC_MS)
        return;
    record.stopCalibratedTestTone();
    state = State::Rewind;
    buzzer.play(BeepPattern::Success);
    drawCycleScreen();
}

void Atc::startPlayback()
{
    record.stopCalibratedTestTone();
    syncDetections = 0;
    syncExitDetections = 0;
    syncFound = false;
    firstLeftRmsSum = firstRightRmsSum = 0.0f;
    secondLeftRmsSum = secondRightRmsSum = 0.0f;
    firstLeftSampleCount = firstRightSampleCount = 0;
    secondLeftSampleCount = secondRightSampleCount = 0;
    clipped = false;
    playbackStartedMs = millis();
    state = State::WaitingStartSync;
    drawCycleScreen();
}

bool Atc::measureSync()
{
    float leftRms, rightRms, leftHz, rightHz;
    bool leftValid, rightValid, windowClipped;
    record.measureTapeResponseToneWindow(leftRms, rightRms, leftHz, rightHz,
                                         leftValid, rightValid, windowClipped);
    clipped = clipped || windowClipped;
    return !windowClipped && leftValid && rightValid &&
           (frequencyMatches(leftHz, SYNC_HZ) || frequencyMatches(rightHz, SYNC_HZ));
}

void Atc::measureTone(uint16_t expectedHz, bool firstTone)
{
    float leftRms, rightRms, leftHz, rightHz;
    bool leftValid, rightValid, windowClipped;
    record.measureTapeResponseToneWindow(leftRms, rightRms, leftHz, rightHz,
                                         leftValid, rightValid, windowClipped);
    clipped = clipped || windowClipped;
    if (windowClipped || (!leftValid && !rightValid) ||
        (!frequencyMatches(leftHz, expectedHz) && !frequencyMatches(rightHz, expectedHz)))
        return;
    if (firstTone)
    {
        if (leftValid)
        {
            firstLeftRmsSum += leftRms;
            ++firstLeftSampleCount;
        }
        if (rightValid)
        {
            firstRightRmsSum += rightRms;
            ++firstRightSampleCount;
        }
    }
    else
    {
        if (leftValid)
        {
            secondLeftRmsSum += leftRms;
            ++secondLeftSampleCount;
        }
        if (rightValid)
        {
            secondRightRmsSum += rightRms;
            ++secondRightSampleCount;
        }
    }
}

void Atc::updatePlayback(uint32_t now)
{
    if (state == State::WaitingStartSync || state == State::WaitingEndSync)
    {
        const bool isSync = measureSync();
        if (clipped)
        {
            fail("PLAYBACK CLIPPED");
            return;
        }
        if (!syncFound)
        {
            syncDetections = isSync ? min<uint8_t>(SYNC_CONFIRMATIONS, syncDetections + 1) : 0;
            if (syncDetections >= SYNC_CONFIRMATIONS)
                syncFound = true;
        }
        else if (!isSync)
        {
            ++syncExitDetections;
        }
        else
        {
            syncExitDetections = 0;
        }

        if (state == State::WaitingStartSync && syncFound &&
            syncExitDetections >= SYNC_CONFIRMATIONS)
        {
            state = State::MeasuringFirst;
            segmentStartedMs = now;
            firstLeftRmsSum = firstRightRmsSum = 0.0f;
            firstLeftSampleCount = firstRightSampleCount = 0;
            drawCycleScreen();
            return;
        }
        if (state == State::WaitingEndSync && syncFound)
        {
            finishMeasurement();
            return;
        }
        const uint32_t timeout = state == State::WaitingStartSync
                                     ? START_SYNC_TIMEOUT_MS
                                     : END_SYNC_TIMEOUT_MS;
        if (now - playbackStartedMs >= timeout)
            fail("NO SYNC / SIGNAL");
        return;
    }

    const uint32_t elapsed = now - segmentStartedMs;
    if (state == State::MeasuringFirst)
    {
        if (elapsed >= TONE_SETTLE_MS && elapsed < TONE_SETTLE_MS + TONE_MEASURE_MS)
            measureTone(firstFrequency(), true);
        if (elapsed < TONE_MS)
            return;
        if (stageHasSecondTone())
        {
            state = State::MeasuringGap;
            segmentStartedMs = now;
            drawCycleScreen();
        }
        else
        {
            state = State::WaitingEndSync;
            syncDetections = syncExitDetections = 0;
            syncFound = false;
            playbackStartedMs = now;
            drawCycleScreen();
        }
        return;
    }

    if (state == State::MeasuringGap)
    {
        if (elapsed < GAP_MS)
            return;
        state = State::MeasuringSecond;
        segmentStartedMs = now;
        secondLeftRmsSum = secondRightRmsSum = 0.0f;
        secondLeftSampleCount = secondRightSampleCount = 0;
        drawCycleScreen();
        return;
    }

    if (elapsed >= TONE_SETTLE_MS && elapsed < TONE_SETTLE_MS + TONE_MEASURE_MS)
        measureTone(secondFrequency(), false);
    if (elapsed < TONE_MS)
        return;
    state = State::WaitingEndSync;
    syncDetections = syncExitDetections = 0;
    syncFound = false;
    playbackStartedMs = now;
    drawCycleScreen();
}

void Atc::finishMeasurement()
{
    const bool leftMeasured = firstLeftSampleCount >= MIN_TONE_SAMPLES &&
                              (!stageHasSecondTone() || secondLeftSampleCount >= MIN_TONE_SAMPLES);
    const bool rightMeasured = firstRightSampleCount >= MIN_TONE_SAMPLES &&
                               (!stageHasSecondTone() || secondRightSampleCount >= MIN_TONE_SAMPLES);
    if (clipped || (!leftMeasured && !rightMeasured))
    {
        fail("SIGNAL INVALID");
        return;
    }
    firstLeftDb = leftMeasured
                      ? record.rmsToReferenceDb(firstLeftRmsSum / firstLeftSampleCount)
                      : NAN;
    firstRightDb = rightMeasured
                       ? record.rmsToReferenceDb(firstRightRmsSum / firstRightSampleCount)
                       : NAN;
    secondLeftDb = stageHasSecondTone() && leftMeasured
                       ? record.rmsToReferenceDb(secondLeftRmsSum / secondLeftSampleCount)
                       : NAN;
    secondRightDb = stageHasSecondTone() && rightMeasured
                        ? record.rmsToReferenceDb(secondRightRmsSum / secondRightSampleCount)
                        : NAN;
    leftErrorDb = stageHasSecondTone() ? secondLeftDb - firstLeftDb : firstLeftDb;
    rightErrorDb = stageHasSecondTone() ? secondRightDb - firstRightDb : firstRightDb;
    if ((!leftMeasured && !rightMeasured) ||
        (leftMeasured && !isfinite(leftErrorDb)) ||
        (rightMeasured && !isfinite(rightErrorDb)))
    {
        fail("MEASURE ERROR");
        return;
    }
    const bool leftOk = leftMeasured && fabsf(leftErrorDb) <= stageToleranceDb();
    const bool rightOk = rightMeasured && fabsf(rightErrorDb) <= stageToleranceDb();
    resultOk = leftOk || rightOk;
    if (!resultOk && leftMeasured && rightMeasured)
    {
        if (leftErrorDb < -stageToleranceDb() && rightErrorDb < -stageToleranceDb())
            recommendation = stage == Stage::Bias ? "DECREASE" : "INCREASE";
        else if (leftErrorDb > stageToleranceDb() && rightErrorDb > stageToleranceDb())
            recommendation = stage == Stage::Bias ? "INCREASE" : "DECREASE";
    }
    if (resultOk)
    {
        completedStages |= (1 << static_cast<uint8_t>(stage));
    }
    state = resultOk ? State::WaitingNext : State::Result;
    segmentStartedMs = millis();
    drawResultScreen();
    buzzer.play(resultOk ? BeepPattern::Success : BeepPattern::Failure);
}

void Atc::fail(const char *message)
{
    record.stopCalibratedTestTone();
    errorMessage = message;
    resultOk = false;
    state = State::Result;
    drawResultScreen();
    buzzer.play(BeepPattern::Failure);
}

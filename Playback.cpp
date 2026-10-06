#include "Azimuth.h"
#include "Playback.h"
#include "Display.h"
#include "Touch.h"
#include "Audio.h"
#include "Config.h"
#include "SignalFrequency.h"
#include "SetAudio.h"
#include <Arduino.h>
#include <math.h>
#include <TFT_eSPI.h>
#include "Buzzer.h"

extern TFT_eSPI tft;
extern Display display;
extern Touch touch;
extern Audio audio;
extern SetAudio setAudio;
extern Azimuth azimuth;
extern Buzzer buzzer;

static const int BAR_X = 22;
static const int BAR_Y_L = 90;
static const int BAR_Y_R = 135;
static const int BAR_H = 14;
static const int BAR_MAX_W = 276;
static const int FREQUENCY_Y = 30;
static const int FREQUENCY_GAP = 4;
static constexpr uint32_t FREQUENCY_REFRESH_MS = 500;
static const int DIFFERENCE_Y = 58;
static const int AZIMUTH_Y = 160;

static int differenceValueX = 0;
static int differenceValueWidth = 0;
static int peakLevelLeftX = 0;
static int peakLevelLeftWidth = 0;
static int peakLevelRightX = 0;
static int peakLevelRightWidth = 0;
static constexpr int peakLevelY = 58;
static constexpr uint8_t peakLevelFont = 4;

//== Constante folosite de VU =============
const uint16_t COL_CYAN_OFF = tft.color565(0, 20, 40);
const uint16_t COL_RED_OFF = tft.color565(40, 0, 0);
const uint16_t COL_RED_LITE = tft.color565(255, 255, 130);
const uint16_t DARKBLUE = tft.color565(0, 0, 127);

constexpr int DB_MIN = -300; // -30.0 dB
constexpr int DB_MAX = 100;  // +10.0 dB
constexpr float BALANCE_MIN_SIGNAL_DB = -30.0f;
constexpr int BAR_PIXELS = BAR_MAX_W - 4;
const int BAL_Y = 190;
const int SEG_W = 10;
const int GAP = 3;
const int CENTER = 160;

// numarul de bricks din VU
constexpr uint8_t VU_BRICKS = 20;

constexpr uint8_t BRICK_W = 8;
constexpr uint8_t BRICK_H = 10;
constexpr uint8_t BRICK_GAP = 2;
const int totalBricks = 28; // Nr total de bricks pe VU-meter

constexpr int ALIGN_FREQ_Y = 38;
constexpr int ALIGN_PHASE_Y = 70;

static void drawChannelDifferenceLabel()
{
    constexpr uint8_t DIFFERENCE_FONT = 4;
    const char *const title = "L-R DIFF.=";
    const char *const value = "-xx.x";
    const char *const unit = "dB";
    const int titleWidth = tft.textWidth(title, DIFFERENCE_FONT);
    const int valueWidth = tft.textWidth(value, DIFFERENCE_FONT);
    const int unitWidth = tft.textWidth(unit, DIFFERENCE_FONT);
    const int blockWidth =
        titleWidth + valueWidth + unitWidth + FREQUENCY_GAP * 2;
    const int blockX = (LCD_WIDTH - blockWidth) / 2;
    const int valueX = blockX + titleWidth + FREQUENCY_GAP;

    differenceValueX = valueX;
    differenceValueWidth = valueWidth;
    const int unitX = valueX + valueWidth + FREQUENCY_GAP;

    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(title, blockX, AZIMUTH_Y, DIFFERENCE_FONT);
    tft.setTextColor(TFT_ORANGE, COL_BG);
    tft.drawString(value, valueX, AZIMUTH_Y, DIFFERENCE_FONT);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(unit, unitX, AZIMUTH_Y, DIFFERENCE_FONT);
}

void Playback::drawChannelDifferenceValue()
{
    if (differenceValueWidth <= 0)
        return;

    char valueText[8];

    const int16_t tenths =
        static_cast<int16_t>(
            channelDifferenceDb * 10.0f +
            (channelDifferenceDb >= 0.0f
                 ? 0.5f
                 : -0.5f));

    snprintf(
        valueText,
        sizeof(valueText),
        "%+d.%d",
        tenths / 10,
        abs(tenths % 10));

    tft.setTextColor(
        TFT_ORANGE,
        COL_BG);
    tft.setTextPadding(differenceValueWidth);

    tft.drawRightString(
        valueText,
        differenceValueX + differenceValueWidth,
        AZIMUTH_Y,
        4);
    tft.setTextPadding(0);
}

static void drawPeakLevelsLabel()
{
    constexpr uint8_t LEVEL_FONT = 4;
    const char *const valueTemplate = "-xx.x";
    const char *const fullLeftLabel = "Level L=";
    const char *const fullLeftUnit = " dB / Level R=";
    const char *const compactLeftLabel = "L=";
    const char *const compactLeftUnit = " dB / R=";
    const char *const rightUnit = " dB";
    const int valueWidth = tft.textWidth(valueTemplate, LEVEL_FONT);
    const int rightUnitWidth = tft.textWidth(rightUnit, LEVEL_FONT);
    const int fullWidth =
        tft.textWidth(fullLeftLabel, LEVEL_FONT) + valueWidth +
        tft.textWidth(fullLeftUnit, LEVEL_FONT) + valueWidth + rightUnitWidth;
    const bool useFullLabels = fullWidth <= LCD_WIDTH;
    const char *const leftLabel = useFullLabels ? fullLeftLabel : compactLeftLabel;
    const char *const leftUnit = useFullLabels ? fullLeftUnit : compactLeftUnit;
    const int leftLabelWidth = tft.textWidth(leftLabel, LEVEL_FONT);
    const int leftUnitWidth = tft.textWidth(leftUnit, LEVEL_FONT);
    const int blockWidth =
        leftLabelWidth + valueWidth + leftUnitWidth + valueWidth + rightUnitWidth;
    const int blockX = (LCD_WIDTH - blockWidth) / 2;

    peakLevelLeftX = blockX + leftLabelWidth;
    peakLevelLeftWidth = valueWidth;
    peakLevelRightX = peakLevelLeftX + valueWidth + leftUnitWidth;
    peakLevelRightWidth = valueWidth;

    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(leftLabel, blockX, peakLevelY, LEVEL_FONT);
    tft.drawString(leftUnit, peakLevelLeftX + valueWidth, peakLevelY, LEVEL_FONT);
    tft.drawString(rightUnit, peakLevelRightX + valueWidth, peakLevelY, LEVEL_FONT);
}

static void drawPeakLevelValue(int x, int width, float value)
{
    char valueText[8];
    const int16_t tenths =
        static_cast<int16_t>(value * 10.0f + (value >= 0.0f ? 0.5f : -0.5f));
    if (tenths <= -100)
    {
        snprintf(valueText, sizeof(valueText), "-%d.%d", abs(tenths / 10), abs(tenths % 10));
    }
    else
    {
        snprintf(
            valueText,
            sizeof(valueText),
            " %c%d.%d",
            tenths < 0 ? '-' : '+',
            abs(tenths / 10),
            abs(tenths % 10));
    }
    tft.setTextColor(TFT_YELLOW, COL_BG);
    tft.setTextPadding(width);
    tft.drawRightString(valueText, x + width, peakLevelY, peakLevelFont);
    tft.setTextPadding(0);
}

static void drawPeakLevelValues(float left, float right)
{
    if (peakLevelLeftWidth <= 0 || peakLevelRightWidth <= 0)
        return;

    drawPeakLevelValue(peakLevelLeftX, peakLevelLeftWidth, left);
    drawPeakLevelValue(peakLevelRightX, peakLevelRightWidth, right);
}

uint8_t oldBarWL = 0;
uint8_t oldBarWR = 0;

void Playback::begin()
{
}

void Playback::run()
{
    alignmentMode = false;
    signalFrequencyHz = 0;

    running = true;
    drawScreen();

    while (running)
    {
        updateLevels();
        updateDisplay();
        display.updateClock();
        processTouch();
        delay(20);
    }
}

void Playback::runAlignment()
{
    alignmentMode = true;

    // Setup alignment UI once
    running = true;
    alignmentDialDrawn = false;
    needlePrevX = -1;
    needlePrevY = -1;
    needleVisualAngleDeg = 0.0f;

    drawAlignmentScreen();

    while (running)
    {
        updateLevels();           // capture & azimuth processing (unchanged)
        updateAlignmentDisplay(); // only update alignment UI
        display.updateClock();
        processTouch();
        delay(20);
    }

    alignmentMode = false;
}

void Playback::drawScreen()
{
    if (alignmentMode)
        display.openApp("HEAD ALIGNMENT", "TOUCH TO BACK");
    else
        display.openApp("PLAYBACK ANALYZER", "TOUCH TO BACK");

    frequencyDisplayInitialized = false;
    drawFrequencyLabel();
    drawChannelDifferenceLabel();
    // deseneaza chenare la Bargraf
    tft.drawRect(BAR_X - 2, BAR_Y_L - 4, BAR_MAX_W + 6, BAR_H + 4, TFT_DARKGREY);
    tft.drawRect(BAR_X - 2, BAR_Y_R - 4, BAR_MAX_W + 6, BAR_H + 4, TFT_DARKGREY);
    // deseneaza gradatiile bargrafului
    // Cratimele ocupă exact pozițiile spațiilor din scala originală.
    tft.setTextColor(TFT_DARKGREY, COL_BG);
    tft.drawString("-30-------20---------10---6--3--", 20, 110, 2);
    tft.setTextColor(COL_TEXT);
    tft.drawString("-30      -20        -10  -6 -3  ", 19, 110, 2);

    tft.setTextColor(TFT_MAROON, COL_BG);
    tft.drawString("0-+3-+5 dB", 230, 110, 2);
    tft.setTextColor(TFT_RED);
    tft.drawString("0 +3 +5 dB", 229, 110, 2);

    drawPeakLevelsLabel();

    //=== BALANCE ===============
    // Litere
    display.printLeft("LEFT", 25, BAL_Y - 2, 2);
    display.printLeft("RIGHT", 250, BAL_Y - 2, 2);
    // Marker Central
    tft.fillRect(CENTER - 1, BAL_Y, 2, 10, TFT_WHITE);
}

// Draw static parts of the alignment dial (arc, ticks, labels)
void Playback::drawAlignmentScreen()
{
    // Open header/footer
    display.openApp("HEAD ALIGNMENT", "TOUCH TO BACK");

    // --------------------------------------------------
    // ALIGNMENT TEXT - STATIC PART
    // --------------------------------------------------
    constexpr uint8_t ALIGN_FONT = 4;

    const int centerX = LCD_WIDTH / 2;

    // ---------- Frequency ----------
    const char *freqLabel = "Frequency =";
    const char *freqUnit = "Hz";

    const int freqLabelW = tft.textWidth(freqLabel, ALIGN_FONT);
    const int freqValueW = tft.textWidth("00000", ALIGN_FONT);
    const int freqUnitW = tft.textWidth(freqUnit, ALIGN_FONT);
    const int freqGap = 4;

    const int freqTotalW =
        freqLabelW + freqGap +
        freqValueW + freqGap +
        freqUnitW;

    const int freqX =
        centerX - freqTotalW / 2;

    alignFreqValueX =
        freqX + freqLabelW + freqGap;

    alignFreqValueWidth =
        freqValueW;

    tft.setTextDatum(TL_DATUM);

    tft.setTextColor(TFT_WHITE, COL_BG);
    tft.drawString(
        freqLabel,
        freqX,
        ALIGN_FREQ_Y,
        ALIGN_FONT);

    tft.drawString(
        freqUnit,
        alignFreqValueX + freqValueW + freqGap,
        ALIGN_FREQ_Y,
        ALIGN_FONT);

    // ---------- Azimuth Phase ----------
    const char *phaseLabel = "Azimuth Phase =";
    const int phaseLabelW =
        tft.textWidth(phaseLabel, ALIGN_FONT);

    alignPhaseValueWidth =
        tft.textWidth("-180.0", ALIGN_FONT);

    const int phaseTotalW =
        phaseLabelW + freqGap +
        alignPhaseValueWidth + 8;

    const int phaseX =
        centerX - phaseTotalW / 2;

    alignPhaseValueX =
        phaseX + phaseLabelW + freqGap;

    tft.setTextColor(TFT_WHITE, COL_BG);
    tft.drawString(
        phaseLabel,
        phaseX,
        ALIGN_PHASE_Y,
        ALIGN_FONT);

    // simbolul de grade este STATIC
    tft.drawCircle(
        alignPhaseValueX + alignPhaseValueWidth + 4,
        ALIGN_PHASE_Y + 4,
        2,
        TFT_WHITE);

    // forțează primul refresh
    displayedAlignmentFrequency = 0xFFFF;
    displayedAlignmentPhaseTenths = 32767;
    displayedAlignmentPhaseValid = false;

    // Draw semicircular arc (from -60deg to +60deg)
    const int cx = alignPivotX;
    const int cy = alignPivotY;
    const int r = alignRadius;

    tft.drawRect(cx - r - 6, cy - r - 6, r * 2 + 12, r + 20, COL_BG);

    // Draw arc as series of pixels for smoothness
    const int ARC_STEP_DEG = 2;
    tft.setTextColor(TFT_DARKGREY, COL_BG);
    for (int a = -60; a <= 60; a += ARC_STEP_DEG)
    {
        const float rad = a * PI / 180.0f;
        const int x = cx + (int)(sinf(rad) * r);
        const int y = cy - (int)(cosf(rad) * r);
        tft.drawPixel(x, y, TFT_DARKGREY);
    }

    // Draw ticks and labels
    const int tickCount = 9; // -60..+60 step 15
    tft.setTextDatum(MC_DATUM);
    for (int i = 0; i < tickCount; ++i)
    {
        const int angle = -60 + i * 15;
        const float rad = angle * PI / 180.0f;
        const int x1 = cx + (int)(sinf(rad) * (r - 6));
        const int y1 = cy - (int)(cosf(rad) * (r - 6));
        const int x2 = cx + (int)(sinf(rad) * (r + 2));
        const int y2 = cy - (int)(cosf(rad) * (r + 2));
        tft.drawLine(x1, y1, x2, y2, TFT_WHITE);
    }

    // Labels LEFT 0 RIGHT
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_WHITE, COL_BG);
    tft.drawString("LEFT", cx - r + 12, cy - r / 2, 2);
    tft.drawString("0", cx, cy - r - 6, 2);
    tft.drawString("RIGHT", cx + r - 12, cy - r / 2, 2);

    // Mark that static dial is ready
    alignmentDialDrawn = true;
}

// Update only the numeric values and needle without redrawing the whole screen
void Playback::updateAlignmentDisplay()
{
    // --------------------------------------------------
    // FREQUENCY - update NUMAI valoarea
    // --------------------------------------------------
    if (signalFrequencyHz != displayedAlignmentFrequency)
    {
        char value[8];

        if (signalFrequencyHz == 0)
            snprintf(value, sizeof(value), "---");
        else
            snprintf(
                value,
                sizeof(value),
                "%u",
                (unsigned int)signalFrequencyHz);

        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_GREEN, COL_BG);

        tft.setTextPadding(alignFreqValueWidth);

        tft.drawRightString(
            value,
            alignFreqValueX + alignFreqValueWidth,
            ALIGN_FREQ_Y,
            4);

        tft.setTextPadding(0);

        displayedAlignmentFrequency =
            signalFrequencyHz;
    }

    // --------------------------------------------------
    // AZIMUTH PHASE - update NUMAI valoarea
    // --------------------------------------------------
    int16_t phaseTenths = 0;

    if (azimuthPhaseValid)
    {
        phaseTenths =
            static_cast<int16_t>(
                azimuthPhaseDegrees * 10.0f +
                (azimuthPhaseDegrees >= 0.0f
                     ? 0.5f
                     : -0.5f));
    }

    if (azimuthPhaseValid != displayedAlignmentPhaseValid ||
        (azimuthPhaseValid &&
         phaseTenths != displayedAlignmentPhaseTenths))
    {
        char value[10];

        if (!azimuthPhaseValid)
        {
            snprintf(value, sizeof(value), "---");
        }
        else
        {
            snprintf(
                value,
                sizeof(value),
                "%+d.%d",
                phaseTenths / 10,
                abs(phaseTenths % 10));
        }

        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_YELLOW, COL_BG);

        tft.setTextPadding(
            alignPhaseValueWidth);

        tft.drawRightString(
            value,
            alignPhaseValueX + alignPhaseValueWidth,
            ALIGN_PHASE_Y,
            4);

        tft.setTextPadding(0);

        displayedAlignmentPhaseTenths =
            phaseTenths;

        displayedAlignmentPhaseValid =
            azimuthPhaseValid;
    }

    // Compute needle visual angle with deadband and smoothing
    float measured = azimuthPhaseDegrees;
    if (fabsf(measured) <= alignmentDeadbandDeg)
        measured = 0.0f;

    // Clamp measured to [-20,20] then map to visual [-60,60]
    const float clamped = constrain(measured, -20.0f, 20.0f);
    const float visual = (clamped / 20.0f) * 60.0f;

    // Smooth visual angle
    needleVisualAngleDeg = alignmentSmoothingAlpha * visual + (1.0f - alignmentSmoothingAlpha) * needleVisualAngleDeg;

    // Compute needle end point
    const int cx = alignPivotX;
    const int cy = alignPivotY;
    const float rad = needleVisualAngleDeg * PI / 180.0f;
    const int nx = cx + (int)(sinf(rad) * alignNeedleLength);
    const int ny = cy - (int)(cosf(rad) * alignNeedleLength);

    // Erase previous needle by drawing over it in background and then redraw static parts
    if (needlePrevX != -1)
    {
        // erase
        tft.drawLine(cx, cy, needlePrevX, needlePrevY, COL_BG);

        // redraw static arc and ticks in the affected area: easiest is to redraw whole static dial
        const int r = alignRadius;
        const int ARC_STEP_DEG = 2;
        tft.setTextColor(TFT_DARKGREY, COL_BG);
        for (int a = -60; a <= 60; a += ARC_STEP_DEG)
        {
            const float ar = a * PI / 180.0f;
            const int x = cx + (int)(sinf(ar) * r);
            const int y = cy - (int)(cosf(ar) * r);
            tft.drawPixel(x, y, TFT_DARKGREY);
        }
        // ticks
        const int tickCount = 9;
        for (int i = 0; i < tickCount; ++i)
        {
            const int angle = -60 + i * 15;
            const float ar = angle * PI / 180.0f;
            const int x1 = cx + (int)(sinf(ar) * (r - 6));
            const int y1 = cy - (int)(cosf(ar) * (r - 6));
            const int x2 = cx + (int)(sinf(ar) * (r + 2));
            const int y2 = cy - (int)(cosf(ar) * (r + 2));
            tft.drawLine(x1, y1, x2, y2, TFT_WHITE);
        }
        // labels
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(TFT_WHITE, COL_BG);
        tft.drawString("LEFT", cx - r + 12, cy - r / 2, 2);
        tft.drawString("0", cx, cy - r - 6, 2);
        tft.drawString("RIGHT", cx + r - 12, cy - r / 2, 2);
    }

    // Draw new needle (thin bright line)
    tft.drawLine(cx, cy, nx, ny, TFT_YELLOW);
    // small pivot cap
    tft.fillCircle(cx, cy, 3, TFT_WHITE);

    needlePrevX = nx;
    needlePrevY = ny;
    // restore default text alignment for other screens
    tft.setTextDatum(TL_DATUM);
}

void Playback::updateLevels()
{
    //------------------------------------------------------
    // Constante
    //------------------------------------------------------
    const float RMS_REF = setAudio.rmsReference();
    const float ALPHA = setAudio.smoothing();
    const float PEAK_DECAY = setAudio.peakDecayDb();
    const uint16_t PEAK_HOLD_TIME = setAudio.peakHoldMs();

    //------------------------------------------------------
    // Variabile locale
    //------------------------------------------------------
    float sumL = 0.0f;
    float sumR = 0.0f;

    //------------------------------------------------------
    // CAPTURĂ COMUNĂ LEFT / RIGHT
    //------------------------------------------------------
    for (uint16_t i = 0;
         i < VU_SAMPLE_COUNT;
         ++i)
    {
        const uint32_t sampleStart = micros();

        const int16_t sampleL =
            audio.readLeftCentered();

        const int16_t sampleR =
            audio.readRightCentered();

        leftSamples[i] = sampleL;
        rightSamples[i] = sampleR;

        sumL +=
            static_cast<float>(sampleL) *
            static_cast<float>(sampleL);

        sumR +=
            static_cast<float>(sampleR) *
            static_cast<float>(sampleR);

        while (
            micros() - sampleStart <
            SignalFrequency::SAMPLE_PERIOD_US)
        {
        }
    }

    const float measuredHz =
        SignalFrequency::measureInterpolated(
            leftSamples,
            VU_SAMPLE_COUNT);

    signalFrequencyHz =
        measuredHz > 0.0f
            ? static_cast<uint16_t>(measuredHz + 0.5f)
            : 0;
    //------------------------------------------------------
    // RMS
    //------------------------------------------------------
    float rmsL = sqrt(sumL / VU_SAMPLE_COUNT);
    float rmsR = sqrt(sumR / VU_SAMPLE_COUNT);

    newLevelL = 20.0f * log10((rmsL + 0.0001f) / RMS_REF);
    newLevelR = 20.0f * log10((rmsR + 0.0001f) / RMS_REF);

    //------------------------------------------------------
    // Smoothing
    //------------------------------------------------------
    smoothedLevelL =
        ALPHA * newLevelL +
        (1.0f - ALPHA) * smoothedLevelL;

    smoothedLevelR =
        ALPHA * newLevelR +
        (1.0f - ALPHA) * smoothedLevelR;

    levelL = smoothedLevelL;
    levelR = smoothedLevelR;

    //------------------------------------------------------
    // LEFT - RIGHT Difference
    //------------------------------------------------------
    channelDifferenceDb =
        levelL - levelR;

    if (newLevelL > BALANCE_MIN_SIGNAL_DB || newLevelR > BALANCE_MIN_SIGNAL_DB)
    {
        balancePos = constrain(
            static_cast<int>(channelDifferenceDb * 2.0f +
                             (channelDifferenceDb >= 0.0f ? 0.5f : -0.5f)),
            -4,
            4);
    }
    else
    {
        balancePos = 0;
    }

    //------------------------------------------------------
    // AZIMUTH PHASE
    //------------------------------------------------------
    azimuth.process(
        leftSamples,
        rightSamples,
        VU_SAMPLE_COUNT,
        static_cast<float>(
            SignalFrequency::SAMPLE_RATE_HZ));

    azimuthPhaseValid =
        azimuth.valid();

    if (azimuthPhaseValid)
    {
        azimuthPhaseDegrees =
            azimuth.phaseDegrees();
    }
    else
    {
        azimuthPhaseDegrees = 0.0f;
    }
    //------------------------------------------------------
    // Peak Hold L
    //------------------------------------------------------
    if (levelL >= peakL)
    {
        peakL = levelL;
        peakTimerL = millis();
    }
    else if (millis() - peakTimerL > PEAK_HOLD_TIME)
    {
        peakL -= PEAK_DECAY;

        if (peakL < levelL)
            peakL = levelL;
    }

    //------------------------------------------------------
    // Peak Hold R
    //------------------------------------------------------
    if (levelR >= peakR)
    {
        peakR = levelR;
        peakTimerR = millis();
    }
    else if (millis() - peakTimerR > PEAK_HOLD_TIME)
    {
        peakR -= PEAK_DECAY;

        if (peakR < levelR)
            peakR = levelR;
    }
    const float OVER_LEVEL = setAudio.overLevelDb();

    // Peak-> OVER
    //  Canal L
    if (peakL >= OVER_LEVEL)
    {
        overL = true;
        overTimerL = millis();
    }

    // Canal R
    if (peakR >= OVER_LEVEL)
    {
        overR = true;
        overTimerR = millis();
    }
}

void Playback::updateDisplay()
{
    barWL = map(
        constrain((int)(levelL * 10), DB_MIN, DB_MAX),
        DB_MIN,
        DB_MAX,
        0,
        BAR_PIXELS);

    barWR = map(
        constrain((int)(levelR * 10), DB_MIN, DB_MAX),
        DB_MIN,
        DB_MAX,
        0,
        BAR_PIXELS);

    peakWL = map(
        constrain((int)(peakL * 10), DB_MIN, DB_MAX),
        DB_MIN,
        DB_MAX,
        0,
        BAR_PIXELS);

    peakWR = map(
        constrain((int)(peakR * 10), DB_MIN, DB_MAX),
        DB_MIN,
        DB_MAX,
        0,
        BAR_PIXELS);

    // Pentru canalul stâng
    drawVUmeter(BAR_X, BAR_Y_L, barWL, peakWL, overL);

    // Pentru canalul drept
    drawVUmeter(BAR_X, BAR_Y_R, barWR, peakWR, overR);

    // Marker OVER-LOAD
    if (overL) // Left Channel
    {
        if (millis() - overTimerL > OVER_HOLD)
            overL = false;
    }

    if (overR) // Right Channel
    {
        if (millis() - overTimerR > OVER_HOLD)
            overR = false;
    }

    // Frecvență
    drawFrequencyLabel();
    drawChannelDifferenceValue();
    drawPeakLevelValues(peakL, peakR);

    const bool balanceSignalDetected =
        newLevelL > BALANCE_MIN_SIGNAL_DB || newLevelR > BALANCE_MIN_SIGNAL_DB;
    const uint16_t inactiveBalanceColor =
        balanceSignalDetected ? COL_CYAN_OFF : COL_BG;

    // Clear or dim the Balance indicator, depending on input signal level.
    for (int i = 0; i < 4; i++)
    {
        int xL = CENTER - GAP - SEG_W - i * (SEG_W + GAP);

        tft.fillRect(
            xL,
            BAL_Y,
            SEG_W,
            10,
            inactiveBalanceColor);

        int xR = CENTER + GAP + i * (SEG_W + GAP);

        tft.fillRect(
            xR,
            BAL_Y,
            SEG_W,
            10,
            inactiveBalanceColor);
    }

    // Aprindem Balance-Left
    if (balanceSignalDetected && balancePos < 0)
    {
        for (int i = 0; i < -balancePos; ++i)
        {
            int x = CENTER - GAP - SEG_W - i * (SEG_W + GAP);

            tft.fillRect(
                x,
                BAL_Y,
                SEG_W,
                10,
                TFT_CYAN);
        }
    }

    // Aprindem Balance-Right
    if (balanceSignalDetected && balancePos > 0)
    {
        for (int i = 0;
             i < balancePos;
             ++i)
        {
            const int x =
                CENTER + GAP +
                i * (SEG_W + GAP);

            tft.fillRect(
                x,
                BAL_Y,
                SEG_W,
                10,
                TFT_CYAN);
        }
    }
}

void Playback::drawFrequencyLabel()
{
    constexpr uint8_t FREQUENCY_FONT = 4;
    const char *const label = "Frequency = ";
    const char *const unit = " Hz";
    const int labelWidth = tft.textWidth(label, FREQUENCY_FONT);
    const int valueWidth = tft.textWidth("20000", FREQUENCY_FONT);
    const int unitWidth = tft.textWidth(unit, FREQUENCY_FONT);
    const int blockWidth = labelWidth + valueWidth + unitWidth;
    const int blockX = (LCD_WIDTH - blockWidth) / 2;
    const int valueRight = blockX + labelWidth + valueWidth;
    const int unitX = valueRight;

    if (!frequencyDisplayInitialized)
    {
        tft.setTextColor(TFT_WHITE, COL_BG);
        tft.drawString(label, blockX, FREQUENCY_Y, FREQUENCY_FONT);
        tft.drawString(unit, unitX, FREQUENCY_Y, FREQUENCY_FONT);
        tft.setTextColor(TFT_GREEN, COL_BG);
        tft.setTextPadding(valueWidth);
        tft.drawRightString("---", valueRight, FREQUENCY_Y, FREQUENCY_FONT);
        tft.setTextPadding(0);

        displayedFrequencyHz = 0;
        frequencyDisplayInitialized = true;
        frequencyLastRefreshMs = millis();
    }
    else
    {
        const uint32_t now = millis();
        if (now - frequencyLastRefreshMs < FREQUENCY_REFRESH_MS)
            return;

        frequencyLastRefreshMs = now;
        if (displayedFrequencyHz == signalFrequencyHz)
            return;

        char frequencyValue[8];
        snprintf(
            frequencyValue, sizeof(frequencyValue),
            signalFrequencyHz ? "%u" : "---",
            static_cast<unsigned int>(signalFrequencyHz));

        // Fixed padding clears only the numeric value; label and unit stay static.
        tft.setTextColor(TFT_GREEN, COL_BG);
        tft.setTextPadding(valueWidth);
        tft.drawRightString(
            frequencyValue, valueRight, FREQUENCY_Y, FREQUENCY_FONT);
        tft.setTextPadding(0);

        displayedFrequencyHz = signalFrequencyHz;
    }
}

void Playback::processTouch()
{
    if (!touch.pressed())
        return;

    running = false;
}

void Playback::drawVUmeter(
    int x,
    int y,
    int barPixels,
    int peakPixels,
    bool isOver,
    bool showRecordTarget)
{
    int activeBricks = map(barPixels, 0, BAR_PIXELS, 0, totalBricks);

    if (activeBricks > totalBricks)
        activeBricks = totalBricks;

    if (activeBricks < 0)
        activeBricks = 0;

    int peakBrick = map(peakPixels, 0, BAR_PIXELS, 0, totalBricks);

    if (peakBrick > totalBricks - 1)
        peakBrick = totalBricks - 1;

    if (peakBrick < 0)
        peakBrick = 0;

    int zeroDbBrick = map(0, -300, 100, 0, totalBricks);
    int targetDbBrick = map(-100, -300, 100, 0, totalBricks); // -10 dB

    const int zeroDbPixels = map(0, DB_MIN, DB_MAX, 0, BAR_PIXELS);

    for (int i = 0; i < totalBricks; i++)
    {
        int brickX = x + (i * (BRICK_W + BRICK_GAP));
        uint16_t brickColor;

        if (isOver && i == (totalBricks - 1))
        {
            brickColor = TFT_RED;
        }
        else if (i < activeBricks)
        {
            brickColor = TFT_CYAN;
        }
        else
        {
            if (showRecordTarget && i < targetDbBrick)
            {
                brickColor = DARKBLUE;
            }
            else
            {
                brickColor =
                    (i >= zeroDbBrick) ? COL_RED_OFF : COL_CYAN_OFF;
            }
        }

        tft.fillRect(brickX, y, BRICK_W, BRICK_H, brickColor);
    }

    if (!isOver)
    {
        int peakX = x + (peakBrick * (BRICK_W + BRICK_GAP));

        uint16_t peakColor =
            (peakPixels > zeroDbPixels) ? TFT_RED : TFT_CYAN;

        tft.fillRect(peakX, y, BRICK_W, BRICK_H, peakColor);
    }
}

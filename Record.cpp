#include "Record.h"
#include "Sine.h"
#include "ProgressBar.h"
#include "Buzzer.h"

#include "AD9833.h"
#include "Atc.h"
#include "Audio.h"
#include "Config.h"
#include "Display.h"
#include "DolbyCheck.h"
#include "Help.h"
#include "Icons.h"
#include "Pot.h"
#include "Playback.h"
#include "SetAudio.h"
#include "SignalFrequency.h"
#include "Storage.h"
#include "Touch.h"

#include <TFT_eSPI.h>
#include <math.h>

extern AD9833 ad9833;
extern Audio audio;
extern Display display;
extern TFT_eSPI tft;
extern Pot pot;
extern Playback playback;
extern SetAudio setAudio;
extern Storage storage;
extern Touch touch;

Record record;

namespace
{
    constexpr uint16_t RECORD_LEVEL_FREQUENCY_3HEAD_HZ = 440;
    constexpr uint16_t RECORD_LEVEL_FREQUENCY_2HEAD_HZ = 440;
    constexpr float RECORD_LEVEL_DB = -10.0f;
    constexpr uint16_t BIAS_LOW_FREQUENCY_HZ = 440;
    constexpr uint16_t BIAS_HIGH_FREQUENCY_HZ = 10000;
    constexpr uint16_t REC_CAL_SYNC_FREQUENCY_HZ = 2000;
    constexpr float REC_CAL_BASE_LEVEL_DB = -15.0f;
    constexpr float REC_CAL_LEVEL_MIN_DB = -20.0f;
    constexpr float REC_CAL_LEVEL_MAX_DB = -10.0f;
    constexpr float THREE_HEAD_BIAS_LEVEL_MAX_DB = -9.0f;
    constexpr float REC_CAL_BIAS_MATCH_TOLERANCE_DB = 0.5f;
    constexpr float REC_CAL_BIAS_ACCEPTABLE_TOLERANCE_DB = 0.8f;
    constexpr uint8_t REC_CAL_REQUIRED_STABLE_CYCLES = 3;
    constexpr float TWO_HEAD_BIAS_SYNC_LEVEL_DB = -12.0f;
    constexpr float TWO_HEAD_BIAS_TEST_LEVEL_DB = -15.0f;
    constexpr float GENERATOR_MAX_LEVEL_DB = 3.0f;
    constexpr uint32_t BIAS_SETTLE_TIME_MS = 500;
    constexpr uint32_t BIAS_MEASUREMENT_WINDOW_MS = 1500;
    constexpr uint32_t TWO_HEAD_LEVEL_RECORD_MS = 20000;
    constexpr uint32_t TWO_HEAD_BIAS_SYNC_MS = 2000;
    constexpr uint32_t TWO_HEAD_BIAS_TONE_MS = 5000;
    constexpr uint32_t TWO_HEAD_BIAS_RECORD_TOTAL_MS =
        3 * TWO_HEAD_BIAS_SYNC_MS + 2 * TWO_HEAD_BIAS_TONE_MS;
    constexpr uint32_t TWO_HEAD_LEVEL_ANALYZE_TIMEOUT_MS = 60000;
    constexpr uint32_t TWO_HEAD_BIAS_ANALYZE_TIMEOUT_MS = 90000;
    constexpr uint16_t TWO_HEAD_EQ_SYNC_HZ = 2000;
    constexpr uint32_t TWO_HEAD_EQ_SYNC_MS = 2000;
    constexpr uint32_t TWO_HEAD_EQ_GAP_MS = 500;
    constexpr uint32_t TWO_HEAD_EQ_TONE_MS = 1000;
    constexpr uint32_t TWO_HEAD_EQ_FIRST_50_SETTLE_MS = 1000;
    constexpr uint32_t TWO_HEAD_EQ_ANALYZE_SETTLE_MS = 250;
    constexpr uint32_t TWO_HEAD_EQ_ANALYZE_WINDOW_MS = 500;
    constexpr uint32_t TWO_HEAD_EQ_50_ANALYZE_SETTLE_MS = 1000;
    constexpr uint32_t TWO_HEAD_EQ_50_ANALYZE_WINDOW_MS = 800;
    constexpr uint32_t TWO_HEAD_EQ_SYNC_TIMEOUT_MS = 60000;
    constexpr uint32_t TWO_HEAD_LEVEL_CAPTURE_MS = 5000;
    constexpr uint32_t TWO_HEAD_LEVEL_STABLE_MS = 500;
    constexpr uint32_t TWO_HEAD_LEVEL_TONE_LOSS_HOLD_MS = 500;
    constexpr uint8_t TWO_HEAD_TONE_CONFIRMATIONS = 3;
    constexpr float SYNC_MIN_HZ = 1850.0f;
    constexpr float SYNC_MAX_HZ = 2150.0f;
    constexpr uint8_t SYNC_REQUIRED_CONFIRMATIONS = 3;
    constexpr uint32_t TWO_HEAD_BIAS_POST_SYNC_IGNORE_MS = 300;
    constexpr uint8_t TWO_HEAD_BIAS_MIN_SAMPLES = 5;

    constexpr uint16_t TAPE_RESPONSE_FREQUENCIES_HZ[TAPE_RESPONSE_BAND_COUNT] =
        {
            50, 80, 125, 200, 315, 500, 800, 1000,
            1600, 2500, 4000, 6300, 8000, 10000, 12500, 15000};
    constexpr float TAPE_RESPONSE_BASE_LEVEL_DB = -10.0f;
    constexpr float TAPE_RESPONSE_MAX_AUTO_CORRECTION_DB = 2.0f;
    constexpr float TAPE_RESPONSE_VERIFY_EXCELLENT_DB = 0.3f;
    constexpr float TAPE_RESPONSE_VERIFY_OK_DB = 0.5f;
    constexpr uint16_t LOOPBACK_PREFLIGHT_FREQUENCY_HZ = 1000;
    constexpr uint32_t LOOPBACK_PREFLIGHT_NOISE_MS = 350;
    constexpr uint32_t LOOPBACK_PREFLIGHT_SETTLE_MS = 500;
    constexpr uint32_t LOOPBACK_PREFLIGHT_SIGNAL_MS = 400;
    constexpr uint32_t LOOPBACK_PREFLIGHT_PASSED_MS = 450;
    constexpr float LOOPBACK_MIN_SIGNAL_RISE_DB = 15.0f;
    constexpr float LOOPBACK_EXPECTED_MIN_DB = -30.0f;
    constexpr float LOOPBACK_EXPECTED_MAX_DB = 0.0f;
    constexpr float LOOPBACK_MAX_CHANNEL_DIFFERENCE_DB = 1.5f;
    constexpr uint16_t AUTOMATIC_TEST_REQUIRED_WINDOWS = 3;
    constexpr float AUTOMATIC_TEST_MIN_LEVEL_DB = LOOPBACK_EXPECTED_MIN_DB;
    constexpr float AUTOMATIC_TEST_MAX_LEVEL_DB = LOOPBACK_EXPECTED_MAX_DB;
    constexpr float AUTOMATIC_PRECHECK_MIN_LEVEL_DB = -13.0f;
    constexpr float AUTOMATIC_PRECHECK_MAX_LEVEL_DB = -7.0f;
    constexpr float SWEEP_FREQUENCY_TOLERANCE = 0.05f;
    constexpr uint16_t MIN_MEASURABLE_SWEEP_FREQUENCY_HZ = 100;
    constexpr uint16_t MAX_RELIABLE_SWEEP_FREQUENCY_HZ = 12500;
    constexpr uint32_t TAPE_RESPONSE_SETTLE_MS = 250;
    constexpr uint32_t TAPE_RESPONSE_MEASURE_MS = 400;
    constexpr uint32_t TAPE_RESPONSE_FIRST_BAND_SETTLE_MS = 750;
    constexpr uint32_t TAPE_RESPONSE_LOW_BAND_MEASURE_MS = 800;
    constexpr uint8_t TAPE_RESPONSE_REFERENCE_INDEX = 7;
    constexpr float TAPE_RESPONSE_GRAPH_MIN_DB = -6.0f;
    constexpr float TAPE_RESPONSE_GRAPH_MAX_DB = 6.0f;
    constexpr int TAPE_RESPONSE_GRAPH_LEFT = 34;
    // Reserve room for the centred 15k label while extending the grid to the
    // usable right edge of the content area.
    constexpr int TAPE_RESPONSE_GRAPH_RIGHT = CONTENT_RIGHT - 10;
    constexpr int TAPE_RESPONSE_GRAPH_TOP = 62;
    constexpr int TAPE_RESPONSE_GRAPH_BOTTOM = 180;

    constexpr int tapeResponseGraphBandX(uint8_t bandIndex)
    {
        return TAPE_RESPONSE_GRAPH_LEFT +
               (static_cast<int>(bandIndex) *
                    (TAPE_RESPONSE_GRAPH_RIGHT - TAPE_RESPONSE_GRAPH_LEFT) +
                (TAPE_RESPONSE_BAND_COUNT - 1) / 2) /
                   (TAPE_RESPONSE_BAND_COUNT - 1);
    }

    constexpr float RECORD_LEVEL_TOLERANCE_2HEAD_DB = 1.0f;
    constexpr float RECORD_LEVEL_TOLERANCE_3HEAD_DB = 0.6f;
    constexpr float RECORD_MATCH_HOLD_TOLERANCE_DB = 0.75f;
    constexpr uint8_t RECORD_MATCH_ENTRY_CONFIRMATIONS = 4;
    constexpr uint8_t RECORD_MATCH_EXIT_CONFIRMATIONS = 3;
    constexpr float BIAS_MATCH_TOLERANCE_DB = 0.8f;
    constexpr float BIAS_CORRECTION_NEAR_ZERO_DB = 0.05f;
    constexpr uint8_t BIAS_AVERAGE_SAMPLE_COUNT = 3;
    constexpr float SMOOTHING_ALPHA = 0.25f;
    constexpr float MIN_SIGNAL_DB = -40.0f;

    bool frequencyInWindow(float frequencyHz, float minimumHz, float maximumHz)
    {
        return isfinite(frequencyHz) &&
               frequencyHz >= minimumHz &&
               frequencyHz <= maximumHz;
    }

    bool sweepFrequencyMatches(float frequencyHz, uint16_t expectedHz)
    {
        if (expectedHz < MIN_MEASURABLE_SWEEP_FREQUENCY_HZ)
            return true;
        const float toleranceHz = expectedHz * SWEEP_FREQUENCY_TOLERANCE;
        return frequencyInWindow(
            frequencyHz,
            expectedHz - toleranceHz,
            expectedHz + toleranceHz);
    }

    bool automaticLevelPlausible(float levelDb)
    {
        return isfinite(levelDb) &&
               levelDb >= AUTOMATIC_TEST_MIN_LEVEL_DB &&
               levelDb <= AUTOMATIC_TEST_MAX_LEVEL_DB;
    }

    bool automaticPrecheckLevelValid(float levelDb)
    {
        return isfinite(levelDb) &&
               levelDb >= AUTOMATIC_PRECHECK_MIN_LEVEL_DB &&
               levelDb <= AUTOMATIC_PRECHECK_MAX_LEVEL_DB;
    }

    constexpr uint8_t ITEM_FONT = 2;
    constexpr int TEXT_X = CONTENT_LEFT + 13;
    constexpr int TEST_Y = 30;
    constexpr int RECORD_FREQUENCY_Y = 52;
    constexpr int GENERATOR_Y = 74;
    constexpr int THREE_HEAD_LEVEL_Y = 48;
    constexpr int THREE_HEAD_BIAS_Y = 66;
    constexpr int THREE_HEAD_STABLE_Y = 84;
    constexpr int RECORD_LEFT_Y = 166;
    constexpr int RECORD_RIGHT_Y = 188;
    constexpr int RECORD_LEFT_LABEL_X = TEXT_X;
    constexpr int RECORD_LEFT_VALUE_X = 62;
    constexpr int RECORD_DIFF_LABEL_X = 148;
    constexpr int RECORD_DIFF_VALUE_X = 206;
    constexpr int BIAS_440_Y = 158;
    constexpr int BIAS_10K_Y = 176;
    constexpr int BIAS_DIFF_Y = 194;
    constexpr int BIAS_LEFT_LABEL_X = TEXT_X;
    constexpr int BIAS_LEFT_VALUE_X = 72;
    constexpr int BIAS_RIGHT_LABEL_X = 153;
    constexpr int BIAS_RIGHT_VALUE_X = 181;
    constexpr int BIAS_VALUE_WIDTH = 72;
    constexpr int BIAS_ACTIVE_X = TEXT_X;
    constexpr int BIAS_ACTIVE_VALUE_X = 100;
    constexpr int VALUE_WIDTH = 76;
    constexpr int START_STOP_X = 238;
    constexpr int START_STOP_Y = CONTENT_TOP + 10;
    constexpr int START_STOP_WIDTH = 64;
    constexpr int START_STOP_HEIGHT = 62;
    constexpr int TAPE_RESPONSE_RETEST_X = START_STOP_X;
    constexpr int TAPE_RESPONSE_RETEST_Y = CONTENT_TOP + 5;
    constexpr int TAPE_RESPONSE_RETEST_WIDTH = START_STOP_WIDTH;
    constexpr int TAPE_RESPONSE_VIEW_X = 168;
    constexpr int TAPE_RESPONSE_VIEW_Y = CONTENT_TOP + 5;
    constexpr int TAPE_RESPONSE_VIEW_WIDTH = 64;
    constexpr int RESPONSE_CHOICE_X = 48;
    constexpr int RESPONSE_CHOICE_WIDTH = 224;
    constexpr int RESPONSE_CHOICE_HEIGHT = 34;
    constexpr int RESPONSE_CHOICE_FIRST_Y = 82;
    constexpr int RESPONSE_CHOICE_SECOND_Y = 132;
    constexpr int RESPONSE_RESULT_FIRST_Y = 116;
    constexpr int RESPONSE_RESULT_SECOND_Y = 164;
    constexpr int LOOPBACK_RESULT_ICON_SIZE = 72;
    constexpr int LOOPBACK_RESULT_ICON_Y = 124;
    constexpr int LOOPBACK_RESULT_TAPE_TEST_X = 69;
    constexpr int LOOPBACK_RESULT_AUTO_CAL_X = 179;
    constexpr int LOOPBACK_RESULT_ACTION_ICON_Y = LOOPBACK_RESULT_ICON_Y - 15;
    constexpr int TAPE_RESPONSE_DYNAMIC_X = TEXT_X;
    constexpr int TAPE_RESPONSE_DYNAMIC_Y = 78;
    constexpr int TAPE_RESPONSE_DYNAMIC_WIDTH = START_STOP_X - TEXT_X - 6;
    constexpr int TAPE_RESPONSE_DYNAMIC_HEIGHT = 86;
    constexpr uint8_t LOOPBACK_PROGRESS_BRICK_COUNT = 16;
    constexpr int LOOPBACK_PROGRESS_BRICK_WIDTH = 8;
    constexpr int LOOPBACK_PROGRESS_BRICK_HEIGHT = 10;
    constexpr int LOOPBACK_PROGRESS_BRICK_GAP = 2;
    constexpr int LOOPBACK_PROGRESS_BAR_WIDTH =
        LOOPBACK_PROGRESS_BRICK_COUNT *
            (LOOPBACK_PROGRESS_BRICK_WIDTH + LOOPBACK_PROGRESS_BRICK_GAP) -
        LOOPBACK_PROGRESS_BRICK_GAP;
    constexpr int LOOPBACK_PROGRESS_BAR_RIGHT = LCD_WIDTH - 1 - 20;
    constexpr int LOOPBACK_PROGRESS_BAR_X =
        LOOPBACK_PROGRESS_BAR_RIGHT - LOOPBACK_PROGRESS_BAR_WIDTH + 1;
    constexpr int LOOPBACK_PROGRESS_CALIBRATE_Y = 122;
    constexpr int LOOPBACK_PROGRESS_VERIFY_1_Y = 148;
    constexpr int LOOPBACK_PROGRESS_VERIFY_2_Y = 174;
    constexpr int THREE_HEAD_STATUS_VALUE_X = 92;
    constexpr int THREE_HEAD_STATUS_VALUE_WIDTH = START_STOP_X - THREE_HEAD_STATUS_VALUE_X - 4;
    constexpr int THREE_HEAD_FREQUENCY_X = 174;
    constexpr int THREE_HEAD_FREQUENCY_WIDTH = START_STOP_X - THREE_HEAD_FREQUENCY_X - 4;
    constexpr int THREE_HEAD_STAGE_COUNTER_X = LCD_WIDTH - 6 - 15;
    constexpr int THREE_HEAD_STAGE_COUNTER_Y = LCD_HEIGHT - 28 - 40;
    constexpr uint8_t THREE_HEAD_STAGE_COUNTER_FONT = 4;

    void drawStopOctagon(int x, int y, int width, int height, int font)
    {
        const int cut = min(width, height) / 4;
        tft.fillRect(x, y, width, height, COL_BG);
        tft.fillRect(x + cut, y, width - 2 * cut, height, TFT_RED);
        tft.fillRect(x, y + cut, width, height - 2 * cut, TFT_RED);

        tft.fillTriangle(x + cut, y, x, y + cut, x + cut, y + cut, TFT_RED);
        tft.fillTriangle(x + width - cut - 1, y,
                         x + width - cut - 1, y + cut,
                         x + width - 1, y + cut, TFT_RED);
        tft.fillTriangle(x, y + height - cut - 1,
                         x + cut, y + height - cut - 1,
                         x + cut, y + height - 1, TFT_RED);
        tft.fillTriangle(x + width - 1, y + height - cut - 1,
                         x + width - cut - 1, y + height - cut - 1,
                         x + width - cut - 1, y + height - 1, TFT_RED);

        tft.drawLine(x + cut, y, x + width - cut - 1, y, TFT_WHITE);
        tft.drawLine(x + width - cut, y + 1,
                     x + width - 1, y + cut, TFT_WHITE);
        tft.drawLine(x + width - 1, y + cut,
                     x + width - 1, y + height - cut - 1, TFT_WHITE);
        tft.drawLine(x + width - 1, y + height - cut,
                     x + width - cut, y + height - 1, TFT_WHITE);
        tft.drawLine(x + width - cut - 1, y + height - 1,
                     x + cut, y + height - 1, TFT_WHITE);
        tft.drawLine(x + cut - 1, y + height - 1,
                     x, y + height - cut, TFT_WHITE);
        tft.drawLine(x, y + height - cut - 1,
                     x, y + cut, TFT_WHITE);
        tft.drawLine(x, y + cut - 1,
                     x + cut, y, TFT_WHITE);

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_RED);
        tft.drawString("STOP", x + width / 2, y + height / 2, font);
        tft.setTextDatum(TL_DATUM);
        tft.setTextPadding(0);
    }

    void drawStartSquare(
        int x,
        int y,
        int width,
        int height,
        int font,
        const char *label = "START")
    {
        tft.fillRect(x, y, width, height, COL_DARKGREEN);
        tft.drawRect(x, y, width, height, TFT_WHITE);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, COL_DARKGREEN);
        tft.drawString(label, x + width / 2, y + height / 2, font);
        tft.setTextDatum(TL_DATUM);
        tft.setTextPadding(0);
    }
    constexpr int VU_X = 22;
    constexpr int VU_LEFT_Y = 104;
    constexpr int VU_RIGHT_Y = 139;
    constexpr int VU_SCALE_Y = 119;
    constexpr int VU_DB_MIN_TENTHS = -300;
    constexpr int VU_DB_MAX_TENTHS = 100;
    constexpr int VU_BAR_PIXELS = 272;

    constexpr char RECORD_HELP_TITLE[] = "RECORD LEVEL";
    const char *const RECORD_HELP_TEXT[] =
        {
            "Set the deck to SOURCE.",
            "",
            "Start recording and switch the",
            "deck monitor to TAPE.",
            "",
            "Adjust the deck REC LEVEL controls",
            "until Left and Right are both",
            "-10.0 dB.",
            "",
            "MATCHED tolerance: +/-0.6 dB."};
    constexpr char BIAS_HELP_TITLE[] = "BIAS ADJUST";
    const char *const BIAS_HELP_TEXT[] =
        {
            "TapeLAB alternates 440 Hz",
            "and 10 kHz continuously.",
            "",
            "All four measured levels must",
            "remain between -20 and -10 dB.",
            "",
            "Adjust the deck BIAS control",
            "until the 10 kHz level matches",
            "the 440 Hz level on both channels.",
            "Target difference: 0.0 dB.",
            "MATCHED tolerance: +/-0.5 dB.",
            "Three matching cycles are required."};
    constexpr char DECK_TYPE_HELP_TITLE[] = "DECK TYPE";
    const char *const DECK_TYPE_HELP_TEXT[] =
        {
            "Select 3 HEAD for live",
            "SOURCE/TAPE monitoring.",
            "",
            "Select 2 HEAD when recording",
            "and playback use the same head.",
            "",
            "The 2 HEAD procedure requires",
            "recording, rewinding and replaying",
            "each test sequence."};
    constexpr char TWO_HEAD_LEVEL_HELP_TITLE[] = "RECORD LEVEL";
    const char *const TWO_HEAD_LEVEL_HELP_TEXT[] =
        {
            "Start the deck recording,",
            "then start the 440 Hz test.",
            "",
            "After the 20 second tone,",
            "stop and rewind the tape.",
            "",
            "Play the test and use ANALYZE.",
            "",
            "Adjust REC LEVEL and repeat",
            "until both channels are matched.",
            "",
            "MATCHED tolerance: +/-1.0 dB."};
    constexpr char TWO_HEAD_BIAS_HELP_TITLE[] = "BIAS ADJUST";
    const char *const TWO_HEAD_BIAS_HELP_TEXT[] =
        {
            "SYNC uses 2 kHz and is found",
            "only by its frequency.",
            "",
            "The 440 Hz and 10 kHz test",
            "levels must be -20 to -10 dB.",
            "",
            "TapeLAB takes the median of",
            "five stable readings per tone.",
            "",
            "Record the complete sequence:",
            "SYNC, 440, SYNC, 10k, SYNC.",
            "Stop and rewind the tape.",
            "Start playback and use ANALYZE.",
    };

    constexpr int DECK_BUTTON_HEIGHT = 34;
    constexpr int DECK_3_BUTTON_Y = 76;
    constexpr int DECK_2_BUTTON_Y = 133;
    constexpr int TEST_BUTTON_Y = 60;
    constexpr int TEST_BUTTON_HEIGHT = 32;
    constexpr int TEST_BUTTON_GAP = 14;
    constexpr float SUMMARY_WARNING_MARGIN_DB = 0.1f;
    constexpr uint8_t CALIBRATION_MENU_FONT = 4;
    constexpr int CALIBRATION_MENU_ITEM_X = 8;
    constexpr int CALIBRATION_MENU_ITEM_WIDTH = 304;
    constexpr int DECK_BUTTON_X = CALIBRATION_MENU_ITEM_X;
    constexpr int DECK_BUTTON_WIDTH = CALIBRATION_MENU_ITEM_WIDTH;
    constexpr int RECORD_HEAD_ICON_SIZE = 72;
    constexpr int RECORD_HEAD_ICON_X[] = {17, 124, 231};
    constexpr int RECORD_HEAD_ICON_Y = 82;
    constexpr int MEMORY_ICON_X = RECORD_HEAD_ICON_X[2];
    constexpr int MEMORY_ICON_Y = RECORD_HEAD_ICON_Y;
    constexpr int MEMORY_ICON_SIZE = RECORD_HEAD_ICON_SIZE;
    constexpr int RECORD_ACTION_ICON_SIZE = 64;
    constexpr int RECORD_ACTION_ICON_X[] = {20, 92, 164, 236};
    constexpr int RECORD_ACTION_FIRST_ROW_Y = 36;
    constexpr int RECORD_ACTION_SECOND_ROW_Y = 128;
    // The 3 HEAD menu keeps a single icon on the second row, centred.
    constexpr int RECORD_ACTION_WORKFLOW_X =
        (LCD_WIDTH - RECORD_ACTION_ICON_SIZE) / 2;
    constexpr int RECORD_ICON_LABEL_OFFSET_Y = 3;
    constexpr int MENU_TOUCH_OFFSET_Y = 3;
    const uint16_t CALIBRATION_MENU_SELECTION_BACKGROUND = tft.color565(0, 20, 40);

    constexpr uint16_t BUTTON_IDLE = TFT_DARKCYAN;
    const uint16_t BUTTON_RUNNING = tft.color565(40, 0, 0);

    bool contains(uint16_t x, uint16_t y, int left, int top, int width, int height)
    {
        return x >= left && x < left + width &&
               y >= top && y < top + height;
    }

    bool setGeneratorOutput(uint32_t frequencyHz, float nominalLevelDb)
    {
        if (!isfinite(nominalLevelDb) ||
            !pot.setLevelDb(nominalLevelDb, GENERATOR_MAX_LEVEL_DB))
        {
            ad9833.disable();
            return false;
        }

        ad9833.setFrequency(frequencyHz);
        return true;
    }

    uint32_t tapeResponseSettleMs(uint8_t bandIndex)
    {
        return bandIndex == 0
                   ? TAPE_RESPONSE_FIRST_BAND_SETTLE_MS
                   : TAPE_RESPONSE_SETTLE_MS;
    }

    uint32_t tapeResponseMeasureMs(uint8_t bandIndex)
    {
        return bandIndex <= 1
                   ? TAPE_RESPONSE_LOW_BAND_MEASURE_MS
                   : TAPE_RESPONSE_MEASURE_MS;
    }

    bool containsMenuItem(uint16_t x, uint16_t y,
                          int left, int top, int width, int height)
    {
        return contains(x, y, left, top - MENU_TOUCH_OFFSET_Y,
                        width, height + 3);
    }

    int16_t toTenths(float value)
    {
        return static_cast<int16_t>(value * 10.0f + (value >= 0.0f ? 0.5f : -0.5f));
    }

    int dbToVuPixels(float levelDb, bool valid)
    {
        if (!valid)
            return 0;

        return map(constrain(static_cast<int>(levelDb * 10.0f), VU_DB_MIN_TENTHS, VU_DB_MAX_TENTHS),
                   VU_DB_MIN_TENTHS, VU_DB_MAX_TENTHS, 0, VU_BAR_PIXELS);
    }

    void formatDb(char *buffer, size_t size, int16_t tenths, bool valid)
    {
        if (!valid)
        {
            snprintf(buffer, size, "---");
            return;
        }

        const int16_t absoluteTenths = tenths < 0 ? -tenths : tenths;
        snprintf(buffer, size, "%s%d.%d dB", tenths < 0 ? "-" : "+",
                 absoluteTenths / 10, absoluteTenths % 10);
    }

    float rmsToDb(float rms, float reference)
    {
        return 20.0f * log10f((rms + 0.0001f) / reference);
    }

    void appendBiasSample(
        float leftSamples[],
        float rightSamples[],
        uint8_t &sampleCount,
        uint8_t &sampleIndex,
        float leftValue,
        float rightValue)
    {
        leftSamples[sampleIndex] = leftValue;
        rightSamples[sampleIndex] = rightValue;
        sampleIndex = (sampleIndex + 1) % BIAS_AVERAGE_SAMPLE_COUNT;
        if (sampleCount < BIAS_AVERAGE_SAMPLE_COUNT)
            ++sampleCount;
    }

    float averageBiasSamples(const float samples[], uint8_t sampleCount)
    {
        float sum = 0.0f;
        for (uint8_t i = 0; i < sampleCount; ++i)
            sum += samples[i];
        return sampleCount ? sum / sampleCount : 0.0f;
    }

    float medianBiasSamples(const float samples[], uint8_t sampleCount)
    {
        float sorted[TWO_HEAD_BIAS_MIN_SAMPLES];
        for (uint8_t i = 0; i < sampleCount; ++i)
            sorted[i] = samples[i];
        for (uint8_t i = 1; i < sampleCount; ++i)
        {
            const float value = sorted[i];
            uint8_t position = i;
            while (position > 0 && sorted[position - 1] > value)
            {
                sorted[position] = sorted[position - 1];
                --position;
            }
            sorted[position] = value;
        }
        return sampleCount ? sorted[sampleCount / 2] : 0.0f;
    }
}

void Record::begin()
{
    GeneratorCalibrationStorage stored;
    if (storage.loadGeneratorCalibration(stored) !=
        GeneratorCalibrationBootStatus::Ok)
    {
        tapeResponseCalibration = {};
        for (uint8_t i = 0; i < TAPE_RESPONSE_BAND_COUNT; ++i)
            tapeResponseCalibration.generatorCorrectionDb[i] = 0.0f;
        generatorCalibrationLoaded = false;
        activeGeneratorCalibrationMaxVerifyErrorDb = NAN;
        activeGeneratorCalibrationVerifyRating = TapeResponseVerifyRating::None;
        return;
    }

    tapeResponseCalibration = {};
    tapeResponseCalibration.valid = true;
    for (uint8_t i = 0; i < TAPE_RESPONSE_BAND_COUNT; ++i)
        tapeResponseCalibration.generatorCorrectionDb[i] = stored.correctionDb[i];
    activeGeneratorCalibrationMaxVerifyErrorDb = stored.maxVerifyErrorDb;
    activeGeneratorCalibrationVerifyRating =
        stored.verifyGrade == GENERATOR_CAL_GRADE_EXCELLENT
            ? TapeResponseVerifyRating::Excellent
            : TapeResponseVerifyRating::Ok;
    generatorCalibrationLoaded = true;
}

bool Record::setCalibratedTestTone(
    uint32_t frequencyHz,
    float nominalLevelDb)
{
    const float correctedLevelDb =
        nominalLevelDb + getGeneratorCalibrationCorrectionDb(frequencyHz);
    if (!setGeneratorOutput(frequencyHz, correctedLevelDb))
        return false;

    ad9833.enable();
    return true;
}

void Record::stopCalibratedTestTone()
{
    stopGenerator();
}

uint32_t Record::tapeResponsePointSettleMs(bool firstPoint) const
{
    return tapeResponseSettleMs(firstPoint ? 0 : 2);
}

uint32_t Record::tapeResponsePointMeasureMs() const
{
    return tapeResponseMeasureMs(2);
}

float Record::rmsToReferenceDb(float rms) const
{
    const float reference = setAudio.rmsReference();
    if (!isfinite(rms) || rms < 0.0f ||
        !isfinite(reference) || reference <= 0.0f)
        return NAN;

    return rmsToDb(rms, reference);
}

void Record::run()
{
    // Entering any RECORD screen takes ownership of the signal generator.
    // Stop a tone left active by another application and wait for the RECORD
    // workflow to request its own output explicitly.
    stopGenerator();
    sine.stopForRecord();
    const bool returnToSystemSettingsOnBack = openAutoCalOnRun;
    resetSession();
    if (openAutoCalOnRun)
    {
        deckType = DeckType::ThreeHead;
        highlightedDeckType = DeckType::ThreeHead;
        highlightedTestType = TestType::TapeResponse;
        state = CalibrationState::ThreeHeadLoopbackReady;
        openAutoCalOnRun = false;
    }
    if (openWorkflowOnRun)
    {
        deckType = DeckType::ThreeHead;
        highlightedDeckType = DeckType::ThreeHead;
        openWorkflowOnRun = false;
        startThreeHeadWorkflow();
    }
    drawScreen();

    while (touch.pressed())
        delay(5);

    while (true)
    {
        buzzer.update();
        if (state == CalibrationState::ThreeHeadLoopbackPreflightNoise ||
            state == CalibrationState::ThreeHeadLoopbackPreflightSignal ||
            state == CalibrationState::ThreeHeadLoopbackPreflightPassed)
        {
            updateLoopbackPreflight();
        }
        else if (state == CalibrationState::ThreeHeadLoopbackRunning ||
                 state == CalibrationState::ThreeHeadLoopbackVerifying)
        {
            updateLoopbackCalibration();
            if (state == CalibrationState::ThreeHeadLoopbackRunning ||
                state == CalibrationState::ThreeHeadLoopbackVerifying)
                drawLoopbackRunningDynamic();
        }
        else if (state == CalibrationState::ThreeHeadResponseRunning)
        {
            updateTapeResponse();
            if (state == CalibrationState::ThreeHeadResponseRunning)
                drawTapeResponseRunningDynamic();
        }
        else if (state == CalibrationState::TwoHeadResponseRecording)
        {
            updateTwoHeadTapeResponseRecording();
            if (state == CalibrationState::TwoHeadResponseRecording)
                drawTwoHeadTapeResponseDynamic();
        }
        else if (state == CalibrationState::TwoHeadResponseWaitingSync ||
                 state == CalibrationState::TwoHeadResponseAnalyzing)
        {
            updateTwoHeadTapeResponseAnalysis();
            if (state == CalibrationState::TwoHeadResponseWaitingSync &&
                !twoHeadEqSyncFound)
                drawTwoHeadTapeResponseDynamic();
        }
        else if (isTwoHeadState() && !isTapeResponseState())
        {
            updateTwoHeadState();
            drawTwoHeadDynamic();
            drawActionButton();
            drawStatus();
        }
        else if (state == CalibrationState::ThreeHeadLevelRunning)
        {
            measureLevels(true, true);
            updateRecordLevel();
            drawRecordMeasurements();
            drawActionButton();
            drawStatus();
        }
        else if (isGeneratorRunning())
        {
            processBiasMeasurement();
            drawBiasMeasurements();
            drawActiveFrequency();
            drawThreeHeadCalibrationStatus();
            drawActionButton();
            drawStatus();
        }

        display.updateClock();

        if (!touch.pressed())
        {
            delay(5);
            continue;
        }

        const uint16_t x = touch.getX();
        const uint16_t y = touch.getY();

        if (y >= FOOTER_Y && state == CalibrationState::SelectDeckType)
        {
            while (touch.pressed())
                delay(5);
            break;
        }

        if (y >= FOOTER_Y && state == CalibrationState::SelectTest)
        {
            while (touch.pressed())
                delay(5);
            stopGenerator();
            state = CalibrationState::SelectDeckType;
            highlightedTestType = TestType::None;
            selectedRecordHeadIndex = 0;
            selectedRecordActionIndex = 0;
            drawScreen();
            continue;
        }

        if (state == CalibrationState::ThreeHeadWorkflowIntro ||
            state == CalibrationState::ThreeHeadWorkflowSummary)
        {
            handleWorkflowTouch(x, y);
            continue;
        }

        if (y >= FOOTER_Y && tapeEqFlowActive && isTapeResponseState() &&
            state != CalibrationState::ThreeHeadLoopbackRunning &&
            state != CalibrationState::ThreeHeadLoopbackVerifying &&
            state != CalibrationState::ThreeHeadResponseRunning &&
            state != CalibrationState::TwoHeadResponseRecording &&
            state != CalibrationState::TwoHeadResponseWaitingSync &&
            state != CalibrationState::TwoHeadResponseAnalyzing)
        {
            while (touch.pressed())
                delay(5);
            promptAndSaveTapeEq();
            stopGenerator();
            resetTapeResponse();
            tapeEqFlowActive = false;
            state = CalibrationState::SelectTest;
            selectedRecordActionIndex = 0;
            drawScreen();
            continue;
        }

        const char *const helpTitle = isLevelState() ? "TEST: RECORD LEVEL" : "TEST: BIAS CAL";
        const int helpX = TEXT_X + tft.textWidth(helpTitle, ITEM_FONT) + 4;

        if (state == CalibrationState::SelectDeckType)
        {
            if (contains(x, y, MEMORY_ICON_X - 3, MEMORY_ICON_Y - 2,
                         MEMORY_ICON_SIZE + 6, MEMORY_ICON_SIZE + 18))
            {
                while (touch.pressed())
                    delay(5);
                drawGraphMemoryList();
                drawScreen();
                continue;
            }
            const int selectedHead = handleRecordIconTouch(x, y, true);
            if (selectedHead < 0)
                continue;

            while (touch.pressed())
            {
                display.updateClock();
                delay(5);
            }
            if (selectedHead == 0)
                continue;

            deckType = selectedHead == 1 ? DeckType::TwoHead : DeckType::ThreeHead;
            highlightedDeckType = deckType;
            resetFinalResults();
            stopGenerator();
            state = CalibrationState::SelectTest;
            highlightedTestType = TestType::None;
            selectedRecordActionIndex = 0;
            status = Status::Ready;
            drawScreen();
            continue;
        }

        if (state == CalibrationState::SelectTest)
        {
            const int selectedAction = handleRecordIconTouch(x, y, false);
            if (selectedAction < 0)
                continue;

            while (touch.pressed())
            {
                display.updateClock();
                delay(5);
            }
            if (selectedAction == 0)
                continue;

            if (selectedAction == 3 && deckType == DeckType::ThreeHead)
            {
                startTapeTest();
                continue;
            }

            if (selectedAction == 5 && deckType == DeckType::ThreeHead)
            {
                startThreeHeadWorkflow();
                continue;
            }

            if (selectedAction == 4)
            {
                dolbyCheck.run(
                    deckType == DeckType::ThreeHead
                        ? DolbyCheck::DeckMode::ThreeHead
                        : DolbyCheck::DeckMode::TwoHead);
                highlightedTestType = TestType::None;
                selectedRecordActionIndex = 0;
                drawScreen();
                continue;
            }

            if (deckType == DeckType::TwoHead && selectedAction == 3)
            {
                atc.run();
                highlightedTestType = TestType::None;
                selectedRecordActionIndex = 0;
                drawScreen();
                continue;
            }

            if (deckType == DeckType::TwoHead && selectedAction == 5)
            {
                startTapeTest();
                continue;
            }

            highlightedTestType = selectedAction == 1
                                      ? TestType::RecordLevel
                                  : selectedAction == 2
                                      ? TestType::BiasCalibration
                                      : TestType::TapeResponse;
            enterSelectedTest();
            drawScreen();
            continue;
        }

        if (isTapeResponseState())
        {
            if (!handleTapeResponseTouch(
                    x, y, returnToSystemSettingsOnBack))
                break;
            continue;
        }

        // Help is checked first so it always takes priority over other controls.
        if (!isTapeResponseState() && help.hitTest(x, y, helpX, TEST_Y))
        {
            showHelp();
            continue;
        }

        if (!contains(x, y, START_STOP_X, START_STOP_Y,
                      START_STOP_WIDTH, START_STOP_HEIGHT))
        {
            while (touch.pressed())
            {
                display.updateClock();
                delay(5);
            }
            if (!isGeneratorRunning())
            {
                stopGenerator();
                returnToTestSelection();
                drawScreen();
            }
            continue;
        }

        if (contains(x, y, START_STOP_X, START_STOP_Y, START_STOP_WIDTH, START_STOP_HEIGHT))
        {
            drawActionButton(true);
            while (touch.pressed())
            {
                display.updateClock();
                delay(5);
            }

            if (isTwoHeadState())
            {
                handleTwoHeadAction();
                drawScreen();
                continue;
            }

            if (state == CalibrationState::ThreeHeadLevelReady)
            {
                startThreeHeadLevel();
            }
            else if (state == CalibrationState::ThreeHeadLevelRunning)
            {
                stopThreeHeadLevel();
            }
            else if (state == CalibrationState::ThreeHeadLevelMatched)
            {
                if (workflowActive)
                    advanceThreeHeadWorkflow();
                else
                    startThreeHeadLevel();
            }
            else if (state == CalibrationState::ThreeHeadBiasReady)
            {
                startBias();
            }
            else if (state == CalibrationState::ThreeHeadBiasRunning)
            {
                stopBias();
            }
            else if (state == CalibrationState::ThreeHeadBiasMatched)
            {
                if (workflowActive)
                {
                    advanceThreeHeadWorkflow();
                }
                else
                {
                    resetBiasTest();
                    state = CalibrationState::ThreeHeadBiasReady;
                }
            }
            else if (state == CalibrationState::ThreeHeadResponseResult)
            {
                promptAndSaveTapeEq();
                if (workflowActive)
                {
                    advanceThreeHeadWorkflow();
                }
                else
                {
                    resetTapeResponse();
                    state = CalibrationState::ThreeHeadResponseReady;
                }
            }
            drawScreen();
            continue;
        }

        while (touch.pressed())
        {
            display.updateClock();
            delay(5);
        }
        break;
    }

    stopGenerator();
    resetSession();
}

void Record::openThreeHeadTapeEqAutoCal()
{
    openAutoCalOnRun = true;
    run();
}

void Record::openThreeHeadCalibrationWorkflow()
{
    openWorkflowOnRun = true;
    run();
}

void Record::invalidateGeneratorCalibration()
{
    tapeResponseCalibration = {};
    for (uint8_t i = 0; i < TAPE_RESPONSE_BAND_COUNT; ++i)
        tapeResponseCalibration.generatorCorrectionDb[i] = 0.0f;
    generatorCalibrationLoaded = false;
    activeGeneratorCalibrationMaxVerifyErrorDb = NAN;
    activeGeneratorCalibrationVerifyRating = TapeResponseVerifyRating::None;
}

void Record::resetSession()
{
    stopGenerator();
    state = CalibrationState::SelectDeckType;
    deckType = DeckType::None;
    highlightedDeckType = DeckType::None;
    highlightedTestType = TestType::None;
    selectedRecordHeadIndex = 0;
    selectedRecordActionIndex = 0;
    tapeEqFlowActive = false;
    workflowActive = false;
    workflowStep = ThreeHeadWorkflowStep::Inactive;
    workflowRetestOnly = false;
    workflowResult = {};
    calibrationCompleted = false;
    session = {};
    resetFinalResults();
    status = Status::Ready;
    statusDisplayed = false;
    resetMeasurements();
    resetBiasMatchCounters();
    resetBiasAverages();
    resetTapeResponse();
    resetTapeResponseCalibration();
    biasCompletedCycles = 0;
    biasHighFrequency = false;
    biasFrequencySetMs = 0;
    resetTwoHeadAnalysis();
}

void Record::resetFinalResults()
{
    finalRecordLevelLeftDb = 0.0f;
    finalRecordLevelRightDb = 0.0f;
    finalRecordLevelDifferenceDb = 0.0f;
    finalBiasDiffLeftDb = 0.0f;
    finalBiasDiffRightDb = 0.0f;
    finalRecordLevelValid = false;
    finalBiasValid = false;
    finalDeckType = DeckType::None;
}

void Record::captureFinalRecordLevel()
{
    if (!leftSignalValid || !rightSignalValid)
        return;

    finalRecordLevelLeftDb = leftLevelDb;
    finalRecordLevelRightDb = rightLevelDb;
    finalRecordLevelDifferenceDb = fabsf(leftLevelDb - rightLevelDb);
    finalRecordLevelValid = true;
    finalDeckType = deckType;
}

void Record::captureFinalBias()
{
    if (!bias440Valid || !bias10kValid)
        return;

    finalBiasDiffLeftDb = bias10kLeftDb - bias440LeftDb;
    finalBiasDiffRightDb = bias10kRightDb - bias440RightDb;
    finalBiasValid = true;
    finalDeckType = deckType;
}

void Record::captureFinalThreeHeadBias()
{
    if (!bias440Valid || !bias10kValid)
        return;

    finalRecordLevelLeftDb = bias440LeftDb;
    finalRecordLevelRightDb = bias440RightDb;
    finalRecordLevelDifferenceDb = fabsf(bias440LeftDb - bias440RightDb);
    finalRecordLevelValid = true;
    finalBiasDiffLeftDb = bias10kLeftDb - bias440LeftDb;
    finalBiasDiffRightDb = bias10kRightDb - bias440RightDb;
    finalBiasValid = true;
    finalDeckType = deckType;
}

void Record::resetMeasurements()
{
    leftSignalValid = false;
    rightSignalValid = false;
    smoothingInitialized = false;
    leftLevelDb = 0.0f;
    rightLevelDb = 0.0f;
    smoothedLeftDb = 0.0f;
    smoothedRightDb = 0.0f;
    lastLeftRms = 0.0f;
    lastRightRms = 0.0f;
    displayedLeftTenths = -32768;
    displayedRightTenths = -32768;
    displayedDiffLeftTenths = -32768;
    displayedDiffRightTenths = -32768;
    displayedLeftVuPixels = -1;
    displayedRightVuPixels = -1;
    displayedLeftValid = false;
    displayedRightValid = false;
    displayedBias440LeftTenths = -32768;
    displayedBias440RightTenths = -32768;
    displayedBias10kLeftTenths = -32768;
    displayedBias10kRightTenths = -32768;
    displayedBiasDiffLeftTenths = -32768;
    displayedBiasDiffRightTenths = -32768;
    displayedBias440LeftValid = false;
    displayedBias440RightValid = false;
    displayedBias10kLeftValid = false;
    displayedBias10kRightValid = false;
    displayedBiasDiffLeftValid = false;
    displayedBiasDiffRightValid = false;
    displayedActiveFrequency = -1;
    displayedLevelStatus = -1;
    displayedBiasStatus = -1;
    displayedStableCycles = -1;
}

void Record::resetBiasMatchCounters()
{
    biasMatchEntryCount = 0;
    biasMatchExitCount = 0;
}

void Record::resetBiasAverages()
{
    bias440Valid = false;
    bias10kValid = false;
    biasTestLevelValid = false;
    bias440LeftDb = 0.0f;
    bias440RightDb = 0.0f;
    bias10kLeftDb = 0.0f;
    bias10kRightDb = 0.0f;
    bias440SampleCount = 0;
    bias10kSampleCount = 0;
    bias440SampleIndex = 0;
    bias10kSampleIndex = 0;
    for (uint8_t i = 0; i < BIAS_AVERAGE_SAMPLE_COUNT; ++i)
    {
        bias440LeftRms[i] = 0.0f;
        bias440RightRms[i] = 0.0f;
        bias10kLeftRms[i] = 0.0f;
        bias10kRightRms[i] = 0.0f;
    }
}

void Record::resetLevelTest()
{
    stopGenerator();
    resetMeasurements();
    recordMatchEntryCount = 0;
    recordMatchExitCount = 0;
    resetTwoHeadAnalysis();
    status = Status::Ready;
    statusDisplayed = false;
}

void Record::resetBiasTest()
{
    stopGenerator();
    resetMeasurements();
    resetBiasMatchCounters();
    resetBiasAverages();
    biasCompletedCycles = 0;
    biasHighFrequency = false;
    resetTwoHeadAnalysis();
    status = Status::Ready;
    statusDisplayed = false;
}

bool Record::isGeneratorRunning() const
{
    return state == CalibrationState::ThreeHeadLevelRunning ||
           state == CalibrationState::ThreeHeadBiasRunning ||
           state == CalibrationState::ThreeHeadLoopbackRunning ||
           state == CalibrationState::ThreeHeadLoopbackVerifying ||
           state == CalibrationState::ThreeHeadResponseRunning ||
           state == CalibrationState::TwoHeadLevelRecording ||
           state == CalibrationState::TwoHeadBiasRecording ||
           state == CalibrationState::TwoHeadResponseRecording;
}

bool Record::isLevelState() const
{
    switch (state)
    {
    case CalibrationState::ThreeHeadLevelReady:
    case CalibrationState::ThreeHeadLevelRunning:
    case CalibrationState::ThreeHeadLevelMatched:
    case CalibrationState::TwoHeadLevelReadyToRecord:
    case CalibrationState::TwoHeadLevelRecording:
    case CalibrationState::TwoHeadLevelRewind:
    case CalibrationState::TwoHeadLevelReadyToAnalyze:
    case CalibrationState::TwoHeadLevelAnalyzing:
    case CalibrationState::TwoHeadLevelResult:
    case CalibrationState::TwoHeadLevelMatched:
        return true;
    default:
        return false;
    }
}

bool Record::isBiasState() const
{
    switch (state)
    {
    case CalibrationState::ThreeHeadBiasReady:
    case CalibrationState::ThreeHeadBiasRunning:
    case CalibrationState::ThreeHeadBiasMatched:
    case CalibrationState::TwoHeadBiasReadyToRecord:
    case CalibrationState::TwoHeadBiasRecording:
    case CalibrationState::TwoHeadBiasRewind:
    case CalibrationState::TwoHeadBiasReadyToAnalyze:
    case CalibrationState::TwoHeadBiasAnalyzing:
    case CalibrationState::TwoHeadBiasResult:
    case CalibrationState::TwoHeadBiasMatched:
        return true;
    default:
        return false;
    }
}

bool Record::isTapeResponseState() const
{
    switch (state)
    {
    case CalibrationState::ThreeHeadLoopbackReady:
    case CalibrationState::ThreeHeadLoopbackPreflightNoise:
    case CalibrationState::ThreeHeadLoopbackPreflightSignal:
    case CalibrationState::ThreeHeadLoopbackPreflightPassed:
    case CalibrationState::ThreeHeadLoopbackPreflightFailed:
    case CalibrationState::ThreeHeadLoopbackSignalLost:
    case CalibrationState::ThreeHeadLoopbackRunning:
    case CalibrationState::ThreeHeadLoopbackReview:
    case CalibrationState::ThreeHeadLoopbackVerifying:
    case CalibrationState::ThreeHeadLoopbackSavePrompt:
    case CalibrationState::ThreeHeadLoopbackResult:
    case CalibrationState::ThreeHeadResponseCalibrationRequired:
    case CalibrationState::ThreeHeadResponseReady:
    case CalibrationState::ThreeHeadResponseRunning:
    case CalibrationState::ThreeHeadResponseResult:
    case CalibrationState::TwoHeadResponseReadyToRecord:
    case CalibrationState::TwoHeadResponseRecording:
    case CalibrationState::TwoHeadResponseRewind:
    case CalibrationState::TwoHeadResponseReadyToAnalyze:
    case CalibrationState::TwoHeadResponseWaitingSync:
    case CalibrationState::TwoHeadResponseAnalyzing:
    case CalibrationState::TwoHeadResponseResult:
        return true;
    default:
        return false;
    }
}

bool Record::isTwoHeadState() const
{
    switch (state)
    {
    case CalibrationState::TwoHeadLevelReadyToRecord:
    case CalibrationState::TwoHeadLevelRecording:
    case CalibrationState::TwoHeadLevelRewind:
    case CalibrationState::TwoHeadLevelReadyToAnalyze:
    case CalibrationState::TwoHeadLevelAnalyzing:
    case CalibrationState::TwoHeadLevelResult:
    case CalibrationState::TwoHeadLevelMatched:
    case CalibrationState::TwoHeadBiasReadyToRecord:
    case CalibrationState::TwoHeadBiasRecording:
    case CalibrationState::TwoHeadBiasRewind:
    case CalibrationState::TwoHeadBiasReadyToAnalyze:
    case CalibrationState::TwoHeadBiasAnalyzing:
    case CalibrationState::TwoHeadBiasResult:
    case CalibrationState::TwoHeadBiasMatched:
    case CalibrationState::TwoHeadResponseReadyToRecord:
    case CalibrationState::TwoHeadResponseRecording:
    case CalibrationState::TwoHeadResponseRewind:
    case CalibrationState::TwoHeadResponseReadyToAnalyze:
    case CalibrationState::TwoHeadResponseWaitingSync:
    case CalibrationState::TwoHeadResponseAnalyzing:
    case CalibrationState::TwoHeadResponseResult:
        return true;
    default:
        return false;
    }
}

bool Record::isTwoHeadBiasState() const
{
    switch (state)
    {
    case CalibrationState::TwoHeadBiasReadyToRecord:
    case CalibrationState::TwoHeadBiasRecording:
    case CalibrationState::TwoHeadBiasRewind:
    case CalibrationState::TwoHeadBiasReadyToAnalyze:
    case CalibrationState::TwoHeadBiasAnalyzing:
    case CalibrationState::TwoHeadBiasResult:
    case CalibrationState::TwoHeadBiasMatched:
        return true;
    default:
        return false;
    }
}

bool Record::isThreeHeadState() const
{
    switch (state)
    {
    case CalibrationState::ThreeHeadLevelReady:
    case CalibrationState::ThreeHeadLevelRunning:
    case CalibrationState::ThreeHeadLevelMatched:
    case CalibrationState::ThreeHeadBiasReady:
    case CalibrationState::ThreeHeadBiasRunning:
    case CalibrationState::ThreeHeadBiasMatched:
    case CalibrationState::ThreeHeadLoopbackReady:
    case CalibrationState::ThreeHeadLoopbackPreflightNoise:
    case CalibrationState::ThreeHeadLoopbackPreflightSignal:
    case CalibrationState::ThreeHeadLoopbackPreflightPassed:
    case CalibrationState::ThreeHeadLoopbackPreflightFailed:
    case CalibrationState::ThreeHeadLoopbackSignalLost:
    case CalibrationState::ThreeHeadLoopbackRunning:
    case CalibrationState::ThreeHeadLoopbackVerifying:
    case CalibrationState::ThreeHeadLoopbackSavePrompt:
    case CalibrationState::ThreeHeadLoopbackResult:
    case CalibrationState::ThreeHeadResponseCalibrationRequired:
    case CalibrationState::ThreeHeadResponseReady:
    case CalibrationState::ThreeHeadResponseRunning:
    case CalibrationState::ThreeHeadResponseResult:
        return true;
    default:
        return false;
    }
}

void Record::drawScreen()
{
    // The application screen may have been cleared by Help or by a stage change.
    displayedActionButtonMode = -1;

    if (state == CalibrationState::ThreeHeadWorkflowIntro)
    {
        if (workflowStep == ThreeHeadWorkflowStep::CalibrationRequired)
            drawWorkflowCalibrationRequiredScreen();
        else
            drawWorkflowIntroScreen();
        return;
    }

    if (state == CalibrationState::ThreeHeadWorkflowSummary)
    {
        drawWorkflowCompleteScreen();
        return;
    }

    if (state == CalibrationState::SelectDeckType)
    {
        drawDeckTypeScreen();
        return;
    }

    if (state == CalibrationState::SelectTest)
    {
        drawTestScreen();
        return;
    }

    if (isTapeResponseState())
    {
        drawTapeResponseScreen();
        return;
    }

    if (isTwoHeadState())
    {
        drawTwoHeadScreen();
        return;
    }

    display.openApp(
        isBiasState() ? "RECORD BIAS CALIBRATION" : "RECORD LEVEL CALIBRATION",
        "READY");
    tft.setTextDatum(TL_DATUM);
    if (isLevelState())
    {
        drawRecordStaticContent();
        drawRecordMeasurements(true);
    }
    else
    {
        drawBiasStaticContent();
        drawBiasMeasurements(true);
        drawActiveFrequency(true);
        drawThreeHeadCalibrationStatus(true);
    }
    drawActionButton();
    drawStatus(true);
}

void Record::drawRecordStaticContent()
{
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("TEST: RECORD LEVEL", TEXT_X, TEST_Y, ITEM_FONT);
    tft.drawString("FREQ: 440 Hz", TEXT_X, RECORD_FREQUENCY_Y, ITEM_FONT);
    tft.drawString("GEN: -10.0 dB", TEXT_X, GENERATOR_Y, ITEM_FONT);
    tft.drawString("LEFT:", RECORD_LEFT_LABEL_X, RECORD_LEFT_Y, ITEM_FONT);
    tft.drawString("RIGHT:", RECORD_LEFT_LABEL_X, RECORD_RIGHT_Y, ITEM_FONT);
    tft.drawString("DIFF L:", RECORD_DIFF_LABEL_X, RECORD_LEFT_Y, ITEM_FONT);
    tft.drawString("DIFF R:", RECORD_DIFF_LABEL_X, RECORD_RIGHT_Y, ITEM_FONT);

    tft.setTextColor(TFT_DARKGREY, COL_BG);
    tft.drawString("-30-------20---------10---6--3--", 20, VU_SCALE_Y, ITEM_FONT);
    tft.setTextColor(COL_TEXT);
    tft.drawString("-30      -20        -10  -6 -3  ", 19, VU_SCALE_Y, ITEM_FONT);
    tft.setTextColor(TFT_MAROON, COL_BG);
    tft.drawString("0-+3-+5 dB", 230, VU_SCALE_Y, ITEM_FONT);
    tft.setTextColor(TFT_RED);
    tft.drawString("0 +3 +5 dB", 229, VU_SCALE_Y, ITEM_FONT);

    const int helpX = TEXT_X + tft.textWidth("TEST: RECORD LEVEL", ITEM_FONT) + 4;
    help.drawButton(helpX, TEST_Y);
}

void Record::drawBiasStaticContent()
{
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("TEST: BIAS CAL", TEXT_X, TEST_Y, ITEM_FONT);
    tft.setTextColor(
        tapeResponseCalibration.valid ? TFT_GREEN : TFT_YELLOW, COL_BG);
    tft.drawString(
        tapeResponseCalibration.valid ? "GEN CAL: ON" : "GEN CAL: OFF",
        155, TEST_Y, ITEM_FONT);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("LEVEL:", TEXT_X, THREE_HEAD_LEVEL_Y, ITEM_FONT);
    tft.drawString("BIAS:", TEXT_X, THREE_HEAD_BIAS_Y, ITEM_FONT);
    tft.drawString("STABLE:", TEXT_X, THREE_HEAD_STABLE_Y, ITEM_FONT);
    tft.drawString("440 L:", BIAS_LEFT_LABEL_X, BIAS_440_Y, ITEM_FONT);
    tft.drawString("10k L:", BIAS_LEFT_LABEL_X, BIAS_10K_Y, ITEM_FONT);
    tft.drawString("DIFF L:", BIAS_LEFT_LABEL_X, BIAS_DIFF_Y, ITEM_FONT);
    tft.drawString("R:", BIAS_RIGHT_LABEL_X, BIAS_440_Y, ITEM_FONT);
    tft.drawString("R:", BIAS_RIGHT_LABEL_X, BIAS_10K_Y, ITEM_FONT);
    tft.drawString("R:", BIAS_RIGHT_LABEL_X, BIAS_DIFF_Y, ITEM_FONT);

    tft.setTextColor(TFT_DARKGREY, COL_BG);
    tft.drawString("-30-------20---------10---6--3--", 20, VU_SCALE_Y, ITEM_FONT);
    tft.setTextColor(COL_TEXT);
    tft.drawString("-30      -20        -10  -6 -3  ", 19, VU_SCALE_Y, ITEM_FONT);
    tft.setTextColor(TFT_MAROON, COL_BG);
    tft.drawString("0-+3-+5 dB", 230, VU_SCALE_Y, ITEM_FONT);
    tft.setTextColor(TFT_RED);
    tft.drawString("0 +3 +5 dB", 229, VU_SCALE_Y, ITEM_FONT);

    const int helpX = TEXT_X + tft.textWidth("TEST: BIAS CAL", ITEM_FONT) + 4;
    help.drawButton(helpX, TEST_Y);
}

void Record::drawCompletionScreen()
{
    displayedActionButtonMode = -1;
    display.openApp("RECORD CALIBRATION", "CALIBRATION COMPLETED");
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_GREEN, COL_BG);
    tft.drawString("SESSION COMPLETED", TEXT_X, 31, ITEM_FONT);

    tft.setTextColor(COL_TEXT, COL_BG);
    const char *mode = finalDeckType == DeckType::ThreeHead ? "MODE: 3 HEAD" : finalDeckType == DeckType::TwoHead ? "MODE: 2 HEAD"
                                                                                                                  : "MODE: ---";
    tft.drawString(mode, TEXT_X, 51, ITEM_FONT);

    const auto drawValue = [](const char *label, int labelX, int valueX, int y,
                              float value, bool valid, bool warning, bool unsignedValue)
    {
        char text[14];
        formatDb(text, sizeof(text), toTenths(value), valid);
        const char *shown = unsignedValue && valid && text[0] == '+' ? text + 1 : text;
        tft.setTextColor(COL_TEXT, COL_BG);
        tft.drawString(label, labelX, y, ITEM_FONT);
        tft.setTextColor(valid ? (warning ? TFT_YELLOW : TFT_GREEN) : TFT_DARKGREY, COL_BG);
        tft.drawString(shown, valueX, y, ITEM_FONT);
    };

    tft.setTextColor(TFT_GREEN, COL_BG);
    tft.drawString("REC LEVEL: MATCHED", TEXT_X, 73, ITEM_FONT);
    const float levelTolerance = RECORD_LEVEL_TOLERANCE_2HEAD_DB;
    const bool leftNearLimit = finalRecordLevelValid &&
                               fabsf(finalRecordLevelLeftDb - RECORD_LEVEL_DB) >= levelTolerance - SUMMARY_WARNING_MARGIN_DB;
    const bool rightNearLimit = finalRecordLevelValid &&
                                fabsf(finalRecordLevelRightDb - RECORD_LEVEL_DB) >= levelTolerance - SUMMARY_WARNING_MARGIN_DB;
    drawValue("L:", TEXT_X, TEXT_X + 18, 94, finalRecordLevelLeftDb,
              finalRecordLevelValid, leftNearLimit, false);
    drawValue("R:", 148, 166, 94, finalRecordLevelRightDb,
              finalRecordLevelValid, rightNearLimit, false);
    drawValue("L-R DIFF:", TEXT_X, 88, 115, finalRecordLevelDifferenceDb,
              finalRecordLevelValid, false, true);

    tft.setTextColor(TFT_GREEN, COL_BG);
    tft.drawString("BIAS: MATCHED", TEXT_X, 137, ITEM_FONT);
    const bool biasLeftNearLimit = finalBiasValid &&
                                   fabsf(finalBiasDiffLeftDb) >= BIAS_MATCH_TOLERANCE_DB - SUMMARY_WARNING_MARGIN_DB;
    const bool biasRightNearLimit = finalBiasValid &&
                                    fabsf(finalBiasDiffRightDb) >= BIAS_MATCH_TOLERANCE_DB - SUMMARY_WARNING_MARGIN_DB;
    drawValue("L DIFF:", TEXT_X, 68, 158, finalBiasDiffLeftDb,
              finalBiasValid, biasLeftNearLimit, false);
    drawValue("R DIFF:", TEXT_X, 68, 179, finalBiasDiffRightDb,
              finalBiasValid, biasRightNearLimit, false);
    drawActionButton();
}

void Record::drawDeckTypeScreen()
{
    display.openApp("RECORD CALIBRATION", "TOUCH TO BACK");
    drawRecordHeadIcons();
}

void Record::drawTestScreen()
{
    display.openApp(
        deckType == DeckType::TwoHead ? "RECORD - 2 HEAD" : "RECORD - 3 HEAD",
        "TOUCH TO BACK");
    drawRecordActionIcons();
}

void Record::drawRecordHeadIcons()
{
    const uint16_t iconBackground = icons.getAdaptiveIconBackground();
    icons.draw2HeadIcon(RECORD_HEAD_ICON_X[0], RECORD_HEAD_ICON_Y,
                        RECORD_HEAD_ICON_SIZE, TFT_WHITE, iconBackground);
    icons.draw3HeadIcon(RECORD_HEAD_ICON_X[1], RECORD_HEAD_ICON_Y,
                        RECORD_HEAD_ICON_SIZE, TFT_WHITE, iconBackground);

    tft.setTextDatum(TC_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("2 HEAD", RECORD_HEAD_ICON_X[0] + RECORD_HEAD_ICON_SIZE / 2,
                   RECORD_HEAD_ICON_Y + RECORD_HEAD_ICON_SIZE + RECORD_ICON_LABEL_OFFSET_Y);
    tft.drawString("3 HEAD", RECORD_HEAD_ICON_X[1] + RECORD_HEAD_ICON_SIZE / 2,
                   RECORD_HEAD_ICON_Y + RECORD_HEAD_ICON_SIZE + RECORD_ICON_LABEL_OFFSET_Y);
    tft.setTextDatum(TL_DATUM);
    tft.setTextPadding(0);

    icons.drawMemoryChecksIcon(
        MEMORY_ICON_X, MEMORY_ICON_Y, MEMORY_ICON_SIZE, TFT_WHITE,
        iconBackground);
    tft.setTextDatum(TC_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("MEMORY", MEMORY_ICON_X + MEMORY_ICON_SIZE / 2,
                   MEMORY_ICON_Y + MEMORY_ICON_SIZE + RECORD_ICON_LABEL_OFFSET_Y);
    tft.setTextDatum(TL_DATUM);
}

void Record::drawMemorySavePrompt()
{
    display.openApp("SAVE RESULT?", "TOUCH TO CHOOSE");
    const int ys[] = {88, 136};
    const char *labels[] = {"SAVE", "NOT SAVE"};
    for (uint8_t i = 0; i < 2; ++i)
    {
        tft.drawRoundRect(52, ys[i], 216, 36, 4, TFT_CYAN);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COL_TEXT, COL_BG);
        tft.drawString(labels[i], 160, ys[i] + 18, ITEM_FONT);
    }
    tft.setTextDatum(TL_DATUM);
}

void Record::showMemoryFull()
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

bool Record::promptAndSaveTapeEq()
{
    const bool completedResult =
        (state == CalibrationState::TwoHeadResponseResult ||
         state == CalibrationState::ThreeHeadResponseResult) &&
        tapeResponseResult.valid;
    if (!completedResult)
        return false;

    drawMemorySavePrompt();
    bool save = false;
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
        if (y >= 88 && y < 124)
        {
            save = true;
            break;
        }
        if (y >= 136 && y < 172)
            break;
    }

    if (save && !graphMemorySaveTapeEq(
                    tapeResponseResult.leftRelativeDb,
                    tapeResponseResult.rightRelativeDb,
                    tapeResponseResult.combinedRelativeDb))
        showMemoryFull();
    return save;
}

void Record::showTapeEqMemory(const GraphMemorySlot &slot)
{
    if (slot.type != GraphMemoryType::TapeEq)
        return;

    for (uint8_t i = 0; i < TAPE_RESPONSE_BAND_COUNT; ++i)
    {
        tapeResponseResult.leftRelativeDb[i] = slot.points.tapeEq[0][i];
        tapeResponseResult.rightRelativeDb[i] = slot.points.tapeEq[1][i];
        tapeResponseResult.combinedRelativeDb[i] = slot.points.tapeEq[2][i];
    }
    tapeResponseResult.valid = true;
    tapeResponseCombinedView = false;
    const CalibrationState previousState = state;
    state = CalibrationState::ThreeHeadResponseResult;
    drawTapeResponseResultScreen();

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
        if (y >= FOOTER_Y)
            break;
        const int buttonHeight =
            ((tft.fontHeight(ITEM_FONT) + 2) * 3) / 2;
        if (contains(x, y, TAPE_RESPONSE_VIEW_X, TAPE_RESPONSE_VIEW_Y,
                     TAPE_RESPONSE_VIEW_WIDTH, buttonHeight))
        {
            tapeResponseCombinedView = !tapeResponseCombinedView;
            drawTapeResponseResultScreen();
        }
    }

    state = previousState;
}

void Record::drawGraphMemoryList()
{
    while (true)
    {
        display.openApp("MEMORY", "TOUCH TO BACK");
        constexpr int MEMORY_LIST_CLEAR_MARGIN = 4;
        const int memoryListClearX = CONTENT_LEFT;
        const int memoryListClearY = HEADER_HEIGHT + MEMORY_LIST_CLEAR_MARGIN;
        const int memoryListClearWidth = CONTENT_WIDTH;
        const int memoryListClearHeight =
            FOOTER_Y - MEMORY_LIST_CLEAR_MARGIN - memoryListClearY;
        tft.fillRect(
            memoryListClearX,
            memoryListClearY,
            memoryListClearWidth,
            memoryListClearHeight,
            TFT_BLACK);
        tft.setTextDatum(TL_DATUM);
        for (uint8_t i = 0; i < GRAPH_MEMORY_SLOT_COUNT; ++i)
        {
            const GraphMemorySlot *slot = graphMemoryGet(i);
            const bool occupied = slot != nullptr &&
                                  slot->type != GraphMemoryType::Empty;
            const char *kind = !occupied ? "---" :
                slot->type == GraphMemoryType::TapeEq ? "EQ" : "DOLBY";
            char row[24];
            snprintf(row, sizeof(row), "MEMORY %u    %s",
                     static_cast<unsigned>(i + 1), kind);
            tft.setTextColor(occupied ? TFT_WHITE : TFT_DARKGREY, TFT_BLACK);
            tft.drawString(row, CONTENT_LEFT + 12, 36 + i * 21, ITEM_FONT);
        }

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
            if (y >= FOOTER_Y)
                return;
            if (x < CONTENT_LEFT || x > CONTENT_RIGHT || y < 36)
                continue;
            const uint8_t index = static_cast<uint8_t>((y - 36) / 21);
            if (index >= GRAPH_MEMORY_SLOT_COUNT)
                continue;
            const GraphMemorySlot *slot = graphMemoryGet(index);
            if (slot == nullptr)
                continue;
            if (slot->type == GraphMemoryType::TapeEq)
                showTapeEqMemory(*slot);
            else if (slot->type == GraphMemoryType::Dolby)
                dolbyCheck.showMemoryResult(*slot);
            break;
        }
    }
}

uint8_t Record::recordActionIconCount() const
{
    return 5;
}

void Record::recordActionIconPosition(uint8_t index, int &x, int &y) const
{
    const bool secondRow = index >= 5;
    y = secondRow ? RECORD_ACTION_SECOND_ROW_Y : RECORD_ACTION_FIRST_ROW_Y;
    if (!secondRow)
    {
        x = RECORD_ACTION_ICON_X[index - 1];
        return;
    }

    x = RECORD_ACTION_WORKFLOW_X;
}

void Record::drawRecordActionIcons()
{
    const uint16_t iconBackground = icons.getAdaptiveIconBackground();
    const bool twoHead = deckType == DeckType::TwoHead;
    const int firstRowY = RECORD_ACTION_FIRST_ROW_Y;
    icons.drawRecordLevelIcon(RECORD_ACTION_ICON_X[0], firstRowY,
                              RECORD_ACTION_ICON_SIZE, TFT_WHITE, iconBackground);
    icons.drawBiasIcon(RECORD_ACTION_ICON_X[1], firstRowY,
                       RECORD_ACTION_ICON_SIZE, TFT_WHITE, iconBackground);
    if (twoHead)
        icons.drawAtcIcon(RECORD_ACTION_ICON_X[2], firstRowY,
                          RECORD_ACTION_ICON_SIZE, TFT_WHITE, iconBackground);
    else
        icons.drawTapeEqIcon(RECORD_ACTION_ICON_X[2], firstRowY,
                             RECORD_ACTION_ICON_SIZE, TFT_WHITE, iconBackground);
    icons.drawDolbyCheckIcon(RECORD_ACTION_ICON_X[3], firstRowY,
                             RECORD_ACTION_ICON_SIZE, TFT_WHITE, iconBackground);

    tft.setTextDatum(TC_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("LEVEL", RECORD_ACTION_ICON_X[0] + RECORD_ACTION_ICON_SIZE / 2,
                   firstRowY + RECORD_ACTION_ICON_SIZE + RECORD_ICON_LABEL_OFFSET_Y);
    tft.drawString("BIAS", RECORD_ACTION_ICON_X[1] + RECORD_ACTION_ICON_SIZE / 2,
                   firstRowY + RECORD_ACTION_ICON_SIZE + RECORD_ICON_LABEL_OFFSET_Y);
    tft.drawString(twoHead ? "ATC" : "TAPE EQ",
                   RECORD_ACTION_ICON_X[2] + RECORD_ACTION_ICON_SIZE / 2,
                   firstRowY + RECORD_ACTION_ICON_SIZE + RECORD_ICON_LABEL_OFFSET_Y);
    tft.drawString("NR TEST", RECORD_ACTION_ICON_X[3] + RECORD_ACTION_ICON_SIZE / 2,
                   firstRowY + RECORD_ACTION_ICON_SIZE + RECORD_ICON_LABEL_OFFSET_Y);
    if (twoHead)
    {
        icons.drawTapeTestIcon(RECORD_ACTION_WORKFLOW_X, RECORD_ACTION_SECOND_ROW_Y,
                               RECORD_ACTION_ICON_SIZE, TFT_WHITE, iconBackground);
        tft.setTextDatum(TC_DATUM);
        tft.setTextFont(2);
        tft.setTextColor(COL_TEXT, COL_BG);
        tft.drawString("TAPE TEST", RECORD_ACTION_WORKFLOW_X + RECORD_ACTION_ICON_SIZE / 2,
                       RECORD_ACTION_SECOND_ROW_Y + RECORD_ACTION_ICON_SIZE +
                           RECORD_ICON_LABEL_OFFSET_Y);
    }
    else
    {
        // Guided full calibration: LEVEL, BIAS and TAPE EQ in one sequence.
        icons.draw3HeadIcon(RECORD_ACTION_WORKFLOW_X, RECORD_ACTION_SECOND_ROW_Y,
                            RECORD_ACTION_ICON_SIZE, TFT_WHITE, iconBackground);
        tft.setTextDatum(TC_DATUM);
        tft.setTextFont(2);
        tft.setTextColor(COL_TEXT, COL_BG);
        tft.drawString("FULL CAL",
                       RECORD_ACTION_WORKFLOW_X + RECORD_ACTION_ICON_SIZE / 2,
                       RECORD_ACTION_SECOND_ROW_Y + RECORD_ACTION_ICON_SIZE +
                           RECORD_ICON_LABEL_OFFSET_Y);
    }
    tft.setTextDatum(TL_DATUM);
    tft.setTextPadding(0);
}

void Record::drawRecordIconSelection(uint8_t index, bool selected, bool headMenu)
{
    const uint8_t iconCount = headMenu ? 2 : recordActionIconCount();
    if (index < 1 || index > iconCount)
        return;

    int x = 0;
    int y = 0;
    if (headMenu)
    {
        x = RECORD_HEAD_ICON_X[index - 1];
        y = RECORD_HEAD_ICON_Y;
    }
    else
    {
        recordActionIconPosition(index, x, y);
    }
    const int size = headMenu ? RECORD_HEAD_ICON_SIZE : RECORD_ACTION_ICON_SIZE;
    const uint16_t color = selected ? TFT_GREEN : TFT_WHITE;

    tft.drawRect(x, y, size, size, color);
    tft.drawRect(x + 1, y + 1, size - 2, size - 2, color);
}

int Record::handleRecordIconTouch(uint16_t x, uint16_t y, bool headMenu)
{
    const uint8_t iconCount = headMenu ? 2 : recordActionIconCount();
    const int size = headMenu ? RECORD_HEAD_ICON_SIZE : RECORD_ACTION_ICON_SIZE;
    uint8_t &selectedIndex = headMenu
                                 ? selectedRecordHeadIndex
                                 : selectedRecordActionIndex;

    for (uint8_t index = 1; index <= iconCount; ++index)
    {
        int iconX = 0;
        int iconY = 0;
        if (headMenu)
        {
            iconX = RECORD_HEAD_ICON_X[index - 1];
            iconY = RECORD_HEAD_ICON_Y;
        }
        else
        {
            recordActionIconPosition(index, iconX, iconY);
        }
        if (!contains(x, y, iconX, iconY, size, size))
            continue;

        if (selectedIndex == index)
            return index;

        const uint8_t previous = selectedIndex;
        selectedIndex = index;
        if (previous != 0)
            drawRecordIconSelection(previous, false, headMenu);
        drawRecordIconSelection(index, true, headMenu);
        return 0;
    }

    return -1;
}

void Record::startTapeTest()
{
    tapeEqFlowActive = true;
    resetTapeResponse();
    if (deckType == DeckType::ThreeHead)
    {
        state = tapeResponseCalibration.valid
                    ? CalibrationState::ThreeHeadResponseReady
                    : CalibrationState::ThreeHeadResponseCalibrationRequired;
    }
    else
    {
        state = CalibrationState::TwoHeadResponseReadyToRecord;
    }
    drawScreen();
}

void Record::drawDeckTypeMenuItem(int y, const char *title, bool selected)
{
    const uint16_t background = selected ? CALIBRATION_MENU_SELECTION_BACKGROUND : COL_BG;
    tft.fillRect(CALIBRATION_MENU_ITEM_X, y - MENU_TOUCH_OFFSET_Y, CALIBRATION_MENU_ITEM_WIDTH,
                 DECK_BUTTON_HEIGHT + 3, background);
    tft.setTextColor(selected ? COL_SELECT : COL_TEXT, background);
    tft.setTextDatum(MC_DATUM);
    tft.drawCentreString(title, LCD_WIDTH / 2, y + DECK_BUTTON_HEIGHT / 2 - 12,
                         CALIBRATION_MENU_FONT);
    tft.setTextDatum(TL_DATUM);
}

void Record::drawTestMenuItem(int y, const char *title, bool selected)
{
    const uint16_t background = selected ? CALIBRATION_MENU_SELECTION_BACKGROUND : COL_BG;
    tft.fillRect(CALIBRATION_MENU_ITEM_X, y - MENU_TOUCH_OFFSET_Y, CALIBRATION_MENU_ITEM_WIDTH,
                 TEST_BUTTON_HEIGHT + 3, background);
    tft.setTextColor(selected ? COL_SELECT : COL_TEXT, background);
    tft.setTextDatum(MC_DATUM);
    tft.drawCentreString(title, LCD_WIDTH / 2, y + TEST_BUTTON_HEIGHT / 2 - 12,
                         CALIBRATION_MENU_FONT);
    tft.setTextDatum(TL_DATUM);
}

void Record::drawTapeResponseScreen()
{
    if (state == CalibrationState::ThreeHeadResponseCalibrationRequired)
        drawTapeResponseCalibrationRequiredScreen();
    else if (state == CalibrationState::ThreeHeadLoopbackReady)
        drawLoopbackReadyScreen();
    else if (state == CalibrationState::ThreeHeadLoopbackPreflightNoise ||
             state == CalibrationState::ThreeHeadLoopbackPreflightSignal ||
             state == CalibrationState::ThreeHeadLoopbackPreflightPassed)
        drawLoopbackPreflightScreen();
    else if (state == CalibrationState::ThreeHeadLoopbackPreflightFailed ||
             state == CalibrationState::ThreeHeadLoopbackSignalLost)
        drawLoopbackPreflightFailedScreen();
    else if (state == CalibrationState::ThreeHeadLoopbackReview)
        drawLoopbackPhase1ReviewScreen();
    else if (state == CalibrationState::ThreeHeadLoopbackRunning ||
             state == CalibrationState::ThreeHeadLoopbackVerifying)
        drawLoopbackRunningScreen();
    else if (state == CalibrationState::ThreeHeadLoopbackSavePrompt)
        drawLoopbackSavePromptScreen();
    else if (state == CalibrationState::ThreeHeadLoopbackResult)
        drawLoopbackResultScreen();
    else if (state == CalibrationState::ThreeHeadResponseReady)
        drawTapeResponseReadyScreen();
    else if (state == CalibrationState::ThreeHeadResponseRunning)
        drawTapeResponseRunningScreen();
    else if (state == CalibrationState::ThreeHeadResponseResult)
        drawTapeResponseResultScreen();
    else
        drawTwoHeadTapeResponseScreen();
}

void Record::drawTapeResponseChoiceButton(int y, const char *label)
{
    tft.fillRect(
        RESPONSE_CHOICE_X,
        y,
        RESPONSE_CHOICE_WIDTH,
        RESPONSE_CHOICE_HEIGHT,
        TFT_DARKCYAN);
    tft.drawRect(
        RESPONSE_CHOICE_X,
        y,
        RESPONSE_CHOICE_WIDTH,
        RESPONSE_CHOICE_HEIGHT,
        TFT_GREEN);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_GREEN, TFT_DARKCYAN);
    tft.drawString(
        label,
        LCD_WIDTH / 2,
        y + RESPONSE_CHOICE_HEIGHT / 2,
        ITEM_FONT);
    tft.setTextDatum(TL_DATUM);
}

void Record::drawTapeResponseCalibrationRequiredScreen()
{
    display.openApp("RECORD EQ CALIBRATION", "TOUCH TO BACK");
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_RED, COL_BG);
    tft.drawString("LOOPBACK CAL REQUIRED", TEXT_X, 48, ITEM_FONT);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("RUN AUTO CAL IN", TEXT_X, 72, ITEM_FONT);
    tft.drawString("SYSTEM SETTINGS", TEXT_X, 94, ITEM_FONT);
    tft.drawString("CONNECT OUT -> IN L/R", TEXT_X, 116, ITEM_FONT);
}

void Record::drawLoopbackReadyScreen()
{
    display.openApp("RECORD CALIBRATION", "TOUCH TO BACK");
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_CYAN, COL_BG);
    tft.drawString("AUTO LOOPBACK CAL", TEXT_X, 30, ITEM_FONT);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("CONNECT:", TEXT_X, 58, ITEM_FONT);
    tft.drawString("OUT -> IN L/R", TEXT_X + 18, 80, ITEM_FONT);
    tft.drawString("GEN: -10.0 dB", TEXT_X, 116, ITEM_FONT);
    tft.drawString("RANGE: 50 Hz - 15 kHz", TEXT_X, 140, ITEM_FONT);
    drawActionButton();
}

void Record::drawLoopbackPreflightScreen()
{
    display.openApp("RECORD CALIBRATION", "CHECKING...");
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_CYAN, COL_BG);
    tft.drawString(
        state == CalibrationState::ThreeHeadLoopbackPreflightPassed
            ? "LOOPBACK DETECTED"
            : "CHECKING LOOPBACK",
        TEXT_X, 30, ITEM_FONT);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(
        state == CalibrationState::ThreeHeadLoopbackPreflightPassed
            ? "STARTING AUTO CAL"
            : "OUT -> IN L/R",
        TEXT_X, 56, ITEM_FONT);
    tft.drawString("TEST: 1 kHz", TEXT_X, 78, ITEM_FONT);
}

void Record::drawLoopbackPreflightFailedScreen()
{
    display.openApp("RECORD CALIBRATION", "TOUCH TO BACK");
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_RED, COL_BG);
    tft.drawString(
        state == CalibrationState::ThreeHeadLoopbackSignalLost
            ? "SIGNAL LOST"
            : "NO LOOPBACK SIGNAL",
        TEXT_X, 30, ITEM_FONT);
    tft.setTextColor(COL_TEXT, COL_BG);
    if (state == CalibrationState::ThreeHeadLoopbackSignalLost)
    {
        char line[24];
        snprintf(line, sizeof(line), "BAND: %u Hz", static_cast<unsigned>(TAPE_RESPONSE_FREQUENCIES_HZ[loopbackFailedBand]));
        tft.drawString(line, TEXT_X, 56, ITEM_FONT);
        drawLoopbackMeasuredValues(76);
    }
    else
    {
        tft.drawString("CONNECT:", TEXT_X, 56, ITEM_FONT);
        tft.drawString("OUT -> IN L/R", TEXT_X + 18, 78, ITEM_FONT);
    }
    tft.drawString("CHECK OUT -> IN", TEXT_X, 97, ITEM_FONT);
    drawStartSquare(
        LOOPBACK_RESULT_TAPE_TEST_X,
        LOOPBACK_RESULT_ICON_Y,
        LOOPBACK_RESULT_ICON_SIZE,
        LOOPBACK_RESULT_ICON_SIZE,
        ITEM_FONT,
        "RETURN");
    drawStartSquare(
        LOOPBACK_RESULT_AUTO_CAL_X,
        LOOPBACK_RESULT_ICON_Y,
        LOOPBACK_RESULT_ICON_SIZE,
        LOOPBACK_RESULT_ICON_SIZE,
        ITEM_FONT,
        "RETEST");
}

void Record::drawLoopbackMeasuredValues(int firstY)
{
    char lines[2][48] = {};
    for (uint8_t band = 0; band < TAPE_RESPONSE_BAND_COUNT; ++band)
    {
        const float measuredDb = loopbackCandidateCalibration.measuredDb[band];
        if (!isfinite(measuredDb))
            continue;

        const uint8_t row = band / 8;
        const size_t used = strlen(lines[row]);
        snprintf(
            lines[row] + used,
            sizeof(lines[row]) - used,
            "%+.1f",
            measuredDb);
    }

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_YELLOW, COL_BG);
    tft.setTextFont(1);
    tft.setTextPadding(CONTENT_RIGHT - TEXT_X);
    tft.drawString(lines[0], TEXT_X, firstY, 1);
    tft.drawString(lines[1], TEXT_X, firstY + 9, 1);
    tft.setTextPadding(0);
    tft.setTextFont(ITEM_FONT);
}

void Record::drawLoopbackPhase1ReviewScreen()
{
    display.openApp("RECORD CALIBRATION", "TOUCH TO CONTINUE");
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(
        tapeResponseCalibrationFailed ? TFT_RED : TFT_CYAN,
        COL_BG);
    tft.drawString(
        tapeResponseCalibrationFailed ? "PHASE 1 FAILED" : "PHASE 1 COMPLETE",
        TEXT_X,
        30,
        ITEM_FONT);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(
        tapeResponseCalibrationFailed
            ? "CORRECTION OUT OF RANGE"
            : "MEASURED LEVELS (dB)",
        TEXT_X,
        52,
        ITEM_FONT);
    tft.setTextColor(TFT_YELLOW, COL_BG);
    tft.setTextFont(1);
    tft.drawString("TOUCH TO CONTINUE", TEXT_X, 76, 1);
    drawLoopbackMeasuredValues(96);
    tft.setTextFont(ITEM_FONT);

    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("CALIBRATE", TEXT_X, LOOPBACK_PROGRESS_CALIBRATE_Y, ITEM_FONT);
    for (uint8_t brick = 0; brick < LOOPBACK_PROGRESS_BRICK_COUNT; ++brick)
    {
        const int brickX = LOOPBACK_PROGRESS_BAR_X + brick *
                                                         (LOOPBACK_PROGRESS_BRICK_WIDTH + LOOPBACK_PROGRESS_BRICK_GAP);
        tft.fillRect(
            brickX,
            LOOPBACK_PROGRESS_CALIBRATE_Y,
            LOOPBACK_PROGRESS_BRICK_WIDTH,
            LOOPBACK_PROGRESS_BRICK_HEIGHT,
            TFT_CYAN);
    }
}

void Record::drawLoopbackRunningScreen()
{
    display.openApp("RECORD CALIBRATION", "RUNNING...");
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_CYAN, COL_BG);
    tft.drawString("AUTO LOOPBACK CAL", TEXT_X, 30, ITEM_FONT);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(
        state == CalibrationState::ThreeHeadLoopbackVerifying
            ? (tapeResponseVerifyPass == 2
                   ? "PHASE: VERIFY 2"
                   : "PHASE: VERIFY 1")
            : "PHASE: CALIBRATE",
        TEXT_X,
        50,
        ITEM_FONT);
    displayedTapeResponseBand = 0xFF;
    displayedLoopbackProgress[0] = 0xFF;
    displayedLoopbackProgress[1] = 0xFF;
    displayedLoopbackProgress[2] = 0xFF;
    drawLoopbackRunningDynamic(true);
    drawActionButton();
}

void Record::drawLoopbackRunningDynamic(bool force)
{
    if (tapeResponseBandIndex >= TAPE_RESPONSE_BAND_COUNT)
        return;

    // The former measurement display cleared this whole area for every RMS
    // sample.  Loopback progress changes only when a frequency band finishes,
    // so update only the affected text and bricks.
    if (force || displayedTapeResponseBand != tapeResponseBandIndex)
    {
        char line[28];
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(COL_TEXT, COL_BG);
        tft.setTextPadding(190);
        snprintf(
            line,
            sizeof(line),
            "ACTIVE: %u Hz",
            TAPE_RESPONSE_FREQUENCIES_HZ[tapeResponseBandIndex]);
        tft.drawString(line, TEXT_X, 74, ITEM_FONT);
        tft.setTextPadding(0);

        if (state == CalibrationState::ThreeHeadLoopbackRunning)
            drawLoopbackMeasuredValues(96);
        displayedTapeResponseBand = tapeResponseBandIndex;
    }

    const uint8_t progress[3] =
        {
            state == CalibrationState::ThreeHeadLoopbackRunning
                ? tapeResponseBandIndex
                : LOOPBACK_PROGRESS_BRICK_COUNT,
            tapeResponseVerifyPass == 1 &&
                    state == CalibrationState::ThreeHeadLoopbackVerifying
                ? tapeResponseBandIndex
                : (tapeResponseVerifyPass >= 2
                       ? LOOPBACK_PROGRESS_BRICK_COUNT
                       : 0),
            tapeResponseVerifyPass == 2 &&
                    state == CalibrationState::ThreeHeadLoopbackVerifying
                ? tapeResponseBandIndex
                : 0};
    const char *const labels[3] = {"CALIBRATE", "VERIFY 1", "VERIFY 2"};
    const int barY[3] =
        {
            LOOPBACK_PROGRESS_CALIBRATE_Y,
            LOOPBACK_PROGRESS_VERIFY_1_Y,
            LOOPBACK_PROGRESS_VERIFY_2_Y};
    const uint16_t DARKBLUE = tft.color565(0, 0, 127);

    for (uint8_t row = 0; row < 3; ++row)
    {
        if (!force && displayedLoopbackProgress[row] == progress[row])
            continue;

        if (force)
        {
            tft.setTextColor(COL_TEXT, COL_BG);
            tft.drawString(labels[row], TEXT_X, barY[row], ITEM_FONT);
        }

        for (uint8_t brick = 0; brick < LOOPBACK_PROGRESS_BRICK_COUNT;
             ++brick)
        {
            const int brickX = LOOPBACK_PROGRESS_BAR_X + brick *
                                                             (LOOPBACK_PROGRESS_BRICK_WIDTH + LOOPBACK_PROGRESS_BRICK_GAP);
            tft.fillRect(
                brickX,
                barY[row],
                LOOPBACK_PROGRESS_BRICK_WIDTH,
                LOOPBACK_PROGRESS_BRICK_HEIGHT,
                brick < progress[row] ? TFT_CYAN : DARKBLUE);
        }
        displayedLoopbackProgress[row] = progress[row];
    }
}

void Record::drawLoopbackResultScreen()
{
    display.openApp("RECORD CALIBRATION", "TOUCH TO BACK");
    tft.setTextDatum(TL_DATUM);
    if (loopbackCandidateCalibration.valid)
    {
        tft.setTextColor(TFT_GREEN, COL_BG);
        tft.drawString(
            tapeResponseVerifyRating == TapeResponseVerifyRating::Excellent
                ? "LOOPBACK CAL EXCELLENT"
                : "LOOPBACK CAL OK",
            TEXT_X,
            30,
            ITEM_FONT);
        char line[32];
        tft.setTextColor(COL_TEXT, COL_BG);
        snprintf(
            line,
            sizeof(line),
            "VERIFY: %s",
            tapeResponseVerifyRating == TapeResponseVerifyRating::Excellent
                ? "EXCELLENT"
                : "OK");
        tft.drawString(line, TEXT_X, 56, ITEM_FONT);
        if (generatorCalibrationSaved)
            tft.drawString("GEN CAL SAVED", TEXT_X, 78, ITEM_FONT);
        else if (generatorCalibrationSaveFailed)
            tft.drawString("SAVE FAILED", TEXT_X, 78, ITEM_FONT);
        else if (loopbackCalibrationNotSaved)
        {
            tft.drawString("CAL ACTIVE / NOT SAVED", TEXT_X, 78, ITEM_FONT);
        }
        drawLoopbackResultActionIcons();
    }
    else if (tapeResponseVerifyRating == TapeResponseVerifyRating::Failed)
    {
        tft.setTextColor(TFT_RED, COL_BG);
        tft.drawString("VERIFY FAILED", TEXT_X, 26, ITEM_FONT);
        char line[32];
        tft.setTextColor(COL_TEXT, COL_BG);
        snprintf(
            line,
            sizeof(line),
            "BAND: %u Hz",
            static_cast<unsigned>(TAPE_RESPONSE_FREQUENCIES_HZ[tapeResponseMaxVerifyErrorBand]));
        tft.drawString(line, TEXT_X, 48, ITEM_FONT);
        if (isfinite(loopbackCandidateCalibration.relativeDb[tapeResponseMaxVerifyErrorBand]))
        {
            snprintf(
                line,
                sizeof(line),
                "ERROR: %+.1f dB",
                loopbackCandidateCalibration.relativeDb[tapeResponseMaxVerifyErrorBand]);
        }
        else
        {
            snprintf(line, sizeof(line), "ERROR: INVALID");
        }
        tft.drawString(line, TEXT_X, 70, ITEM_FONT);
        if (isfinite(tapeResponseMaxVerifyErrorDb))
            snprintf(
                line,
                sizeof(line),
                "MAX ERROR: %.1f dB",
                tapeResponseMaxVerifyErrorDb);
        else
            snprintf(line, sizeof(line), "MAX ERROR: INVALID");
        tft.drawString(line, TEXT_X, 92, ITEM_FONT);
        tft.drawString("VERIFY: FAILED", TEXT_X, 114, ITEM_FONT);
        drawStartSquare(
            START_STOP_X,
            START_STOP_Y,
            START_STOP_WIDTH,
            START_STOP_HEIGHT,
            ITEM_FONT,
            "RETEST");
    }
    else
    {
        tft.setTextColor(TFT_RED, COL_BG);
        tft.drawString("CALIBRATION FAILED", TEXT_X, 48, ITEM_FONT);
        tft.setTextColor(COL_TEXT, COL_BG);
        tft.drawString("CHECK OUT -> IN", TEXT_X, 74, ITEM_FONT);
        drawStartSquare(
            START_STOP_X,
            START_STOP_Y,
            START_STOP_WIDTH,
            START_STOP_HEIGHT,
            ITEM_FONT,
            "RETEST");
    }
}

void Record::drawLoopbackResultActionIcons()
{
    const uint16_t iconBackground = icons.getAdaptiveIconBackground();
    icons.drawAutoCalIcon(
        LOOPBACK_RESULT_TAPE_TEST_X,
        LOOPBACK_RESULT_ACTION_ICON_Y,
        LOOPBACK_RESULT_ICON_SIZE,
        TFT_WHITE,
        iconBackground);
    icons.drawTapeTestIcon(
        LOOPBACK_RESULT_AUTO_CAL_X,
        LOOPBACK_RESULT_ACTION_ICON_Y,
        LOOPBACK_RESULT_ICON_SIZE,
        TFT_WHITE,
        iconBackground);

    tft.setTextDatum(TC_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(
        "AUTO CAL",
        LOOPBACK_RESULT_TAPE_TEST_X + LOOPBACK_RESULT_ICON_SIZE / 2,
        LOOPBACK_RESULT_ACTION_ICON_Y + LOOPBACK_RESULT_ICON_SIZE + RECORD_ICON_LABEL_OFFSET_Y);
    tft.drawString(
        "TAPE TEST",
        LOOPBACK_RESULT_AUTO_CAL_X + LOOPBACK_RESULT_ICON_SIZE / 2,
        LOOPBACK_RESULT_ACTION_ICON_Y + LOOPBACK_RESULT_ICON_SIZE + RECORD_ICON_LABEL_OFFSET_Y);
    tft.setTextDatum(TL_DATUM);
    tft.setTextPadding(0);
}

void Record::drawLoopbackSavePromptScreen()
{
    display.openApp("RECORD CALIBRATION", "TOUCH TO BACK");
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_GREEN, COL_BG);
    tft.drawString("LOOPBACK CAL OK", TEXT_X, 30, ITEM_FONT);
    char line[32];
    snprintf(line, sizeof(line), "MAX ERROR: %.1f dB", tapeResponseMaxVerifyErrorDb);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(line, TEXT_X, 56, ITEM_FONT);
    tft.drawString("SAVE?", TEXT_X, 82, ITEM_FONT);
    drawTapeResponseChoiceButton(RESPONSE_RESULT_FIRST_Y, "YES");
    drawTapeResponseChoiceButton(RESPONSE_RESULT_SECOND_Y, "NO");
}

void Record::drawTapeResponseReadyScreen()
{
    display.openApp("RECORD EQ CALIBRATION", "TOUCH TO BACK");
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_CYAN, COL_BG);
    tft.drawString("TAPE EQ RESPONSE", TEXT_X, 30, ITEM_FONT);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("MODE: 3 HEAD", TEXT_X, 50, ITEM_FONT);
    tft.drawString("SET DECK:", TEXT_X, 76, ITEM_FONT);
    tft.drawString("REC ON", TEXT_X + 18, 96, ITEM_FONT);
    tft.drawString("MONITOR TAPE", TEXT_X + 18, 116, ITEM_FONT);
    tft.drawString("GEN: -10.0 dB", TEXT_X, 146, ITEM_FONT);
    tft.drawString("RANGE: 50 Hz - 15 kHz", TEXT_X, 166, ITEM_FONT);
    tft.drawString("TIME: ~11 s", TEXT_X, 186, ITEM_FONT);
    drawActionButton();
}

void Record::drawTapeResponseRunningScreen()
{
    display.openApp("RECORD EQ CALIBRATION", "RUNNING...");
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_CYAN, COL_BG);
    tft.drawString("TAPE EQ RESPONSE", TEXT_X, 30, ITEM_FONT);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("MODE: 3 HEAD", TEXT_X, 50, ITEM_FONT);
    tft.setTextColor(TFT_GREEN, COL_BG);
    tft.drawString("CAL: ON", TEXT_X + 126, 50, ITEM_FONT);
    tft.setTextColor(COL_TEXT, COL_BG);
    displayedTapeResponseBand = 0xFF;
    displayedTapeResponseLeftTenths = -32768;
    displayedTapeResponseRightTenths = -32768;
    drawTapeResponseRunningDynamic(true);
    drawActionButton();
}

void Record::drawTapeResponseRunningDynamic(bool force)
{
    if (tapeResponseBandIndex >= TAPE_RESPONSE_BAND_COUNT)
        return;

    if (tapeResponsePrecheckActive)
    {
        if (!force && displayedTapeResponseBand == 0xFE)
            return;
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_CYAN, COL_BG);
        tft.setTextPadding(190);
        tft.drawString("CHECK: 1 kHz", TEXT_X, 78, ITEM_FONT);
        tft.drawString("VALIDATING SIGNAL", TEXT_X, 100, ITEM_FONT);
        tft.setTextPadding(0);
        displayedTapeResponseBand = 0xFE;
        return;
    }

    // A frequency band, rather than each RMS sample, is the visible unit of
    // progress.  This avoids clearing and repainting the labels continuously.
    if (!force && displayedTapeResponseBand == tapeResponseBandIndex)
        return;

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_TEXT, COL_BG);

    char line[28];
    tft.setTextPadding(190);
    snprintf(
        line,
        sizeof(line),
        "ACTIVE: %u Hz",
        TAPE_RESPONSE_FREQUENCIES_HZ[tapeResponseBandIndex]);
    tft.drawString(line, TEXT_X, 78, ITEM_FONT);
    snprintf(
        line,
        sizeof(line),
        "BAND: %u/%u",
        static_cast<unsigned>(tapeResponseBandIndex + 1),
        static_cast<unsigned>(TAPE_RESPONSE_BAND_COUNT));
    tft.drawString(line, TEXT_X, 100, ITEM_FONT);
    tft.setTextPadding(0);

    const char *const labels[2] = {"LEFT", "RIGHT"};
    const int barY[2] = {138, 178};
    const uint16_t DARKBLUE = tft.color565(0, 0, 127);
    for (uint8_t row = 0; row < 2; ++row)
    {
        tft.drawString(labels[row], TEXT_X, barY[row], ITEM_FONT);
        for (uint8_t brick = 0; brick < LOOPBACK_PROGRESS_BRICK_COUNT;
             ++brick)
        {
            const int brickX = LOOPBACK_PROGRESS_BAR_X + brick *
                                                             (LOOPBACK_PROGRESS_BRICK_WIDTH + LOOPBACK_PROGRESS_BRICK_GAP);
            tft.fillRect(
                brickX,
                barY[row],
                LOOPBACK_PROGRESS_BRICK_WIDTH,
                LOOPBACK_PROGRESS_BRICK_HEIGHT,
                brick < tapeResponseBandIndex ? TFT_CYAN : DARKBLUE);
        }
    }

    displayedTapeResponseBand = tapeResponseBandIndex;
}

void Record::drawTapeResponseResultScreen(bool liveAnalysis)
{
    display.openApp(
        "RECORD EQ CALIBRATION",
        liveAnalysis ? "ANALYZING" : "TOUCH TO BACK");
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_CYAN, COL_BG);
    tft.drawString("TAPE EQ RESPONSE", TEXT_X, 29, ITEM_FONT);

    if (liveAnalysis)
    {
        tft.setTextColor(TFT_CYAN, COL_BG);
        tft.drawString("LIVE ANALYSIS - REF 50 Hz", TEXT_X, 49, 1);

        drawTapeResponseGraph(true);
        return;
    }

    tft.setTextColor(TFT_GREEN, COL_BG);
    tft.drawString("LOOPBACK CAL: PASSED", TEXT_X, 49, 1);

    if (!tapeResponseResult.valid)
    {
        tft.setTextColor(TFT_RED, COL_BG);
        tft.drawString(
            tapeResponseSignalLost ? "SIGNAL LOST" : "REFERENCE INVALID",
            TEXT_X,
            92,
            ITEM_FONT);
        tft.setTextColor(COL_TEXT, COL_BG);
        tft.drawString(
            tapeResponseSignalLost ? "TEST ABORTED" : "1 kHz band is missing",
            TEXT_X,
            116,
            ITEM_FONT);
    }
    else
    {
        drawTapeResponseGraph();
        drawTapeResponseToggleViewButton();
    }

    drawTapeResponseResultRetestButton();
}

void Record::drawTapeResponseResultRetestButton(bool pressed)
{
    const int buttonHeight =
        ((tft.fontHeight(ITEM_FONT) + 2) * 3) / 2;
    const uint16_t background = TFT_GREEN;
    const uint16_t foreground = TFT_BLACK;

    tft.fillRect(
        TAPE_RESPONSE_RETEST_X,
        TAPE_RESPONSE_RETEST_Y,
        TAPE_RESPONSE_RETEST_WIDTH,
        buttonHeight,
        background);
    tft.drawRect(
        TAPE_RESPONSE_RETEST_X,
        TAPE_RESPONSE_RETEST_Y,
        TAPE_RESPONSE_RETEST_WIDTH,
        buttonHeight,
        TFT_BLUE);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(foreground, background);
    tft.drawString(
        "RETEST",
        TAPE_RESPONSE_RETEST_X + TAPE_RESPONSE_RETEST_WIDTH / 2,
        TAPE_RESPONSE_RETEST_Y + buttonHeight / 2,
        ITEM_FONT);
    tft.setTextDatum(TL_DATUM);
}

void Record::drawTapeResponseToggleViewButton()
{
    const int buttonHeight =
        ((tft.fontHeight(ITEM_FONT) + 2) * 3) / 2;
    const uint16_t background = TFT_DARKCYAN;
    tft.fillRect(
        TAPE_RESPONSE_VIEW_X,
        TAPE_RESPONSE_VIEW_Y,
        TAPE_RESPONSE_VIEW_WIDTH,
        buttonHeight,
        background);
    tft.drawRect(
        TAPE_RESPONSE_VIEW_X,
        TAPE_RESPONSE_VIEW_Y,
        TAPE_RESPONSE_VIEW_WIDTH,
        buttonHeight,
        TFT_CYAN);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, background);
    tft.drawString(
        "TOGGLE",
        TAPE_RESPONSE_VIEW_X + TAPE_RESPONSE_VIEW_WIDTH / 2,
        TAPE_RESPONSE_VIEW_Y + buttonHeight / 3,
        1);
    tft.drawString(
        "VIEW",
        TAPE_RESPONSE_VIEW_X + TAPE_RESPONSE_VIEW_WIDTH / 2,
        TAPE_RESPONSE_VIEW_Y + (buttonHeight * 2) / 3,
        1);
    tft.setTextDatum(TL_DATUM);
}

void Record::drawTapeResponseGraph(bool liveAnalysis)
{
    constexpr int graphWidth =
        TAPE_RESPONSE_GRAPH_RIGHT - TAPE_RESPONSE_GRAPH_LEFT;
    constexpr int graphHeight =
        TAPE_RESPONSE_GRAPH_BOTTOM - TAPE_RESPONSE_GRAPH_TOP;

    if (liveAnalysis)
    {
        tft.fillRect(
            TAPE_RESPONSE_GRAPH_LEFT - 15,
            TAPE_RESPONSE_GRAPH_TOP,
            graphWidth + 16,
            graphHeight + 34,
            COL_BG);
    }

    float liveLeftRelativeDb[TAPE_RESPONSE_BAND_COUNT];
    float liveRightRelativeDb[TAPE_RESPONSE_BAND_COUNT];
    const float *leftTrace = tapeResponseResult.leftRelativeDb;
    const float *rightTrace = tapeResponseResult.rightRelativeDb;
    if (liveAnalysis)
    {
        const float leftReference = tapeResponseResult.leftDb[0];
        const float rightReference = tapeResponseResult.rightDb[0];

        for (uint8_t i = 0; i < TAPE_RESPONSE_BAND_COUNT; ++i)
        {
            const bool completed = i < tapeResponseBandIndex;
            liveLeftRelativeDb[i] =
                completed && isfinite(tapeResponseResult.leftDb[i]) &&
                        isfinite(leftReference)
                    ? tapeResponseResult.leftDb[i] - leftReference
                    : NAN;
            liveRightRelativeDb[i] =
                completed && isfinite(tapeResponseResult.rightDb[i]) &&
                        isfinite(rightReference)
                    ? tapeResponseResult.rightDb[i] - rightReference
                    : NAN;
        }
        leftTrace = liveLeftRelativeDb;
        rightTrace = liveRightRelativeDb;
    }

    tft.setTextDatum(TR_DATUM);
    for (int8_t db = 6; db >= -6; db -= 2)
    {
        const int y = TAPE_RESPONSE_GRAPH_TOP +
                      (6 - db) * graphHeight / 12;
        tft.drawFastHLine(
            TAPE_RESPONSE_GRAPH_LEFT,
            y,
            graphWidth + 1,
            TFT_DARKGREY);
        char label[6];
        if (db == 0)
            snprintf(label, sizeof(label), "0");
        else
            snprintf(label, sizeof(label), "%+d", db);
        tft.setTextColor(db == 0 ? TFT_CYAN : TFT_DARKGREY, COL_BG);
        tft.drawString(label, TAPE_RESPONSE_GRAPH_LEFT - 3, y, 1);
    }

    for (uint8_t i = 0; i < TAPE_RESPONSE_BAND_COUNT; ++i)
    {
        const int x = tapeResponseGraphBandX(i);
        tft.drawFastVLine(
            x,
            TAPE_RESPONSE_GRAPH_TOP,
            graphHeight + 1,
            COL_FRAME);
    }

    const auto drawTrace = [this, graphHeight](
                               const float *values,
                               uint16_t color,
                               bool squareMarkers)
    {
        bool previousValid = false;
        int previousX = 0;
        int previousY = 0;

        for (uint8_t i = 0; i < TAPE_RESPONSE_BAND_COUNT; ++i)
        {
            const int x = tapeResponseGraphBandX(i);
            const float db = values[i];
            const bool valid = isfinite(db);
            if (valid)
            {
                const float clipped = constrain(
                    db,
                    TAPE_RESPONSE_GRAPH_MIN_DB,
                    TAPE_RESPONSE_GRAPH_MAX_DB);
                const int y = TAPE_RESPONSE_GRAPH_TOP +
                              static_cast<int>((TAPE_RESPONSE_GRAPH_MAX_DB - clipped) *
                                               graphHeight /
                                               (TAPE_RESPONSE_GRAPH_MAX_DB - TAPE_RESPONSE_GRAPH_MIN_DB));
                if (previousValid)
                    tft.drawLine(previousX, previousY, x, y, color);
                if (squareMarkers)
                    tft.fillRect(x - 2, y - 2, 5, 5, color);
                else
                    tft.fillCircle(x, y, 2, color);
                previousX = x;
                previousY = y;
            }
            previousValid = valid;
        }
    };

    if (tapeResponseCombinedView)
    {
        drawTrace(tapeResponseResult.combinedRelativeDb, TFT_ORANGE, false);
    }
    else
    {
        drawTrace(leftTrace, TFT_GREEN, false);
        drawTrace(rightTrace, TFT_YELLOW, true);
    }

    static const uint8_t labelBands[] = {0, 2, 4, 6, 7, 9, 11, 13, 15};
    static const char *const labels[] =
        {"50", "125", "315", "800", "1k", "2k5", "6k3", "10k", "15k"};
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COL_TEXT, COL_BG);
    for (uint8_t i = 0; i < sizeof(labelBands); ++i)
    {
        const int x = tapeResponseGraphBandX(labelBands[i]);
        const int labelX = x + (labelBands[i] == 0 ? 3 : 0);
        tft.drawString(
            labels[i],
            labelX,
            TAPE_RESPONSE_GRAPH_BOTTOM + 5,
            1);
    }

    tft.setTextDatum(TL_DATUM);
    if (tapeResponseCombinedView)
    {
        tft.setTextColor(TFT_ORANGE, COL_BG);
        tft.drawString(
            "COMBINED L+R",
            TAPE_RESPONSE_GRAPH_LEFT + 2,
            TAPE_RESPONSE_GRAPH_TOP + 2,
            ITEM_FONT);
    }
    else
    {
        tft.setTextColor(TFT_GREEN, COL_BG);
        tft.drawString(
            "LEFT",
            TAPE_RESPONSE_GRAPH_LEFT + 2,
            TAPE_RESPONSE_GRAPH_TOP + 2,
            ITEM_FONT);
        tft.setTextColor(TFT_YELLOW, COL_BG);
        tft.drawString(
            "RIGHT",
            TAPE_RESPONSE_GRAPH_LEFT + 2,
            TAPE_RESPONSE_GRAPH_BOTTOM - tft.fontHeight(ITEM_FONT) - 2,
            ITEM_FONT);
    }
}

void Record::drawTapeResponseLiveBand(uint8_t bandIndex)
{
    if (bandIndex >= TAPE_RESPONSE_BAND_COUNT ||
        !isfinite(tapeResponseResult.leftDb[bandIndex]) ||
        !isfinite(tapeResponseResult.rightDb[bandIndex]) ||
        !isfinite(tapeResponseResult.leftDb[0]) ||
        !isfinite(tapeResponseResult.rightDb[0]))
        return;

    if (bandIndex > 0 &&
        (!isfinite(tapeResponseResult.leftDb[bandIndex - 1]) ||
         !isfinite(tapeResponseResult.rightDb[bandIndex - 1])))
        return;

    constexpr int graphHeight =
        TAPE_RESPONSE_GRAPH_BOTTOM - TAPE_RESPONSE_GRAPH_TOP;
    const auto graphY = [graphHeight](float relativeDb)
    {
        const float clipped = constrain(
            relativeDb,
            TAPE_RESPONSE_GRAPH_MIN_DB,
            TAPE_RESPONSE_GRAPH_MAX_DB);
        return TAPE_RESPONSE_GRAPH_TOP +
               static_cast<int>((TAPE_RESPONSE_GRAPH_MAX_DB - clipped) *
                                graphHeight /
                                (TAPE_RESPONSE_GRAPH_MAX_DB - TAPE_RESPONSE_GRAPH_MIN_DB));
    };

    const int currentX = tapeResponseGraphBandX(bandIndex);
    const int leftCurrentY = graphY(
        tapeResponseResult.leftDb[bandIndex] - tapeResponseResult.leftDb[0]);
    const int rightCurrentY = graphY(
        tapeResponseResult.rightDb[bandIndex] - tapeResponseResult.rightDb[0]);

    if (bandIndex > 0)
    {
        const int previousX = tapeResponseGraphBandX(bandIndex - 1);
        const int leftPreviousY = graphY(
            tapeResponseResult.leftDb[bandIndex - 1] - tapeResponseResult.leftDb[0]);
        const int rightPreviousY = graphY(
            tapeResponseResult.rightDb[bandIndex - 1] - tapeResponseResult.rightDb[0]);
        tft.drawLine(previousX, leftPreviousY, currentX, leftCurrentY, TFT_GREEN);
        tft.drawLine(previousX, rightPreviousY, currentX, rightCurrentY, TFT_YELLOW);
    }
    tft.fillCircle(currentX, leftCurrentY, 2, TFT_GREEN);
    tft.fillRect(currentX - 2, rightCurrentY - 2, 5, 5, TFT_YELLOW);
}

void Record::drawTwoHeadTapeResponseScreen()
{
    if ((state == CalibrationState::TwoHeadResponseWaitingSync &&
         twoHeadEqSyncFound) ||
        state == CalibrationState::TwoHeadResponseAnalyzing)
    {
        drawTapeResponseResultScreen(true);
        return;
    }

    if (state == CalibrationState::TwoHeadResponseResult &&
        tapeResponseResult.valid)
    {
        drawTapeResponseResultScreen();
        return;
    }

    display.openApp("RECORD EQ CALIBRATION", "TOUCH TO BACK");
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_CYAN, COL_BG);
    tft.drawString("TAPE EQ RESPONSE", TEXT_X, 30, ITEM_FONT);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("MODE: 2 HEAD ", TEXT_X, 52, ITEM_FONT);
    tft.drawString("GEN TEST: -10.0 dB", TEXT_X, 74, 2);
    tft.setTextColor(
        tapeResponseCalibration.valid ? TFT_GREEN : TFT_YELLOW, COL_BG);
    tft.drawString(
        tapeResponseCalibration.valid ? " GEN CAL: ON" : "GEN CAL: OFF",
        TEXT_X + tft.textWidth("MODE: 2 HEAD ", ITEM_FONT) +
            tft.textWidth(" ", 2),
        52,
        2);

    if (state == CalibrationState::TwoHeadResponseResult)
    {
        tft.setTextColor(TFT_RED, COL_BG);
        if (twoHeadEqNoSync)
        {
            tft.drawString("NO SYNC / NO SIGNAL", TEXT_X, 112, ITEM_FONT);
        }
        else if (tapeResponseSignalLost)
        {
            tft.drawString("SIGNAL LOST", TEXT_X, 100, ITEM_FONT);
            tft.setTextColor(COL_TEXT, COL_BG);
            tft.drawString("TEST ABORTED", TEXT_X, 124, ITEM_FONT);
        }
        else
        {
            tft.drawString("TEST RESULT INVALID", TEXT_X, 112, ITEM_FONT);
        }
    }
    else
    {
        drawTwoHeadTapeResponseDynamic(true);
    }
    drawActionButton();
}

void Record::drawTwoHeadTapeResponseDynamic(bool force)
{
    static CalibrationState lastState = CalibrationState::SelectDeckType;
    static TwoHeadEqRecordPhase lastPhase = TwoHeadEqRecordPhase::Sync;
    static bool lastSyncFound = false;
    static bool lastProgressBarActive = false;
    static uint8_t lastProgressBarValue = 0xFF;

    const bool stateChanged = force || lastState != state;
    const bool phaseChanged = lastPhase != twoHeadEqRecordPhase;
    const bool bandChanged =
        displayedTapeResponseBand != tapeResponseBandIndex;
    const bool syncChanged = lastSyncFound != twoHeadEqSyncFound;
    if (!stateChanged && !phaseChanged && !bandChanged && !syncChanged)
        return;

    if (stateChanged)
    {
        tft.fillRect(TEXT_X, 108, 210, 70, COL_BG);
        lastProgressBarActive = false;
        lastProgressBarValue = 0xFF;
    }

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_CYAN, COL_BG);
    const char *heading = "";
    char detail[28] = "";
    char progress[20] = "";

    switch (state)
    {
    case CalibrationState::TwoHeadResponseReadyToRecord:
        heading = "READY TO RECORD";
        snprintf(detail, sizeof(detail), "START DECK RECORD");
        break;
    case CalibrationState::TwoHeadResponseRecording:
        heading = "RECORDING TEST";
        if (twoHeadEqRecordPhase == TwoHeadEqRecordPhase::Sync)
            snprintf(detail, sizeof(detail), "SYNC 2 kHz");
        else if (twoHeadEqRecordPhase == TwoHeadEqRecordPhase::Gap)
            snprintf(detail, sizeof(detail), "GAP");
        else
            snprintf(detail, sizeof(detail), "%u Hz",
                     TAPE_RESPONSE_FREQUENCIES_HZ[tapeResponseBandIndex]);
        if (twoHeadEqRecordPhase != TwoHeadEqRecordPhase::Sync)
            snprintf(progress, sizeof(progress), "BAND %u/16",
                     static_cast<unsigned>(tapeResponseBandIndex + 1));
        break;
    case CalibrationState::TwoHeadResponseRewind:
        heading = "REWIND TAPE";
        snprintf(detail, sizeof(detail), "THEN PRESS NEXT");
        break;
    case CalibrationState::TwoHeadResponseReadyToAnalyze:
        heading = "READY TO ANALYZE";
        snprintf(detail, sizeof(detail), "START PLAYBACK");
        snprintf(progress, sizeof(progress), "THEN PRESS ANALYZE");
        break;
    case CalibrationState::TwoHeadResponseWaitingSync:
        heading = twoHeadEqSyncFound ? "SYNC FOUND" : "WAITING FOR SYNC";
        break;
    case CalibrationState::TwoHeadResponseAnalyzing:
        heading = "ANALYZING";
        snprintf(detail, sizeof(detail), "%u Hz",
                 TAPE_RESPONSE_FREQUENCIES_HZ[tapeResponseBandIndex]);
        snprintf(progress, sizeof(progress), "BAND %u/16",
                 static_cast<unsigned>(tapeResponseBandIndex + 1));
        break;
    default:
        break;
    }

    if (stateChanged || syncChanged)
    {
        tft.setTextPadding(210);
        tft.drawString(heading, TEXT_X, 112, ITEM_FONT);
        tft.setTextPadding(0);
    }

    const bool contentChanged =
        stateChanged || bandChanged ||
        (state == CalibrationState::TwoHeadResponseRecording && phaseChanged);
    if (contentChanged)
    {
        tft.setTextPadding(210);
        tft.drawString(detail, TEXT_X, 134, ITEM_FONT);
        tft.setTextPadding(LOOPBACK_PROGRESS_BAR_X - TEXT_X - 6);
        tft.drawString(progress, TEXT_X, 156, ITEM_FONT);
        tft.setTextPadding(0);
    }

    const bool progressBarActive =
        (state == CalibrationState::TwoHeadResponseRecording &&
         twoHeadEqRecordPhase != TwoHeadEqRecordPhase::Sync) ||
        state == CalibrationState::TwoHeadResponseAnalyzing;
    const uint8_t progressBarValue = tapeResponseBandIndex;
    if (progressBarActive &&
        (!lastProgressBarActive ||
         lastProgressBarValue != progressBarValue))
    {
        drawCalibrationProgressBar(
            tft,
            LOOPBACK_PROGRESS_BAR_X,
            156,
            progressBarValue,
            lastProgressBarActive ? lastProgressBarValue : 0xFF);
    }
    lastProgressBarActive = progressBarActive;
    lastProgressBarValue = progressBarActive ? progressBarValue : 0xFF;
    displayedTapeResponseBand = tapeResponseBandIndex;
    lastState = state;
    lastPhase = twoHeadEqRecordPhase;
    lastSyncFound = twoHeadEqSyncFound;
}

void Record::drawTwoHeadScreen()
{
    const bool bias = isTwoHeadBiasState();
    display.openApp(
        bias ? "RECORD BIAS CALIBRATION" : "RECORD LEVEL CALIBRATION",
        "READY");
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(bias ? "TEST: BIAS ADJUST" : "TEST: RECORD LEVEL", TEXT_X, TEST_Y, ITEM_FONT);
    tft.drawString("MODE: 2 HEAD ", TEXT_X, RECORD_FREQUENCY_Y, ITEM_FONT);
    if (bias)
    {
        tft.drawString("GEN TEST: -15.0 dB", TEXT_X, GENERATOR_Y, 2);
        tft.setTextColor(
            tapeResponseCalibration.valid ? TFT_GREEN : TFT_YELLOW, COL_BG);
        tft.drawString(
            tapeResponseCalibration.valid ? " GEN CAL: ON" : "GEN CAL: OFF",
            TEXT_X + tft.textWidth("MODE: 2 HEAD ", ITEM_FONT) + 12,
            RECORD_FREQUENCY_Y,
            2);
        tft.setTextColor(COL_TEXT, COL_BG);
        tft.drawString("SEQUENCE: 16 s", TEXT_X, 94, 1);
    }
    else
    {
        tft.drawString("GEN: 440 Hz / -10.0 dB", TEXT_X, GENERATOR_Y, ITEM_FONT);
    }

    if (bias)
    {
        tft.drawString("440 L:", BIAS_LEFT_LABEL_X, BIAS_440_Y, ITEM_FONT);
        tft.drawString("10k L:", BIAS_LEFT_LABEL_X, BIAS_10K_Y, ITEM_FONT);
        tft.drawString("DIFF L:", BIAS_LEFT_LABEL_X, BIAS_DIFF_Y, ITEM_FONT);
        tft.drawString("R:", BIAS_RIGHT_LABEL_X, BIAS_440_Y, ITEM_FONT);
        tft.drawString("R:", BIAS_RIGHT_LABEL_X, BIAS_10K_Y, ITEM_FONT);
        tft.drawString("R:", BIAS_RIGHT_LABEL_X, BIAS_DIFF_Y, ITEM_FONT);
    }
    else
    {
        tft.drawString("LEFT:", RECORD_LEFT_LABEL_X, RECORD_LEFT_Y, ITEM_FONT);
        tft.drawString("RIGHT:", RECORD_LEFT_LABEL_X, RECORD_RIGHT_Y, ITEM_FONT);
        tft.drawString("DIFF L:", RECORD_DIFF_LABEL_X, RECORD_LEFT_Y, ITEM_FONT);
        tft.drawString("DIFF R:", RECORD_DIFF_LABEL_X, RECORD_RIGHT_Y, ITEM_FONT);
    }

    const int helpX = TEXT_X + tft.textWidth(bias ? "TEST: BIAS ADJUST" : "TEST: RECORD LEVEL", ITEM_FONT) + 4;
    help.drawButton(helpX, TEST_Y);
    drawTwoHeadDynamic(true);
    drawActionButton();
    drawStatus(true);
}

void Record::drawTwoHeadDynamic(bool force)
{
    static int lastSeconds = -1;
    static int lastLowCount = -1;
    static int lastHighCount = -1;
    static int lastElapsedProgress = -1;
    static Status lastBiasDiagnosticStatus = Status::Ready;
    static CalibrationState lastState = CalibrationState::SelectDeckType;
    static int8_t displayedTwoHeadBiasFinal = -1;
    static int8_t displayedTwoHeadLevelFinal = -1;
    if (force || lastState != state)
    {
        // 2 HEAD uses this central area for deck instructions and sequence
        // progress. Clear the entire former VU area so no bar fragments remain.
        tft.fillRect(CONTENT_LEFT, 102, CONTENT_WIDTH, 54, COL_BG);
        lastSeconds = -1;
        lastLowCount = -1;
        lastHighCount = -1;
        lastElapsedProgress = -1;
        lastBiasDiagnosticStatus = Status::Ready;
        lastState = state;
    }

    const uint32_t now = millis();
    char line[28] = "";
    char detail[28] = "";
    char detail2[28] = "";
    int seconds = -1;
    uint32_t elapsedRecordingMs = 0;
    uint32_t recordingDurationMs = 0;
    const bool showAnalysisVu =
        state == CalibrationState::TwoHeadLevelAnalyzing ||
        state == CalibrationState::TwoHeadBiasAnalyzing;

    switch (state)
    {
    case CalibrationState::TwoHeadLevelReadyToRecord:
        snprintf(line, sizeof(line), "START THE DECK RECORDING");
        snprintf(detail, sizeof(detail), "THEN PRESS START");
        break;
    case CalibrationState::TwoHeadLevelRecording:
        snprintf(line, sizeof(line), "RECORDING TEST");
        recordingDurationMs = TWO_HEAD_LEVEL_RECORD_MS;
        elapsedRecordingMs = min(recordingDurationMs, now - twoHeadStateStartedMs);
        seconds = static_cast<int>(elapsedRecordingMs / 1000);
        break;
    case CalibrationState::TwoHeadLevelRewind:
    case CalibrationState::TwoHeadBiasRewind:
        snprintf(line, sizeof(line), "STOP AND REWIND TAPE");
        break;
    case CalibrationState::TwoHeadLevelReadyToAnalyze:
    case CalibrationState::TwoHeadBiasReadyToAnalyze:
        snprintf(line, sizeof(line), "START PLAYBACK");
        snprintf(detail, sizeof(detail), "THEN PRESS ANALYZE");
        break;
    case CalibrationState::TwoHeadLevelAnalyzing:
        break;
    case CalibrationState::TwoHeadBiasReadyToRecord:
        snprintf(line, sizeof(line), "START THE DECK RECORDING");
        snprintf(detail, sizeof(detail), "THEN PRESS START");
        break;
    case CalibrationState::TwoHeadBiasRecording:
    {
        elapsedRecordingMs = min(
            TWO_HEAD_BIAS_RECORD_TOTAL_MS,
            now - twoHeadBiasRecordingStartedMs);
        recordingDurationMs = TWO_HEAD_BIAS_RECORD_TOTAL_MS;
        seconds = static_cast<int>(elapsedRecordingMs / 1000);
        const char *active = twoHeadBiasSegment == BiasSequenceSegment::Sync1 ? "SYNC 1" : twoHeadBiasSegment == BiasSequenceSegment::TestLow440 ? "TEST 440"
                                                                                       : twoHeadBiasSegment == BiasSequenceSegment::Sync2        ? "SYNC 2"
                                                                                       : twoHeadBiasSegment == BiasSequenceSegment::TestHigh10k  ? "TEST 10k"
                                                                                                                                                 : "SYNC 3";
        snprintf(line, sizeof(line), "ACTIVE: %s", active);
        break;
    }
    case CalibrationState::TwoHeadBiasAnalyzing:
        break;
    case CalibrationState::TwoHeadBiasResult:
        if (!twoHeadSequenceFound)
            snprintf(line, sizeof(line), "SEQUENCE NOT FOUND");
        else if (status == Status::LevelTooHigh)
        {
            snprintf(line, sizeof(line), "LEVEL TOO HIGH");
            snprintf(detail, sizeof(detail), "ADJUST REC LEVEL");
        }
        else if (status == Status::LevelTooLow)
        {
            snprintf(line, sizeof(line), "LEVEL TOO LOW");
            snprintf(detail, sizeof(detail), "ADJUST REC LEVEL");
        }
        else if (status == Status::InvalidTestLevel)
        {
            snprintf(line, sizeof(line), "INVALID TEST LEVEL");
            snprintf(detail, sizeof(detail), "RECORD SEQUENCE AGAIN");
        }
        else if (status == Status::DecreaseBias)
            snprintf(line, sizeof(line), "DECREASE BIAS");
        else if (status == Status::IncreaseBias)
            snprintf(line, sizeof(line), "INCREASE BIAS");
        else if (status == Status::BiasBorderline)
            snprintf(line, sizeof(line), "BIAS MATCHED / BORDERLINE");
        else if (status == Status::ChannelMismatch)
        {
            snprintf(line, sizeof(line), "CHANNEL MISMATCH");
            snprintf(detail, sizeof(detail), "CHECK TAPE / AZIMUTH");
            snprintf(detail2, sizeof(detail2), "DO TEST AGAIN");
        }
        else
            snprintf(line, sizeof(line), "ADJUST BIAS");
        break;
    default:
        break;
    }

    const bool countsChanged = twoHead440Count != lastLowCount ||
                               twoHead10kCount != lastHighCount;
    const bool elapsedActive = (state == CalibrationState::TwoHeadLevelRecording ||
                                state == CalibrationState::TwoHeadBiasRecording) &&
                               seconds >= 0;
    if (!showAnalysisVu &&
        (force || seconds != lastSeconds || state != lastState || countsChanged))
    {
        tft.setTextColor(TFT_CYAN, COL_BG);
        tft.setTextPadding(210);
        tft.drawString(line, TEXT_X, 112, ITEM_FONT);
        tft.setTextPadding(0);
        if (detail2[0] != '\0')
        {
            tft.drawString(detail, TEXT_X, 134, 1);
            tft.drawString(detail2, TEXT_X, 145, 1);
        }
        else
        {
            if (elapsedActive)
            {
                tft.setTextColor(TFT_CYAN, COL_BG);
                tft.drawString("ELAPSED: ", TEXT_X, 132, ITEM_FONT);
                const int elapsedX = TEXT_X + tft.textWidth("ELAPSED: ", ITEM_FONT);
                tft.setTextColor(TFT_YELLOW, COL_BG);
                char elapsedValue[8];
                snprintf(elapsedValue, sizeof(elapsedValue), "%d", seconds);
                tft.drawString(elapsedValue, elapsedX, 132, ITEM_FONT);
                const int unitsX = elapsedX + tft.textWidth(elapsedValue, ITEM_FONT);
                tft.setTextColor(TFT_CYAN, COL_BG);
                tft.drawString(" s", unitsX, 132, ITEM_FONT);

                const uint8_t progress = recordingDurationMs > 0
                                             ? static_cast<uint8_t>(min(
                                                   static_cast<uint32_t>(PROGRESS_BAR_BRICK_COUNT),
                                                   elapsedRecordingMs * PROGRESS_BAR_BRICK_COUNT /
                                                       recordingDurationMs))
                                             : 0;
                if (lastElapsedProgress != progress)
                {
                    const int progressY =
                        132 + (tft.fontHeight(ITEM_FONT) - PROGRESS_BAR_BRICK_HEIGHT) / 2;
                    drawCalibrationProgressBar(
                        tft,
                        PROGRESS_BAR_X,
                        progressY,
                        progress,
                        lastElapsedProgress < 0
                            ? 0xFF
                            : static_cast<uint8_t>(lastElapsedProgress));
                    lastElapsedProgress = progress;
                }
            }
            else
            {
                tft.setTextColor(TFT_CYAN, COL_BG);
                tft.drawString(detail, TEXT_X, 132, ITEM_FONT);
            }
        }
        tft.setTextPadding(0);
        lastSeconds = seconds;
        lastLowCount = twoHead440Count;
        lastHighCount = twoHead10kCount;
    }

    if (showAnalysisVu && (force || lastState != state))
    {
        tft.setTextColor(TFT_DARKGREY, COL_BG);
        tft.drawString("-30-------20---------10---6--3--", 20, VU_SCALE_Y, ITEM_FONT);
        tft.setTextColor(COL_TEXT);
        tft.drawString("-30      -20        -10  -6 -3  ", 19, VU_SCALE_Y, ITEM_FONT);
        tft.setTextColor(TFT_MAROON, COL_BG);
        tft.drawString("0-+3-+5 dB", 230, VU_SCALE_Y, ITEM_FONT);
        tft.setTextColor(TFT_RED);
        tft.drawString("0 +3 +5 dB", 229, VU_SCALE_Y, ITEM_FONT);
    }

    if (state == CalibrationState::TwoHeadBiasAnalyzing)
    {
        const bool waitingForSync = status == Status::WaitingForSync;
        if (force || countsChanged || status != lastBiasDiagnosticStatus)
        {
            if (waitingForSync)
            {
                tft.fillRect(TEXT_X, 90, 210, 11, COL_BG);
            }
            else
            {
                tft.setTextColor(TFT_CYAN, COL_BG);
                char diagnostic[32];
                snprintf(diagnostic, sizeof(diagnostic), "SAMPLES: 440 %u/5  10k %u/5",
                         twoHead440Count, twoHead10kCount);
                tft.setTextPadding(210);
                tft.drawString(diagnostic, TEXT_X, 92, 1);
                tft.setTextPadding(0);
            }
            lastBiasDiagnosticStatus = status;
            lastLowCount = twoHead440Count;
            lastHighCount = twoHead10kCount;
        }
    }

    if (isTwoHeadBiasState())
        drawBiasMeasurements(force);
    else if (state == CalibrationState::TwoHeadLevelAnalyzing ||
             state == CalibrationState::TwoHeadLevelResult ||
             state == CalibrationState::TwoHeadLevelMatched)
        drawRecordMeasurements(force);

    const int8_t biasFinalMessage =
        state == CalibrationState::TwoHeadBiasMatched ? 1 : state == CalibrationState::TwoHeadBiasResult ? 2
                                                                                                         : -1;
    if (biasFinalMessage >= 0 &&
        (force || displayedTwoHeadBiasFinal != biasFinalMessage))
    {
        tft.fillRect(CONTENT_LEFT, 102, CONTENT_WIDTH, 48, COL_BG);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(
            biasFinalMessage == 1 ? TFT_GREEN : TFT_ORANGE,
            COL_BG);
        tft.drawString(
            biasFinalMessage == 1 ? "BIAS MATCHED" : "ADJUST BIAS !",
            LCD_WIDTH / 2,
            129,
            4);
        tft.setTextDatum(TL_DATUM);
        displayedTwoHeadBiasFinal = biasFinalMessage;
    }
    else if (biasFinalMessage < 0)
        displayedTwoHeadBiasFinal = -1;

    const int8_t levelFinalMessage =
        state == CalibrationState::TwoHeadLevelMatched ? 1 : state == CalibrationState::TwoHeadLevelResult ? 2
                                                                                                           : -1;
    if (levelFinalMessage >= 0 &&
        (force || displayedTwoHeadLevelFinal != levelFinalMessage))
    {
        tft.fillRect(CONTENT_LEFT, 102, CONTENT_WIDTH, 48, COL_BG);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(
            levelFinalMessage == 1 ? TFT_GREEN : TFT_ORANGE,
            COL_BG);
        tft.drawString(
            levelFinalMessage == 1 ? "LEVEL OK" : "ADJUST LEVEL!",
            LCD_WIDTH / 2,
            129,
            4);
        tft.setTextDatum(TL_DATUM);
        displayedTwoHeadLevelFinal = levelFinalMessage;
    }
    else if (levelFinalMessage < 0)
        displayedTwoHeadLevelFinal = -1;
}

void Record::drawActionButton(bool pressed)
{
    const bool running = isGeneratorRunning();
    const bool matched = state == CalibrationState::ThreeHeadLevelMatched ||
                         state == CalibrationState::ThreeHeadBiasMatched ||
                         state == CalibrationState::TwoHeadLevelMatched ||
                         state == CalibrationState::TwoHeadBiasMatched;
    const uint16_t background = matched ? TFT_GREEN : (running ? BUTTON_RUNNING : BUTTON_IDLE);
    const uint16_t border = running && !matched ? TFT_ORANGE : TFT_GREEN;
    const uint16_t foreground = pressed ? TFT_WHITE : (matched ? TFT_BLACK : border);
    const char *label = "START";
    int8_t mode = 0;

    if (state == CalibrationState::ThreeHeadLevelReady ||
        state == CalibrationState::ThreeHeadBiasReady ||
        state == CalibrationState::ThreeHeadLoopbackReady ||
        state == CalibrationState::ThreeHeadResponseReady)
    {
        label = "START";
        mode = 0;
    }
    else if (state == CalibrationState::ThreeHeadLevelRunning ||
             state == CalibrationState::ThreeHeadBiasRunning ||
             state == CalibrationState::ThreeHeadLoopbackRunning ||
             state == CalibrationState::ThreeHeadLoopbackVerifying ||
             state == CalibrationState::ThreeHeadResponseRunning)
    {
        label = "STOP";
        mode = 1;
    }
    else if (state == CalibrationState::ThreeHeadLevelMatched ||
             state == CalibrationState::ThreeHeadBiasMatched ||
             state == CalibrationState::ThreeHeadResponseResult)
    {
        // Inside the guided workflow the step is already accepted, so the
        // button advances instead of repeating the test.
        label = workflowActive ? "NEXT" : "RETEST";
        mode = workflowActive ? 6 : 5;
    }
    else if (isTwoHeadState())
    {
        switch (state)
        {
        case CalibrationState::TwoHeadLevelRecording:
        case CalibrationState::TwoHeadBiasRecording:
        case CalibrationState::TwoHeadResponseRecording:
            label = "STOP";
            mode = 1;
            break;
        case CalibrationState::TwoHeadLevelAnalyzing:
        case CalibrationState::TwoHeadBiasAnalyzing:
        case CalibrationState::TwoHeadResponseWaitingSync:
        case CalibrationState::TwoHeadResponseAnalyzing:
            label = "RETEST";
            mode = 5;
            break;
        case CalibrationState::TwoHeadLevelRewind:
        case CalibrationState::TwoHeadBiasRewind:
        case CalibrationState::TwoHeadResponseRewind:
            label = "NEXT";
            mode = 2;
            break;
        case CalibrationState::TwoHeadLevelMatched:
        case CalibrationState::TwoHeadBiasMatched:
            label = "RETEST";
            mode = 5;
            break;
        case CalibrationState::TwoHeadLevelReadyToAnalyze:
        case CalibrationState::TwoHeadBiasReadyToAnalyze:
        case CalibrationState::TwoHeadResponseReadyToAnalyze:
            label = "ANALYZE";
            mode = 4;
            break;
        case CalibrationState::TwoHeadLevelResult:
            label = "RETEST";
            mode = 5;
            break;
        case CalibrationState::TwoHeadBiasResult:
        case CalibrationState::TwoHeadResponseResult:
            label = "RETEST";
            mode = 5;
            break;
        case CalibrationState::TwoHeadResponseReadyToRecord:
            label = "START";
            mode = 0;
            break;
        default:
            break;
        }
    }
    else
    {
        label = matched ? "NEXT" : (running ? "STOP" : "START");
        mode = matched ? 2 : (running ? 1 : 0);
    }

    // Avoid filling the button every measurement pass.  Repainting only after a
    // state change removes visible flicker while preserving pressed feedback.
    if (!pressed && displayedActionButtonMode == mode)
        return;

    const bool stopLabel = label[0] == 'S' && label[1] == 'T' &&
                           label[2] == 'O' && label[3] == 'P' &&
                           label[4] == '\0';
    if (stopLabel)
    {
        drawStopOctagon(
            START_STOP_X,
            START_STOP_Y,
            START_STOP_WIDTH,
            START_STOP_HEIGHT,
            ITEM_FONT);
    }
    else if (label[0] == 'S' && label[1] == 'T' && label[2] == 'A' &&
             label[3] == 'R' && label[4] == 'T' && label[5] == '\0')
    {
        drawStartSquare(
            START_STOP_X,
            START_STOP_Y,
            START_STOP_WIDTH,
            START_STOP_HEIGHT,
            ITEM_FONT);
    }
    else
    {
        tft.fillRect(START_STOP_X, START_STOP_Y, START_STOP_WIDTH, START_STOP_HEIGHT, background);
        tft.drawRect(START_STOP_X, START_STOP_Y, START_STOP_WIDTH, START_STOP_HEIGHT, border);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(foreground, background);
        tft.drawString(label, START_STOP_X + START_STOP_WIDTH / 2,
                       START_STOP_Y + START_STOP_HEIGHT / 2, ITEM_FONT);
        tft.setTextDatum(TL_DATUM);
    }
    displayedActionButtonMode = pressed ? -1 : mode;
}

void Record::drawRecordMeasurements(bool force)
{
    const int16_t leftTenths = toTenths(leftLevelDb);
    const int16_t rightTenths = toTenths(rightLevelDb);
    const int16_t diffLeftTenths = toTenths(leftLevelDb - RECORD_LEVEL_DB);
    const int16_t diffRightTenths = toTenths(rightLevelDb - RECORD_LEVEL_DB);
    const bool drawLeft = force || leftSignalValid != displayedLeftValid ||
                          (leftSignalValid && leftTenths != displayedLeftTenths);
    const bool drawRight = force || rightSignalValid != displayedRightValid ||
                           (rightSignalValid && rightTenths != displayedRightTenths);
    const bool drawDiffLeft = force || leftSignalValid != displayedLeftValid ||
                              (leftSignalValid && diffLeftTenths != displayedDiffLeftTenths);
    const bool drawDiffRight = force || rightSignalValid != displayedRightValid ||
                               (rightSignalValid && diffRightTenths != displayedDiffRightTenths);

    if (drawLeft)
    {
        char value[14];
        formatDb(value, sizeof(value), leftTenths, leftSignalValid);
        tft.setTextColor(leftSignalValid ? TFT_GREEN : TFT_DARKGREY, COL_BG);
        tft.setTextPadding(VALUE_WIDTH);
        tft.drawString(value, RECORD_LEFT_VALUE_X, RECORD_LEFT_Y, ITEM_FONT);
        tft.setTextPadding(0);
    }
    if (drawRight)
    {
        char value[14];
        formatDb(value, sizeof(value), rightTenths, rightSignalValid);
        tft.setTextColor(rightSignalValid ? TFT_GREEN : TFT_DARKGREY, COL_BG);
        tft.setTextPadding(VALUE_WIDTH);
        tft.drawString(value, RECORD_LEFT_VALUE_X, RECORD_RIGHT_Y, ITEM_FONT);
        tft.setTextPadding(0);
    }
    if (drawDiffLeft)
    {
        char value[14];
        formatDb(value, sizeof(value), diffLeftTenths, leftSignalValid);
        tft.setTextColor(leftSignalValid ? TFT_YELLOW : TFT_DARKGREY, COL_BG);
        tft.setTextPadding(VALUE_WIDTH);
        tft.drawString(value, RECORD_DIFF_VALUE_X, RECORD_LEFT_Y, ITEM_FONT);
        tft.setTextPadding(0);
    }
    if (drawDiffRight)
    {
        char value[14];
        formatDb(value, sizeof(value), diffRightTenths, rightSignalValid);
        tft.setTextColor(rightSignalValid ? TFT_YELLOW : TFT_DARKGREY, COL_BG);
        tft.setTextPadding(VALUE_WIDTH);
        tft.drawString(value, RECORD_DIFF_VALUE_X, RECORD_RIGHT_Y, ITEM_FONT);
        tft.setTextPadding(0);
    }

    displayedLeftTenths = leftTenths;
    displayedRightTenths = rightTenths;
    displayedDiffLeftTenths = diffLeftTenths;
    displayedDiffRightTenths = diffRightTenths;
    displayedLeftValid = leftSignalValid;
    displayedRightValid = rightSignalValid;
    // 2 HEAD shows VU only while the replay signal is being analyzed.
    if (!isTwoHeadState() ||
        state == CalibrationState::TwoHeadLevelAnalyzing)
        drawVuMeters(force);
}

void Record::drawBiasMeasurements(bool force)
{
    const int16_t lowLeft = toTenths(bias440LeftDb);
    const int16_t lowRight = toTenths(bias440RightDb);
    const int16_t highLeft = toTenths(bias10kLeftDb);
    const int16_t highRight = toTenths(bias10kRightDb);
    const bool diffValid = bias440Valid && bias10kValid;
    const int16_t diffLeft = toTenths(bias10kLeftDb - bias440LeftDb);
    const int16_t diffRight = toTenths(bias10kRightDb - bias440RightDb);

    const auto drawValue = [force](int x, int y, int16_t value, bool valid,
                                   int16_t &displayedValue, bool &displayedValid)
    {
        if (!force && valid == displayedValid && (!valid || value == displayedValue))
            return;
        char text[14];
        formatDb(text, sizeof(text), value, valid);
        tft.setTextColor(valid ? TFT_GREEN : TFT_DARKGREY, COL_BG);
        tft.setTextPadding(BIAS_VALUE_WIDTH);
        tft.drawString(text, x, y, ITEM_FONT);
        tft.setTextPadding(0);
        displayedValue = value;
        displayedValid = valid;
    };

    drawValue(BIAS_LEFT_VALUE_X, BIAS_440_Y, lowLeft, bias440Valid, displayedBias440LeftTenths, displayedBias440LeftValid);
    drawValue(BIAS_LEFT_VALUE_X, BIAS_10K_Y, highLeft, bias10kValid, displayedBias10kLeftTenths, displayedBias10kLeftValid);
    drawValue(BIAS_LEFT_VALUE_X, BIAS_DIFF_Y, diffLeft, diffValid, displayedBiasDiffLeftTenths, displayedBiasDiffLeftValid);
    drawValue(BIAS_RIGHT_VALUE_X, BIAS_440_Y, lowRight, bias440Valid, displayedBias440RightTenths, displayedBias440RightValid);
    drawValue(BIAS_RIGHT_VALUE_X, BIAS_10K_Y, highRight, bias10kValid, displayedBias10kRightTenths, displayedBias10kRightValid);
    drawValue(BIAS_RIGHT_VALUE_X, BIAS_DIFF_Y, diffRight, diffValid, displayedBiasDiffRightTenths, displayedBiasDiffRightValid);
    if (!isTwoHeadState() ||
        state == CalibrationState::TwoHeadBiasAnalyzing)
        drawVuMeters(force);
}

void Record::drawVuMeters(bool force)
{
    const int leftPixels = dbToVuPixels(leftLevelDb, leftSignalValid);
    const int rightPixels = dbToVuPixels(rightLevelDb, rightSignalValid);

    if (force || leftPixels != displayedLeftVuPixels)
    {
        playback.drawVUmeter(VU_X, VU_LEFT_Y, leftPixels, leftPixels, false, true);
        displayedLeftVuPixels = leftPixels;
    }
    if (force || rightPixels != displayedRightVuPixels)
    {
        playback.drawVUmeter(VU_X, VU_RIGHT_Y, rightPixels, rightPixels, false, true);
        displayedRightVuPixels = rightPixels;
    }
}

void Record::drawActiveFrequency(bool force)
{
    const int8_t active = biasHighFrequency ? 1 : 0;
    if (!force && active == displayedActiveFrequency)
        return;
    tft.setTextColor(biasHighFrequency ? TFT_ORANGE : TFT_CYAN, COL_BG);
    tft.setTextPadding(THREE_HEAD_FREQUENCY_WIDTH);
    tft.drawString(biasHighFrequency ? "10 kHz" : "440 Hz",
                   THREE_HEAD_FREQUENCY_X, THREE_HEAD_STABLE_Y, ITEM_FONT);
    tft.setTextPadding(0);
    displayedActiveFrequency = active;
}

void Record::drawThreeHeadCalibrationStatus(bool force)
{
    const bool levelsAvailable = bias440Valid && bias10kValid;
    int8_t levelState = 0;
    if (levelsAvailable)
    {
        const bool tooHigh = bias440LeftDb > THREE_HEAD_BIAS_LEVEL_MAX_DB ||
                             bias440RightDb > THREE_HEAD_BIAS_LEVEL_MAX_DB ||
                             bias10kLeftDb > THREE_HEAD_BIAS_LEVEL_MAX_DB ||
                             bias10kRightDb > THREE_HEAD_BIAS_LEVEL_MAX_DB;
        const bool tooLow = bias440LeftDb < REC_CAL_LEVEL_MIN_DB ||
                            bias440RightDb < REC_CAL_LEVEL_MIN_DB ||
                            bias10kLeftDb < REC_CAL_LEVEL_MIN_DB ||
                            bias10kRightDb < REC_CAL_LEVEL_MIN_DB;
        levelState = tooHigh ? 1 : (tooLow ? 2 : 3);
    }

    int8_t biasState = 0;
    if (levelsAvailable)
    {
        const float diffLeft = bias10kLeftDb - bias440LeftDb;
        const float diffRight = bias10kRightDb - bias440RightDb;
        const float largestDiff = max(fabsf(diffLeft), fabsf(diffRight));
        biasState = largestDiff <= REC_CAL_BIAS_MATCH_TOLERANCE_DB ? 1 : (largestDiff <= REC_CAL_BIAS_ACCEPTABLE_TOLERANCE_DB ? 2 : 3);
    }

    if (force || levelState != displayedLevelStatus)
    {
        const char *text = levelState == 1 ? "LEVEL TOO HIGH" : levelState == 2 ? "LEVEL TOO LOW"
                                                            : levelState == 3   ? "LEVEL OK"
                                                                                : "MEASURING";
        const uint16_t color = levelState == 3 ? TFT_GREEN : (levelState == 0 ? TFT_CYAN : TFT_ORANGE);
        tft.setTextColor(color, COL_BG);
        tft.setTextPadding(THREE_HEAD_STATUS_VALUE_WIDTH);
        tft.drawString(text, THREE_HEAD_STATUS_VALUE_X, THREE_HEAD_LEVEL_Y, ITEM_FONT);
        tft.setTextPadding(0);
        displayedLevelStatus = levelState;
    }

    if (force || biasState != displayedBiasStatus)
    {
        const char *text = biasState == 1 ? "BIAS MATCHED" : biasState == 2 ? "BIAS ACCEPTABLE"
                                                         : biasState == 3   ? "ADJUST BIAS"
                                                                            : "MEASURING";
        const uint16_t color = biasState == 1 ? TFT_GREEN : (biasState == 2 ? TFT_YELLOW : (biasState == 0 ? TFT_CYAN : TFT_ORANGE));
        tft.setTextColor(color, COL_BG);
        tft.setTextPadding(THREE_HEAD_STATUS_VALUE_WIDTH);
        tft.drawString(text, THREE_HEAD_STATUS_VALUE_X, THREE_HEAD_BIAS_Y, ITEM_FONT);
        tft.setTextPadding(0);
        displayedBiasStatus = biasState;
    }

    if (force || displayedStableCycles != static_cast<int8_t>(biasCompletedCycles))
    {
        char text[16];
        snprintf(text, sizeof(text), "%u/%u", biasCompletedCycles,
                 REC_CAL_REQUIRED_STABLE_CYCLES);
        tft.setTextColor(biasCompletedCycles == REC_CAL_REQUIRED_STABLE_CYCLES ? TFT_GREEN : TFT_CYAN, COL_BG);
        tft.setTextPadding(0);
        tft.setTextDatum(TR_DATUM);
        tft.drawString(text, THREE_HEAD_STAGE_COUNTER_X, THREE_HEAD_STAGE_COUNTER_Y,
                       THREE_HEAD_STAGE_COUNTER_FONT);
        tft.setTextDatum(TL_DATUM);
        displayedStableCycles = biasCompletedCycles;
    }
}

void Record::drawStatus(bool force)
{
    if (!force && statusDisplayed && status == displayedStatus)
        return;

    const char *text = "READY";
    switch (status)
    {
    case Status::Ready:
        text = "READY";
        break;
    case Status::AdjustRecLevel:
        text = "ADJUST REC LEVEL";
        break;
    case Status::LevelMatched:
        text = "LEVEL MATCHED";
        break;
    case Status::NoSignal:
        text = "NO SIGNAL";
        break;
    case Status::BiasMeasuring:
        text = "BIAS MEASURING";
        break;
    case Status::AdjustBias:
        text = "ADJUST BIAS";
        break;
    case Status::BiasMatched:
        text = "BIAS MATCHED";
        break;
    case Status::Analyzing:
        text = "ANALYZING";
        break;
    case Status::WaitingFor440:
        text = "WAITING FOR 440 Hz";
        break;
    case Status::WaitingFor10k:
        text = "WAITING FOR 10 kHz";
        break;
    case Status::Capturing440:
        text = "CAPTURING 440 Hz";
        break;
    case Status::Capturing10k:
        text = "CAPTURING 10 kHz";
        break;
    case Status::WaitingForSync:
        text = "WAITING FOR SYNC";
        break;
    case Status::SyncFound:
        text = "SYNC FOUND";
        break;
    case Status::CapturingLow:
        text = "CAPTURING LOW";
        break;
    case Status::CapturingHigh:
        text = "CAPTURING HIGH";
        break;
    case Status::InvalidTestLevel:
        text = "INVALID TEST LEVEL";
        break;
    case Status::LevelTooHigh:
        text = "LEVEL TOO HIGH";
        break;
    case Status::LevelTooLow:
        text = "LEVEL TOO LOW";
        break;
    case Status::DecreaseBias:
        text = "DECREASE BIAS";
        break;
    case Status::IncreaseBias:
        text = "INCREASE BIAS";
        break;
    case Status::ChannelMismatch:
        text = "CHANNEL MISMATCH";
        break;
    case Status::BiasBorderline:
        text = isTwoHeadState() ? "BIAS MATCHED / BORDERLINE" : "BIAS ACCEPTABLE";
        break;
    case Status::CalibrationPassed:
        text = "CALIBRATION PASSED";
        break;
    case Status::CalibrationCompleted:
        text = "CALIBRATION COMPLETED";
        break;
    }
    display.setAppStatus(text);
    displayedStatus = status;
    statusDisplayed = true;
}

bool Record::handleTapeResponseTouch(
    uint16_t x,
    uint16_t y,
    bool returnToSystemSettingsOnBack)
{
    const bool running =
        state == CalibrationState::ThreeHeadLoopbackRunning ||
        state == CalibrationState::ThreeHeadLoopbackReview ||
        state == CalibrationState::ThreeHeadLoopbackVerifying ||
        state == CalibrationState::ThreeHeadResponseRunning ||
        state == CalibrationState::TwoHeadResponseRecording ||
        state == CalibrationState::TwoHeadResponseWaitingSync ||
        state == CalibrationState::TwoHeadResponseAnalyzing;

    if (y >= FOOTER_Y && !running)
    {
        while (touch.pressed())
            delay(5);

        promptAndSaveTapeEq();

        const bool backToSystemSettings =
            state == CalibrationState::ThreeHeadLoopbackReady ||
            state == CalibrationState::ThreeHeadLoopbackPreflightFailed ||
            state == CalibrationState::ThreeHeadLoopbackSignalLost ||
            state == CalibrationState::ThreeHeadLoopbackSavePrompt ||
            state == CalibrationState::ThreeHeadLoopbackResult ||
            state == CalibrationState::ThreeHeadResponseCalibrationRequired ||
            state == CalibrationState::ThreeHeadResponseReady ||
            state == CalibrationState::ThreeHeadResponseResult;
        if (returnToSystemSettingsOnBack && backToSystemSettings)
        {
            stopGenerator();
            return false;
        }

        stopGenerator();
        resetTapeResponse();
        tapeEqFlowActive = false;
        state = CalibrationState::SelectTest;
        selectedRecordActionIndex = 0;
        drawScreen();
        return true;
    }

    if (state == CalibrationState::ThreeHeadLoopbackPreflightFailed ||
        state == CalibrationState::ThreeHeadLoopbackSignalLost)
    {
        const bool retest = contains(
            x, y, LOOPBACK_RESULT_AUTO_CAL_X, LOOPBACK_RESULT_ICON_Y,
            LOOPBACK_RESULT_ICON_SIZE, LOOPBACK_RESULT_ICON_SIZE);
        const bool returnToResponse = contains(
            x, y, LOOPBACK_RESULT_TAPE_TEST_X, LOOPBACK_RESULT_ICON_Y,
            LOOPBACK_RESULT_ICON_SIZE, LOOPBACK_RESULT_ICON_SIZE);
        if (!retest && !returnToResponse)
        {
            while (touch.pressed())
                delay(5);
            return true;
        }
        while (touch.pressed())
            delay(5);
        if (retest)
            startLoopbackCalibration(false);
        else if (returnToSystemSettingsOnBack)
            return false;
        else
        {
            tapeEqFlowActive = false;
            state = CalibrationState::SelectTest;
            selectedRecordActionIndex = 0;
        }
        drawScreen();
        return true;
    }

    if (state == CalibrationState::ThreeHeadLoopbackReview)
    {
        while (touch.pressed())
            delay(5);
        tapeResponseVerifyPass = 1;
        startLoopbackCalibration(true);
        drawScreen();
        return true;
    }

    if (state == CalibrationState::ThreeHeadLoopbackSavePrompt)
    {
        const bool yes = contains(
            x, y, RESPONSE_CHOICE_X, RESPONSE_RESULT_FIRST_Y,
            RESPONSE_CHOICE_WIDTH, RESPONSE_CHOICE_HEIGHT);
        const bool no = contains(
            x, y, RESPONSE_CHOICE_X, RESPONSE_RESULT_SECOND_Y,
            RESPONSE_CHOICE_WIDTH, RESPONSE_CHOICE_HEIGHT);
        if (!yes && !no)
        {
            while (touch.pressed())
                delay(5);
            return true;
        }
        while (touch.pressed())
            delay(5);

        applyLoopbackCandidateCalibration();
        if (yes)
        {
            generatorCalibrationSaved = saveActiveGeneratorCalibration();
            generatorCalibrationSaveFailed = !generatorCalibrationSaved;
        }
        else
        {
            loopbackCalibrationNotSaved = true;
        }
        state = CalibrationState::ThreeHeadLoopbackResult;
        drawScreen();

        if (generatorCalibrationSaveFailed)
            buzzer.play(BeepPattern::Failure);
        else
            buzzer.play(BeepPattern::Success);

        return true;
    }

    if ((state == CalibrationState::TwoHeadResponseWaitingSync &&
         twoHeadEqSyncFound) ||
        state == CalibrationState::TwoHeadResponseAnalyzing)
    {
        while (touch.pressed())
            delay(5);
        return true;
    }

    if (state == CalibrationState::TwoHeadResponseReadyToRecord ||
        state == CalibrationState::TwoHeadResponseRecording ||
        state == CalibrationState::TwoHeadResponseRewind ||
        state == CalibrationState::TwoHeadResponseReadyToAnalyze ||
        state == CalibrationState::TwoHeadResponseWaitingSync ||
        state == CalibrationState::TwoHeadResponseAnalyzing ||
        state == CalibrationState::TwoHeadResponseResult)
    {
        const bool result = state == CalibrationState::TwoHeadResponseResult;
        const bool graphResult = result && tapeResponseResult.valid;
        const int viewButtonHeight =
            ((tft.fontHeight(ITEM_FONT) + 2) * 3) / 2;
        if (graphResult && contains(
                               x,
                               y,
                               TAPE_RESPONSE_VIEW_X,
                               TAPE_RESPONSE_VIEW_Y,
                               TAPE_RESPONSE_VIEW_WIDTH,
                               viewButtonHeight))
        {
            while (touch.pressed())
                delay(5);
            tapeResponseCombinedView = !tapeResponseCombinedView;
            drawTapeResponseResultScreen();
            return true;
        }
        const int buttonX = graphResult ? TAPE_RESPONSE_RETEST_X : START_STOP_X;
        const int buttonY = graphResult ? TAPE_RESPONSE_RETEST_Y : START_STOP_Y;
        const int buttonWidth = graphResult ? TAPE_RESPONSE_RETEST_WIDTH : START_STOP_WIDTH;
        const int buttonHeight = graphResult
                                     ? ((tft.fontHeight(ITEM_FONT) + 2) * 3) / 2
                                     : START_STOP_HEIGHT;
        if (!contains(x, y, buttonX, buttonY, buttonWidth, buttonHeight))
        {
            while (touch.pressed())
                delay(5);
            return true;
        }

        if (graphResult)
            drawTapeResponseResultRetestButton(true);
        else
            drawActionButton(true);
        while (touch.pressed())
            delay(5);

        if (graphResult)
            promptAndSaveTapeEq();

        handleTwoHeadTapeResponseAction();
        drawScreen();
        return true;
    }

    const bool hasActionButton =
        state == CalibrationState::ThreeHeadLoopbackReady ||
        state == CalibrationState::ThreeHeadLoopbackRunning ||
        state == CalibrationState::ThreeHeadLoopbackVerifying ||
        state == CalibrationState::ThreeHeadResponseReady ||
        state == CalibrationState::ThreeHeadResponseRunning ||
        state == CalibrationState::ThreeHeadResponseResult;
    const int viewButtonHeight =
        ((tft.fontHeight(ITEM_FONT) + 2) * 3) / 2;
    if (state == CalibrationState::ThreeHeadResponseResult &&
        tapeResponseResult.valid &&
        contains(
            x,
            y,
            TAPE_RESPONSE_VIEW_X,
            TAPE_RESPONSE_VIEW_Y,
            TAPE_RESPONSE_VIEW_WIDTH,
            viewButtonHeight))
    {
        while (touch.pressed())
            delay(5);
        tapeResponseCombinedView = !tapeResponseCombinedView;
        drawTapeResponseResultScreen();
        return true;
    }
    const int actionX = state == CalibrationState::ThreeHeadResponseResult
                            ? TAPE_RESPONSE_RETEST_X
                            : START_STOP_X;
    const int actionY = state == CalibrationState::ThreeHeadResponseResult
                            ? TAPE_RESPONSE_RETEST_Y
                            : START_STOP_Y;
    const int actionWidth = state == CalibrationState::ThreeHeadResponseResult
                                ? TAPE_RESPONSE_RETEST_WIDTH
                                : START_STOP_WIDTH;
    const int actionHeight = state == CalibrationState::ThreeHeadResponseResult
                                 ? ((tft.fontHeight(ITEM_FONT) + 2) * 3) / 2
                                 : START_STOP_HEIGHT;
    if (hasActionButton && contains(
                               x,
                               y,
                               actionX,
                               actionY,
                               actionWidth,
                               actionHeight))
    {
        if (state == CalibrationState::ThreeHeadResponseResult)
            drawTapeResponseResultRetestButton(true);
        else
            drawActionButton(true);
        while (touch.pressed())
            delay(5);

        if (state == CalibrationState::ThreeHeadResponseResult)
            promptAndSaveTapeEq();

        if (state == CalibrationState::ThreeHeadLoopbackReady)
            startLoopbackCalibration(false);
        else if (state == CalibrationState::ThreeHeadLoopbackRunning ||
                 state == CalibrationState::ThreeHeadLoopbackVerifying)
            stopLoopbackCalibration();
        else if (state == CalibrationState::ThreeHeadResponseReady)
        {
            if (tapeResponseCalibration.valid)
                startTapeResponse();
            else
                state = CalibrationState::ThreeHeadResponseCalibrationRequired;
        }
        else if (state == CalibrationState::ThreeHeadResponseRunning)
            stopTapeResponse();
        else if (state == CalibrationState::ThreeHeadResponseResult)
        {
            resetTapeResponse();
            state = CalibrationState::ThreeHeadResponseReady;
        }
        else
            return true;

        drawScreen();
        return true;
    }

    if (state == CalibrationState::ThreeHeadLoopbackResult &&
        loopbackCandidateCalibration.valid)
    {
        const bool autoCal = contains(
            x,
            y,
            LOOPBACK_RESULT_TAPE_TEST_X,
            LOOPBACK_RESULT_ACTION_ICON_Y,
            LOOPBACK_RESULT_ICON_SIZE,
            LOOPBACK_RESULT_ICON_SIZE);
        const bool tapeTest = contains(
            x,
            y,
            LOOPBACK_RESULT_AUTO_CAL_X,
            LOOPBACK_RESULT_ACTION_ICON_Y,
            LOOPBACK_RESULT_ICON_SIZE,
            LOOPBACK_RESULT_ICON_SIZE);

        while (touch.pressed())
            delay(5);

        if (autoCal)
        {
            startLoopbackCalibration(false);
            drawScreen();
        }
        else if (tapeTest)
        {
            resetTapeResponse();
            state = deckType == DeckType::TwoHead
                        ? CalibrationState::TwoHeadResponseReadyToRecord
                        : CalibrationState::ThreeHeadResponseReady;
            drawScreen();
        }
        return true;
    }

    if (state == CalibrationState::ThreeHeadLoopbackResult &&
        !loopbackCandidateCalibration.valid)
    {
        const bool retest = contains(
            x, y, START_STOP_X, START_STOP_Y,
            START_STOP_WIDTH, START_STOP_HEIGHT);
        if (!retest)
        {
            while (touch.pressed())
                delay(5);
            return true;
        }

        while (touch.pressed())
            delay(5);
        startLoopbackCalibration(false);
        drawScreen();
        return true;
    }

    if (state == CalibrationState::ThreeHeadResponseCalibrationRequired)
    {
        while (touch.pressed())
            delay(5);
        return true;
    }

    const int responseFirstY =
        state == CalibrationState::ThreeHeadLoopbackResult
            ? RESPONSE_RESULT_FIRST_Y
            : RESPONSE_CHOICE_FIRST_Y;
    const int responseSecondY =
        state == CalibrationState::ThreeHeadLoopbackResult
            ? RESPONSE_RESULT_SECOND_Y
            : RESPONSE_CHOICE_SECOND_Y;
    const bool firstChoice =
        (state != CalibrationState::ThreeHeadLoopbackResult ||
         tapeResponseCalibration.valid) &&
        contains(
            x,
            y,
            RESPONSE_CHOICE_X,
            responseFirstY,
            RESPONSE_CHOICE_WIDTH,
            RESPONSE_CHOICE_HEIGHT);
    const bool secondChoice = contains(
        x,
        y,
        RESPONSE_CHOICE_X,
        responseSecondY,
        RESPONSE_CHOICE_WIDTH,
        RESPONSE_CHOICE_HEIGHT);
    if (!firstChoice && !secondChoice)
    {
        while (touch.pressed())
            delay(5);
        return true;
    }

    while (touch.pressed())
        delay(5);

    if (state == CalibrationState::ThreeHeadLoopbackResult)
    {
        if (tapeResponseCalibration.valid && firstChoice)
        {
            resetTapeResponse();
            state = deckType == DeckType::TwoHead
                        ? CalibrationState::TwoHeadResponseReadyToRecord
                        : CalibrationState::ThreeHeadResponseReady;
        }
        else if (secondChoice)
        {
            startLoopbackCalibration(false);
        }
    }

    drawScreen();
    return true;
}

bool Record::measureTapeResponseWindow(
    float &leftRms,
    float &rightRms,
    bool &leftValid,
    bool &rightValid,
    bool &clipped)
{
    float sumLeft = 0.0f;
    float sumRight = 0.0f;
    clipped = false;
    for (uint16_t i = 0; i < SAMPLE_COUNT; ++i)
    {
        const uint32_t sampleStart = micros();
        const int16_t sampleLeft = audio.readLeftCentered();
        const int16_t sampleRight = audio.readRightCentered();
        if (abs(sampleLeft) >= 2000 || abs(sampleRight) >= 2000)
            clipped = true;
        sumLeft += static_cast<float>(sampleLeft) * sampleLeft;
        sumRight += static_cast<float>(sampleRight) * sampleRight;
        while (micros() - sampleStart < SignalFrequency::SAMPLE_PERIOD_US)
        {
        }
    }

    const float reference = setAudio.rmsReference();
    if (reference <= 0.0f)
    {
        leftRms = 0.0f;
        rightRms = 0.0f;
        leftValid = false;
        rightValid = false;
        return false;
    }

    leftRms = sqrtf(sumLeft / SAMPLE_COUNT);
    rightRms = sqrtf(sumRight / SAMPLE_COUNT);
    leftValid = rmsToDb(leftRms, reference) >= MIN_SIGNAL_DB;
    rightValid = rmsToDb(rightRms, reference) >= MIN_SIGNAL_DB;
    if (clipped)
    {
        leftValid = false;
        rightValid = false;
    }
    return leftValid || rightValid;
}

bool Record::measureTapeResponseToneWindow(
    float &leftRms,
    float &rightRms,
    float &leftFrequencyHz,
    float &rightFrequencyHz,
    bool &leftValid,
    bool &rightValid,
    bool &clipped)
{
    float sumLeft = 0.0f;
    float sumRight = 0.0f;
    clipped = false;
    const uint32_t captureStartedUs = micros();
    for (uint16_t i = 0; i < SAMPLE_COUNT; ++i)
    {
        const uint32_t sampleStart = micros();
        const int16_t sampleLeft = audio.readLeftCentered();
        const int16_t sampleRight = audio.readRightCentered();
        frequencySamples[i] = sampleLeft;
        amplifiedFrequencySamples[i] = sampleRight;
        if (abs(sampleLeft) >= 2000 || abs(sampleRight) >= 2000)
            clipped = true;
        sumLeft += static_cast<float>(sampleLeft) * sampleLeft;
        sumRight += static_cast<float>(sampleRight) * sampleRight;
        while (micros() - sampleStart < SignalFrequency::SAMPLE_PERIOD_US)
        {
        }
    }

    leftRms = sqrtf(sumLeft / SAMPLE_COUNT);
    rightRms = sqrtf(sumRight / SAMPLE_COUNT);
    leftFrequencyHz = 0.0f;
    rightFrequencyHz = 0.0f;
    const float reference = setAudio.rmsReference();
    if (!isfinite(reference) || reference <= 0.0f)
    {
        leftValid = false;
        rightValid = false;
        return false;
    }

    leftValid = rmsToDb(leftRms, reference) >= MIN_SIGNAL_DB;
    rightValid = rmsToDb(rightRms, reference) >= MIN_SIGNAL_DB;
    if (clipped)
    {
        leftValid = false;
        rightValid = false;
        return false;
    }

    const uint32_t captureElapsedUs = micros() - captureStartedUs;
    const float frequencyTimeScale = captureElapsedUs > 0
                                         ? (static_cast<float>(SAMPLE_COUNT) * SignalFrequency::SAMPLE_PERIOD_US) /
                                               static_cast<float>(captureElapsedUs)
                                         : 1.0f;
    if (leftValid)
        leftFrequencyHz = SignalFrequency::measureInterpolated(
                              frequencySamples,
                              SAMPLE_COUNT) *
                          frequencyTimeScale;
    if (rightValid)
        rightFrequencyHz = SignalFrequency::measureInterpolated(
                               amplifiedFrequencySamples,
                               SAMPLE_COUNT) *
                           frequencyTimeScale;
    return leftValid || rightValid;
}

void Record::resetTapeResponse()
{
    tapeResponseResult.valid = false;
    tapeResponseCombinedView = false;
    for (uint8_t i = 0; i < TAPE_RESPONSE_BAND_COUNT; ++i)
    {
        tapeResponseResult.leftDb[i] = NAN;
        tapeResponseResult.rightDb[i] = NAN;
        tapeResponseResult.leftRelativeDb[i] = NAN;
        tapeResponseResult.rightRelativeDb[i] = NAN;
        tapeResponseResult.combinedDb[i] = NAN;
        tapeResponseResult.combinedRelativeDb[i] = NAN;
    }

    tapeResponseBandIndex = 0;
    tapeResponseBandStartedMs = 0;
    tapeResponseSumLeftRms = 0.0f;
    tapeResponseSumRightRms = 0.0f;
    tapeResponseLeftWindowCount = 0;
    tapeResponseRightWindowCount = 0;
    tapeResponseLiveLeftDb = 0.0f;
    tapeResponseLiveRightDb = 0.0f;
    tapeResponseLiveLeftValid = false;
    tapeResponseLiveRightValid = false;
    tapeResponseBandClipped = false;
    tapeResponsePrecheckActive = false;
    tapeResponsePrecheckStartedMs = 0;
    tapeResponsePrecheckSumLeftRms = 0.0f;
    tapeResponsePrecheckSumRightRms = 0.0f;
    tapeResponsePrecheckLeftWindowCount = 0;
    tapeResponsePrecheckRightWindowCount = 0;
    tapeResponsePrecheckLeftToneCount = 0;
    tapeResponsePrecheckRightToneCount = 0;
    tapeResponsePrecheckClipped = false;
    tapeResponseSignalLost = false;
    displayedTapeResponseBand = 0xFF;
    displayedTapeResponseLeftTenths = -32768;
    displayedTapeResponseRightTenths = -32768;
    displayedTapeResponseLeftValid = false;
    displayedTapeResponseRightValid = false;
}

void Record::resetTapeResponseCalibration()
{
    loopbackCandidateCalibration.valid = false;
    for (uint8_t i = 0; i < TAPE_RESPONSE_BAND_COUNT; ++i)
    {
        loopbackCandidateCalibration.measuredDb[i] = NAN;
        loopbackCandidateCalibration.relativeDb[i] = NAN;
        loopbackCandidateCalibration.generatorCorrectionDb[i] = 0.0f;
    }
    tapeResponseCalibrationFailed = false;
    tapeResponseVerificationFailed = false;
    tapeResponseMaxCorrectionDb = 0.0f;
    tapeResponseVerifyPass = 0;
    tapeResponseMaxVerifyErrorDb = NAN;
    tapeResponseMaxVerifyErrorBand = TAPE_RESPONSE_REFERENCE_INDEX;
    tapeResponseVerifyRating = TapeResponseVerifyRating::None;
    generatorCalibrationSaved = false;
    generatorCalibrationSaveFailed = false;
    loopbackCalibrationNotSaved = false;
}

void Record::startLoopbackCalibration(bool verification)
{
    stopGenerator();
    if (verification)
    {
        loopbackCandidateCalibration.valid = false;
        for (uint8_t i = 0; i < TAPE_RESPONSE_BAND_COUNT; ++i)
        {
            loopbackCandidateCalibration.measuredDb[i] = NAN;
            loopbackCandidateCalibration.relativeDb[i] = NAN;
        }
        tapeResponseVerificationFailed = false;
        startLoopbackSweep(true);
        return;
    }

    resetTapeResponseCalibration();
    resetTapeResponse();
    loopbackPreflightSumLeftRms = 0.0f;
    loopbackPreflightSumRightRms = 0.0f;
    loopbackPreflightWindowCount = 0;
    loopbackPreflightNoiseLeftDb = NAN;
    loopbackPreflightNoiseRightDb = NAN;
    loopbackPreflightSignalLeftDb = NAN;
    loopbackPreflightSignalRightDb = NAN;
    loopbackPreflightClipped = false;
    loopbackPreflightLeftToneCount = 0;
    loopbackPreflightRightToneCount = 0;
    loopbackPreflightPassed = false;
    loopbackAllBandsValid = true;
    loopbackFailedBand = 0;
    tapeResponseBandStartedMs = millis();
    state = CalibrationState::ThreeHeadLoopbackPreflightNoise;
}

void Record::startLoopbackSweep(bool verification)
{
    resetTapeResponse();
    const float correction = verification
                                 ? loopbackCandidateCalibration.generatorCorrectionDb[0]
                                 : 0.0f;
    if (!setGeneratorOutput(
            TAPE_RESPONSE_FREQUENCIES_HZ[0],
            TAPE_RESPONSE_BASE_LEVEL_DB + correction))
    {
        failLoopbackSignal();
        return;
    }
    ad9833.enable();
    tapeResponseBandStartedMs = millis();
    state = verification
                ? CalibrationState::ThreeHeadLoopbackVerifying
                : CalibrationState::ThreeHeadLoopbackRunning;
}

void Record::updateLoopbackPreflight()
{
    const uint32_t elapsed = millis() - tapeResponseBandStartedMs;
    if (state == CalibrationState::ThreeHeadLoopbackPreflightPassed)
    {
        if (elapsed >= LOOPBACK_PREFLIGHT_PASSED_MS)
        {
            startLoopbackSweep(false);
            drawScreen();
        }
        return;
    }

    if (state == CalibrationState::ThreeHeadLoopbackPreflightNoise &&
        elapsed >= LOOPBACK_PREFLIGHT_NOISE_MS)
    {
        const float reference = setAudio.rmsReference();
        if (reference <= 0.0f || loopbackPreflightWindowCount == 0)
        {
            state = CalibrationState::ThreeHeadLoopbackPreflightFailed;
            drawScreen();
            buzzer.play(BeepPattern::Failure);
            return;
        }
        loopbackPreflightNoiseLeftDb = rmsToDb(
            loopbackPreflightSumLeftRms / loopbackPreflightWindowCount, reference);
        loopbackPreflightNoiseRightDb = rmsToDb(
            loopbackPreflightSumRightRms / loopbackPreflightWindowCount, reference);
        loopbackPreflightSumLeftRms = 0.0f;
        loopbackPreflightSumRightRms = 0.0f;
        loopbackPreflightWindowCount = 0;
        if (!setGeneratorOutput(LOOPBACK_PREFLIGHT_FREQUENCY_HZ,
                                TAPE_RESPONSE_BASE_LEVEL_DB))
        {
            stopGenerator();
            state = CalibrationState::ThreeHeadLoopbackPreflightFailed;
            drawScreen();
            buzzer.play(BeepPattern::Failure);
            return;
        }
        ad9833.enable();
        tapeResponseBandStartedMs = millis();
        state = CalibrationState::ThreeHeadLoopbackPreflightSignal;
        return;
    }

    if (state == CalibrationState::ThreeHeadLoopbackPreflightSignal &&
        elapsed >= LOOPBACK_PREFLIGHT_SETTLE_MS + LOOPBACK_PREFLIGHT_SIGNAL_MS)
    {
        const float reference = setAudio.rmsReference();
        if (reference <= 0.0f || loopbackPreflightWindowCount == 0)
        {
            stopGenerator();
            state = CalibrationState::ThreeHeadLoopbackPreflightFailed;
            drawScreen();
            buzzer.play(BeepPattern::Failure);
            return;
        }
        loopbackPreflightSignalLeftDb = rmsToDb(
            loopbackPreflightSumLeftRms / loopbackPreflightWindowCount, reference);
        loopbackPreflightSignalRightDb = rmsToDb(
            loopbackPreflightSumRightRms / loopbackPreflightWindowCount, reference);
        const bool passed = !loopbackPreflightClipped &&
                            isfinite(loopbackPreflightNoiseLeftDb) &&
                            isfinite(loopbackPreflightNoiseRightDb) &&
                            isfinite(loopbackPreflightSignalLeftDb) &&
                            isfinite(loopbackPreflightSignalRightDb) &&
                            automaticPrecheckLevelValid(
                                loopbackPreflightSignalLeftDb) &&
                            automaticPrecheckLevelValid(
                                loopbackPreflightSignalRightDb) &&
                            loopbackPreflightLeftToneCount >=
                                AUTOMATIC_TEST_REQUIRED_WINDOWS &&
                            loopbackPreflightRightToneCount >=
                                AUTOMATIC_TEST_REQUIRED_WINDOWS &&
                            loopbackPreflightSignalLeftDb - loopbackPreflightNoiseLeftDb >=
                                LOOPBACK_MIN_SIGNAL_RISE_DB &&
                            loopbackPreflightSignalRightDb - loopbackPreflightNoiseRightDb >=
                                LOOPBACK_MIN_SIGNAL_RISE_DB &&
                            fabsf(loopbackPreflightSignalLeftDb - loopbackPreflightSignalRightDb) <=
                                LOOPBACK_MAX_CHANNEL_DIFFERENCE_DB;
        if (!passed)
        {
            stopGenerator();
            state = CalibrationState::ThreeHeadLoopbackPreflightFailed;
        }
        else
        {
            loopbackPreflightPassed = true;
            state = CalibrationState::ThreeHeadLoopbackPreflightPassed;
            tapeResponseBandStartedMs = millis();

            // Update only the two phase labels; a full screen redraw here
            // produces visible flicker between the preflight checks.
            tft.setTextDatum(TL_DATUM);
            tft.setTextPadding(CONTENT_WIDTH - TEXT_X);
            tft.setTextColor(TFT_GREEN, COL_BG);
            tft.drawString("LOOPBACK DETECTED", TEXT_X, 30, ITEM_FONT);
            tft.setTextColor(COL_TEXT, COL_BG);
            tft.drawString("STARTING AUTO CAL", TEXT_X, 56, ITEM_FONT);
            tft.setTextPadding(0);
        }
        if (!passed)
        {
            drawScreen();
            buzzer.play(BeepPattern::Failure);
        }
        return;
    }

    if (state == CalibrationState::ThreeHeadLoopbackPreflightSignal &&
        elapsed < LOOPBACK_PREFLIGHT_SETTLE_MS)
        return;

    float leftRms = 0.0f;
    float rightRms = 0.0f;
    bool leftValid = false;
    bool rightValid = false;
    bool clipped = false;
    if (state == CalibrationState::ThreeHeadLoopbackPreflightSignal)
    {
        float leftFrequencyHz = 0.0f;
        float rightFrequencyHz = 0.0f;
        measureTapeResponseToneWindow(
            leftRms,
            rightRms,
            leftFrequencyHz,
            rightFrequencyHz,
            leftValid,
            rightValid,
            clipped);
        if (leftValid && sweepFrequencyMatches(
                             leftFrequencyHz,
                             LOOPBACK_PREFLIGHT_FREQUENCY_HZ))
            ++loopbackPreflightLeftToneCount;
        if (rightValid && sweepFrequencyMatches(
                              rightFrequencyHz,
                              LOOPBACK_PREFLIGHT_FREQUENCY_HZ))
            ++loopbackPreflightRightToneCount;
    }
    else
    {
        measureTapeResponseWindow(
            leftRms,
            rightRms,
            leftValid,
            rightValid,
            clipped);
    }
    loopbackPreflightClipped = loopbackPreflightClipped || clipped;
    loopbackPreflightSumLeftRms += leftRms;
    loopbackPreflightSumRightRms += rightRms;
    ++loopbackPreflightWindowCount;
}

void Record::stopLoopbackCalibration()
{
    stopGenerator();
    resetTapeResponse();
    resetTapeResponseCalibration();
    state = CalibrationState::ThreeHeadLoopbackReady;
}

void Record::updateLoopbackCalibration()
{
    if ((state != CalibrationState::ThreeHeadLoopbackRunning &&
         state != CalibrationState::ThreeHeadLoopbackVerifying) ||
        tapeResponseBandIndex >= TAPE_RESPONSE_BAND_COUNT)
        return;

    const uint32_t elapsed = millis() - tapeResponseBandStartedMs;
    const uint32_t settleMs = tapeResponseSettleMs(tapeResponseBandIndex);
    const uint32_t measureMs = tapeResponseMeasureMs(tapeResponseBandIndex);
    if (elapsed < settleMs)
        return;

    if (state == CalibrationState::ThreeHeadLoopbackVerifying &&
        tapeResponseBandIndex == 0 &&
        elapsed < settleMs + 300)
    {
        return;
    }

    if (elapsed < settleMs + measureMs)
    {
        float leftRms = 0.0f;
        float rightRms = 0.0f;
        float leftFrequencyHz = 0.0f;
        float rightFrequencyHz = 0.0f;
        bool leftValid = false;
        bool rightValid = false;
        bool clipped = false;
        measureTapeResponseToneWindow(
            leftRms,
            rightRms,
            leftFrequencyHz,
            rightFrequencyHz,
            leftValid,
            rightValid,
            clipped);
        tapeResponseBandClipped = tapeResponseBandClipped || clipped;

        const float reference = setAudio.rmsReference();
        const uint16_t expectedHz =
            TAPE_RESPONSE_FREQUENCIES_HZ[tapeResponseBandIndex];
        const float leftDb = reference > 0.0f
                                 ? rmsToDb(leftRms, reference)
                                 : NAN;
        const float rightDb = reference > 0.0f
                                  ? rmsToDb(rightRms, reference)
                                  : NAN;
        if (leftValid && automaticLevelPlausible(leftDb) &&
            sweepFrequencyMatches(leftFrequencyHz, expectedHz))
        {
            tapeResponseSumLeftRms += leftRms;
            ++tapeResponseLeftWindowCount;
            tapeResponseLiveLeftDb = rmsToDb(
                tapeResponseSumLeftRms / tapeResponseLeftWindowCount,
                reference);
            tapeResponseLiveLeftValid = true;
        }
        if (rightValid && automaticLevelPlausible(rightDb) &&
            sweepFrequencyMatches(rightFrequencyHz, expectedHz))
        {
            tapeResponseSumRightRms += rightRms;
            ++tapeResponseRightWindowCount;
            tapeResponseLiveRightDb = rmsToDb(
                tapeResponseSumRightRms / tapeResponseRightWindowCount,
                reference);
            tapeResponseLiveRightValid = true;
        }
        return;
    }

    finishLoopbackBand();
}

void Record::finishLoopbackBand()
{
    const float reference = setAudio.rmsReference();
    if (tapeResponseBandClipped || reference <= 0.0f ||
        tapeResponseLeftWindowCount < AUTOMATIC_TEST_REQUIRED_WINDOWS ||
        tapeResponseRightWindowCount < AUTOMATIC_TEST_REQUIRED_WINDOWS)
    {
        failLoopbackSignal();
        return;
    }
    const float leftRms = tapeResponseSumLeftRms / tapeResponseLeftWindowCount;
    const float rightRms = tapeResponseSumRightRms / tapeResponseRightWindowCount;
    const float leftDb = rmsToDb(leftRms, reference);
    const float rightDb = rmsToDb(rightRms, reference);
    if (!isfinite(leftDb) || !isfinite(rightDb) ||
        leftDb < LOOPBACK_EXPECTED_MIN_DB || leftDb > LOOPBACK_EXPECTED_MAX_DB ||
        rightDb < LOOPBACK_EXPECTED_MIN_DB || rightDb > LOOPBACK_EXPECTED_MAX_DB ||
        fabsf(leftDb - rightDb) > LOOPBACK_MAX_CHANNEL_DIFFERENCE_DB)
    {
        failLoopbackSignal();
        return;
    }
    loopbackCandidateCalibration.measuredDb[tapeResponseBandIndex] =
        0.5f * (leftDb + rightDb);

    ++tapeResponseBandIndex;
    if (tapeResponseBandIndex >= TAPE_RESPONSE_BAND_COUNT)
    {
        stopGenerator();
        if (state == CalibrationState::ThreeHeadLoopbackRunning)
        {
            calculateLoopbackCorrections();
            state = CalibrationState::ThreeHeadLoopbackReview;
            drawScreen();
            return;
        }
        else
        {
            const bool verified = validateLoopbackVerification();
            if (tapeResponseVerifyPass == 1 &&
                refineLoopbackCorrections())
            {
                tapeResponseVerifyPass = 2;
                startLoopbackCalibration(true);
                drawScreen();
                return;
            }
            loopbackCandidateCalibration.valid = verified;
            if (verified && loopbackPreflightPassed && loopbackAllBandsValid &&
                tapeResponseVerifyRating == TapeResponseVerifyRating::Excellent)
            {
                applyLoopbackCandidateCalibration();
                display.openApp("RECORD CALIBRATION", "SAVING...");
                tft.setTextDatum(TL_DATUM);
                tft.setTextColor(TFT_GREEN, COL_BG);
                tft.drawString("LOOPBACK CAL EXCELLENT", TEXT_X, 30, ITEM_FONT);
                tft.setTextColor(COL_TEXT, COL_BG);
                tft.drawString("SAVING...", TEXT_X, 56, ITEM_FONT);
                generatorCalibrationSaved = saveActiveGeneratorCalibration();
                generatorCalibrationSaveFailed = !generatorCalibrationSaved;
            }
        }

        state = loopbackCandidateCalibration.valid && loopbackPreflightPassed &&
                        loopbackAllBandsValid &&
                        tapeResponseVerifyRating == TapeResponseVerifyRating::Ok
                    ? CalibrationState::ThreeHeadLoopbackSavePrompt
                    : CalibrationState::ThreeHeadLoopbackResult;
        drawScreen();
        if (state == CalibrationState::ThreeHeadLoopbackResult)
        {
            if (generatorCalibrationSaveFailed ||
                !loopbackCandidateCalibration.valid)
            {
                buzzer.play(BeepPattern::Failure);
            }
            else
            {
                buzzer.play(BeepPattern::Success);
            }
        }
        return;
    }

    const bool verification =
        state == CalibrationState::ThreeHeadLoopbackVerifying;
    const float correction = verification
                                 ? loopbackCandidateCalibration.generatorCorrectionDb[tapeResponseBandIndex]
                                 : 0.0f;
    tapeResponseSumLeftRms = 0.0f;
    tapeResponseSumRightRms = 0.0f;
    tapeResponseLeftWindowCount = 0;
    tapeResponseRightWindowCount = 0;
    tapeResponseLiveLeftDb = 0.0f;
    tapeResponseLiveRightDb = 0.0f;
    tapeResponseLiveLeftValid = false;
    tapeResponseLiveRightValid = false;
    tapeResponseBandClipped = false;
    if (!setGeneratorOutput(
            TAPE_RESPONSE_FREQUENCIES_HZ[tapeResponseBandIndex],
            TAPE_RESPONSE_BASE_LEVEL_DB + correction))
    {
        failLoopbackSignal();
        return;
    }
    tapeResponseBandStartedMs = millis();
    displayedTapeResponseBand = 0xFF;
}

void Record::failLoopbackSignal()
{
    stopGenerator();
    loopbackAllBandsValid = false;
    loopbackFailedBand = tapeResponseBandIndex;
    if (tapeResponseBandIndex < TAPE_RESPONSE_BAND_COUNT)
    {
        if (tapeResponseLiveLeftValid && tapeResponseLiveRightValid)
        {
            loopbackCandidateCalibration.measuredDb[tapeResponseBandIndex] =
                0.5f * (tapeResponseLiveLeftDb + tapeResponseLiveRightDb);
        }
        else if (tapeResponseLiveLeftValid)
        {
            loopbackCandidateCalibration.measuredDb[tapeResponseBandIndex] =
                tapeResponseLiveLeftDb;
        }
        else if (tapeResponseLiveRightValid)
        {
            loopbackCandidateCalibration.measuredDb[tapeResponseBandIndex] =
                tapeResponseLiveRightDb;
        }
    }
    loopbackCandidateCalibration.valid = false;
    state = CalibrationState::ThreeHeadLoopbackSignalLost;
    drawScreen();
    buzzer.play(BeepPattern::Failure);
}

bool Record::calculateLoopbackCorrections()
{
    const float reference =
        loopbackCandidateCalibration.measuredDb[TAPE_RESPONSE_REFERENCE_INDEX];
    if (!isfinite(reference))
    {
        tapeResponseCalibrationFailed = true;
        return false;
    }

    tapeResponseMaxCorrectionDb = 0.0f;
    for (uint8_t i = 0; i < TAPE_RESPONSE_BAND_COUNT; ++i)
    {
        if (!isfinite(loopbackCandidateCalibration.measuredDb[i]))
        {
            tapeResponseCalibrationFailed = true;
            return false;
        }

        const float relative =
            loopbackCandidateCalibration.measuredDb[i] - reference;
        const float correction = -relative;
        loopbackCandidateCalibration.relativeDb[i] = relative;
        loopbackCandidateCalibration.generatorCorrectionDb[i] = correction;
        const float absoluteCorrection = fabsf(correction);
        if (absoluteCorrection > tapeResponseMaxCorrectionDb)
            tapeResponseMaxCorrectionDb = absoluteCorrection;
        if (absoluteCorrection > TAPE_RESPONSE_MAX_AUTO_CORRECTION_DB)
        {
            tapeResponseCalibrationFailed = true;
            return false;
        }
    }

    loopbackCandidateCalibration.relativeDb[TAPE_RESPONSE_REFERENCE_INDEX] = 0.0f;
    loopbackCandidateCalibration.generatorCorrectionDb[TAPE_RESPONSE_REFERENCE_INDEX] = 0.0f;
    tapeResponseCalibrationFailed = false;
    return true;
}

bool Record::validateLoopbackVerification()
{
    const float reference =
        loopbackCandidateCalibration.measuredDb[TAPE_RESPONSE_REFERENCE_INDEX];
    tapeResponseMaxVerifyErrorDb = NAN;
    tapeResponseMaxVerifyErrorBand = TAPE_RESPONSE_REFERENCE_INDEX;
    tapeResponseVerifyRating = TapeResponseVerifyRating::Failed;
    if (!isfinite(reference))
    {
        tapeResponseVerificationFailed = true;
        return false;
    }

    bool allBandsValid = true;
    float maxAbsoluteError = 0.0f;
    for (uint8_t i = 0; i < TAPE_RESPONSE_BAND_COUNT; ++i)
    {
        if (!isfinite(loopbackCandidateCalibration.measuredDb[i]))
        {
            loopbackCandidateCalibration.relativeDb[i] = NAN;
            if (allBandsValid)
                tapeResponseMaxVerifyErrorBand = i;
            allBandsValid = false;
            continue;
        }
        const float relative =
            loopbackCandidateCalibration.measuredDb[i] - reference;
        loopbackCandidateCalibration.relativeDb[i] = relative;
        const float absoluteError = fabsf(relative);
        if (absoluteError > maxAbsoluteError)
        {
            maxAbsoluteError = absoluteError;
            tapeResponseMaxVerifyErrorBand = i;
        }
    }

    loopbackCandidateCalibration.relativeDb[TAPE_RESPONSE_REFERENCE_INDEX] = 0.0f;
    if (!allBandsValid)
    {
        tapeResponseVerificationFailed = true;
        return false;
    }

    tapeResponseMaxVerifyErrorDb = maxAbsoluteError;
    if (maxAbsoluteError <= TAPE_RESPONSE_VERIFY_EXCELLENT_DB)
        tapeResponseVerifyRating = TapeResponseVerifyRating::Excellent;
    else if (maxAbsoluteError <= TAPE_RESPONSE_VERIFY_OK_DB)
        tapeResponseVerifyRating = TapeResponseVerifyRating::Ok;
    else
        tapeResponseVerifyRating = TapeResponseVerifyRating::Failed;

    tapeResponseVerificationFailed =
        tapeResponseVerifyRating == TapeResponseVerifyRating::Failed;
    return !tapeResponseVerificationFailed;
}

bool Record::refineLoopbackCorrections()
{
    for (uint8_t i = 0; i < TAPE_RESPONSE_BAND_COUNT; ++i)
    {
        if (!isfinite(loopbackCandidateCalibration.relativeDb[i]))
            return false;
    }

    float maxAbsoluteCorrection = 0.0f;
    for (uint8_t i = 0; i < TAPE_RESPONSE_BAND_COUNT; ++i)
    {
        const float verifyRelativeDb =
            loopbackCandidateCalibration.relativeDb[i];
        const float refinedCorrection = constrain(
            loopbackCandidateCalibration.generatorCorrectionDb[i] -
                verifyRelativeDb,
            -TAPE_RESPONSE_MAX_AUTO_CORRECTION_DB,
            TAPE_RESPONSE_MAX_AUTO_CORRECTION_DB);
        loopbackCandidateCalibration.generatorCorrectionDb[i] =
            refinedCorrection;
        const float absoluteCorrection = fabsf(refinedCorrection);
        if (absoluteCorrection > maxAbsoluteCorrection)
            maxAbsoluteCorrection = absoluteCorrection;
    }

    loopbackCandidateCalibration.generatorCorrectionDb[TAPE_RESPONSE_REFERENCE_INDEX] = 0.0f;
    tapeResponseMaxCorrectionDb = maxAbsoluteCorrection;
    return true;
}

void Record::applyLoopbackCandidateCalibration()
{
    tapeResponseCalibration = loopbackCandidateCalibration;
    tapeResponseCalibration.valid = true;
    activeGeneratorCalibrationMaxVerifyErrorDb = tapeResponseMaxVerifyErrorDb;
    activeGeneratorCalibrationVerifyRating = tapeResponseVerifyRating;
    generatorCalibrationLoaded = false;
}

bool Record::saveActiveGeneratorCalibration()
{
    if (!loopbackPreflightPassed || !loopbackAllBandsValid)
        return false;
    GeneratorCalibrationStorage stored = {};
    for (uint8_t i = 0; i < TAPE_RESPONSE_BAND_COUNT; ++i)
        stored.correctionDb[i] = tapeResponseCalibration.generatorCorrectionDb[i];
    stored.maxVerifyErrorDb = tapeResponseMaxVerifyErrorDb;
    stored.verifyGrade = tapeResponseVerifyRating == TapeResponseVerifyRating::Excellent
                             ? GENERATOR_CAL_GRADE_EXCELLENT
                             : GENERATOR_CAL_GRADE_OK;
    return storage.saveGeneratorCalibration(stored);
}

void Record::startTapeResponse()
{
    stopGenerator();
    resetTapeResponse();
    if (!setGeneratorOutput(
            LOOPBACK_PREFLIGHT_FREQUENCY_HZ,
            TAPE_RESPONSE_BASE_LEVEL_DB +
                getGeneratorCalibrationCorrectionDb(
                    LOOPBACK_PREFLIGHT_FREQUENCY_HZ)))
    {
        session.responseTestDone = false;
        if (workflowActive)
            workflowResult.responseValid = false;
        state = CalibrationState::ThreeHeadResponseResult;
        drawScreen();
        buzzer.play(BeepPattern::Failure);
        return;
    }
    ad9833.enable();
    tapeResponsePrecheckActive = true;
    tapeResponsePrecheckStartedMs = millis();
    state = CalibrationState::ThreeHeadResponseRunning;
}

bool Record::startTapeResponseSweep()
{
    tapeResponsePrecheckActive = false;
    tapeResponseBandIndex = 0;
    if (!setGeneratorOutput(
            TAPE_RESPONSE_FREQUENCIES_HZ[0],
            TAPE_RESPONSE_BASE_LEVEL_DB +
                tapeResponseCalibration.generatorCorrectionDb[0]))
    {
        failTapeResponseSignal();
        return false;
    }
    tapeResponseBandStartedMs = millis();
    displayedTapeResponseBand = 0xFF;
    return true;
}

void Record::updateTapeResponsePrecheck()
{
    const uint32_t elapsed = millis() - tapeResponsePrecheckStartedMs;
    if (elapsed < TAPE_RESPONSE_FIRST_BAND_SETTLE_MS)
        return;

    if (elapsed < TAPE_RESPONSE_FIRST_BAND_SETTLE_MS +
                      TAPE_RESPONSE_MEASURE_MS)
    {
        float leftRms = 0.0f;
        float rightRms = 0.0f;
        float leftFrequencyHz = 0.0f;
        float rightFrequencyHz = 0.0f;
        bool leftValid = false;
        bool rightValid = false;
        bool clipped = false;
        measureTapeResponseToneWindow(
            leftRms,
            rightRms,
            leftFrequencyHz,
            rightFrequencyHz,
            leftValid,
            rightValid,
            clipped);

        tapeResponsePrecheckClipped =
            tapeResponsePrecheckClipped || clipped;
        if (leftValid)
        {
            tapeResponsePrecheckSumLeftRms += leftRms;
            ++tapeResponsePrecheckLeftWindowCount;
            if (sweepFrequencyMatches(
                    leftFrequencyHz,
                    LOOPBACK_PREFLIGHT_FREQUENCY_HZ))
                ++tapeResponsePrecheckLeftToneCount;
        }
        if (rightValid)
        {
            tapeResponsePrecheckSumRightRms += rightRms;
            ++tapeResponsePrecheckRightWindowCount;
            if (sweepFrequencyMatches(
                    rightFrequencyHz,
                    LOOPBACK_PREFLIGHT_FREQUENCY_HZ))
                ++tapeResponsePrecheckRightToneCount;
        }
        return;
    }

    const float leftRms = tapeResponsePrecheckLeftWindowCount > 0
                              ? tapeResponsePrecheckSumLeftRms /
                                    tapeResponsePrecheckLeftWindowCount
                              : 0.0f;
    const float rightRms = tapeResponsePrecheckRightWindowCount > 0
                               ? tapeResponsePrecheckSumRightRms /
                                     tapeResponsePrecheckRightWindowCount
                               : 0.0f;
    const float leftDb = rmsToReferenceDb(leftRms);
    const float rightDb = rmsToReferenceDb(rightRms);
    const bool valid =
        !tapeResponsePrecheckClipped &&
        tapeResponsePrecheckLeftToneCount >=
            AUTOMATIC_TEST_REQUIRED_WINDOWS &&
        tapeResponsePrecheckRightToneCount >=
            AUTOMATIC_TEST_REQUIRED_WINDOWS &&
        automaticPrecheckLevelValid(leftDb) &&
        automaticPrecheckLevelValid(rightDb);
    if (!valid)
    {
        failTapeResponseSignal();
        return;
    }

    startTapeResponseSweep();
}

void Record::failTapeResponseSignal()
{
    stopGenerator();
    tapeResponsePrecheckActive = false;
    tapeResponseSignalLost = true;
    tapeResponseResult.valid = false;
    session.responseTestDone = false;
    status = Status::NoSignal;
    state = CalibrationState::ThreeHeadResponseResult;
    drawScreen();
    buzzer.play(BeepPattern::Failure);
}

void Record::stopTapeResponse()
{
    stopGenerator();
    resetTapeResponse();
    state = CalibrationState::ThreeHeadResponseReady;
}

void Record::updateTapeResponse()
{
    if (state != CalibrationState::ThreeHeadResponseRunning ||
        tapeResponseBandIndex >= TAPE_RESPONSE_BAND_COUNT)
        return;

    if (tapeResponsePrecheckActive)
    {
        updateTapeResponsePrecheck();
        return;
    }

    const uint32_t elapsed = millis() - tapeResponseBandStartedMs;
    const uint32_t settleMs = tapeResponseSettleMs(tapeResponseBandIndex);
    const uint32_t measureMs = tapeResponseMeasureMs(tapeResponseBandIndex);
    if (elapsed < settleMs)
        return;

    if (elapsed < settleMs + measureMs)
    {
        float leftRms = 0.0f;
        float rightRms = 0.0f;
        float leftFrequencyHz = 0.0f;
        float rightFrequencyHz = 0.0f;
        bool leftValid = false;
        bool rightValid = false;
        bool clipped = false;
        measureTapeResponseToneWindow(
            leftRms,
            rightRms,
            leftFrequencyHz,
            rightFrequencyHz,
            leftValid,
            rightValid,
            clipped);
        tapeResponseBandClipped = tapeResponseBandClipped || clipped;

        const float reference = setAudio.rmsReference();
        const uint16_t expectedHz =
            TAPE_RESPONSE_FREQUENCIES_HZ[tapeResponseBandIndex];
        const float leftDb = reference > 0.0f
                                 ? rmsToDb(leftRms, reference)
                                 : NAN;
        const float rightDb = reference > 0.0f
                                  ? rmsToDb(rightRms, reference)
                                  : NAN;
        // The 15 kHz point has only 2.67 samples per period in this 40 kS/s
        // validation capture, so its zero-crossing frequency estimate is not
        // a reliable pass/fail gate.  The mandatory 1 kHz pre-check has
        // already verified both playback paths.  At 15 kHz retain the level,
        // clipping and minimum-window checks; all points through 12.5 kHz
        // keep the existing frequency validation.
        const bool frequencyCheckReliable =
            expectedHz <= MAX_RELIABLE_SWEEP_FREQUENCY_HZ;
        if (leftValid && automaticLevelPlausible(leftDb) &&
            (!frequencyCheckReliable ||
             sweepFrequencyMatches(leftFrequencyHz, expectedHz)))
        {
            tapeResponseSumLeftRms += leftRms;
            ++tapeResponseLeftWindowCount;
            tapeResponseLiveLeftDb = rmsToDb(
                tapeResponseSumLeftRms / tapeResponseLeftWindowCount,
                reference);
            tapeResponseLiveLeftValid = true;
        }
        if (rightValid && automaticLevelPlausible(rightDb) &&
            (!frequencyCheckReliable ||
             sweepFrequencyMatches(rightFrequencyHz, expectedHz)))
        {
            tapeResponseSumRightRms += rightRms;
            ++tapeResponseRightWindowCount;
            tapeResponseLiveRightDb = rmsToDb(
                tapeResponseSumRightRms / tapeResponseRightWindowCount,
                reference);
            tapeResponseLiveRightValid = true;
        }
        return;
    }

    finishTapeResponseBand();
}

void Record::finishTapeResponseBand()
{
    storeTapeResponseBand(tapeResponseBandIndex);

    if (!isfinite(tapeResponseResult.leftDb[tapeResponseBandIndex]) ||
        !isfinite(tapeResponseResult.rightDb[tapeResponseBandIndex]))
    {
        failTapeResponseSignal();
        return;
    }

    ++tapeResponseBandIndex;
    if (tapeResponseBandIndex >= TAPE_RESPONSE_BAND_COUNT)
    {
        stopGenerator();
        normalizeTapeResponse();
        session.responseTestDone = tapeResponseResult.valid;
        if (workflowActive)
            workflowResult.responseValid = tapeResponseResult.valid;
        state = CalibrationState::ThreeHeadResponseResult;
        drawScreen();

        buzzer.play(
            tapeResponseResult.valid
                ? BeepPattern::Success
                : BeepPattern::Failure);

        return;
    }

    if (!setGeneratorOutput(
            TAPE_RESPONSE_FREQUENCIES_HZ[tapeResponseBandIndex],
            TAPE_RESPONSE_BASE_LEVEL_DB +
                tapeResponseCalibration.generatorCorrectionDb[tapeResponseBandIndex]))
    {
        session.responseTestDone = false;
        if (workflowActive)
            workflowResult.responseValid = false;
        state = CalibrationState::ThreeHeadResponseResult;
        drawScreen();
        buzzer.play(BeepPattern::Failure);
        return;
    }
    tapeResponseBandStartedMs = millis();
    tapeResponseSumLeftRms = 0.0f;
    tapeResponseSumRightRms = 0.0f;
    tapeResponseLeftWindowCount = 0;
    tapeResponseRightWindowCount = 0;
    tapeResponseLiveLeftDb = 0.0f;
    tapeResponseLiveRightDb = 0.0f;
    tapeResponseLiveLeftValid = false;
    tapeResponseLiveRightValid = false;
    tapeResponseBandClipped = false;
    displayedTapeResponseBand = 0xFF;
}

void Record::storeTapeResponseBand(uint8_t bandIndex)
{
    const float reference = setAudio.rmsReference();
    if (!tapeResponseBandClipped &&
        reference > 0.0f &&
        tapeResponseLeftWindowCount >= AUTOMATIC_TEST_REQUIRED_WINDOWS)
    {
        const float averageRms =
            tapeResponseSumLeftRms / tapeResponseLeftWindowCount;
        const float levelDb = rmsToDb(averageRms, reference);
        if (automaticLevelPlausible(levelDb))
            tapeResponseResult.leftDb[bandIndex] = levelDb;
    }
    if (!tapeResponseBandClipped &&
        reference > 0.0f &&
        tapeResponseRightWindowCount >= AUTOMATIC_TEST_REQUIRED_WINDOWS)
    {
        const float averageRms =
            tapeResponseSumRightRms / tapeResponseRightWindowCount;
        const float levelDb = rmsToDb(averageRms, reference);
        if (automaticLevelPlausible(levelDb))
            tapeResponseResult.rightDb[bandIndex] = levelDb;
    }

    if (!tapeResponseBandClipped &&
        reference > 0.0f &&
        tapeResponseLeftWindowCount >= AUTOMATIC_TEST_REQUIRED_WINDOWS &&
        tapeResponseRightWindowCount >= AUTOMATIC_TEST_REQUIRED_WINDOWS)
    {
        const float averageLeftRms =
            tapeResponseSumLeftRms / tapeResponseLeftWindowCount;
        const float averageRightRms =
            tapeResponseSumRightRms / tapeResponseRightWindowCount;
        const float combinedRms = sqrtf(
            0.5f * (averageLeftRms * averageLeftRms +
                    averageRightRms * averageRightRms));
        const float levelDb = rmsToReferenceDb(combinedRms);
        if (isfinite(levelDb))
            tapeResponseResult.combinedDb[bandIndex] = levelDb;
    }
}

void Record::normalizeTapeResponse()
{
    const float leftReference =
        tapeResponseResult.leftDb[TAPE_RESPONSE_REFERENCE_INDEX];
    const float rightReference =
        tapeResponseResult.rightDb[TAPE_RESPONSE_REFERENCE_INDEX];
    if (!isfinite(leftReference) || !isfinite(rightReference))
    {
        tapeResponseResult.valid = false;
        for (uint8_t i = 0; i < TAPE_RESPONSE_BAND_COUNT; ++i)
        {
            tapeResponseResult.leftRelativeDb[i] = NAN;
            tapeResponseResult.rightRelativeDb[i] = NAN;
            tapeResponseResult.combinedRelativeDb[i] = NAN;
        }
        return;
    }

    const float combinedReference =
        tapeResponseResult.combinedDb[TAPE_RESPONSE_REFERENCE_INDEX];
    bool allBandsValid = true;
    for (uint8_t i = 0; i < TAPE_RESPONSE_BAND_COUNT; ++i)
    {
        if (!isfinite(tapeResponseResult.leftDb[i]) ||
            !isfinite(tapeResponseResult.rightDb[i]))
            allBandsValid = false;

        tapeResponseResult.leftRelativeDb[i] =
            isfinite(tapeResponseResult.leftDb[i])
                ? tapeResponseResult.leftDb[i] - leftReference
                : NAN;
        tapeResponseResult.rightRelativeDb[i] =
            isfinite(tapeResponseResult.rightDb[i])
                ? tapeResponseResult.rightDb[i] - rightReference
                : NAN;
        tapeResponseResult.combinedRelativeDb[i] =
            isfinite(tapeResponseResult.combinedDb[i]) &&
                    isfinite(combinedReference)
                ? tapeResponseResult.combinedDb[i] - combinedReference
                : NAN;
    }
    tapeResponseResult.leftRelativeDb[TAPE_RESPONSE_REFERENCE_INDEX] = 0.0f;
    tapeResponseResult.rightRelativeDb[TAPE_RESPONSE_REFERENCE_INDEX] = 0.0f;
    if (isfinite(combinedReference))
        tapeResponseResult.combinedRelativeDb[TAPE_RESPONSE_REFERENCE_INDEX] = 0.0f;
    tapeResponseResult.valid = allBandsValid;
    if (!allBandsValid)
        session.responseTestDone = false;
}

bool Record::measureLevels(bool smooth, bool captureFrequency)
{
    float sumLeft = 0.0f;
    float sumRight = 0.0f;
    const uint32_t captureStartedUs = micros();
    for (uint16_t i = 0; i < SAMPLE_COUNT; ++i)
    {
        const uint32_t sampleStart = micros();
        const int16_t sampleLeft = audio.readLeftCentered();
        const int16_t sampleRight = audio.readRightCentered();
        if (captureFrequency)
        {
            frequencySamples[i] = sampleLeft;
            amplifiedFrequencySamples[i] = static_cast<int16_t>(
                constrain(static_cast<int32_t>(sampleLeft) * 4, -32768L, 32767L));
        }
        sumLeft += static_cast<float>(sampleLeft) * sampleLeft;
        sumRight += static_cast<float>(sampleRight) * sampleRight;
        while (micros() - sampleStart < SignalFrequency::SAMPLE_PERIOD_US)
        {
        }
    }

    const float reference = setAudio.rmsReference();
    if (reference <= 0.0f)
    {
        leftSignalValid = false;
        rightSignalValid = false;
        return false;
    }

    const float rmsLeft = sqrtf(sumLeft / SAMPLE_COUNT);
    const float rmsRight = sqrtf(sumRight / SAMPLE_COUNT);
    const float rawLeftDb = rmsToDb(rmsLeft, reference);
    const float rawRightDb = rmsToDb(rmsRight, reference);
    leftSignalValid = rawLeftDb >= MIN_SIGNAL_DB;
    rightSignalValid = rawRightDb >= MIN_SIGNAL_DB;
    if (!leftSignalValid || !rightSignalValid)
    {
        lastFrequencyHz = 0.0f;
        lastAmplifiedFrequencyHz = 0.0f;
        return false;
    }

    lastLeftRms = rmsLeft;
    lastRightRms = rmsRight;
    lastFrequencyHz = 0.0f;
    lastAmplifiedFrequencyHz = 0.0f;
    if (captureFrequency)
    {
        const uint32_t captureElapsedUs = micros() - captureStartedUs;
        const float frequencyTimeScale = captureElapsedUs > 0
                                             ? (static_cast<float>(SAMPLE_COUNT) * SignalFrequency::SAMPLE_PERIOD_US) /
                                                   static_cast<float>(captureElapsedUs)
                                             : 1.0f;
        lastFrequencyHz = SignalFrequency::measureInterpolated(
                              frequencySamples, SAMPLE_COUNT) *
                          frequencyTimeScale;
        lastAmplifiedFrequencyHz = SignalFrequency::measureInterpolated(
                                       amplifiedFrequencySamples, SAMPLE_COUNT) *
                                   frequencyTimeScale;
    }

    if (smooth)
    {
        if (!smoothingInitialized)
        {
            smoothedLeftDb = rawLeftDb;
            smoothedRightDb = rawRightDb;
            smoothingInitialized = true;
        }
        else
        {
            smoothedLeftDb = SMOOTHING_ALPHA * rawLeftDb + (1.0f - SMOOTHING_ALPHA) * smoothedLeftDb;
            smoothedRightDb = SMOOTHING_ALPHA * rawRightDb + (1.0f - SMOOTHING_ALPHA) * smoothedRightDb;
        }
        leftLevelDb = smoothedLeftDb;
        rightLevelDb = smoothedRightDb;
    }
    else
    {
        leftLevelDb = rawLeftDb;
        rightLevelDb = rawRightDb;
    }
    return true;
}

void Record::processBiasMeasurement()
{
    const uint32_t elapsed = millis() - biasFrequencySetMs;
    if (elapsed < BIAS_SETTLE_TIME_MS)
        return;

    if (elapsed < BIAS_SETTLE_TIME_MS + BIAS_MEASUREMENT_WINDOW_MS)
    {
        const bool levelsMeasured = measureLevels(false, true);
        const bool expectedFrequency = biasHighFrequency
                                           ? (frequencyInWindow(
                                                  lastFrequencyHz,
                                                  9500.0f,
                                                  10500.0f) ||
                                              frequencyInWindow(
                                                  lastAmplifiedFrequencyHz,
                                                  9500.0f,
                                                  10500.0f))
                                           : (frequencyInWindow(
                                                  lastFrequencyHz,
                                                  370.0f,
                                                  510.0f) ||
                                              frequencyInWindow(
                                                  lastAmplifiedFrequencyHz,
                                                  370.0f,
                                                  510.0f));
        const bool plausibleLevel =
            automaticLevelPlausible(leftLevelDb) &&
            automaticLevelPlausible(rightLevelDb);
        if (levelsMeasured && expectedFrequency && plausibleLevel)
        {
            biasSegmentSumLeftRms += lastLeftRms;
            biasSegmentSumRightRms += lastRightRms;
            ++biasSegmentMeasurementCount;
            if (status != Status::InvalidTestLevel)
                status = Status::BiasMeasuring;
        }
        else
        {
            status = Status::NoSignal;
        }
        return;
    }

    const bool valid =
        biasSegmentMeasurementCount >= AUTOMATIC_TEST_REQUIRED_WINDOWS;
    if (valid)
    {
        lastLeftRms = biasSegmentSumLeftRms / biasSegmentMeasurementCount;
        lastRightRms = biasSegmentSumRightRms / biasSegmentMeasurementCount;
        leftSignalValid = true;
        rightSignalValid = true;
        storeBiasMeasurement();
    }

    if (biasHighFrequency)
    {
        if (valid && bias440Valid && bias10kValid)
            updateBiasMatchStatus();
        else
        {
            biasCompletedCycles = 0;
            status = Status::NoSignal;
        }
    }
    else if (!valid)
    {
        biasCompletedCycles = 0;
        status = Status::NoSignal;
    }
    else
    {
        status = Status::BiasMeasuring;
    }

    if (state == CalibrationState::ThreeHeadBiasRunning &&
        !setBiasFrequency(!biasHighFrequency))
    {
        stopGenerator();
        state = CalibrationState::ThreeHeadBiasReady;
        status = Status::NoSignal;
        statusDisplayed = false;
    }
}

void Record::updateRecordLevel()
{
    const bool expectedFrequency =
        frequencyInWindow(lastFrequencyHz, 390.0f, 490.0f) ||
        frequencyInWindow(lastAmplifiedFrequencyHz, 390.0f, 490.0f);
    const bool plausibleLevel =
        automaticLevelPlausible(leftLevelDb) &&
        automaticLevelPlausible(rightLevelDb);
    if (!leftSignalValid || !rightSignalValid ||
        !expectedFrequency || !plausibleLevel)
    {
        recordMatchEntryCount = 0;
        recordMatchExitCount = 0;
        if (state == CalibrationState::ThreeHeadLevelMatched)
            state = CalibrationState::ThreeHeadLevelRunning;
        status = Status::NoSignal;
        return;
    }

    const bool entryRange = leftLevelDb >= RECORD_LEVEL_DB - RECORD_LEVEL_TOLERANCE_3HEAD_DB &&
                            leftLevelDb <= RECORD_LEVEL_DB + RECORD_LEVEL_TOLERANCE_3HEAD_DB &&
                            rightLevelDb >= RECORD_LEVEL_DB - RECORD_LEVEL_TOLERANCE_3HEAD_DB &&
                            rightLevelDb <= RECORD_LEVEL_DB + RECORD_LEVEL_TOLERANCE_3HEAD_DB;
    const bool holdRange = leftLevelDb >= RECORD_LEVEL_DB - RECORD_MATCH_HOLD_TOLERANCE_DB &&
                           leftLevelDb <= RECORD_LEVEL_DB + RECORD_MATCH_HOLD_TOLERANCE_DB &&
                           rightLevelDb >= RECORD_LEVEL_DB - RECORD_MATCH_HOLD_TOLERANCE_DB &&
                           rightLevelDb <= RECORD_LEVEL_DB + RECORD_MATCH_HOLD_TOLERANCE_DB;

    if (state == CalibrationState::ThreeHeadLevelMatched)
    {
        recordMatchEntryCount = 0;
        if (holdRange)
        {
            recordMatchExitCount = 0;
            status = Status::LevelMatched;
            return;
        }
        if (++recordMatchExitCount >= RECORD_MATCH_EXIT_CONFIRMATIONS)
        {
            recordMatchExitCount = 0;
            state = CalibrationState::ThreeHeadLevelRunning;
            status = Status::AdjustRecLevel;
        }
        return;
    }

    recordMatchExitCount = 0;
    if (entryRange)
    {
        if (++recordMatchEntryCount >= RECORD_MATCH_ENTRY_CONFIRMATIONS)
        {
            recordMatchEntryCount = 0;
            state = CalibrationState::ThreeHeadLevelMatched;
            stopGenerator();
            session.levelTestDone = true;
            session.levelMatched = true;
            if (workflowActive)
                workflowResult.levelMatched = true;
            captureFinalRecordLevel();
            status = Status::LevelMatched;
        }
        else
            status = Status::AdjustRecLevel;
        return;
    }
    recordMatchEntryCount = 0;
    status = Status::AdjustRecLevel;
}

void Record::storeBiasMeasurement()
{
    if (!leftSignalValid || !rightSignalValid)
        return;

    const float reference = setAudio.rmsReference();
    if (reference <= 0.0f)
        return;

    if (biasHighFrequency)
    {
        bias10kLeftDb = rmsToDb(lastLeftRms, reference);
        bias10kRightDb = rmsToDb(lastRightRms, reference);
        bias10kValid = true;
    }
    else
    {
        bias440LeftDb = rmsToDb(lastLeftRms, reference);
        bias440RightDb = rmsToDb(lastRightRms, reference);
        bias440Valid = true;
    }
}

void Record::updateBiasMatchStatus()
{
    const bool levelsInRange =
        bias440LeftDb >= REC_CAL_LEVEL_MIN_DB && bias440LeftDb <= THREE_HEAD_BIAS_LEVEL_MAX_DB &&
        bias440RightDb >= REC_CAL_LEVEL_MIN_DB && bias440RightDb <= THREE_HEAD_BIAS_LEVEL_MAX_DB &&
        bias10kLeftDb >= REC_CAL_LEVEL_MIN_DB && bias10kLeftDb <= THREE_HEAD_BIAS_LEVEL_MAX_DB &&
        bias10kRightDb >= REC_CAL_LEVEL_MIN_DB && bias10kRightDb <= THREE_HEAD_BIAS_LEVEL_MAX_DB;
    biasTestLevelValid = levelsInRange;

    const float diffLeft = bias10kLeftDb - bias440LeftDb;
    const float diffRight = bias10kRightDb - bias440RightDb;
    const float largestDiff = max(fabsf(diffLeft), fabsf(diffRight));
    const bool biasAccepted = largestDiff <= REC_CAL_BIAS_ACCEPTABLE_TOLERANCE_DB;

    if (levelsInRange && biasAccepted)
    {
        if (biasCompletedCycles < REC_CAL_REQUIRED_STABLE_CYCLES)
            ++biasCompletedCycles;
    }
    else
        biasCompletedCycles = 0;

    if (biasCompletedCycles == REC_CAL_REQUIRED_STABLE_CYCLES)
    {
        state = CalibrationState::ThreeHeadBiasMatched;
        stopGenerator();
        captureFinalThreeHeadBias();
        session.biasTestDone = true;
        session.biasMatched = true;
        if (workflowActive)
            workflowResult.biasMatched = true;
        status = Status::CalibrationPassed;
        statusDisplayed = false;
        buzzer.play(BeepPattern::Success);
        return;
    }

    status = largestDiff <= REC_CAL_BIAS_MATCH_TOLERANCE_DB
                 ? Status::BiasMatched
                 : (biasAccepted ? Status::BiasBorderline : Status::AdjustBias);
}

void Record::startBias()
{
    state = CalibrationState::ThreeHeadBiasRunning;
    resetMeasurements();
    resetBiasMatchCounters();
    biasCompletedCycles = 0;
    resetBiasAverages();
    if (!setBiasFrequency(false))
    {
        state = CalibrationState::ThreeHeadBiasReady;
        status = Status::NoSignal;
        statusDisplayed = false;
        return;
    }
    ad9833.enable();
    status = Status::BiasMeasuring;
    statusDisplayed = false;
}

void Record::stopBias()
{
    stopGenerator();
    state = CalibrationState::ThreeHeadBiasReady;
    resetMeasurements();
    resetBiasMatchCounters();
    biasCompletedCycles = 0;
    resetBiasAverages();
    biasHighFrequency = false;
    status = Status::Ready;
    statusDisplayed = false;
}

void Record::startThreeHeadLevel()
{
    if (!setGeneratorOutput(
            RECORD_LEVEL_FREQUENCY_3HEAD_HZ,
            RECORD_LEVEL_DB))
    {
        state = CalibrationState::ThreeHeadLevelReady;
        resetMeasurements();
        session.levelTestDone = false;
        session.levelMatched = false;
        status = Status::NoSignal;
        statusDisplayed = false;
        return;
    }
    ad9833.enable();
    state = CalibrationState::ThreeHeadLevelRunning;
    resetMeasurements();
    recordMatchEntryCount = 0;
    recordMatchExitCount = 0;
    session.levelTestDone = false;
    session.levelMatched = false;
    status = Status::NoSignal;
    statusDisplayed = false;
}

void Record::stopThreeHeadLevel()
{
    stopGenerator();
    state = CalibrationState::ThreeHeadLevelReady;
    resetMeasurements();
    recordMatchEntryCount = 0;
    recordMatchExitCount = 0;
    status = Status::Ready;
    statusDisplayed = false;
}

float Record::getGeneratorCalibrationCorrectionDb(float frequencyHz) const
{
    if (!tapeResponseCalibration.valid || !isfinite(frequencyHz) ||
        frequencyHz <= 0.0f)
        return 0.0f;

    const float firstCorrection = tapeResponseCalibration.generatorCorrectionDb[0];
    const float lastCorrection = tapeResponseCalibration.generatorCorrectionDb[TAPE_RESPONSE_BAND_COUNT - 1];
    if (!isfinite(firstCorrection) || !isfinite(lastCorrection))
        return 0.0f;

    if (frequencyHz <= TAPE_RESPONSE_FREQUENCIES_HZ[0])
        return constrain(
            firstCorrection,
            -TAPE_RESPONSE_MAX_AUTO_CORRECTION_DB,
            TAPE_RESPONSE_MAX_AUTO_CORRECTION_DB);

    for (uint8_t i = 1; i < TAPE_RESPONSE_BAND_COUNT; ++i)
    {
        const float upperFrequency = TAPE_RESPONSE_FREQUENCIES_HZ[i];
        if (frequencyHz > upperFrequency)
            continue;

        const float lowerFrequency = TAPE_RESPONSE_FREQUENCIES_HZ[i - 1];
        const float lowerCorrection =
            tapeResponseCalibration.generatorCorrectionDb[i - 1];
        const float upperCorrection =
            tapeResponseCalibration.generatorCorrectionDb[i];
        if (!isfinite(lowerCorrection) || !isfinite(upperCorrection))
            return 0.0f;

        const float t = (frequencyHz - lowerFrequency) /
                        (upperFrequency - lowerFrequency);
        return constrain(
            lowerCorrection + t * (upperCorrection - lowerCorrection),
            -TAPE_RESPONSE_MAX_AUTO_CORRECTION_DB,
            TAPE_RESPONSE_MAX_AUTO_CORRECTION_DB);
    }

    return constrain(
        lastCorrection,
        -TAPE_RESPONSE_MAX_AUTO_CORRECTION_DB,
        TAPE_RESPONSE_MAX_AUTO_CORRECTION_DB);
}

bool Record::setBiasFrequency(bool highFrequency)
{
    biasHighFrequency = highFrequency;
    const uint16_t frequencyHz = highFrequency
                                     ? BIAS_HIGH_FREQUENCY_HZ
                                     : BIAS_LOW_FREQUENCY_HZ;
    const float generatorLevelDb = REC_CAL_BASE_LEVEL_DB +
                                   getGeneratorCalibrationCorrectionDb(frequencyHz);
    if (!setGeneratorOutput(frequencyHz, generatorLevelDb))
        return false;
    biasFrequencySetMs = millis();
    biasSegmentSumLeftRms = 0.0f;
    biasSegmentSumRightRms = 0.0f;
    biasSegmentMeasurementCount = 0;
    displayedActiveFrequency = -1;
    return true;
}

void Record::completeCalibration()
{
    stopGenerator();
    resetBiasMatchCounters();
    resetBiasAverages();
    resetTwoHeadAnalysis();
    finalDeckType = deckType;
    calibrationCompleted = true;
    status = Status::CalibrationCompleted;
    statusDisplayed = false;
}

void Record::enterSelectedTest()
{
    stopGenerator();
    if (highlightedTestType == TestType::TapeResponse)
    {
        resetTapeResponse();
        session.responseTestDone = false;
        state = deckType == DeckType::ThreeHead &&
                        !tapeResponseCalibration.valid
                    ? CalibrationState::ThreeHeadResponseCalibrationRequired
                : deckType == DeckType::ThreeHead
                    ? CalibrationState::ThreeHeadResponseReady
                    : CalibrationState::TwoHeadResponseReadyToRecord;
        return;
    }

    if (highlightedTestType == TestType::RecordLevel)
    {
        session.levelTestDone = false;
        session.levelMatched = false;
        resetLevelTest();
        state = deckType == DeckType::ThreeHead
                    ? CalibrationState::ThreeHeadLevelReady
                    : CalibrationState::TwoHeadLevelReadyToRecord;
    }
    else if (highlightedTestType == TestType::BiasCalibration)
    {
        session.biasTestDone = false;
        session.biasMatched = false;
        resetBiasTest();
        state = deckType == DeckType::ThreeHead
                    ? CalibrationState::ThreeHeadBiasReady
                    : CalibrationState::TwoHeadBiasReadyToRecord;
    }
}

void Record::returnToTestSelection()
{
    stopGenerator();
    tapeEqFlowActive = false;
    if (isLevelState())
        resetLevelTest();
    else if (isBiasState())
        resetBiasTest();
    else if (isTapeResponseState())
    {
        stopGenerator();
        resetTapeResponse();
    }
    state = CalibrationState::SelectTest;
    highlightedTestType = TestType::None;
    selectedRecordActionIndex = 0;
    status = Status::Ready;
    statusDisplayed = false;
}

void Record::stopGenerator()
{
    ad9833.disable();
}

void Record::resetTwoHeadTapeResponse()
{
    stopGenerator();
    resetTapeResponse();
    twoHeadEqRecordPhase = TwoHeadEqRecordPhase::Sync;
    twoHeadEqSegmentStartedMs = 0;
    twoHeadEqAnalysisStartedMs = 0;
    twoHeadEqSyncFound = false;
    twoHeadEqSyncDetections = 0;
    twoHeadEqSyncExitDetections = 0;
    twoHeadEqAllBandsValid = true;
    twoHeadEqNoSync = false;
    status = Status::Ready;
}

void Record::startTwoHeadTapeResponseRecording()
{
    resetTwoHeadTapeResponse();
    twoHeadEqRecordPhase = TwoHeadEqRecordPhase::Sync;
    tapeResponseBandIndex = 0;
    if (!setGeneratorOutput(
            TWO_HEAD_EQ_SYNC_HZ,
            TAPE_RESPONSE_BASE_LEVEL_DB +
                getGeneratorCalibrationCorrectionDb(TWO_HEAD_EQ_SYNC_HZ)))
    {
        state = CalibrationState::TwoHeadResponseReadyToRecord;
        status = Status::NoSignal;
        return;
    }
    ad9833.enable();
    twoHeadEqSegmentStartedMs = millis();
    state = CalibrationState::TwoHeadResponseRecording;
    displayedTapeResponseBand = 0xFF;
}

void Record::updateTwoHeadTapeResponseRecording()
{
    const uint32_t now = millis();
    const uint32_t elapsed = now - twoHeadEqSegmentStartedMs;

    if (twoHeadEqRecordPhase == TwoHeadEqRecordPhase::Sync)
    {
        if (elapsed < TWO_HEAD_EQ_SYNC_MS)
            return;
        twoHeadEqRecordPhase = TwoHeadEqRecordPhase::Tone;
        if (!setGeneratorOutput(
                TAPE_RESPONSE_FREQUENCIES_HZ[0],
                TAPE_RESPONSE_BASE_LEVEL_DB +
                    getGeneratorCalibrationCorrectionDb(
                        TAPE_RESPONSE_FREQUENCIES_HZ[0])))
        {
            state = CalibrationState::TwoHeadResponseReadyToRecord;
            status = Status::NoSignal;
            drawScreen();
            return;
        }
        ad9833.enable();
        twoHeadEqSegmentStartedMs += TWO_HEAD_EQ_SYNC_MS;
        displayedTapeResponseBand = 0xFF;
        return;
    }

    if (twoHeadEqRecordPhase == TwoHeadEqRecordPhase::Tone)
    {
        const uint32_t toneMs = tapeResponseBandIndex == 0
                                    ? TWO_HEAD_EQ_FIRST_50_SETTLE_MS + TWO_HEAD_EQ_TONE_MS
                                    : TWO_HEAD_EQ_TONE_MS;
        if (elapsed < toneMs)
            return;

        stopGenerator();
        if (tapeResponseBandIndex + 1 >= TAPE_RESPONSE_BAND_COUNT)
        {
            state = CalibrationState::TwoHeadResponseRewind;
            status = Status::Ready;
            drawScreen();
            buzzer.play(BeepPattern::Success);
            return;
        }

        twoHeadEqRecordPhase = TwoHeadEqRecordPhase::Gap;
        twoHeadEqSegmentStartedMs += toneMs;
        displayedTapeResponseBand = 0xFF;
        return;
    }

    if (elapsed < TWO_HEAD_EQ_GAP_MS)
        return;

    ++tapeResponseBandIndex;
    twoHeadEqRecordPhase = TwoHeadEqRecordPhase::Tone;
    if (!setGeneratorOutput(
            TAPE_RESPONSE_FREQUENCIES_HZ[tapeResponseBandIndex],
            TAPE_RESPONSE_BASE_LEVEL_DB +
                getGeneratorCalibrationCorrectionDb(
                    TAPE_RESPONSE_FREQUENCIES_HZ[tapeResponseBandIndex])))
    {
        state = CalibrationState::TwoHeadResponseReadyToRecord;
        status = Status::NoSignal;
        drawScreen();
        return;
    }
    ad9833.enable();
    twoHeadEqSegmentStartedMs += TWO_HEAD_EQ_GAP_MS;
    displayedTapeResponseBand = 0xFF;
}

void Record::startTwoHeadTapeResponseAnalysis()
{
    stopGenerator();
    resetTapeResponse();
    twoHeadEqSyncFound = false;
    twoHeadEqSyncDetections = 0;
    twoHeadEqSyncExitDetections = 0;
    twoHeadEqAllBandsValid = true;
    twoHeadEqNoSync = false;
    twoHeadEqAnalysisStartedMs = millis();
    twoHeadEqSegmentStartedMs = 0;
    twoHeadEqRecordPhase = TwoHeadEqRecordPhase::Tone;
    state = CalibrationState::TwoHeadResponseWaitingSync;
    status = Status::WaitingForSync;
    displayedTapeResponseBand = 0xFF;
}

void Record::updateTwoHeadTapeResponseAnalysis()
{
    const uint32_t now = millis();
    if (state == CalibrationState::TwoHeadResponseWaitingSync)
    {
        const bool valid = measureLevels(false, true);
        const bool plausibleLevel =
            automaticLevelPlausible(leftLevelDb) &&
            automaticLevelPlausible(rightLevelDb);
        const bool syncFrequency = valid && plausibleLevel &&
                                   ((lastFrequencyHz >= SYNC_MIN_HZ && lastFrequencyHz <= SYNC_MAX_HZ) ||
                                    (lastAmplifiedFrequencyHz >= SYNC_MIN_HZ &&
                                     lastAmplifiedFrequencyHz <= SYNC_MAX_HZ));

        if (!twoHeadEqSyncFound)
        {
            if (syncFrequency)
            {
                if (twoHeadEqSyncDetections < SYNC_REQUIRED_CONFIRMATIONS)
                    ++twoHeadEqSyncDetections;
            }
            else if (twoHeadEqSyncDetections > 0)
            {
                --twoHeadEqSyncDetections;
            }

            if (twoHeadEqSyncDetections >= SYNC_REQUIRED_CONFIRMATIONS)
            {
                twoHeadEqSyncFound = true;
                twoHeadEqSyncExitDetections = 0;
                status = Status::SyncFound;
                displayedTapeResponseBand = 0xFF;
                drawScreen();
            }
        }
        else
        {
            if (syncFrequency)
            {
                twoHeadEqSyncExitDetections = 0;
            }
            else if (twoHeadEqSyncExitDetections < SYNC_REQUIRED_CONFIRMATIONS)
            {
                ++twoHeadEqSyncExitDetections;
            }

            if (twoHeadEqSyncExitDetections >= SYNC_REQUIRED_CONFIRMATIONS)
            {
                state = CalibrationState::TwoHeadResponseAnalyzing;
                status = Status::Analyzing;
                tapeResponseBandIndex = 0;
                twoHeadEqSegmentStartedMs = now;
                twoHeadEqRecordPhase = TwoHeadEqRecordPhase::Tone;
                displayedTapeResponseBand = 0xFF;
                return;
            }
        }

        if (now - twoHeadEqAnalysisStartedMs >= TWO_HEAD_EQ_SYNC_TIMEOUT_MS)
        {
            resetTapeResponse();
            twoHeadEqNoSync = true;
            state = CalibrationState::TwoHeadResponseResult;
            status = Status::NoSignal;
            drawScreen();
            buzzer.play(BeepPattern::Failure);
        }
        return;
    }

    if (state != CalibrationState::TwoHeadResponseAnalyzing)
        return;

    const uint32_t elapsed = now - twoHeadEqSegmentStartedMs;
    if (twoHeadEqRecordPhase == TwoHeadEqRecordPhase::Gap)
    {
        if (elapsed >= TWO_HEAD_EQ_GAP_MS)
        {
            twoHeadEqRecordPhase = TwoHeadEqRecordPhase::Tone;
            twoHeadEqSegmentStartedMs += TWO_HEAD_EQ_GAP_MS;
            displayedTapeResponseBand = 0xFF;
        }
        return;
    }

    const uint32_t toneMs = tapeResponseBandIndex == 0
                                ? TWO_HEAD_EQ_FIRST_50_SETTLE_MS + TWO_HEAD_EQ_TONE_MS
                                : TWO_HEAD_EQ_TONE_MS;
    const uint32_t settleMs = tapeResponseBandIndex == 0
                                  ? TWO_HEAD_EQ_50_ANALYZE_SETTLE_MS
                                  : TWO_HEAD_EQ_ANALYZE_SETTLE_MS;
    const uint32_t measureMs = tapeResponseBandIndex == 0
                                   ? TWO_HEAD_EQ_50_ANALYZE_WINDOW_MS
                                   : TWO_HEAD_EQ_ANALYZE_WINDOW_MS;

    if (elapsed >= settleMs && elapsed < settleMs + measureMs)
    {
        float leftRms = 0.0f;
        float rightRms = 0.0f;
        float leftFrequencyHz = 0.0f;
        float rightFrequencyHz = 0.0f;
        bool leftValid = false;
        bool rightValid = false;
        bool clipped = false;
        measureTapeResponseToneWindow(
            leftRms,
            rightRms,
            leftFrequencyHz,
            rightFrequencyHz,
            leftValid,
            rightValid,
            clipped);
        tapeResponseBandClipped = tapeResponseBandClipped || clipped;
        const float reference = setAudio.rmsReference();
        const uint16_t expectedHz =
            TAPE_RESPONSE_FREQUENCIES_HZ[tapeResponseBandIndex];
        const float leftDb = reference > 0.0f
                                 ? rmsToDb(leftRms, reference)
                                 : NAN;
        const float rightDb = reference > 0.0f
                                  ? rmsToDb(rightRms, reference)
                                  : NAN;
        // As in the validated 3-head sweep, the 15 kHz zero-crossing
        // estimate is not a reliable gate at the 40 kS/s validation rate.
        // Sync has already confirmed the playback path, so retain level,
        // clipping and minimum-window validation at 15 kHz; all lower
        // bands keep the frequency check.
        const bool frequencyCheckReliable =
            expectedHz <= MAX_RELIABLE_SWEEP_FREQUENCY_HZ;
        if (leftValid && automaticLevelPlausible(leftDb) &&
            (!frequencyCheckReliable ||
             sweepFrequencyMatches(leftFrequencyHz, expectedHz)))
        {
            tapeResponseSumLeftRms += leftRms;
            ++tapeResponseLeftWindowCount;
        }
        if (rightValid && automaticLevelPlausible(rightDb) &&
            (!frequencyCheckReliable ||
             sweepFrequencyMatches(rightFrequencyHz, expectedHz)))
        {
            tapeResponseSumRightRms += rightRms;
            ++tapeResponseRightWindowCount;
        }
    }

    if (elapsed >= toneMs)
    {
        finishTwoHeadTapeResponseBand();
        return;
    }
}

void Record::finishTwoHeadTapeResponseBand()
{
    const uint32_t toneMs = tapeResponseBandIndex == 0
                                ? TWO_HEAD_EQ_FIRST_50_SETTLE_MS + TWO_HEAD_EQ_TONE_MS
                                : TWO_HEAD_EQ_TONE_MS;
    storeTapeResponseBand(tapeResponseBandIndex);
    if (!isfinite(tapeResponseResult.leftDb[tapeResponseBandIndex]) ||
        !isfinite(tapeResponseResult.rightDb[tapeResponseBandIndex]))
    {
        twoHeadEqAllBandsValid = false;
        tapeResponseSignalLost = true;
        tapeResponseResult.valid = false;
        session.responseTestDone = false;
        // The 2 kHz sync was already acquired; this is a playback loss
        // during analysis, not a synchronization failure.
        twoHeadEqNoSync = false;
        state = CalibrationState::TwoHeadResponseResult;
        status = Status::NoSignal;
        drawScreen();
        buzzer.play(BeepPattern::Failure);
        return;
    }

    drawTapeResponseLiveBand(tapeResponseBandIndex);
    ++tapeResponseBandIndex;
    if (tapeResponseBandIndex >= TAPE_RESPONSE_BAND_COUNT)
    {
        normalizeTapeResponse();
        if (!twoHeadEqAllBandsValid)
            tapeResponseResult.valid = false;
        session.responseTestDone = tapeResponseResult.valid;
        state = CalibrationState::TwoHeadResponseResult;
        drawScreen();
        buzzer.play(
            tapeResponseResult.valid
                ? BeepPattern::Success
                : BeepPattern::Failure);
        return;
    }

    tapeResponseSumLeftRms = 0.0f;
    tapeResponseSumRightRms = 0.0f;
    tapeResponseLeftWindowCount = 0;
    tapeResponseRightWindowCount = 0;
    tapeResponseBandClipped = false;
    twoHeadEqRecordPhase = TwoHeadEqRecordPhase::Gap;
    twoHeadEqSegmentStartedMs += toneMs;
    displayedTapeResponseBand = 0xFF;
}

void Record::handleTwoHeadTapeResponseAction()
{
    switch (state)
    {
    case CalibrationState::TwoHeadResponseReadyToRecord:
        startTwoHeadTapeResponseRecording();
        break;
    case CalibrationState::TwoHeadResponseRecording:
        resetTwoHeadTapeResponse();
        state = CalibrationState::TwoHeadResponseReadyToRecord;
        break;
    case CalibrationState::TwoHeadResponseRewind:
        state = CalibrationState::TwoHeadResponseReadyToAnalyze;
        status = Status::Ready;
        break;
    case CalibrationState::TwoHeadResponseReadyToAnalyze:
        startTwoHeadTapeResponseAnalysis();
        break;
    case CalibrationState::TwoHeadResponseWaitingSync:
    case CalibrationState::TwoHeadResponseAnalyzing:
    case CalibrationState::TwoHeadResponseResult:
        if (state == CalibrationState::TwoHeadResponseResult &&
            (twoHeadEqNoSync || tapeResponseSignalLost))
        {
            // The test tones are already on tape. Retry only playback
            // synchronization and analysis; recording starts over only when
            // the user leaves through TOUCH TO BACK and enters the test again.
            session.responseTestDone = false;
            startTwoHeadTapeResponseAnalysis();
            break;
        }
        resetTwoHeadTapeResponse();
        state = CalibrationState::TwoHeadResponseReadyToRecord;
        break;
    default:
        break;
    }
}

void Record::resetTwoHeadAnalysis()
{
    biasTestLevelValid = false;
    twoHeadStateStartedMs = 0;
    twoHeadLastMeasurementMs = 0;
    twoHeadValidMeasurementMs = 0;
    twoHeadToneStableMs = 0;
    twoHeadToneLostMs = 0;
    twoHeadToneValidMs = 0;
    toneCaptureStartedMs = 0;
    toneCaptureWindowStartedMs = 0;
    accumulatedValidMs = 0;
    waitingForPlaybackTone = true;
    consecutiveToneDetections = 0;
    expectedTwoHeadTone = TwoHeadTone::Low440;
    twoHeadBiasCycle = 0;
    twoHeadBiasSegment = BiasSequenceSegment::Sync1;
    twoHeadBiasPlaybackState = TwoHeadBiasPlaybackState::WaitSync1;
    twoHeadBiasPostSyncStartedMs = 0;
    twoHeadBiasAwaitingToneStart = true;
    twoHeadDetectedTone = TwoHeadTone::None;
    twoHeadToneCaptured = false;
    twoHeadSequenceFound = false;
    twoHeadResultMatched = false;
    twoHeadSumLeftRms = 0.0f;
    twoHeadSumRightRms = 0.0f;
    twoHeadRmsMeasurementCount = 0;
    twoHead440Count = 0;
    twoHead10kCount = 0;
    waitingForBiasSync = true;
    biasSyncDetections = 0;
    biasSyncCandidateLeftRms = 0.0f;
    biasSyncCandidateRightRms = 0.0f;
    biasSyncCandidateMeasurements = 0;
    biasPlaybackCycleStartedMs = 0;
    biasLowValidMs = 0;
    biasHighValidMs = 0;
    biasCycleLowLeftRms = 0.0f;
    biasCycleLowRightRms = 0.0f;
    biasCycleHighLeftRms = 0.0f;
    biasCycleHighRightRms = 0.0f;
    biasCycleLowMeasurements = 0;
    biasCycleHighMeasurements = 0;
    for (uint8_t i = 0; i < TWO_HEAD_BIAS_MIN_SAMPLES; ++i)
    {
        twoHead440LeftRms[i] = 0.0f;
        twoHead440RightRms[i] = 0.0f;
        twoHead10kLeftRms[i] = 0.0f;
        twoHead10kRightRms[i] = 0.0f;
    }
}

void Record::startTwoHeadLevelRecording()
{
    stopGenerator();
    resetMeasurements();
    if (!setGeneratorOutput(
            RECORD_LEVEL_FREQUENCY_2HEAD_HZ,
            RECORD_LEVEL_DB))
    {
        state = CalibrationState::TwoHeadLevelReadyToRecord;
        status = Status::NoSignal;
        return;
    }
    ad9833.enable();
    state = CalibrationState::TwoHeadLevelRecording;
    twoHeadStateStartedMs = millis();
    status = Status::Ready;
}

void Record::startTwoHeadLevelAnalysis()
{
    stopGenerator();
    resetMeasurements();
    resetTwoHeadAnalysis();
    state = CalibrationState::TwoHeadLevelAnalyzing;
    twoHeadStateStartedMs = millis();
    twoHeadLastMeasurementMs = twoHeadStateStartedMs;
    status = Status::WaitingFor440;
}

void Record::startTwoHeadBiasRecording()
{
    stopGenerator();
    resetMeasurements();
    resetBiasAverages();
    resetTwoHeadAnalysis();
    state = CalibrationState::TwoHeadBiasRecording;
    twoHeadBiasRecordingStartedMs = millis();
    if (!setTwoHeadBiasSegment(BiasSequenceSegment::Sync1))
    {
        state = CalibrationState::TwoHeadBiasReadyToRecord;
        status = Status::NoSignal;
        return;
    }
    status = Status::Ready;
}

void Record::startTwoHeadBiasAnalysis()
{
    stopGenerator();
    resetMeasurements();
    resetBiasAverages();
    resetTwoHeadAnalysis();
    state = CalibrationState::TwoHeadBiasAnalyzing;
    twoHeadStateStartedMs = millis();
    twoHeadLastMeasurementMs = twoHeadStateStartedMs;
    status = Status::WaitingForSync;
}

void Record::updateTwoHeadState()
{
    switch (state)
    {
    case CalibrationState::TwoHeadLevelRecording:
        if (millis() - twoHeadStateStartedMs >= TWO_HEAD_LEVEL_RECORD_MS)
        {
            stopGenerator();
            buzzer.play(BeepPattern::Success);
            state = CalibrationState::TwoHeadLevelRewind;
            status = Status::Ready;
            drawScreen();
        }
        break;
    case CalibrationState::TwoHeadBiasRecording:
        advanceTwoHeadBiasSequence();
        break;
    case CalibrationState::TwoHeadLevelAnalyzing:
        updateTwoHeadLevelAnalysis();
        break;
    case CalibrationState::TwoHeadBiasAnalyzing:
        updateTwoHeadBiasAnalysis();
        break;
    default:
        break;
    }
}

void Record::updateTwoHeadLevelAnalysis()
{
    const bool valid = measureLevels(false, true);
    const uint32_t now = millis();
    const uint32_t elapsed = now - twoHeadLastMeasurementMs;
    twoHeadLastMeasurementMs = now;
    const bool toneDetected = valid &&
                              automaticLevelPlausible(leftLevelDb) &&
                              automaticLevelPlausible(rightLevelDb) &&
                              ((lastFrequencyHz >= 390.0f && lastFrequencyHz <= 490.0f) ||
                               (lastAmplifiedFrequencyHz >= 390.0f && lastAmplifiedFrequencyHz <= 490.0f));

    if (waitingForPlaybackTone)
    {
        consecutiveToneDetections = toneDetected
                                        ? consecutiveToneDetections + 1
                                        : 0;
        if (consecutiveToneDetections >= TWO_HEAD_TONE_CONFIRMATIONS)
        {
            waitingForPlaybackTone = false;
            toneCaptureStartedMs = now;
            toneCaptureWindowStartedMs = now;
            accumulatedValidMs = 0;
            twoHeadToneLostMs = 0;
            twoHeadSumLeftRms = 0.0f;
            twoHeadSumRightRms = 0.0f;
            twoHeadRmsMeasurementCount = 0;
            status = Status::Analyzing;
        }
    }
    else if (toneDetected)
    {
        twoHeadToneLostMs = 0;
        if (now - toneCaptureStartedMs >= TWO_HEAD_LEVEL_STABLE_MS)
        {
            accumulatedValidMs += elapsed;
            twoHeadValidMeasurementMs = accumulatedValidMs;
            twoHeadSumLeftRms += lastLeftRms;
            twoHeadSumRightRms += lastRightRms;
            ++twoHeadRmsMeasurementCount;
        }
        if (accumulatedValidMs >= TWO_HEAD_LEVEL_CAPTURE_MS)
        {
            finalizeTwoHeadLevelAnalysis();
            return;
        }
    }
    else
    {
        if (twoHeadToneLostMs == 0)
            twoHeadToneLostMs = now;
        if (now - twoHeadToneLostMs > TWO_HEAD_LEVEL_TONE_LOSS_HOLD_MS)
        {
            waitingForPlaybackTone = true;
            consecutiveToneDetections = 0;
            accumulatedValidMs = 0;
            twoHeadValidMeasurementMs = 0;
            twoHeadSumLeftRms = 0.0f;
            twoHeadSumRightRms = 0.0f;
            twoHeadRmsMeasurementCount = 0;
            status = Status::WaitingFor440;
        }
    }

    // Keep waiting and analyzing until the user chooses RETEST or a valid
    // LEVEL MATCHED result is obtained.
}

void Record::updateTwoHeadBiasAnalysis()
{
    const bool valid = measureLevels(false, true);
    const uint32_t now = millis();
    twoHeadLastMeasurementMs = now;
    const auto inFrequencyWindow = [](float frequency, float minimum, float maximum)
    {
        return frequency >= minimum && frequency <= maximum;
    };
    const bool plausibleLevel = valid &&
                                automaticLevelPlausible(leftLevelDb) &&
                                automaticLevelPlausible(rightLevelDb);
    const bool syncFrequency = plausibleLevel &&
                               (inFrequencyWindow(lastFrequencyHz, SYNC_MIN_HZ, SYNC_MAX_HZ) ||
                                inFrequencyWindow(lastAmplifiedFrequencyHz, SYNC_MIN_HZ, SYNC_MAX_HZ));
    const bool lowFrequency = plausibleLevel &&
                              (inFrequencyWindow(lastFrequencyHz, 370.0f, 510.0f) ||
                               inFrequencyWindow(lastAmplifiedFrequencyHz, 370.0f, 510.0f));
    const bool highFrequency = plausibleLevel &&
                               (inFrequencyWindow(lastFrequencyHz, 9500.0f, 10500.0f) ||
                                inFrequencyWindow(lastAmplifiedFrequencyHz, 9500.0f, 10500.0f));

    const auto resetSyncDetections = [&]()
    {
        biasSyncDetections = 0;
    };
    const auto syncIsFound = [&]()
    {
        if (syncFrequency)
        {
            if (biasSyncDetections < SYNC_REQUIRED_CONFIRMATIONS)
                ++biasSyncDetections;
        }
        else if (biasSyncDetections > 0)
        {
            --biasSyncDetections;
        }
        return biasSyncDetections >= SYNC_REQUIRED_CONFIRMATIONS;
    };
    const auto startToneMeasurement = [&](TwoHeadBiasPlaybackState measurementState,
                                          Status measurementStatus)
    {
        twoHeadBiasPlaybackState = measurementState;
        twoHeadBiasAwaitingToneStart = true;
        twoHeadBiasPostSyncStartedMs = 0;
        resetSyncDetections();
        status = measurementStatus;
    };
    const auto appendMeasurement = [&](float leftSamples[], float rightSamples[],
                                       uint8_t &sampleCount)
    {
        const float reference = setAudio.rmsReference();
        if (reference <= 0.0f || sampleCount >= TWO_HEAD_BIAS_MIN_SAMPLES)
            return;
        leftSamples[sampleCount] = rmsToDb(lastLeftRms, reference);
        rightSamples[sampleCount] = rmsToDb(lastRightRms, reference);
        ++sampleCount;
    };
    const auto updateLiveBiasMeasurement = [&](bool highFrequency)
    {
        if (highFrequency)
        {
            if (twoHead10kCount == 0)
                return;
            bias10kLeftDb = medianBiasSamples(twoHead10kLeftRms, twoHead10kCount);
            bias10kRightDb = medianBiasSamples(twoHead10kRightRms, twoHead10kCount);
            bias10kValid = true;
        }
        else
        {
            if (twoHead440Count == 0)
                return;
            bias440LeftDb = medianBiasSamples(twoHead440LeftRms, twoHead440Count);
            bias440RightDb = medianBiasSamples(twoHead440RightRms, twoHead440Count);
            bias440Valid = true;
        }
    };

    switch (twoHeadBiasPlaybackState)
    {
    case TwoHeadBiasPlaybackState::WaitSync1:
        status = Status::WaitingForSync;
        if (syncIsFound())
            startToneMeasurement(TwoHeadBiasPlaybackState::Measure440, Status::CapturingLow);
        break;
    case TwoHeadBiasPlaybackState::Measure440:
        if (twoHeadBiasAwaitingToneStart)
        {
            if (syncFrequency)
                break;
            twoHeadBiasAwaitingToneStart = false;
            twoHeadBiasPostSyncStartedMs = now;
        }
        if (syncIsFound())
        {
            twoHeadBiasPlaybackState = TwoHeadBiasPlaybackState::WaitSync2;
            resetSyncDetections();
            status = Status::WaitingForSync;
        }
        else if ((uint32_t)(now - twoHeadBiasPostSyncStartedMs) >=
                     TWO_HEAD_BIAS_POST_SYNC_IGNORE_MS &&
                 lowFrequency)
        {
            appendMeasurement(twoHead440LeftRms, twoHead440RightRms, twoHead440Count);
            updateLiveBiasMeasurement(false);
            status = Status::CapturingLow;
        }
        break;
    case TwoHeadBiasPlaybackState::WaitSync2:
        status = Status::WaitingForSync;
        if (syncIsFound())
            startToneMeasurement(TwoHeadBiasPlaybackState::Measure10k, Status::CapturingHigh);
        break;
    case TwoHeadBiasPlaybackState::Measure10k:
        if (twoHeadBiasAwaitingToneStart)
        {
            if (syncFrequency)
                break;
            twoHeadBiasAwaitingToneStart = false;
            twoHeadBiasPostSyncStartedMs = now;
        }
        if (syncIsFound())
        {
            twoHeadBiasPlaybackState = TwoHeadBiasPlaybackState::WaitSync3;
            resetSyncDetections();
            status = Status::WaitingForSync;
        }
        else if ((uint32_t)(now - twoHeadBiasPostSyncStartedMs) >=
                     TWO_HEAD_BIAS_POST_SYNC_IGNORE_MS &&
                 highFrequency)
        {
            appendMeasurement(twoHead10kLeftRms, twoHead10kRightRms, twoHead10kCount);
            updateLiveBiasMeasurement(true);
            status = Status::CapturingHigh;
        }
        break;
    case TwoHeadBiasPlaybackState::WaitSync3:
        status = Status::WaitingForSync;
        if (syncIsFound())
        {
            twoHeadBiasPlaybackState = TwoHeadBiasPlaybackState::ShowResult;
            resetSyncDetections();
            if (twoHead440Count >= TWO_HEAD_BIAS_MIN_SAMPLES &&
                twoHead10kCount >= TWO_HEAD_BIAS_MIN_SAMPLES)
                finalizeTwoHeadBiasAnalysis();
            else
            {
                twoHeadSequenceFound = false;
                session.biasTestDone = true;
                session.biasMatched = false;
                state = CalibrationState::TwoHeadBiasResult;
                status = Status::NoSignal;
                drawScreen();
            }
        }
        break;
    case TwoHeadBiasPlaybackState::ShowResult:
        break;
    }

    if (state == CalibrationState::TwoHeadBiasAnalyzing &&
        now - twoHeadStateStartedMs >= TWO_HEAD_BIAS_ANALYZE_TIMEOUT_MS)
    {
        twoHeadSequenceFound = false;
        session.biasTestDone = true;
        session.biasMatched = false;
        state = CalibrationState::TwoHeadBiasResult;
        status = Status::NoSignal;
        drawScreen();
    }
}

void Record::finalizeTwoHeadLevelAnalysis()
{
    if (twoHeadRmsMeasurementCount == 0)
        return;

    const float reference = setAudio.rmsReference();
    leftLevelDb = rmsToDb(twoHeadSumLeftRms / twoHeadRmsMeasurementCount, reference);
    rightLevelDb = rmsToDb(twoHeadSumRightRms / twoHeadRmsMeasurementCount, reference);
    leftSignalValid = true;
    rightSignalValid = true;
    const bool matched = fabsf(leftLevelDb - RECORD_LEVEL_DB) <= RECORD_LEVEL_TOLERANCE_2HEAD_DB &&
                         fabsf(rightLevelDb - RECORD_LEVEL_DB) <= RECORD_LEVEL_TOLERANCE_2HEAD_DB;
    if (matched)
    {
        twoHeadSequenceFound = true;
        state = CalibrationState::TwoHeadLevelMatched;
        session.levelTestDone = true;
        session.levelMatched = true;
        captureFinalRecordLevel();
        status = Status::LevelMatched;
        drawScreen();
        return;
    }

    twoHeadSequenceFound = true;
    session.levelTestDone = true;
    session.levelMatched = false;
    state = CalibrationState::TwoHeadLevelResult;
    status = Status::AdjustRecLevel;
    drawScreen();
}

void Record::finalizeTwoHeadBiasAnalysis()
{
    bias440LeftDb = medianBiasSamples(twoHead440LeftRms, twoHead440Count);
    bias440RightDb = medianBiasSamples(twoHead440RightRms, twoHead440Count);
    bias10kLeftDb = medianBiasSamples(twoHead10kLeftRms, twoHead10kCount);
    bias10kRightDb = medianBiasSamples(twoHead10kRightRms, twoHead10kCount);
    bias440Valid = twoHead440Count >= TWO_HEAD_BIAS_MIN_SAMPLES;
    bias10kValid = twoHead10kCount >= TWO_HEAD_BIAS_MIN_SAMPLES;
    twoHeadSequenceFound = bias440Valid && bias10kValid;
    if (!twoHeadSequenceFound)
    {
        session.biasTestDone = true;
        session.biasMatched = false;
        biasTestLevelValid = false;
        twoHeadResultMatched = false;
        state = CalibrationState::TwoHeadBiasResult;
        status = Status::NoSignal;
        drawScreen();
        buzzer.play(BeepPattern::Failure);
        return;
    }

    const bool levelTooHigh = bias440LeftDb > REC_CAL_LEVEL_MAX_DB ||
                              bias440RightDb > REC_CAL_LEVEL_MAX_DB ||
                              bias10kLeftDb > REC_CAL_LEVEL_MAX_DB ||
                              bias10kRightDb > REC_CAL_LEVEL_MAX_DB;
    const bool levelTooLow = bias440LeftDb < REC_CAL_LEVEL_MIN_DB ||
                             bias440RightDb < REC_CAL_LEVEL_MIN_DB ||
                             bias10kLeftDb < REC_CAL_LEVEL_MIN_DB ||
                             bias10kRightDb < REC_CAL_LEVEL_MIN_DB;
    biasTestLevelValid = !levelTooHigh && !levelTooLow;
    if (!biasTestLevelValid)
    {
        session.biasTestDone = true;
        session.biasMatched = false;
        twoHeadResultMatched = false;
        state = CalibrationState::TwoHeadBiasResult;
        status = levelTooHigh ? Status::LevelTooHigh : Status::LevelTooLow;
        drawScreen();
        buzzer.play(BeepPattern::Failure);
        return;
    }

    const float diffLeft = bias10kLeftDb - bias440LeftDb;
    const float diffRight = bias10kRightDb - bias440RightDb;
    twoHeadResultMatched = twoHeadSequenceFound &&
                           fabsf(diffLeft) <= BIAS_MATCH_TOLERANCE_DB &&
                           fabsf(diffRight) <= BIAS_MATCH_TOLERANCE_DB;
    if (twoHeadResultMatched)
    {
        state = CalibrationState::TwoHeadBiasMatched;
        session.biasTestDone = true;
        session.biasMatched = true;
        status = Status::BiasMatched;
        captureFinalBias();
        drawScreen();
        buzzer.play(BeepPattern::Success);
        return;
    }

    const float leftMinCorrection = -BIAS_MATCH_TOLERANCE_DB - diffLeft;
    const float leftMaxCorrection = BIAS_MATCH_TOLERANCE_DB - diffLeft;
    const float rightMinCorrection = -BIAS_MATCH_TOLERANCE_DB - diffRight;
    const float rightMaxCorrection = BIAS_MATCH_TOLERANCE_DB - diffRight;
    const float commonMin = max(leftMinCorrection, rightMinCorrection);
    const float commonMax = min(leftMaxCorrection, rightMaxCorrection);

    state = CalibrationState::TwoHeadBiasResult;
    session.biasTestDone = true;
    session.biasMatched = false;
    if (commonMin > commonMax)
        status = Status::ChannelMismatch;
    else
    {
        const float commonCorrection = 0.5f * (commonMin + commonMax);
        if (commonCorrection > BIAS_CORRECTION_NEAR_ZERO_DB)
            status = Status::DecreaseBias;
        else if (commonCorrection < -BIAS_CORRECTION_NEAR_ZERO_DB)
            status = Status::IncreaseBias;
        else
            status = Status::BiasBorderline;
    }
    drawScreen();
    buzzer.play(BeepPattern::Success);
}

bool Record::setTwoHeadBiasSegment(BiasSequenceSegment segment)
{
    twoHeadBiasSegment = segment;
    twoHeadStateStartedMs = millis();
    bool outputReady = true;
    if (segment == BiasSequenceSegment::Sync1 ||
        segment == BiasSequenceSegment::Sync2 ||
        segment == BiasSequenceSegment::SyncFinal)
    {
        outputReady = setGeneratorOutput(
            REC_CAL_SYNC_FREQUENCY_HZ,
            TWO_HEAD_BIAS_SYNC_LEVEL_DB +
                getGeneratorCalibrationCorrectionDb(REC_CAL_SYNC_FREQUENCY_HZ));
    }
    else if (segment == BiasSequenceSegment::TestLow440)
    {
        outputReady = setGeneratorOutput(
            BIAS_LOW_FREQUENCY_HZ,
            TWO_HEAD_BIAS_TEST_LEVEL_DB +
                getGeneratorCalibrationCorrectionDb(BIAS_LOW_FREQUENCY_HZ));
    }
    else if (segment == BiasSequenceSegment::TestHigh10k)
    {
        outputReady = setGeneratorOutput(
            BIAS_HIGH_FREQUENCY_HZ,
            TWO_HEAD_BIAS_TEST_LEVEL_DB +
                getGeneratorCalibrationCorrectionDb(BIAS_HIGH_FREQUENCY_HZ));
    }
    else
    {
        stopGenerator();
        return true;
    }

    if (!outputReady)
        return false;

    ad9833.enable();
    return true;
}

void Record::advanceTwoHeadBiasSequence()
{
    const uint32_t duration = twoHeadBiasSegment == BiasSequenceSegment::Sync1 ||
                                      twoHeadBiasSegment == BiasSequenceSegment::Sync2 ||
                                      twoHeadBiasSegment == BiasSequenceSegment::SyncFinal
                                  ? TWO_HEAD_BIAS_SYNC_MS
                                  : TWO_HEAD_BIAS_TONE_MS;
    if (millis() - twoHeadStateStartedMs < duration)
        return;

    bool segmentReady = true;
    switch (twoHeadBiasSegment)
    {
    case BiasSequenceSegment::Sync1:
        segmentReady = setTwoHeadBiasSegment(BiasSequenceSegment::TestLow440);
        break;
    case BiasSequenceSegment::TestLow440:
        segmentReady = setTwoHeadBiasSegment(BiasSequenceSegment::Sync2);
        break;
    case BiasSequenceSegment::Sync2:
        segmentReady = setTwoHeadBiasSegment(BiasSequenceSegment::TestHigh10k);
        break;
    case BiasSequenceSegment::TestHigh10k:
        segmentReady = setTwoHeadBiasSegment(BiasSequenceSegment::SyncFinal);
        break;
    case BiasSequenceSegment::SyncFinal:
        stopGenerator();
        buzzer.play(BeepPattern::Success);
        state = CalibrationState::TwoHeadBiasRewind;
        status = Status::Ready;
        drawScreen();
        break;
    }

    if (!segmentReady)
    {
        stopGenerator();
        state = CalibrationState::TwoHeadBiasReadyToRecord;
        status = Status::NoSignal;
        drawScreen();
    }
}

void Record::handleTwoHeadAction()
{
    switch (state)
    {
    case CalibrationState::TwoHeadLevelReadyToRecord:
        startTwoHeadLevelRecording();
        break;
    case CalibrationState::TwoHeadLevelRecording:
        stopGenerator();
        state = CalibrationState::TwoHeadLevelReadyToRecord;
        status = Status::Ready;
        break;
    case CalibrationState::TwoHeadLevelRewind:
        stopGenerator();
        state = CalibrationState::TwoHeadLevelReadyToAnalyze;
        status = Status::Ready;
        break;
    case CalibrationState::TwoHeadLevelReadyToAnalyze:
        startTwoHeadLevelAnalysis();
        break;
    case CalibrationState::TwoHeadLevelAnalyzing:
        startTwoHeadLevelAnalysis();
        break;
    case CalibrationState::TwoHeadLevelResult:
        resetLevelTest();
        state = CalibrationState::TwoHeadLevelReadyToRecord;
        break;
    case CalibrationState::TwoHeadLevelMatched:
        resetLevelTest();
        state = CalibrationState::TwoHeadLevelReadyToRecord;
        break;
    case CalibrationState::TwoHeadBiasReadyToRecord:
        startTwoHeadBiasRecording();
        break;
    case CalibrationState::TwoHeadBiasRecording:
        stopGenerator();
        resetMeasurements();
        resetBiasAverages();
        resetTwoHeadAnalysis();
        state = CalibrationState::TwoHeadBiasReadyToRecord;
        status = Status::Ready;
        break;
    case CalibrationState::TwoHeadBiasRewind:
        stopGenerator();
        state = CalibrationState::TwoHeadBiasReadyToAnalyze;
        status = Status::Ready;
        break;
    case CalibrationState::TwoHeadBiasReadyToAnalyze:
        startTwoHeadBiasAnalysis();
        break;
    case CalibrationState::TwoHeadBiasAnalyzing:
        stopGenerator();
        resetMeasurements();
        resetBiasAverages();
        resetTwoHeadAnalysis();
        state = CalibrationState::TwoHeadBiasReadyToRecord;
        status = Status::Ready;
        break;
    case CalibrationState::TwoHeadBiasResult:
        resetBiasTest();
        state = CalibrationState::TwoHeadBiasReadyToRecord;
        break;
    case CalibrationState::TwoHeadBiasMatched:
        resetBiasTest();
        state = CalibrationState::TwoHeadBiasReadyToRecord;
        break;
    default:
        break;
    }
}

void Record::showHelp()
{
    const bool twoHeadBias = isTwoHeadBiasState();
    const bool twoHeadLevel = isTwoHeadState() && !twoHeadBias;
    const char *const helpTitle = isLevelState() ? "TEST: RECORD LEVEL" : "TEST: BIAS CAL";
    const int helpX = TEXT_X + tft.textWidth(helpTitle, ITEM_FONT) + 4;
    help.drawButton(helpX, TEST_Y, true);
    while (touch.pressed())
        delay(5);

    if (twoHeadLevel)
        help.showModal(TWO_HEAD_LEVEL_HELP_TITLE, TWO_HEAD_LEVEL_HELP_TEXT,
                       sizeof(TWO_HEAD_LEVEL_HELP_TEXT) / sizeof(TWO_HEAD_LEVEL_HELP_TEXT[0]));
    else if (twoHeadBias)
        help.showModal(TWO_HEAD_BIAS_HELP_TITLE, TWO_HEAD_BIAS_HELP_TEXT,
                       sizeof(TWO_HEAD_BIAS_HELP_TEXT) / sizeof(TWO_HEAD_BIAS_HELP_TEXT[0]));
    else
        help.showModal(BIAS_HELP_TITLE, BIAS_HELP_TEXT,
                       sizeof(BIAS_HELP_TEXT) / sizeof(BIAS_HELP_TEXT[0]));
    drawScreen();
}

// =========================================================================
// Unified 3-HEAD Calibration Workflow Implementation
// =========================================================================

void Record::startThreeHeadWorkflow()
{
    // Check generator calibration first
    if (!tapeResponseCalibration.valid)
    {
        workflowActive = true;
        workflowStep = ThreeHeadWorkflowStep::CalibrationRequired;
        workflowRetestOnly = false;
        workflowResult = {};
        state = CalibrationState::ThreeHeadWorkflowIntro;
        drawScreen();
        return;
    }

    workflowActive = true;
    workflowRetestOnly = false;
    workflowResult = {};
    enterWorkflowLevelStep();
}

void Record::cancelThreeHeadWorkflow()
{
    workflowActive = false;
    workflowRetestOnly = false;
    workflowStep = ThreeHeadWorkflowStep::Inactive;
    workflowResult = {};
    stopGenerator();
    resetMeasurements();
    resetBiasMatchCounters();
    resetBiasAverages();
    biasCompletedCycles = 0;
    biasHighFrequency = false;
    resetLevelTest();
    resetBiasTest();
    resetTapeResponse();
    state = CalibrationState::SelectTest;
    highlightedTestType = TestType::None;
    selectedRecordActionIndex = 0;
    drawScreen();
}

void Record::advanceThreeHeadWorkflow()
{
    if (!workflowActive || workflowRetestOnly)
        return;

    switch (workflowStep)
    {
    case ThreeHeadWorkflowStep::Level:
        if (workflowResult.levelMatched)
        {
            captureWorkflowLevelResult();
            enterWorkflowBiasStep();
        }
        break;

    case ThreeHeadWorkflowStep::Bias:
        if (workflowResult.biasMatched)
        {
            captureWorkflowBiasResult();
            enterWorkflowResponseStep();
        }
        break;

    case ThreeHeadWorkflowStep::Response:
        if (workflowResult.responseValid)
        {
            captureWorkflowResponseResult();
            workflowStep = ThreeHeadWorkflowStep::Complete;
            state = CalibrationState::ThreeHeadWorkflowSummary;
            drawScreen();
        }
        break;

    default:
        break;
    }
}

void Record::enterWorkflowLevelStep()
{
    workflowStep = ThreeHeadWorkflowStep::LevelIntro;
    state = CalibrationState::ThreeHeadWorkflowIntro;
    drawScreen();
}

void Record::enterWorkflowBiasStep()
{
    workflowStep = ThreeHeadWorkflowStep::BiasIntro;
    state = CalibrationState::ThreeHeadWorkflowIntro;
    drawScreen();
}

void Record::enterWorkflowResponseStep()
{
    workflowStep = ThreeHeadWorkflowStep::ResponseIntro;
    state = CalibrationState::ThreeHeadWorkflowIntro;
    drawScreen();
}

void Record::captureWorkflowLevelResult()
{
    if (!leftSignalValid || !rightSignalValid)
        return;

    workflowResult.levelDone = true;
    workflowResult.levelLeftDb = leftLevelDb;
    workflowResult.levelRightDb = rightLevelDb;
    workflowResult.levelValid = true;

    const float diffLeft = fabsf(leftLevelDb - RECORD_LEVEL_DB);
    const float diffRight = fabsf(rightLevelDb - RECORD_LEVEL_DB);
    workflowResult.levelMatched = (diffLeft <= RECORD_LEVEL_TOLERANCE_3HEAD_DB) &&
                                  (diffRight <= RECORD_LEVEL_TOLERANCE_3HEAD_DB);

    captureFinalRecordLevel();
}

void Record::captureWorkflowBiasResult()
{
    if (!bias440Valid || !bias10kValid)
        return;

    workflowResult.biasDone = true;
    workflowResult.biasDiffLeftDb = bias10kLeftDb - bias440LeftDb;
    workflowResult.biasDiffRightDb = bias10kRightDb - bias440RightDb;
    workflowResult.biasValid = biasTestLevelValid;

    workflowResult.biasMatched = biasTestLevelValid &&
                                 (fabsf(workflowResult.biasDiffLeftDb) <= REC_CAL_BIAS_ACCEPTABLE_TOLERANCE_DB) &&
                                 (fabsf(workflowResult.biasDiffRightDb) <= REC_CAL_BIAS_ACCEPTABLE_TOLERANCE_DB);

    captureFinalThreeHeadBias();
}

void Record::captureWorkflowResponseResult()
{
    if (!tapeResponseResult.valid)
        return;

    workflowResult.responseDone = true;
    workflowResult.responseValid = true;

    // Calculate max deviation from relative response
    float maxDev = 0.0f;
    for (uint8_t i = 0; i < TAPE_RESPONSE_BAND_COUNT; ++i)
    {
        if (isfinite(tapeResponseResult.leftRelativeDb[i]))
            maxDev = fmaxf(maxDev, fabsf(tapeResponseResult.leftRelativeDb[i]));
        if (isfinite(tapeResponseResult.rightRelativeDb[i]))
            maxDev = fmaxf(maxDev, fabsf(tapeResponseResult.rightRelativeDb[i]));
    }
    workflowResult.responseMaxDeviationDb = maxDev;

    // Store final tape response result
    session.responseTestDone = true;
}

bool Record::handleWorkflowTouch(uint16_t x, uint16_t y)
{
    while (touch.pressed())
        delay(5);

    // Check for footer BACK button
    if (y >= FOOTER_Y)
    {
        // Check if workflow was launched from System Configuration (Auto Cal)
        const bool returnToSystemSettingsOnBack = openAutoCalOnRun;
        
        // If workflow was launched from System Configuration and we're in CalibrationRequired state,
        // return to System Configuration instead of Record screen
        if (returnToSystemSettingsOnBack && state == CalibrationState::ThreeHeadWorkflowIntro && 
            workflowStep == ThreeHeadWorkflowStep::CalibrationRequired)
        {
            // Return to System Configuration
            workflowActive = false;
            workflowRetestOnly = false;
            workflowStep = ThreeHeadWorkflowStep::Inactive;
            workflowResult = {};
            stopGenerator();
            resetMeasurements();
            resetBiasMatchCounters();
            resetBiasAverages();
            biasCompletedCycles = 0;
            biasHighFrequency = false;
            resetLevelTest();
            resetBiasTest();
            resetTapeResponse();
            state = CalibrationState::ThreeHeadLoopbackReady;
            // Keep deckType, highlightedDeckType, and highlightedTestType as set when launched from System Configuration
            deckType = DeckType::ThreeHead;
            highlightedDeckType = DeckType::ThreeHead;
            highlightedTestType = TestType::TapeResponse;
            selectedRecordActionIndex = 0;
            drawScreen();
            return true;
        }
        else
        {
            // Normal behavior - cancel workflow and return to Record screen
            cancelThreeHeadWorkflow();
            return true;
        }
    }

    // Workflow Intro screens (LevelIntro, BiasIntro, ResponseIntro, CalibrationRequired)
    if (state == CalibrationState::ThreeHeadWorkflowIntro)
    {
        // Check NEXT button to proceed to the actual test
        if (workflowStep != ThreeHeadWorkflowStep::CalibrationRequired)
        {
            const int buttonX = LCD_WIDTH / 2 - 60;
            const int buttonY = 170;
            const int buttonW = 120;
            const int buttonH = 36;
            if (contains(x, y, buttonX, buttonY, buttonW, buttonH))
            {
                if (workflowStep == ThreeHeadWorkflowStep::LevelIntro)
                {
                    workflowStep = ThreeHeadWorkflowStep::Level;
                    startThreeHeadLevel();
                }
                else if (workflowStep == ThreeHeadWorkflowStep::BiasIntro)
                {
                    workflowStep = ThreeHeadWorkflowStep::Bias;
                    startBias();
                }
                else if (workflowStep == ThreeHeadWorkflowStep::ResponseIntro)
                {
                    workflowStep = ThreeHeadWorkflowStep::Response;
                    if (tapeResponseCalibration.valid)
                        startTapeResponse();
                    else
                        state = CalibrationState::ThreeHeadResponseCalibrationRequired;
                }
                drawScreen();
                return true;
            }
        }

        return true;
    }

    // Workflow Summary screen
    if (state == CalibrationState::ThreeHeadWorkflowSummary)
    {
        // Check RETEST button (re-run the last step)
        const int retestX = 20;
        const int retestY = 200;
        const int retestW = 100;
        const int retestH = 36;
        if (contains(x, y, retestX, retestY, retestW, retestH))
        {
            workflowRetestOnly = true;
            if (workflowResult.responseDone && !workflowResult.responseValid)
            {
                // Retest response
                resetTapeResponse();
                workflowStep = ThreeHeadWorkflowStep::Response;
                if (tapeResponseCalibration.valid)
                    startTapeResponse();
                else
                    state = CalibrationState::ThreeHeadResponseCalibrationRequired;
            }
            else if (workflowResult.biasDone && !workflowResult.biasMatched)
            {
                // Retest bias
                resetBiasTest();
                workflowStep = ThreeHeadWorkflowStep::Bias;
                startBias();
            }
            else if (workflowResult.levelDone && !workflowResult.levelMatched)
            {
                // Retest level
                resetLevelTest();
                workflowStep = ThreeHeadWorkflowStep::Level;
                startThreeHeadLevel();
            }
            drawScreen();
            return true;
        }

        // Check NEXT/FINISH button
        const int nextX = 200;
        const int nextY = 200;
        const int nextW = 100;
        const int nextH = 36;
        if (contains(x, y, nextX, nextY, nextW, nextH))
        {
            workflowActive = false;
            workflowRetestOnly = false;
            stopGenerator();
            resetMeasurements();
            resetBiasMatchCounters();
            resetBiasAverages();
            biasCompletedCycles = 0;
            biasHighFrequency = false;
            resetTapeResponse();
            state = CalibrationState::SelectTest;
            highlightedTestType = TestType::None;
            selectedRecordActionIndex = 0;
            drawScreen();
            return true;
        }

        return true;
    }

    return false;
}

void Record::drawWorkflowIntroScreen()
{
    const char *title = "";
    const char *subtitle = "";
    const char *detail1 = "";
    const char *detail2 = "";

    switch (workflowStep)
    {
    case ThreeHeadWorkflowStep::LevelIntro:
        title = "STEP 1: RECORD LEVEL";
        subtitle = "Adjust REC LEVEL for -10.0 dB";
        detail1 = "FREQ: 440 Hz";
        detail2 = "GEN: -10.0 dB";
        break;

    case ThreeHeadWorkflowStep::BiasIntro:
        title = "STEP 2: BIAS ADJUST";
        subtitle = "Match 10 kHz to 440 Hz level";
        detail1 = "TARGET: 0.0 dB diff";
        detail2 = "3 stable cycles needed";
        break;

    case ThreeHeadWorkflowStep::ResponseIntro:
        title = "STEP 3: TAPE RESPONSE";
        subtitle = "Full frequency sweep";
        detail1 = "RANGE: 50 Hz - 15 kHz";
        detail2 = "GEN: -10.0 dB";
        break;

    default:
        break;
    }

    display.openApp("3 HEAD FULL CAL", "TOUCH TO BACK");
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_CYAN, COL_BG);
    tft.drawString(title, TEXT_X, 30, ITEM_FONT);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(subtitle, TEXT_X, 56, ITEM_FONT);
    tft.drawString(detail1, TEXT_X, 84, ITEM_FONT);
    tft.drawString(detail2, TEXT_X, 108, ITEM_FONT);

    if (workflowStep == ThreeHeadWorkflowStep::ResponseIntro && !tapeResponseCalibration.valid)
    {
        tft.setTextColor(TFT_YELLOW, COL_BG);
        tft.drawString("LOOPBACK CAL REQUIRED", TEXT_X, 140, ITEM_FONT);
    }

    // Draw NEXT button
    const int buttonX = LCD_WIDTH / 2 - 60;
    const int buttonY = 170;
    const int buttonW = 120;
    const int buttonH = 36;
    drawWorkflowButton(buttonX, buttonY, "NEXT");
}

void Record::drawWorkflowCalibrationRequiredScreen()
{
    display.openApp("3 HEAD FULL CAL", "TOUCH TO BACK");
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_RED, COL_BG);
    tft.drawString("GENERATOR CAL REQUIRED", TEXT_X, 30, ITEM_FONT);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("Run AUTO CAL in", TEXT_X, 56, ITEM_FONT);
    tft.drawString("SYSTEM SETTINGS first.", TEXT_X, 80, ITEM_FONT);
    tft.drawString("Connect OUT -> IN L/R", TEXT_X, 104, ITEM_FONT);
}

void Record::drawWorkflowCompleteScreen()
{
    display.openApp("3 HEAD FULL CAL", "TOUCH TO BACK");
    tft.setTextDatum(TL_DATUM);

    tft.setTextColor(TFT_GREEN, COL_BG);
    tft.drawString("WORKFLOW COMPLETE", TEXT_X, 18, ITEM_FONT);

    int y = 48;

    // Level result
    const bool levelDone = workflowResult.levelDone;
    const bool levelPassed = levelDone && workflowResult.levelMatched;
    drawWorkflowSummaryLine(y, "REC LEVEL",
                            levelDone ? (levelPassed ? "MATCHED" : "UNMATCHED") : "NOT RUN",
                            levelDone, levelPassed);
    y += 22;

    // Bias result
    const bool biasDone = workflowResult.biasDone;
    const bool biasPassed = biasDone && workflowResult.biasMatched;
    drawWorkflowSummaryLine(y, "BIAS",
                            biasDone ? (biasPassed ? "MATCHED" : "UNMATCHED") : "NOT RUN",
                            biasDone, biasPassed);
    y += 22;

    // Tape Response result
    const bool respDone = workflowResult.responseDone;
    const bool respPassed = respDone && workflowResult.responseValid;
    char respValue[32];
    if (respDone && workflowResult.responseValid && isfinite(workflowResult.responseMaxDeviationDb))
    {
        snprintf(respValue, sizeof(respValue), "%.1f dB max", workflowResult.responseMaxDeviationDb);
    }
    else
    {
        snprintf(respValue, sizeof(respValue), "%s", respDone ? "COMPLETED" : "NOT RUN");
    }
    drawWorkflowSummaryLine(y, "TAPE EQ",
                            respValue,
                            respDone, respPassed);

    // Buttons: RETEST and FINISH
    const int retestX = 20;
    const int retestY = 200;
    const int nextX = 200;
    const int nextY = 200;
    const int btnW = 100;
    const int btnH = 36;

    drawWorkflowButton(retestX, retestY, "RETEST");
    drawWorkflowButton(nextX, nextY, "FINISH");
}

void Record::drawWorkflowSummaryLine(int y, const char *label, const char *value,
                                     bool done, bool passed)
{
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(label, TEXT_X, y, ITEM_FONT);

    uint16_t color = TFT_DARKGREY;
    if (done)
        color = passed ? TFT_GREEN : TFT_ORANGE;

    tft.setTextColor(color, COL_BG);
    tft.setTextPadding(150);
    tft.drawString(value, TEXT_X + 80, y, ITEM_FONT);
    tft.setTextPadding(0);
}

void Record::drawWorkflowButton(int x, int y, const char *label)
{
    tft.fillRect(x, y, 100, 36, TFT_DARKCYAN);
    tft.drawRect(x, y, 100, 36, TFT_GREEN);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_GREEN, TFT_DARKCYAN);
    tft.drawString(label, x + 50, y + 18, ITEM_FONT);
    tft.setTextDatum(TL_DATUM);
}

#include "SetAudio.h"

#include "Audio.h"
#include "Config.h"
#include "Display.h"
#include "Help.h"
#include "SignalFrequency.h"
#include "Storage.h"
#include "Touch.h"

#include <TFT_eSPI.h>

extern Display display;
extern Audio audio;
extern Storage storage;
extern TFT_eSPI tft;
extern Touch touch;

SetAudio setAudio;

namespace
{
    constexpr uint8_t ITEM_FONT = 2;
    constexpr int ITEM_X = CONTENT_LEFT + 8;
    constexpr int VALUE_RIGHT = CONTENT_RIGHT - 8;
    constexpr int ITEM_HEIGHT = 16;
    constexpr int ITEM_ZONE_HEIGHT = ITEM_HEIGHT + 3;
    constexpr int ITEM_Y[] = {30, 52, 74, 96, 118, 140, 184};
    constexpr int ADC_CALIBRATION_ITEM_Y = 162;
    constexpr int TOUCH_PADDING_Y = 3;
    constexpr int RUN_BUTTON_PADDING_X = 5;
    constexpr int FOOTER_BUTTON_Y = 215;
    constexpr int FOOTER_BUTTON_HEIGHT = 16;
    constexpr int FOOTER_BUTTON_PADDING_X = RUN_BUTTON_PADDING_X;
    constexpr int FOOTER_BUTTON_GAP = 2;
    constexpr int FOOTER_CLOCK_GAP = 4;
    constexpr int FOOTER_CLOCK_RIGHT = 308;
    constexpr char RMS_REFERENCE_LABEL[] = "1. 0dB RMS Reference";
    constexpr char RMS_REFERENCE_HELP_TITLE[] = "0 dB RMS REFERENCE";
    const char *const RMS_REFERENCE_HELP_TEXT[] =
    {
        "Set Generator to:",
        "1 kHz / 0.0 dB / RUN",
        "",
        "Connect OUT to IN L and R.",
        "",
        "Adjust this value until Playback",
        "shows 0 dB on both channels.",
        "",
        "The same reference is used by",
        "Spectrum Analyzer."
    };
    constexpr char VU_SETTINGS_HELP_TITLE[] = "VU SETTINGS";
    const char *const VU_SETTINGS_HELP_TEXT[] =
    {
        "VU smoothing controls display stability.",
        "Higher values give a steadier but slower",
        "response.",
        "",
        "Peak hold sets how long the maximum",
        "marker remains visible.",
        "",
        "Peak decay controls how fast the marker",
        "falls after the hold period."
    };
    constexpr char WF_NOISE_FLOOR_HELP_TITLE[] = "W&F NOISE FLOOR";
    const char *const WF_NOISE_FLOOR_HELP_TEXT[] =
    {
        "Noise level removed from the displayed",
        "Wow & Flutter RMS result.",
        "",
        "Adjust only during TapeLAB calibration.",
        "Do not increase it to improve a deck result.",
        "",
        "A value that is too high may hide",
        "real speed variations."
    };
    constexpr char ADC_ZERO_CAL_HELP_TITLE[] = "ADC ZERO CALIBRATION";
    const char *const ADC_ZERO_CAL_HELP_TEXT[] =
    {
        "Disconnect the input cable or make sure",
        "no signal is applied to either input.",
        "",
        "Run the calibration and wait until it",
        "finishes.",
        "",
        "The measured ADC offsets become the",
        "zero reference for subsequent readings."
    };
    constexpr char CALIBRATION_3150_HELP_TITLE[] =
        "3150 Hz RAW CALIBRATION";
    const char *const CALIBRATION_3150_HELP_TEXT[] =
    {
        "Use TapeLAB Generator or a calibrated",
        "external 3150 Hz source.",
        "",
        "Connect the signal to input L.",
        "",
        "Touch RUN, wait for a stable valid value,",
        "then touch OK to store the calibration.",
        "",
        "Valid range: 3000 to 3300 Hz."
    };

    constexpr uint16_t RMS_REFERENCE_MIN = 50;
    constexpr uint16_t RMS_REFERENCE_MAX = 1000;
    constexpr uint16_t RMS_REFERENCE_STEP = 1;
    constexpr uint8_t SMOOTHING_MIN = 10;
    constexpr uint8_t SMOOTHING_MAX = 95;
    constexpr uint8_t SMOOTHING_STEP = 5;
    constexpr uint16_t PEAK_HOLD_MIN = 100;
    constexpr uint16_t PEAK_HOLD_MAX = 2000;
    constexpr uint16_t PEAK_HOLD_STEP = 50;
    constexpr uint8_t PEAK_DECAY_MIN = 5;
    constexpr uint8_t PEAK_DECAY_MAX = 200;
    constexpr uint8_t PEAK_DECAY_STEP = 5;
    constexpr int8_t OVER_LEVEL_MIN = 0;
    constexpr int8_t OVER_LEVEL_MAX = 12;
    constexpr uint16_t WF_NOISE_FLOOR_MIN = 0;
    constexpr uint16_t WF_NOISE_FLOOR_MAX = 200;
    constexpr uint16_t WF_NOISE_FLOOR_STEP = 1;
    const uint16_t COL_CYAN_OFF = tft.color565(0, 20, 40);

    struct FooterButtonLayout
    {
        int minusLeft;
        int minusWidth;
        int plusLeft;
        int plusWidth;
    };

    FooterButtonLayout getFooterButtonLayout()
    {
        const int clockLeft =
            FOOTER_CLOCK_RIGHT - tft.textWidth("00:00:00", ITEM_FONT);
        const int plusWidth =
            tft.textWidth("[+]", ITEM_FONT) + 2 * FOOTER_BUTTON_PADDING_X;
        const int plusLeft = clockLeft - FOOTER_CLOCK_GAP - plusWidth;
        const int minusWidth =
            tft.textWidth("[-]", ITEM_FONT) + 2 * FOOTER_BUTTON_PADDING_X;
        const int minusLeft = plusLeft - FOOTER_BUTTON_GAP - minusWidth;

        return {minusLeft, minusWidth, plusLeft, plusWidth};
    }

    int getRmsReferenceHelpX()
    {
        return ITEM_X +
               tft.textWidth(RMS_REFERENCE_LABEL, ITEM_FONT) + 4;
    }

    int getVuSettingsHelpX()
    {
        return ITEM_X +
               tft.textWidth("2. VU smoothing", ITEM_FONT) + 4;
    }

    int getWfNoiseFloorHelpX()
    {
        return ITEM_X +
               tft.textWidth("6. W&F Noise Floor", ITEM_FONT) + 4;
    }

    int getAdcCalibrationHelpX()
    {
        return ITEM_X +
               tft.textWidth("7. ADC ZERO CAL.", ITEM_FONT) + 4;
    }

    int getCalibration3150HzHelpX()
    {
        return ITEM_X +
               tft.textWidth("8. 3150 Hz RAW", ITEM_FONT) + 4;
    }

    int getCalibration3150HzFrequencyX()
    {
        return getCalibration3150HzHelpX() +
               help.buttonWidth() + 4;
    }

    void drawCalibrationItem(
        const char *label,
        int y,
        bool selected,
        bool runPressed)
    {
        const uint16_t background = selected ? COL_CYAN_OFF : COL_BG;
        const int runWidth = tft.textWidth("[RUN]", ITEM_FONT);
        const int runLeft = VALUE_RIGHT - runWidth;

        tft.fillRect(
            CONTENT_LEFT + 2,
            y - TOUCH_PADDING_Y,
            CONTENT_WIDTH - 4,
            ITEM_ZONE_HEIGHT,
            background);
        tft.setTextColor(selected ? TFT_GREEN : TFT_WHITE, background);
        tft.drawString(label, ITEM_X, y, ITEM_FONT);
        const uint16_t runBackground = runPressed ? TFT_DARKGREY : background;
        tft.fillRect(
            runLeft - RUN_BUTTON_PADDING_X,
            y - TOUCH_PADDING_Y,
            runWidth + 2 * RUN_BUTTON_PADDING_X,
            ITEM_ZONE_HEIGHT,
            runBackground);
        tft.setTextColor(TFT_WHITE, runBackground);
        tft.drawRightString("[RUN]", VALUE_RIGHT, y, ITEM_FONT);
    }

    void drawAdcCalibrationItem(bool selected, bool runPressed)
    {
        drawCalibrationItem(
            "7. ADC ZERO CAL.",
            ADC_CALIBRATION_ITEM_Y,
            selected,
            runPressed);
        help.drawButton(
            getAdcCalibrationHelpX(),
            ADC_CALIBRATION_ITEM_Y,
            false,
            selected ? COL_CYAN_OFF : COL_BG);
    }

    bool validSettingValues(
        const AudioSettings &value)
    {
        return value.rmsReference >= RMS_REFERENCE_MIN &&
               value.rmsReference <= RMS_REFERENCE_MAX &&

               value.smoothingPercent >= SMOOTHING_MIN &&
               value.smoothingPercent <= SMOOTHING_MAX &&

               value.peakHoldMs >= PEAK_HOLD_MIN &&
               value.peakHoldMs <= PEAK_HOLD_MAX &&

               value.peakDecayHundredths >= PEAK_DECAY_MIN &&
               value.peakDecayHundredths <= PEAK_DECAY_MAX &&

               value.overLevelDb >= OVER_LEVEL_MIN &&
               value.overLevelDb <= OVER_LEVEL_MAX &&

               value.wfNoiseFloorThousandths >=
                   WF_NOISE_FLOOR_MIN &&
               value.wfNoiseFloorThousandths <=
                   WF_NOISE_FLOOR_MAX &&

               value.frequencyCalibrationPpm >= 950000UL &&
               value.frequencyCalibrationPpm <= 1050000UL;
    }

    bool validSettings(const AudioSettings &value)
    {
        return value.signature == AUDIO_SETTINGS_SIGNATURE &&
               value.version == AUDIO_SETTINGS_VERSION &&
               validSettingValues(value);
    }

}

void SetAudio::begin()
{
    AudioSettings stored;
    if (storage.loadAudioSettings(stored) && validSettings(stored))
    {
        settings = stored;
        SignalFrequency::setCalibrationPpm(
            settings.frequencyCalibrationPpm);
        return;
    }

    if (stored.signature == AUDIO_SETTINGS_SIGNATURE &&
        stored.version == 2 &&
        validSettingValues(stored))
    {
        stored.version = AUDIO_SETTINGS_VERSION;
        stored.rawFrequency = 3150;
        stored.frequencyCalibrationPpm = 1000000;
        settings = stored;
        SignalFrequency::setCalibrationPpm(
            settings.frequencyCalibrationPpm);
        save();
    }
}

bool SetAudio::bootCheck() const
{
    const bool structureValid =
        settings.signature ==
            AUDIO_SETTINGS_SIGNATURE &&
        settings.version ==
            AUDIO_SETTINGS_VERSION;

    const bool valuesValid =
        validSettingValues(settings);

    /*
       Factorul implicit 1000000 este acceptat.
       Înseamnă calibrare neutră, nu eroare.
    */
    const bool frequencyCalibrationUsable =
        settings.frequencyCalibrationPpm >= 950000UL &&
        settings.frequencyCalibrationPpm <= 1050000UL;

    return structureValid &&
           valuesValid &&
           frequencyCalibrationUsable;
}

void SetAudio::run()
{
    drawScreen();

    while (touch.pressed())
        delay(5);

    while (true)
    {
        display.updateClock();

        if (calibration3150HzRunning)
        {
            update3150HzCalibrationInput();
            const uint32_t now = millis();
            if (now - lastCalibrationDisplayMs >= 500)
            {
                lastCalibrationDisplayMs = now;
                drawCalibration3150HzFrequency();
            }
        }

        if (touch.pressed())
        {
            const bool shouldExit = processTouch();
            while (touch.pressed())
            {
                display.updateClock();
                delay(5);
            }

            if (shouldExit)
                break;

            if (adjustmentButtonPressed)
            {
                adjustmentButtonPressed = false;
                drawAdjustmentButtons();
            }

            if (calibrationRunPressed)
            {
                calibrationRunPressed = false;
                drawField(Field::Calibration3150Hz);

                if (calibration3150HzConfirmed)
                {
                    display.setAppStatus("3150 Hz RAW. CONFIRMED");
                    drawAdjustmentButtons();
                }
            }
        }

        delay(20);
    }

    if (changed)
        save();
}

void SetAudio::drawScreen()
{
    selectedField = Field::None;
    adcCalibrationSelected = false;
    calibration3150HzRunning = false;
    calibration3150HzConfirmed = false;
    calibrationRunPressed = false;
    calibration3150HzFrequency = 0;
    adjustmentButtonPressed = false;
    lastCalibrationDisplayMs = 0;
    changed = false;

    redrawScreen();
}

void SetAudio::redrawScreen()
{
    const char *const status = calibration3150HzRunning
        ? "TOUCH [OK] TO CONFIRM"
        : "TOUCH TO BACK";

    display.openApp("AUDIO SETTINGS", status);
    tft.setTextDatum(TL_DATUM);

    drawField(Field::RmsReference);
    drawField(Field::Smoothing);
    drawField(Field::PeakHold);
    drawField(Field::PeakDecay);
    drawField(Field::OverLevel);
    drawField(Field::WfNoiseFloor);
    drawAdcCalibrationItem(false, false);
    drawField(Field::Calibration3150Hz);
    drawAdjustmentButtons();
}

void SetAudio::drawField(Field field, bool runPressed)
{
    const uint8_t index = static_cast<uint8_t>(field) - 1;
    const int y = ITEM_Y[index];
    const bool selected = selectedField == field;
    const uint16_t background = selected ? COL_CYAN_OFF : COL_BG;
    const char *label = "";
    char value[20];
    bool hasRunButton = false;
    bool hasFrequencyValue = false;

    switch (field)
    {
    case Field::RmsReference:
        label = RMS_REFERENCE_LABEL;
        snprintf(value, sizeof(value), "%u ADC", settings.rmsReference);
        break;
    case Field::Smoothing:
        label = "2. VU smoothing";
        snprintf(value, sizeof(value), "%u %%", settings.smoothingPercent);
        break;
    case Field::PeakHold:
        label = "3. VU peak hold";
        snprintf(value, sizeof(value), "%u ms", settings.peakHoldMs);
        break;
    case Field::PeakDecay:
        label = "4. VU peak decay";
        snprintf(
            value,
            sizeof(value),
            "%u.%02u dB",
            settings.peakDecayHundredths / 100,
            settings.peakDecayHundredths % 100);
        break;
    case Field::OverLevel:
        label = "5. OVER level";
        snprintf(value, sizeof(value), "+%d dB", settings.overLevelDb);
        break;
    case Field::WfNoiseFloor:
        label = "6. W&F Noise Floor";
        snprintf(
            value,
            sizeof(value),
            "%u.%03u %%",
            settings.wfNoiseFloorThousandths / 1000,
            settings.wfNoiseFloorThousandths % 1000);
        break;
    case Field::Calibration3150Hz:
        label = "8. 3150 Hz RAW";
        snprintf(
            value,
            sizeof(value),
            "%s",
            (calibration3150HzRunning || calibration3150HzConfirmed)
                ? "[OK]"
                : "[RUN]");
        hasRunButton = true;
        hasFrequencyValue =
            calibration3150HzRunning || calibration3150HzConfirmed;
        break;
    case Field::None:
        return;
    }

    tft.fillRect(
        CONTENT_LEFT + 2,
        y - TOUCH_PADDING_Y,
        CONTENT_WIDTH - 4,
        ITEM_ZONE_HEIGHT,
        background);
    tft.setTextColor(selected ? TFT_GREEN : TFT_WHITE, background);
    tft.drawString(label, ITEM_X, y, ITEM_FONT);

    if (hasFrequencyValue)
    {
        const int frequencyX =
            field == Field::Calibration3150Hz
                ? getCalibration3150HzFrequencyX()
                : ITEM_X + tft.textWidth(label, ITEM_FONT) + 4;
        char frequencyValue[18];

        if (calibration3150HzFrequency <= 0.0f)
        {
            snprintf(
                frequencyValue,
                sizeof(frequencyValue),
                "[----.-] Hz");
        }
        else
        {
            const uint32_t frequencyTenths =
                static_cast<uint32_t>(
                    calibration3150HzFrequency * 10.0f + 0.5f);

            snprintf(
                frequencyValue,
                sizeof(frequencyValue),
                "[%lu.%lu] Hz",
                static_cast<unsigned long>(
                    frequencyTenths / 10),
                static_cast<unsigned long>(
                    frequencyTenths % 10));
        }

        tft.setTextColor(selected ? TFT_GREEN : TFT_WHITE, background);
        tft.drawString(frequencyValue, frequencyX, y, ITEM_FONT);
    }

    if (!hasRunButton)
    {
        tft.drawRightString(value, VALUE_RIGHT, y, ITEM_FONT);

        if (field == Field::RmsReference)
        {
            help.drawButton(
                getRmsReferenceHelpX(),
                y,
                false,
                background);
        }
        else if (field == Field::Smoothing)
        {
            help.drawButton(
                getVuSettingsHelpX(),
                y,
                false,
                background);
        }
        else if (field == Field::WfNoiseFloor)
        {
            help.drawButton(
                getWfNoiseFloorHelpX(),
                y,
                false,
                background);
        }

        return;
    }

    const int runWidth = tft.textWidth(value, ITEM_FONT);
    const int runLeft = VALUE_RIGHT - runWidth;
    const uint16_t runBackground = runPressed ? TFT_DARKGREY : background;
    tft.fillRect(
        runLeft - RUN_BUTTON_PADDING_X,
        y - TOUCH_PADDING_Y,
        runWidth + 2 * RUN_BUTTON_PADDING_X,
        ITEM_ZONE_HEIGHT,
        runBackground);
    tft.setTextColor(TFT_WHITE, runBackground);
    tft.drawRightString(value, VALUE_RIGHT, y, ITEM_FONT);

    if (field == Field::Calibration3150Hz)
    {
        help.drawButton(
            getCalibration3150HzHelpX(),
            y,
            false,
            background);
    }
}

void SetAudio::drawCalibration3150HzFrequency()
{
    const int y = ITEM_Y[static_cast<uint8_t>(Field::Calibration3150Hz) - 1];
    const bool selected = selectedField == Field::Calibration3150Hz;
    const uint16_t background = selected ? COL_CYAN_OFF : COL_BG;
    const int frequencyX = getCalibration3150HzFrequencyX();
    const int valueRight = VALUE_RIGHT -
                           tft.textWidth("[OK]", ITEM_FONT) - 2 * RUN_BUTTON_PADDING_X - 4;

    tft.fillRect(
        frequencyX,
        y - TOUCH_PADDING_Y,
        valueRight - frequencyX,
        ITEM_ZONE_HEIGHT,
        background);

    char frequencyValue[18];

    if (calibration3150HzFrequency <= 0.0f)
    {
        snprintf(
            frequencyValue,
            sizeof(frequencyValue),
            "[----.-] Hz");
    }
    else
    {
        const uint32_t frequencyTenths =
            static_cast<uint32_t>(
                calibration3150HzFrequency * 10.0f + 0.5f);

        snprintf(
            frequencyValue,
            sizeof(frequencyValue),
            "[%lu.%lu] Hz",
            static_cast<unsigned long>(
                frequencyTenths / 10),
            static_cast<unsigned long>(
                frequencyTenths % 10));
    }

    tft.setTextColor(selected ? TFT_GREEN : TFT_WHITE, background);
    tft.drawString(frequencyValue, frequencyX, y, ITEM_FONT);
}

void SetAudio::drawAdjustmentButtons(bool minusPressed, bool plusPressed)
{
    const FooterButtonLayout layout = getFooterButtonLayout();
    const int minusCenter = layout.minusLeft + layout.minusWidth / 2;
    const int plusCenter = layout.plusLeft + layout.plusWidth / 2;
    const uint16_t minusBackground = minusPressed ? TFT_DARKGREY : COL_BG;
    const uint16_t plusBackground = plusPressed ? TFT_DARKGREY : COL_BG;

    tft.fillRect(
        layout.minusLeft,
        FOOTER_BUTTON_Y,
        layout.minusWidth,
        FOOTER_BUTTON_HEIGHT,
        minusBackground);
    tft.setTextColor(TFT_WHITE, minusBackground);
    tft.drawCentreString("[-]", minusCenter, FOOTER_BUTTON_Y, ITEM_FONT);

    tft.fillRect(
        layout.plusLeft,
        FOOTER_BUTTON_Y,
        layout.plusWidth,
        FOOTER_BUTTON_HEIGHT,
        plusBackground);
    tft.setTextColor(TFT_WHITE, plusBackground);
    tft.drawCentreString("[+]", plusCenter, FOOTER_BUTTON_Y, ITEM_FONT);
}

bool SetAudio::processTouch()
{
    const uint16_t x = touch.getX();
    const uint16_t y = touch.getY();

    const int rmsHelpX = getRmsReferenceHelpX();
    const int rmsHelpY = ITEM_Y[0];
    if (help.hitTest(x, y, rmsHelpX, rmsHelpY))
    {
        showHelp(
            rmsHelpX,
            rmsHelpY,
            selectedField == Field::RmsReference ? COL_CYAN_OFF : COL_BG,
            RMS_REFERENCE_HELP_TITLE,
            RMS_REFERENCE_HELP_TEXT,
            sizeof(RMS_REFERENCE_HELP_TEXT) /
                sizeof(RMS_REFERENCE_HELP_TEXT[0]));
        return false;
    }

    const int vuHelpX = getVuSettingsHelpX();
    const int vuHelpY = ITEM_Y[1];
    if (help.hitTest(x, y, vuHelpX, vuHelpY))
    {
        showHelp(
            vuHelpX,
            vuHelpY,
            selectedField == Field::Smoothing ? COL_CYAN_OFF : COL_BG,
            VU_SETTINGS_HELP_TITLE,
            VU_SETTINGS_HELP_TEXT,
            sizeof(VU_SETTINGS_HELP_TEXT) /
                sizeof(VU_SETTINGS_HELP_TEXT[0]));
        return false;
    }

    const int wfHelpX = getWfNoiseFloorHelpX();
    const int wfHelpY = ITEM_Y[5];
    if (help.hitTest(x, y, wfHelpX, wfHelpY))
    {
        showHelp(
            wfHelpX,
            wfHelpY,
            selectedField == Field::WfNoiseFloor ? COL_CYAN_OFF : COL_BG,
            WF_NOISE_FLOOR_HELP_TITLE,
            WF_NOISE_FLOOR_HELP_TEXT,
            sizeof(WF_NOISE_FLOOR_HELP_TEXT) /
                sizeof(WF_NOISE_FLOOR_HELP_TEXT[0]));
        return false;
    }

    const int adcHelpX = getAdcCalibrationHelpX();
    if (help.hitTest(x, y, adcHelpX, ADC_CALIBRATION_ITEM_Y))
    {
        showHelp(
            adcHelpX,
            ADC_CALIBRATION_ITEM_Y,
            adcCalibrationSelected ? COL_CYAN_OFF : COL_BG,
            ADC_ZERO_CAL_HELP_TITLE,
            ADC_ZERO_CAL_HELP_TEXT,
            sizeof(ADC_ZERO_CAL_HELP_TEXT) /
                sizeof(ADC_ZERO_CAL_HELP_TEXT[0]));
        return false;
    }

    const int calibrationHelpY =
        ITEM_Y[static_cast<uint8_t>(Field::Calibration3150Hz) - 1];
    const int calibrationHelpX = getCalibration3150HzHelpX();
    if (help.hitTest(x, y, calibrationHelpX, calibrationHelpY))
    {
        showHelp(
            calibrationHelpX,
            calibrationHelpY,
            selectedField == Field::Calibration3150Hz
                ? COL_CYAN_OFF
                : COL_BG,
            CALIBRATION_3150_HELP_TITLE,
            CALIBRATION_3150_HELP_TEXT,
            sizeof(CALIBRATION_3150_HELP_TEXT) /
                sizeof(CALIBRATION_3150_HELP_TEXT[0]));
        return false;
    }

    for (uint8_t i = 0; i < 6; ++i)
    {
        const int top = ITEM_Y[i] - TOUCH_PADDING_Y;
        if (x >= CONTENT_LEFT && x <= CONTENT_RIGHT &&
            y >= top && y < top + ITEM_ZONE_HEIGHT)
        {
            const Field field = static_cast<Field>(i + 1);
            if (selectedField != field)
                selectField(field);
            return false;
        }
    }

    const int runWidth = tft.textWidth("[RUN]", ITEM_FONT);
    const char *const calibration3150HzButtonText =
        (calibration3150HzRunning || calibration3150HzConfirmed)
            ? "[OK]"
            : "[RUN]";
    const int calibration3150HzRunWidth =
        tft.textWidth(calibration3150HzButtonText, ITEM_FONT);
    const int runLeft = VALUE_RIGHT - runWidth;
    const int calibrationTop = ADC_CALIBRATION_ITEM_Y - TOUCH_PADDING_Y;
    if (x >= CONTENT_LEFT && x <= CONTENT_RIGHT &&
        y >= calibrationTop && y < calibrationTop + ITEM_ZONE_HEIGHT)
    {
        reset3150HzCalibration();
        if (!adcCalibrationSelected)
        {
            const Field previous = selectedField;
            selectedField = Field::None;
            if (previous != Field::None)
                drawField(previous);
            adcCalibrationSelected = true;
            drawAdcCalibrationItem(true, false);
            display.setAppStatus("TOUCH TO BACK");
            drawAdjustmentButtons();
        }

        if (x < runLeft - RUN_BUTTON_PADDING_X ||
            x > VALUE_RIGHT + RUN_BUTTON_PADDING_X)
            return false;

        drawAdcCalibrationItem(true, true);
        audio.calibrate();
        display.setAppStatus("ADC CALIBRATED!");
        drawAdjustmentButtons();
        delay(1500);
        drawAdcCalibrationItem(true, false);
        display.setAppStatus("TOUCH TO BACK");
        drawAdjustmentButtons();
        return false;
    }

    const Field calibration3150HzField = Field::Calibration3150Hz;
    const int calibration3150HzTop =
        ITEM_Y[static_cast<uint8_t>(calibration3150HzField) - 1] -
        TOUCH_PADDING_Y;
    if (x >= CONTENT_LEFT && x <= CONTENT_RIGHT &&
        y >= calibration3150HzTop &&
        y < calibration3150HzTop + ITEM_ZONE_HEIGHT)
    {
        if (selectedField != calibration3150HzField)
            selectField(calibration3150HzField);

        const int calibration3150HzRunLeft =
            VALUE_RIGHT - calibration3150HzRunWidth;
        if (x < calibration3150HzRunLeft - RUN_BUTTON_PADDING_X ||
            x > VALUE_RIGHT + RUN_BUTTON_PADDING_X)
            return false;

        if (!calibration3150HzRunning &&
            !calibration3150HzConfirmed)
        {
            drawField(calibration3150HzField, true);
            calibration3150HzRunning = true;
            calibration3150HzConfirmed = false;
            calibration3150HzFrequency = 0;
            drawField(calibration3150HzField);
            lastCalibrationDisplayMs = millis();
            display.setAppStatus("TOUCH [OK] TO CONFIRM");
            drawAdjustmentButtons();
            return false;
        }

        if (calibration3150HzConfirmed)
        {
            reset3150HzCalibration();
            calibrationRunPressed = true;
            display.setAppStatus("TOUCH TO BACK");
            drawField(calibration3150HzField, true);
            return false;
        }

        calibrationRunPressed = true;
        calibration3150HzRunning = false;

        // settings.rawFrequency = calibration3150HzFrequency;
        //**********************************************
        constexpr float FREQUENCY_CAL_REFERENCE_HZ = 3150.0f;

        if (calibration3150HzFrequency >= 3000 &&
            calibration3150HzFrequency <= 3300)
        {
            const float calibrationFactor =
                FREQUENCY_CAL_REFERENCE_HZ /
                calibration3150HzFrequency;

            settings.rawFrequency = calibration3150HzFrequency;
            settings.frequencyCalibrationPpm =
                static_cast<uint32_t>(
                    calibrationFactor * 1000000.0f +
                    0.5f);

            save();

            SignalFrequency::setCalibrationPpm(
                settings.frequencyCalibrationPpm);

            // afișezi:
            // "3150 Hz RAW CONFIRMED"
            calibration3150HzConfirmed = true;
        }
        else
        {
            // "CALIBRATION FAILED"
            calibration3150HzConfirmed = false;
        }
        //**********************************************
        changed = true;
        // save();
        drawField(calibration3150HzField, true);
        return false;
    }

    const FooterButtonLayout layout = getFooterButtonLayout();
    if (y >= FOOTER_BUTTON_Y &&
        y < FOOTER_BUTTON_Y + FOOTER_BUTTON_HEIGHT)
    {
        if (x >= layout.minusLeft &&
            x < layout.minusLeft + layout.minusWidth)
        {
            adjustmentButtonPressed = true;
            drawAdjustmentButtons(true, false);
            if (selectedField != Field::None)
                adjustSelectedField(-1);
            return false;
        }

        if (x >= layout.plusLeft &&
            x < layout.plusLeft + layout.plusWidth)
        {
            adjustmentButtonPressed = true;
            drawAdjustmentButtons(false, true);
            if (selectedField != Field::None)
                adjustSelectedField(1);
            return false;
        }
    }

    if (y >= FOOTER_Y)
        return true;

    return true;
}

void SetAudio::showHelp(
    int buttonX,
    int buttonY,
    uint16_t idleBackground,
    const char *title,
    const char *const lines[],
    uint8_t lineCount)
{
    help.drawButton(buttonX, buttonY, true, idleBackground);

    while (touch.pressed())
        delay(5);

    help.showModal(title, lines, lineCount);
    redrawScreen();
}

void SetAudio::selectField(Field field)
{
    if (field != Field::Calibration3150Hz)
        reset3150HzCalibration();

    const Field previous = selectedField;
    const bool previousAdcCalibration = adcCalibrationSelected;
    selectedField = field;
    adcCalibrationSelected = false;

    if (previous != Field::None)
        drawField(previous);
    else if (previousAdcCalibration)
        drawAdcCalibrationItem(false, false);
    drawField(selectedField);
    display.setAppStatus("TOUCH TO BACK");
    drawAdjustmentButtons();
}

void SetAudio::reset3150HzCalibration()
{
    calibration3150HzRunning = false;
    calibration3150HzConfirmed = false;
    calibration3150HzFrequency = 0;
    lastCalibrationDisplayMs = 0;
}

void SetAudio::adjustSelectedField(int8_t direction)
{
    switch (selectedField)
    {
    case Field::RmsReference:
        settings.rmsReference = constrain(
            static_cast<int>(settings.rmsReference) +
                direction * RMS_REFERENCE_STEP,
            static_cast<int>(RMS_REFERENCE_MIN),
            static_cast<int>(RMS_REFERENCE_MAX));
        break;
    case Field::Smoothing:
        settings.smoothingPercent = constrain(
            static_cast<int>(settings.smoothingPercent) +
                direction * SMOOTHING_STEP,
            static_cast<int>(SMOOTHING_MIN),
            static_cast<int>(SMOOTHING_MAX));
        break;
    case Field::PeakHold:
        settings.peakHoldMs = constrain(
            static_cast<int>(settings.peakHoldMs) +
                direction * PEAK_HOLD_STEP,
            static_cast<int>(PEAK_HOLD_MIN),
            static_cast<int>(PEAK_HOLD_MAX));
        break;
    case Field::PeakDecay:
        settings.peakDecayHundredths = constrain(
            static_cast<int>(settings.peakDecayHundredths) +
                direction * PEAK_DECAY_STEP,
            static_cast<int>(PEAK_DECAY_MIN),
            static_cast<int>(PEAK_DECAY_MAX));
        break;
    case Field::OverLevel:
        settings.overLevelDb = constrain(
            static_cast<int>(settings.overLevelDb) + direction,
            static_cast<int>(OVER_LEVEL_MIN),
            static_cast<int>(OVER_LEVEL_MAX));
        break;
    case Field::WfNoiseFloor:
        settings.wfNoiseFloorThousandths = constrain(
            static_cast<int>(settings.wfNoiseFloorThousandths) +
                direction * WF_NOISE_FLOOR_STEP,
            static_cast<int>(WF_NOISE_FLOOR_MIN),
            static_cast<int>(WF_NOISE_FLOOR_MAX));
        break;
    case Field::Calibration3150Hz:
    case Field::None:
        return;
    }

    changed = true;
    drawField(selectedField);
}

void SetAudio::update3150HzCalibrationInput()
{
    for (uint16_t i = 0; i < FREQUENCY_SAMPLE_COUNT; ++i)
    {
        const uint32_t sampleStart = micros();
        frequencySamples[i] = audio.readLeftCentered();

        while (micros() - sampleStart < SignalFrequency::SAMPLE_PERIOD_US)
            ;
    }

    calibration3150HzFrequency =
        SignalFrequency::measureInterpolatedRaw(
            frequencySamples,
            FREQUENCY_SAMPLE_COUNT);
}

void SetAudio::save()
{
    if (!storage.saveAudioSettings(settings))
        return;

    changed = false;
}

float SetAudio::rmsReference() const
{
    return settings.rmsReference;
}

float SetAudio::smoothing() const
{
    return settings.smoothingPercent / 100.0f;
}

uint16_t SetAudio::peakHoldMs() const
{
    return settings.peakHoldMs;
}

float SetAudio::peakDecayDb() const
{
    return settings.peakDecayHundredths / 100.0f;
}

float SetAudio::overLevelDb() const
{
    return settings.overLevelDb;
}

float SetAudio::wfNoiseFloorPercent() const
{
    return settings.wfNoiseFloorThousandths / 1000.0f;
}

uint16_t SetAudio::rawFrequency() const
{
    return settings.rawFrequency;
}

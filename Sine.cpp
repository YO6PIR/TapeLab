#include "Sine.h"

#include "Config.h"
#include "Display.h"
#include "Help.h"
#include "Touch.h"
#include "AD9833.h"
#include "Pot.h"
#include "Buzzer.h"

#include <TFT_eSPI.h>

extern Display display;
extern TFT_eSPI tft;
extern Touch touch;
extern Buzzer buzzer;

Sine sine;

namespace
{
    constexpr uint16_t MIN_FREQUENCY_HZ = 20;
    constexpr uint16_t MAX_FREQUENCY_HZ = 20000;
    constexpr uint16_t FREQUENCY_STEP_HZ = 10;
    constexpr uint32_t FREQUENCY_REPEAT_MS = 100;
    constexpr uint32_t LEVEL_REPEAT_MS = 200;

    constexpr int16_t MUTE_LEVEL_TENTHS_DB = -205;
    constexpr int16_t MIN_LEVEL_TENTHS_DB = -200;
    constexpr int16_t MAX_LEVEL_TENTHS_DB = 30;
    constexpr int16_t LEVEL_STEP_TENTHS_DB = 5;

    constexpr float MAX_LEVEL_DB = 3.0f;

    constexpr uint8_t CONTROL_FONT = 2;
    constexpr int VALUE_FREQUENCY_Y = 42;
    constexpr int VALUE_LEVEL_Y = 75;
    constexpr int LEVEL_HELP_X = 270;
    constexpr int LEVEL_HELP_Y = VALUE_LEVEL_Y;
    constexpr int FREQUENCY_CONTROL_Y = 112;
    constexpr int LEVEL_CONTROL_Y = 146;
    constexpr int PRESET_Y = 181;
    constexpr int BUTTON_HEIGHT = 28;
    constexpr int BUTTON_GAP = 4;
    constexpr int SMALL_BUTTON_WIDTH = 36;
    constexpr int CONTROL_LABEL_WIDTH = 130;
    constexpr int RUN_BUTTON_X = 242;
    constexpr int RUN_BUTTON_Y = FREQUENCY_CONTROL_Y;
    constexpr int RUN_BUTTON_WIDTH = 62;
    constexpr int RUN_BUTTON_HEIGHT = 62;

    void drawStopOctagon(int x, int y, int width, int height)
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
        tft.drawLine(x + width - cut, y + 1, x + width - 1, y + cut, TFT_WHITE);
        tft.drawLine(x + width - 1, y + cut, x + width - 1, y + height - cut - 1, TFT_WHITE);
        tft.drawLine(x + width - 1, y + height - cut, x + width - cut, y + height - 1, TFT_WHITE);
        tft.drawLine(x + width - cut - 1, y + height - 1, x + cut, y + height - 1, TFT_WHITE);
        tft.drawLine(x + cut - 1, y + height - 1, x, y + height - cut, TFT_WHITE);
        tft.drawLine(x, y + height - cut - 1, x, y + cut, TFT_WHITE);
        tft.drawLine(x, y + cut - 1, x + cut, y, TFT_WHITE);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_RED);
        tft.drawString("STOP", x + width / 2, y + height / 2, 2);
        tft.setTextDatum(TL_DATUM);
        tft.setTextPadding(0);
    }

    void drawStartSquare(int x, int y, int width, int height)
    {
        tft.fillRect(x, y, width, height, COL_DARKGREEN);
        tft.drawRect(x, y, width, height, TFT_WHITE);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, COL_DARKGREEN);
        tft.drawString("START", x + width / 2, y + height / 2, 2);
        tft.setTextDatum(TL_DATUM);
        tft.setTextPadding(0);
    }
    constexpr int PRESET_BUTTON_WIDTH = 50;
    constexpr int CONTROL_ROW_WIDTH =
        SMALL_BUTTON_WIDTH * 2 + CONTROL_LABEL_WIDTH + BUTTON_GAP * 2;
    constexpr int CONTROL_ROW_X = CONTENT_LEFT + 5;
    constexpr int PRESET_ROW_WIDTH =
        PRESET_BUTTON_WIDTH * 5 + BUTTON_GAP * 4;
    constexpr int PRESET_ROW_X = (LCD_WIDTH - PRESET_ROW_WIDTH) / 2;
    constexpr uint16_t BUTTON_IDLE_COLOR = TFT_DARKGREY;
    constexpr uint16_t BUTTON_PRESSED_COLOR = TFT_GREEN;
    const uint16_t COL_CYAN_OFF = tft.color565(0, 20, 40);
    const uint16_t COL_RED_OFF = tft.color565(40, 0, 0);
    constexpr char GENERATOR_LEVEL_HELP_TITLE[] = "GENERATOR LEVEL";
    const char *const GENERATOR_LEVEL_HELP_TEXT[] =
        {
            "Output range:",
            "-20.0 dB to +3.0 dB",
            "",
            "0 dB = approximately 1.00 Vpp.",
            "",
            "Below -20.0 dB the output enters MUTE.",
            "",
            "Adjustment step: 0.5 dB."};

    bool contains(uint16_t value, int left, int width)
    {
        return value >= left && value < left + width;
    }

    bool applyOutputLevel(int16_t levelTenthsDb)
    {
        if (levelTenthsDb == MUTE_LEVEL_TENTHS_DB)
            return pot.setWiper(0);

        return pot.setLevelDb(
            static_cast<float>(levelTenthsDb) / 10.0f,
            MAX_LEVEL_DB);
    }
}

void Sine::run()
{
    ad9833.setFrequency(frequency);

    if (!applyOutputLevel(levelTenthsDb))
    {
        ad9833.disable();
        running = false;
    }

    drawScreen();

    // Do not handle the second tap that activated GENERATOR as a control.
    while (touch.isDown())
    {
        display.updateClock();
        delay(5);
    }

    const auto applyWithFeedback = [this](Button button)
    {
        const uint16_t previousFrequency = frequency;
        const int16_t previousLevel = levelTenthsDb;
        apply(button);

        if (!isAdjustmentButton(button))
        {
            buzzer.beepNow(20);
            return;
        }

        if (frequency != previousFrequency || levelTenthsDb != previousLevel)
            buzzer.play(BeepPattern::Touch);
    };

    while (true)
    {
        display.updateClock();
        buzzer.update();

        if (!touch.pressed(false))
        {
            delay(10);
            continue;
        }

        if (help.hitTest(
                touch.getX(),
                touch.getY(),
                LEVEL_HELP_X,
                LEVEL_HELP_Y))
        {
            buzzer.beepNow(20);
            help.drawButton(LEVEL_HELP_X, LEVEL_HELP_Y, true);
            while (touch.isDown())
            {
                display.updateClock();
                delay(5);
            }
            help.showModal(
                GENERATOR_LEVEL_HELP_TITLE,
                GENERATOR_LEVEL_HELP_TEXT,
                sizeof(GENERATOR_LEVEL_HELP_TEXT) /
                    sizeof(GENERATOR_LEVEL_HELP_TEXT[0]));
            drawScreen();
            continue;
        }

        const Button button = buttonAt(touch.getX(), touch.getY());
        if (button == Button::None)
        {
            buzzer.beepNow(20);
            while (touch.isDown())
            {
                display.updateClock();
                delay(5);
            }
            break;
        }

        const Button previousPreset = selectedPreset;
        if (button == Button::FrequencyDown ||
            button == Button::FrequencyUp)
        {
            selectedPreset = Button::None;
        }
        else if (button == Button::Preset440 ||
                 button == Button::Preset1000 ||
                 button == Button::Preset3150 ||
                 button == Button::Preset10000 ||
                 button == Button::Preset15000)
        {
            selectedPreset = button;
        }

        if (previousPreset != selectedPreset &&
            previousPreset != Button::None)
        {
            drawPresetButton(previousPreset, false);
        }

        drawControlButton(button, true);
        applyWithFeedback(button);

        uint32_t lastRepeatMs = millis();
        const uint32_t repeatIntervalMs =
            button == Button::FrequencyDown || button == Button::FrequencyUp
                ? FREQUENCY_REPEAT_MS
                : LEVEL_REPEAT_MS;

        while (touch.isDown())
        {
            display.updateClock();
            buzzer.update();

            if (isAdjustmentButton(button) &&
                millis() - lastRepeatMs >= repeatIntervalMs)
            {
                applyWithFeedback(button);
                lastRepeatMs = millis();
            }
            delay(5);
        }

        drawControlButton(button, false);
    }
}

uint16_t Sine::frequencyHz() const
{
    return frequency;
}

bool Sine::isRunning() const
{
    return running;
}

void Sine::drawServiceButton(
    int x,
    int y,
    const char *line1,
    const char *line2,
    bool active)
{
    constexpr int BUTTON_WIDTH = 78;
    constexpr int BUTTON_HEIGHT = 40;
    const uint16_t background = active ? TFT_RED : COL_DARKGREEN;

    if (active)
        drawStopOctagon(x, y, BUTTON_WIDTH, BUTTON_HEIGHT);
    else
        drawStartSquare(x, y, BUTTON_WIDTH, BUTTON_HEIGHT);

    tft.fillRect(x + 7, y + 5, BUTTON_WIDTH - 14, BUTTON_HEIGHT - 10, background);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, background);
    tft.drawString(line1, x + BUTTON_WIDTH / 2, y + 13, 2);
    tft.drawString(line2, x + BUTTON_WIDTH / 2, y + 28, 2);
    tft.setTextDatum(TL_DATUM);
    tft.setTextPadding(0);
}

void Sine::stopForRecord()
{
    running = false;
}

float Sine::levelDb() const
{
    return static_cast<float>(levelTenthsDb) / 10.0f;
}

void Sine::drawScreen()
{
    display.openApp("AUDIO GENERATOR", "TOUCH TO BACK");
    tft.setTextDatum(TL_DATUM);
    frequencyDisplayInitialized = false;
    levelDisplayInitialized = false;
    drawFrequency();
    drawLevel();
    help.drawButton(LEVEL_HELP_X, LEVEL_HELP_Y);
    drawControls();
    drawRunButton();
    drawRunningStatus();
}

void Sine::drawFrequency()
{
    constexpr uint8_t FREQUENCY_FONT = 4;
    const char *const label = "Frequency = ";
    const char *const unit = " Hz";
    char value[8];
    snprintf(value, sizeof(value), "%u", frequency);

    const int labelWidth = tft.textWidth(label, FREQUENCY_FONT);
    const int valueWidth = tft.textWidth("20000", FREQUENCY_FONT);
    const int unitWidth = tft.textWidth(unit, FREQUENCY_FONT);
    const int blockWidth = labelWidth + valueWidth + unitWidth;
    const int x = (LCD_WIDTH - blockWidth) / 2;
    const int valueRight = x + labelWidth + valueWidth;

    // Keep the label and unit static; redraw only the padded numeric field.
    const bool drawStaticText = !frequencyDisplayInitialized;
    if (drawStaticText)
    {
        tft.setTextColor(COL_TEXT, COL_BG);
        tft.drawString(label, x, VALUE_FREQUENCY_Y, FREQUENCY_FONT);
        tft.drawString(unit, valueRight, VALUE_FREQUENCY_Y, FREQUENCY_FONT);
        frequencyDisplayInitialized = true;
    }
    if (!drawStaticText && displayedFrequency == frequency)
        return;

    tft.setTextColor(TFT_GREEN, COL_BG);
    tft.setTextPadding(valueWidth);
    tft.drawRightString(value, valueRight, VALUE_FREQUENCY_Y, FREQUENCY_FONT);
    tft.setTextPadding(0);
    displayedFrequency = frequency;
}

void Sine::drawLevel()
{
    char value[16];
    const bool muted =
        levelTenthsDb == MUTE_LEVEL_TENTHS_DB;

    if (muted)
    {
        snprintf(value, sizeof(value), "MUTE");
    }
    else
    {
        const int whole = abs(levelTenthsDb) / 10;
        const int decimal = abs(levelTenthsDb) % 10;
        if (levelTenthsDb < 0)
            snprintf(value, sizeof(value), "-%d.%d", whole, decimal);
        else if (levelTenthsDb > 0)
            snprintf(value, sizeof(value), "+%d.%d", whole, decimal);
        else
            snprintf(value, sizeof(value), "0.0");
    }

    const int labelWidth = tft.textWidth("Level = ", 2);
    const int valueWidth = max(
        max(tft.textWidth("-20.0", 2), tft.textWidth("+3.0", 2)),
        tft.textWidth("MUTE", 2));
    const int unitWidth = tft.textWidth(" dB", 2);
    const int x = (LCD_WIDTH - labelWidth - valueWidth - unitWidth) / 2;
    const int valueRight = x + labelWidth + valueWidth;
    const int unitX = valueRight;

    const bool drawStaticText = !levelDisplayInitialized;
    if (drawStaticText)
    {
        tft.setTextColor(COL_TEXT, COL_BG);
        tft.drawString("Level = ", x, VALUE_LEVEL_Y, 2);
        if (!muted)
            tft.drawString(" dB", unitX, VALUE_LEVEL_Y, 2);
        levelDisplayInitialized = true;
    }
    else if (displayedLevelMuted != muted)
    {
        if (muted)
            tft.fillRect(unitX, VALUE_LEVEL_Y, unitWidth, 18, COL_BG);
        else
        {
            tft.setTextColor(COL_TEXT, COL_BG);
            tft.drawString(" dB", unitX, VALUE_LEVEL_Y, 2);
        }
    }
    if (!drawStaticText && displayedLevelTenthsDb == levelTenthsDb)
        return;

    tft.setTextColor(TFT_YELLOW, COL_BG);
    tft.setTextPadding(valueWidth);
    tft.drawRightString(value, valueRight, VALUE_LEVEL_Y, 2);
    tft.setTextPadding(0);
    displayedLevelTenthsDb = levelTenthsDb;
    displayedLevelMuted = muted;
}

void Sine::drawPresetButton(Button button, bool pressed)
{
    const char *label = nullptr;
    switch (button)
    {
    case Button::Preset440:
        label = "440";
        break;
    case Button::Preset1000:
        label = "1000";
        break;
    case Button::Preset3150:
        label = "3150";
        break;
    case Button::Preset10000:
        label = "10K";
        break;
    case Button::Preset15000:
        label = "15K";
        break;
    default:
        return;
    }

    const Button buttons[] = {
        Button::Preset440, Button::Preset1000, Button::Preset3150,
        Button::Preset10000, Button::Preset15000};
    uint8_t index = 0;
    while (index < 5 && buttons[index] != button)
        ++index;

    if (index >= 5)
        return;

    const int x = PRESET_ROW_X + index * (PRESET_BUTTON_WIDTH + BUTTON_GAP);
    drawButton(label, x, PRESET_Y, PRESET_BUTTON_WIDTH,
               pressed || selectedPreset == button, CONTROL_FONT);
}

void Sine::drawControlButton(Button button, bool pressed)
{
    const int plusX = CONTROL_ROW_X + SMALL_BUTTON_WIDTH + BUTTON_GAP +
                      CONTROL_LABEL_WIDTH + BUTTON_GAP;

    switch (button)
    {
    case Button::FrequencyDown:
        drawButton("-", CONTROL_ROW_X, FREQUENCY_CONTROL_Y,
                   SMALL_BUTTON_WIDTH, pressed, 4);
        break;
    case Button::FrequencyUp:
        drawButton("+", plusX, FREQUENCY_CONTROL_Y,
                   SMALL_BUTTON_WIDTH, pressed, 4);
        break;
    case Button::LevelDown:
        drawButton("-", CONTROL_ROW_X, LEVEL_CONTROL_Y,
                   SMALL_BUTTON_WIDTH, pressed, 4);
        break;
    case Button::LevelUp:
        drawButton("+", plusX, LEVEL_CONTROL_Y,
                   SMALL_BUTTON_WIDTH, pressed, 4);
        break;
    case Button::Preset440:
    case Button::Preset1000:
    case Button::Preset3150:
    case Button::Preset10000:
    case Button::Preset15000:
        drawPresetButton(button, pressed);
        break;
    case Button::StartStop:
        drawRunButton(pressed);
        break;
    case Button::None:
        break;
    }
}

void Sine::drawControls(Button pressed)
{
    const int frequencyLabelX = CONTROL_ROW_X + SMALL_BUTTON_WIDTH + BUTTON_GAP;
    const int levelLabelX = frequencyLabelX;
    const int plusX = frequencyLabelX + CONTROL_LABEL_WIDTH + BUTTON_GAP;

    drawButton("-", CONTROL_ROW_X, FREQUENCY_CONTROL_Y,
               SMALL_BUTTON_WIDTH, pressed == Button::FrequencyDown, 4);
    tft.fillRect(
        frequencyLabelX,
        FREQUENCY_CONTROL_Y,
        CONTROL_LABEL_WIDTH,
        BUTTON_HEIGHT,
        COL_BG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("FREQUENCY", frequencyLabelX + CONTROL_LABEL_WIDTH / 2,
                   FREQUENCY_CONTROL_Y + BUTTON_HEIGHT / 2, CONTROL_FONT);
    tft.setTextDatum(TL_DATUM);
    drawButton("+", plusX, FREQUENCY_CONTROL_Y,
               SMALL_BUTTON_WIDTH, pressed == Button::FrequencyUp, 4);

    drawButton("-", CONTROL_ROW_X, LEVEL_CONTROL_Y,
               SMALL_BUTTON_WIDTH, pressed == Button::LevelDown, 4);
    tft.fillRect(
        levelLabelX,
        LEVEL_CONTROL_Y,
        CONTROL_LABEL_WIDTH,
        BUTTON_HEIGHT,
        COL_BG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("LEVEL", levelLabelX + CONTROL_LABEL_WIDTH / 2,
                   LEVEL_CONTROL_Y + BUTTON_HEIGHT / 2, CONTROL_FONT);
    tft.setTextDatum(TL_DATUM);
    drawButton("+", plusX, LEVEL_CONTROL_Y,
               SMALL_BUTTON_WIDTH, pressed == Button::LevelUp, 4);

    const char *labels[] = {"440", "1000", "3150", "10K", "15K"};
    const Button buttons[] = {
        Button::Preset440, Button::Preset1000, Button::Preset3150,
        Button::Preset10000, Button::Preset15000};

    for (uint8_t index = 0; index < 5; ++index)
    {
        const int x = PRESET_ROW_X + index * (PRESET_BUTTON_WIDTH + BUTTON_GAP);
        drawButton(labels[index], x, PRESET_Y, PRESET_BUTTON_WIDTH,
                   pressed == buttons[index] ||
                       selectedPreset == buttons[index],
                   CONTROL_FONT);
    }
}

void Sine::drawRunButton(bool pressed)
{
    const uint16_t background =
        running ? COL_RED_OFF : COL_CYAN_OFF;
    const uint16_t stateColor =
        running ? TFT_ORANGE : TFT_GREEN;
    const uint16_t foreground = pressed ? TFT_WHITE : stateColor;

    if (running)
    {
        drawStopOctagon(
            RUN_BUTTON_X,
            RUN_BUTTON_Y,
            RUN_BUTTON_WIDTH,
            RUN_BUTTON_HEIGHT);
    }
    else
    {
        drawStartSquare(
            RUN_BUTTON_X,
            RUN_BUTTON_Y,
            RUN_BUTTON_WIDTH,
            RUN_BUTTON_HEIGHT);
    }
}

void Sine::drawRunningStatus()
{
    const int titleEnd =
        12 + tft.textWidth("AUDIO GENERATOR", 2) + 5;
    const int versionLeft =
        308 - tft.textWidth(FIRMWARE_VERSION, 2);
    const int statusWidth = tft.textWidth("running...", 2);
    const int statusX =
        titleEnd + (versionLeft - titleEnd - statusWidth) / 2;

    if (statusX < titleEnd || statusX + statusWidth > versionLeft)
        return;

    tft.fillRect(titleEnd, 8, versionLeft - titleEnd - 3, 16, COL_BG);
    if (running)
    {
        tft.setTextColor(TFT_GREEN, COL_BG);
        tft.drawString("running...", statusX, 8, 2);
    }
}

void Sine::drawButton(
    const char *text,
    int x,
    int y,
    int width,
    bool pressed,
    uint8_t font)
{
    const uint16_t background =
        pressed ? BUTTON_PRESSED_COLOR : COL_CYAN_OFF;
    const uint16_t foreground = pressed ? TFT_BLACK : COL_TEXT;

    tft.fillRect(x, y, width, BUTTON_HEIGHT, background);
    tft.drawRect(x, y, width, BUTTON_HEIGHT, BUTTON_IDLE_COLOR);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(foreground, background);
    tft.drawString(text, x + width / 2, y + BUTTON_HEIGHT / 2, font);
    tft.setTextDatum(TL_DATUM);
}

Sine::Button Sine::buttonAt(uint16_t x, uint16_t y) const
{
    const int plusX =
        CONTROL_ROW_X +
        SMALL_BUTTON_WIDTH +
        BUTTON_GAP +
        CONTROL_LABEL_WIDTH +
        BUTTON_GAP;

    if (y >= FREQUENCY_CONTROL_Y && y < FREQUENCY_CONTROL_Y + BUTTON_HEIGHT)
    {
        if (contains(x, CONTROL_ROW_X, SMALL_BUTTON_WIDTH))
            return Button::FrequencyDown;
        if (contains(x, plusX, SMALL_BUTTON_WIDTH))
            return Button::FrequencyUp;
    }

    if (y >= LEVEL_CONTROL_Y && y < LEVEL_CONTROL_Y + BUTTON_HEIGHT)
    {
        if (contains(x, CONTROL_ROW_X, SMALL_BUTTON_WIDTH))
            return Button::LevelDown;
        if (contains(x, plusX, SMALL_BUTTON_WIDTH))
            return Button::LevelUp;
    }

    if (y >= PRESET_Y && y < PRESET_Y + BUTTON_HEIGHT)
    {
        for (uint8_t index = 0; index < 5; ++index)
        {
            const int left = PRESET_ROW_X + index * (PRESET_BUTTON_WIDTH + BUTTON_GAP);
            if (contains(x, left, PRESET_BUTTON_WIDTH))
                return static_cast<Button>(static_cast<uint8_t>(Button::Preset440) + index);
        }
    }

    if (y >= RUN_BUTTON_Y && y < RUN_BUTTON_Y + RUN_BUTTON_HEIGHT &&
        contains(x, RUN_BUTTON_X, RUN_BUTTON_WIDTH))
    {
        return Button::StartStop;
    }

    return Button::None;
}

bool Sine::isAdjustmentButton(Button button) const
{
    return button == Button::FrequencyDown ||
           button == Button::FrequencyUp ||
           button == Button::LevelDown ||
           button == Button::LevelUp;
}

void Sine::apply(Button button)
{
    switch (button)
    {
    case Button::FrequencyDown:
        selectedPreset = Button::None;

        frequency =
            frequency > MIN_FREQUENCY_HZ + FREQUENCY_STEP_HZ
                ? frequency - FREQUENCY_STEP_HZ
                : MIN_FREQUENCY_HZ;

        ad9833.setFrequency(frequency);
        drawFrequency();
        break;

    case Button::FrequencyUp:
        selectedPreset = Button::None;

        frequency =
            frequency < MAX_FREQUENCY_HZ - FREQUENCY_STEP_HZ
                ? frequency + FREQUENCY_STEP_HZ
                : MAX_FREQUENCY_HZ;

        ad9833.setFrequency(frequency);
        drawFrequency();
        break;

    case Button::LevelDown:
    {
        bool levelChanged = false;
        if (levelTenthsDb == MIN_LEVEL_TENTHS_DB)
        {
            // Sub -20.0 dB trecem direct în MUTE.
            levelTenthsDb = MUTE_LEVEL_TENTHS_DB;
            levelChanged = true;
        }
        else if (levelTenthsDb > MIN_LEVEL_TENTHS_DB)
        {
            levelTenthsDb -= LEVEL_STEP_TENTHS_DB;
            levelChanged = true;
        }

        if (levelChanged && !applyOutputLevel(levelTenthsDb))
        {
            ad9833.disable();
            running = false;
            drawRunningStatus();
            drawRunButton();
        }

        drawLevel();
        break;
    }

    case Button::LevelUp:
    {
        bool levelChanged = false;
        if (levelTenthsDb == MUTE_LEVEL_TENTHS_DB)
        {
            // Prima apăsare după MUTE revine la -20.0 dB.
            levelTenthsDb = MIN_LEVEL_TENTHS_DB;
            levelChanged = true;
        }
        else if (levelTenthsDb < MAX_LEVEL_TENTHS_DB)
        {
            levelTenthsDb += LEVEL_STEP_TENTHS_DB;

            if (levelTenthsDb > MAX_LEVEL_TENTHS_DB)
                levelTenthsDb = MAX_LEVEL_TENTHS_DB;

            levelChanged = true;
        }

        if (levelChanged && !applyOutputLevel(levelTenthsDb))
        {
            ad9833.disable();
            running = false;
            drawRunningStatus();
            drawRunButton();
        }

        drawLevel();
        break;
    }

    case Button::Preset440:
        selectedPreset = Button::Preset440;
        frequency = 440;

        ad9833.setFrequency(frequency);
        drawFrequency();
        break;

    case Button::Preset1000:
        selectedPreset = Button::Preset1000;
        frequency = 1000;

        ad9833.setFrequency(frequency);
        drawFrequency();
        break;

    case Button::Preset3150:
        selectedPreset = Button::Preset3150;
        frequency = 3150;

        ad9833.setFrequency(frequency);
        drawFrequency();
        break;

    case Button::Preset10000:
        selectedPreset = Button::Preset10000;
        frequency = 10000;

        ad9833.setFrequency(frequency);
        drawFrequency();
        break;

    case Button::Preset15000:
        selectedPreset = Button::Preset15000;
        frequency = 15000;

        ad9833.setFrequency(frequency);
        drawFrequency();
        break;

    case Button::StartStop:
        if (running)
        {
            ad9833.disable();
            running = false;
        }
        else if (applyOutputLevel(levelTenthsDb))
        {
            ad9833.enable();
            running = true;
        }
        else
        {
            ad9833.disable();
            running = false;
        }

        drawRunningStatus();
        drawRunButton();
        break;

    case Button::None:
        break;
    }
}

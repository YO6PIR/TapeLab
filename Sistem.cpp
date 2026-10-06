#include "Sistem.h"

#include "Config.h"
#include "Buzzer.h"
#include "Display.h"
#include "Icons.h"
#include "Rtc.h"
#include "SetAudio.h"
#include "Storage.h"
#include "Record.h"
#include "Touch.h"
#include "TouchCal.h"

#include <TFT_eSPI.h>

extern Display display;
extern Storage storage;
extern SetAudio setAudio;
extern TFT_eSPI tft;
extern Touch touch;
extern TouchCal touchCal;
extern Record record;
extern Buzzer buzzer;

Sistem sistem;

namespace
{
    constexpr uint8_t ITEM_FONT = 4;
    constexpr int SISTEM_ITEMS_X = CONTENT_LEFT + 5;
    constexpr int CLOCK_ROW_Y = 30; // poziția verticală a rândului ceasului;
    constexpr int CLOCK_LABEL_GAP = 6;
    constexpr int CLOCK_GROUP_GAP = 8;
    constexpr int CLOCK_BUTTON_GAP = 4;
    constexpr int CLOCK_ADJUST_BUTTON_WIDTH = 36;
    constexpr int CLOCK_ADJUST_BUTTON_HEIGHT = 28;
    constexpr int CLOCK_TOUCH_OFFSET_Y = 0;
    constexpr int CLOCK_TOUCH_PADDING_Y = 3; // extinde zona Touch cu 7 px deasupra și dedesubt;
    constexpr int CLOCK_TOUCH_PADDING_X = 3; // extinde lateral zona HH și SS cu 3 px.
    // Includes both brackets and adds tolerance at the outer button edges.
    constexpr int CLOCK_BUTTON_TOUCH_PADDING_X = 3;
    constexpr int TOUCH_CAL_ITEM_Y = 62;
    constexpr int DISPLAY_SETTINGS_ITEM_Y = 90;
    constexpr int AUDIO_SETTINGS_ITEM_Y = 118;
    constexpr int FACTORY_RESET_ITEM_Y = 146;
    constexpr int SYSTEM_INFO_ITEM_Y = 174;
    constexpr int SYSTEM_ICON_SIZE = 55;
    constexpr int SYSTEM_ICON_LABEL_OFFSET_Y = 3;
    constexpr int SYSTEM_ICON_FIRST_ROW_Y = 64;
    constexpr int SYSTEM_ICON_SECOND_ROW_Y = 144;
    constexpr int SYSTEM_ICON_X[] = {45, 133, 221, 45, 133, 221};
    constexpr int SYSTEM_ICON_Y[] = {
        SYSTEM_ICON_FIRST_ROW_Y,
        SYSTEM_ICON_FIRST_ROW_Y,
        SYSTEM_ICON_FIRST_ROW_Y,
        SYSTEM_ICON_SECOND_ROW_Y,
        SYSTEM_ICON_SECOND_ROW_Y,
        SYSTEM_ICON_SECOND_ROW_Y};
    constexpr int DISPLAY_BACKGROUND_DARKNAVY_Y = 70;
    constexpr int DISPLAY_BACKGROUND_DEFAULT_Y = 100;
    constexpr int DISPLAY_BEEP_Y = 140;
    constexpr int DISPLAY_ICONS_Y = 170;
    constexpr int MENU_TOUCH_OFFSET_Y = 3;
    constexpr uint32_t RTC_READ_INTERVAL_MS = 250;
    const uint16_t COL_CYAN_OFF = tft.color565(0, 20, 40);

    uint8_t compileMonth(const char *date)
    {
        static const char *const MONTHS[] = {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

        for (uint8_t month = 0; month < 12; ++month)
        {
            if (date[0] == MONTHS[month][0] &&
                date[1] == MONTHS[month][1] &&
                date[2] == MONTHS[month][2])
            {
                return month + 1;
            }
        }

        return 0;
    }

    void formatCompileDate(char (&buffer)[11])
    {
        constexpr char DATE[] = __DATE__;
        const uint8_t month = compileMonth(DATE);

        buffer[0] = DATE[4] == ' ' ? '0' : DATE[4];
        buffer[1] = DATE[5];
        buffer[2] = '/';
        buffer[3] = '0' + month / 10;
        buffer[4] = '0' + month % 10;
        buffer[5] = '/';
        buffer[6] = DATE[7];
        buffer[7] = DATE[8];
        buffer[8] = DATE[9];
        buffer[9] = DATE[10];
        buffer[10] = '\0';
    }

    struct ClockLayout
    {
        int labelX;
        int hourX;
        int firstColonX;
        int minuteX;
        int secondColonX;
        int secondX;
        int minusX;
        int plusX;
        int fieldWidth;
        int colonWidth;
        int minusWidth;
        int plusWidth;
        int fieldHeight;
    };

    struct TouchRect
    {
        int left;
        int top;
        int right;
        int bottom;

        bool contains(uint16_t x, uint16_t y) const
        {
            return x >= left && x < right &&
                   y >= top && y < bottom;
        }
    };

    ClockLayout getClockLayout()
    {
        constexpr const char *LABEL = "1. Clock";
        ClockLayout layout;
        const int labelWidth = tft.textWidth(LABEL, ITEM_FONT);
        layout.fieldWidth = tft.textWidth("00", ITEM_FONT);
        layout.colonWidth = tft.textWidth(":", ITEM_FONT);
        layout.minusWidth = CLOCK_ADJUST_BUTTON_WIDTH;
        layout.plusWidth = CLOCK_ADJUST_BUTTON_WIDTH;
        layout.fieldHeight = tft.fontHeight(ITEM_FONT);

        layout.labelX = SISTEM_ITEMS_X;
        layout.hourX =
            layout.labelX + labelWidth + CLOCK_LABEL_GAP;
        layout.firstColonX = layout.hourX + layout.fieldWidth;
        layout.minuteX =
            layout.firstColonX + layout.colonWidth;
        layout.secondColonX =
            layout.minuteX + layout.fieldWidth;
        layout.secondX =
            layout.secondColonX + layout.colonWidth;
        layout.minusX =
            layout.secondX + layout.fieldWidth + CLOCK_GROUP_GAP;
        layout.plusX =
            layout.minusX + layout.minusWidth + CLOCK_BUTTON_GAP;

        return layout;
    }

    TouchRect getMinusButtonHitbox(const ClockLayout &layout)
    {
        TouchRect hitbox;
        hitbox.left = layout.minusX;
        hitbox.top = CLOCK_ROW_Y;
        hitbox.right = layout.minusX + layout.minusWidth;
        hitbox.bottom = CLOCK_ROW_Y + CLOCK_ADJUST_BUTTON_HEIGHT;
        return hitbox;
    }

    TouchRect getPlusButtonHitbox(const ClockLayout &layout)
    {
        TouchRect hitbox;
        hitbox.left = layout.plusX;
        hitbox.top = CLOCK_ROW_Y;
        hitbox.right = layout.plusX + layout.plusWidth;
        hitbox.bottom = CLOCK_ROW_Y + CLOCK_ADJUST_BUTTON_HEIGHT;
        return hitbox;
    }

    TouchRect getMenuItemHitbox(int y)
    {
        TouchRect hitbox;
        hitbox.left = CONTENT_LEFT;
        hitbox.top = y - MENU_TOUCH_OFFSET_Y;
        hitbox.right = CONTENT_RIGHT + 1;
        hitbox.bottom =
            y + tft.fontHeight(ITEM_FONT) + 3 - MENU_TOUCH_OFFSET_Y;
        return hitbox;
    }

    void drawConfigMenuItem(const char *text, int y, bool selected)
    {
        const uint16_t background = selected ? COL_CYAN_OFF : COL_BG;
        tft.fillRect(
            CONTENT_LEFT + 2,
            y - MENU_TOUCH_OFFSET_Y,
            CONTENT_WIDTH - 4,
            tft.fontHeight(ITEM_FONT) + 3,
            background);
        tft.setTextColor(selected ? TFT_GREEN : TFT_WHITE, background);
        tft.drawString(text, SISTEM_ITEMS_X, y, ITEM_FONT);
    }

    void drawDisplayBackgroundOption(
        const char *text,
        int y,
        bool selected)
    {
        const uint16_t background = selected ? COL_CYAN_OFF : COL_BG;
        tft.fillRect(
            CONTENT_LEFT + 2,
            y - MENU_TOUCH_OFFSET_Y,
            CONTENT_WIDTH - 4,
            tft.fontHeight(ITEM_FONT) + 3,
            background);

        if (selected)
        {
            // Bifă de confirmare: brațul din stânga este intenționat
            // mai scurt decât brațul ascendent din dreapta.
            const int checkX = SISTEM_ITEMS_X;
            tft.drawLine(checkX, y + 11, checkX + 7, y + 18, TFT_GREEN);
            tft.drawLine(checkX + 7, y + 18, checkX + 20, y + 2, TFT_GREEN);
        }

        tft.setTextColor(selected ? TFT_GREEN : TFT_WHITE, background);
        tft.drawString(text, SISTEM_ITEMS_X + 30, y, ITEM_FONT);
    }

    void drawDisplayBeepOption(bool enabled)
    {
        tft.fillRect(CONTENT_LEFT + 2, DISPLAY_BEEP_Y - MENU_TOUCH_OFFSET_Y,
                     CONTENT_WIDTH - 4, tft.fontHeight(ITEM_FONT) + 3, COL_BG);

        if (enabled)
        {
            const int checkX = SISTEM_ITEMS_X;
            tft.drawLine(checkX, DISPLAY_BEEP_Y + 11, checkX + 7, DISPLAY_BEEP_Y + 18, TFT_GREEN);
            tft.drawLine(checkX + 7, DISPLAY_BEEP_Y + 18, checkX + 20, DISPLAY_BEEP_Y + 2, TFT_GREEN);
        }

        tft.setTextColor(enabled ? TFT_GREEN : TFT_WHITE, COL_BG);
        tft.drawString("BEEP SOUND",
                       SISTEM_ITEMS_X + 30, DISPLAY_BEEP_Y, ITEM_FONT);
    }

    void drawDisplayIconsOption(bool selected)
    {
        tft.fillRect(CONTENT_LEFT + 2, DISPLAY_ICONS_Y - MENU_TOUCH_OFFSET_Y,
                     CONTENT_WIDTH - 4, tft.fontHeight(ITEM_FONT) + 3, COL_BG);

        tft.setTextColor(selected ? TFT_GREEN : TFT_WHITE, COL_BG);
        tft.drawString("ICONS",
                       SISTEM_ITEMS_X + 30, DISPLAY_ICONS_Y, ITEM_FONT);
    }

    void showActionStatus(const char *message)
    {
        display.setAppStatus(message, 1, 220);
    }
}

void Sistem::showDisplaySettingsScreen()
{
    display.openApp("DISPLAY SETTINGS", "TOUCH TO BACK");
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, COL_BG);
    tft.drawString("Background color", SISTEM_ITEMS_X, 30, ITEM_FONT);

    const bool darkNavySelected = currentBackgroundColor == TFT_DARKNAVY;
    drawDisplayBackgroundOption(
        "DARK NAVY",
        DISPLAY_BACKGROUND_DARKNAVY_Y,
        darkNavySelected);

    const bool defaultSelected = currentBackgroundColor == TFT_BLACK;
    drawDisplayBackgroundOption(
        "DEFAULT",
        DISPLAY_BACKGROUND_DEFAULT_Y,
        defaultSelected);
    drawDisplayBeepOption(buzzer.isEnabled());

    displaySettingsScreenVisible = true;
    iconsSettingsScreenVisible = false;
    selectedConfigItem = ConfigMenuItem::None;
}

void Sistem::showIconsSettingsScreen()
{
    icons.setBackgroundColor(currentBackgroundColor);
    icons.showSettingsScreen();

    iconsSettingsScreenVisible = true;
    displaySettingsScreenVisible = false;
}

void Sistem::showSystemInformationScreen()
{
    display.openApp("About Tape-LAB", "TOUCH TO BACK");
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, COL_BG);

    constexpr int ABOUT_LABEL_X = CONTENT_LEFT + 8;
    constexpr int ABOUT_VALUE_RIGHT = CONTENT_RIGHT - 8;
    constexpr int ABOUT_LABEL_VALUE_GAP = 4;
    const int rowY[] = {30, 50, 70, 90, 110, 130, 150, 170, 190};
    const char *labels[] = {
        "Tape-LAB Firmware",
        "Date",
        "Processor",
        "Memory",
        "Touch",
        "Display",
        "EEprom",
        "RTC",
        "DDS",
    };
    char buildDate[11];
    formatCompileDate(buildDate);
    const char *values[] = {
        FIRMWARE_VERSION,
        buildDate,
        "STM32F401",
        "256K",
        "XPT2046",
        "ILI9341",
        "24C32",
        "DS1307",
        "AD9833",
    };

    for (uint8_t i = 0; i < 9; ++i)
    {
        tft.setTextColor(TFT_WHITE, COL_BG);
        const int labelRight =
            ABOUT_LABEL_X + tft.textWidth(labels[i], 2);
        const int valueLeft =
            ABOUT_VALUE_RIGHT - tft.textWidth(values[i], 2);
        const int dotWidth = tft.textWidth(".", 2);

        tft.drawString(labels[i], ABOUT_LABEL_X, rowY[i], 2);
        for (int dotX = labelRight + ABOUT_LABEL_VALUE_GAP;
             dotX + dotWidth + ABOUT_LABEL_VALUE_GAP <= valueLeft;
             dotX += dotWidth)
        {
            tft.drawString(".", dotX, rowY[i], 2);
        }

        tft.setTextColor(TFT_YELLOW, COL_BG);
        tft.drawRightString(values[i], ABOUT_VALUE_RIGHT, rowY[i], 2);
    }

    systemInfoScreenVisible = true;
    selectedConfigItem = ConfigMenuItem::None;
}

void Sistem::run()
{
    drawScreen();

    // Do not interpret the menu-opening press as an immediate request to exit.
    while (touch.pressed())
    {
        display.updateClock();
        delay(5);
    }

    while (true)
    {
        buzzer.update();
        if (displaySettingsScreenVisible)
        {
            display.updateClock();

            if (touch.pressed(false))
            {
                const uint16_t touchX = touch.getX();
                const uint16_t touchY = touch.getY();
                const bool togglesBeep =
                    getMenuItemHitbox(DISPLAY_BEEP_Y).contains(touchX, touchY);

                // This screen handles feedback explicitly so enabling BEEP
                // does not add a generic touch tick before its confirmation.
                if (!togglesBeep)
                    buzzer.play(BeepPattern::Touch);

                if (getMenuItemHitbox(DISPLAY_BACKGROUND_DARKNAVY_Y)
                        .contains(touchX, touchY))
                {
                    if (currentBackgroundColor != TFT_DARKNAVY)
                    {
                        currentBackgroundColor = TFT_DARKNAVY;
                        storage.saveBackgroundColor(
                            currentBackgroundColor);
                    }
                    showDisplaySettingsScreen();
                }
                else if (getMenuItemHitbox(DISPLAY_BACKGROUND_DEFAULT_Y)
                             .contains(touchX, touchY))
                {
                    if (currentBackgroundColor != TFT_BLACK)
                    {
                        currentBackgroundColor = TFT_BLACK;
                        storage.saveBackgroundColor(
                            currentBackgroundColor);
                    }
                    showDisplaySettingsScreen();
                }
                else if (togglesBeep)
                {
                    const bool enabled = !buzzer.isEnabled();
                    if (enabled)
                    {
                        buzzer.setEnabled(true);
                        storage.saveBeepEnabled(true);
                        buzzer.play(BeepPattern::Success);
                    }
                    else
                    {
                        // Confirm the OFF action before disabling further tones.
                        buzzer.beepFailureNow();
                        buzzer.setEnabled(false);
                        storage.saveBeepEnabled(false);
                    }
                    showDisplaySettingsScreen();
                }
                else
                {
                    displaySettingsScreenVisible = false;
                    drawScreen();
                }

                while (touch.isDown())
                    delay(5);
            }

            delay(20);
            continue;
        }

        if (iconsSettingsScreenVisible)
        {
            if (touch.pressed())
            {
                const uint16_t touchX = touch.getX();
                const uint16_t touchY = touch.getY();

                if (!icons.handleTouch(touchX, touchY))
                {
                    displayIconsOptionSelected = false;
                    iconsSettingsScreenVisible = false;
                    showDisplaySettingsScreen();
                }

                while (touch.pressed())
                    delay(5);
            }

            delay(20);
            continue;
        }

        if (systemInfoScreenVisible)
        {
            if (touch.pressed())
            {
                while (touch.pressed())
                {
                    delay(5);
                }

                systemInfoScreenVisible = false;
                drawScreen();
                continue;
            }

            delay(20);
            continue;
        }

        display.updateClock();
        refreshClockAdjust();

        if (touch.pressed())
        {
            const bool shouldExit = processTouch();

            while (touch.pressed())
            {
                display.updateClock();
                refreshClockAdjust();
                delay(5);
            }

            if (pressedAdjustButton != AdjustButton::None)
            {
                drawAdjustButton(pressedAdjustButton, false);
                pressedAdjustButton = AdjustButton::None;
            }

            if (shouldExit)
                break;
        }

        delay(20);
    }
}

void Sistem::redrawConfigMenuItem(const char *text, int y, ConfigMenuItem item)
{
    drawConfigMenuItem(text, y, selectedConfigItem == item);
}

void Sistem::clearConfigMenuSelection()
{
    const ConfigMenuItem previous = selectedConfigItem;
    selectedConfigItem = ConfigMenuItem::None;

    switch (previous)
    {
    case ConfigMenuItem::TouchCalibration:
        redrawConfigMenuItem(
            "2. TOUCH Calibration",
            TOUCH_CAL_ITEM_Y,
            previous);
        break;
    case ConfigMenuItem::DisplaySettings:
        redrawConfigMenuItem(
            "3. DISPLAY Settings",
            DISPLAY_SETTINGS_ITEM_Y,
            previous);
        break;
    case ConfigMenuItem::AudioSettings:
        redrawConfigMenuItem(
            "4. AUDIO Settings",
            AUDIO_SETTINGS_ITEM_Y,
            previous);
        break;
    case ConfigMenuItem::FactoryReset:
        redrawConfigMenuItem(
            "5. Factory RESET",
            FACTORY_RESET_ITEM_Y,
            previous);
        break;
    case ConfigMenuItem::SystemInformation:
        redrawConfigMenuItem(
            "6. About Tape-LAB",
            SYSTEM_INFO_ITEM_Y,
            previous);
        break;
    case ConfigMenuItem::None:
        break;
    }
}

void Sistem::drawScreen()
{
    display.openApp("SISTEM CONFIGURATION", "TOUCH TO BACK");
    tft.setTextDatum(TL_DATUM);

    selectedTimeField = TimeField::None;
    pressedAdjustButton = AdjustButton::None;
    displayedTimeValid = false;
    clockSelectionHintVisible = false;
    selectedConfigItem = ConfigMenuItem::None;
    selectedSystemIndex = 0;
    lastRtcReadMs = 0;

    const ClockLayout layout = getClockLayout();
    tft.setTextColor(TFT_WHITE, COL_BG);
    tft.drawString("  Clock:", layout.labelX, CLOCK_ROW_Y, ITEM_FONT);
    tft.drawString(
        ":",
        layout.firstColonX,
        CLOCK_ROW_Y,
        ITEM_FONT);
    tft.drawString(
        ":",
        layout.secondColonX,
        CLOCK_ROW_Y,
        ITEM_FONT);
    drawAdjustButton(AdjustButton::Minus, false);
    drawAdjustButton(AdjustButton::Plus, false);

    drawSystemIcons();

    refreshClockAdjust(true);
}

void Sistem::drawSystemIcons()
{
    for (uint8_t index = 1; index <= 6; ++index)
        drawSystemIcon(index);

    tft.setTextDatum(TL_DATUM);
}

void Sistem::drawSystemIcon(uint8_t index)
{
    if (index < 1 || index > 6)
        return;

    const int entry = index - 1;
    const int x = SYSTEM_ICON_X[entry];
    const int y = SYSTEM_ICON_Y[entry] - 3 - (index >= 4 ? 3 : 0);
    const char *label = "";

    switch (index)
    {
    case 1:
        icons.drawTouchCalibrationIcon(x, y, SYSTEM_ICON_SIZE, TFT_WHITE, COL_BG);
        label = "TOUCH";
        break;
    case 2:
        icons.drawDisplaySettingsIcon(x, y, SYSTEM_ICON_SIZE, TFT_WHITE, COL_BG);
        label = "DISPLAY";
        break;
    case 3:
        icons.drawAudioSettingsIcon(x, y, SYSTEM_ICON_SIZE, TFT_WHITE, COL_BG);
        label = "AUDIO";
        break;
    case 4:
        icons.drawFactoryResetIcon(x, y, SYSTEM_ICON_SIZE, TFT_WHITE, COL_BG);
        label = "RESET";
        break;
    case 5:
        icons.drawAboutIcon(x, y, SYSTEM_ICON_SIZE, TFT_WHITE, COL_BG);
        label = "ABOUT";
        break;
    case 6:
        icons.drawAutoCalIcon(x, y, SYSTEM_ICON_SIZE, TFT_WHITE, COL_BG);
        label = "AUTO CAL";
        break;
    }

    tft.setTextDatum(TC_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(TFT_WHITE, COL_BG);
    tft.drawString(label, x + SYSTEM_ICON_SIZE / 2,
                   y + SYSTEM_ICON_SIZE + SYSTEM_ICON_LABEL_OFFSET_Y);
}

void Sistem::drawSystemSelectionBorder(uint8_t index, bool selected)
{
    if (index < 1 || index > 6)
        return;

    const int entry = index - 1;
    const int x = SYSTEM_ICON_X[entry];
    const int y = SYSTEM_ICON_Y[entry] - 3 - (index >= 4 ? 3 : 0);
    const uint16_t borderColor = selected ? TFT_GREEN : TFT_WHITE;

    tft.drawRect(x, y, SYSTEM_ICON_SIZE, SYSTEM_ICON_SIZE, borderColor);
    tft.drawRect(x + 1, y + 1, SYSTEM_ICON_SIZE - 2, SYSTEM_ICON_SIZE - 2, borderColor);
}

int Sistem::handleSystemIconTouch(uint16_t x, uint16_t y)
{
    for (uint8_t index = 1; index <= 6; ++index)
    {
        const int entry = index - 1;
        const int iconX = SYSTEM_ICON_X[entry];
        const int iconY = SYSTEM_ICON_Y[entry];

        if (x < iconX || x >= iconX + SYSTEM_ICON_SIZE ||
            y < iconY || y >= iconY + SYSTEM_ICON_SIZE)
            continue;

        if (selectedSystemIndex == index)
            return index;

        const uint8_t previous = selectedSystemIndex;
        selectedSystemIndex = index;

        if (previous != 0)
            drawSystemSelectionBorder(previous, false);
        drawSystemSelectionBorder(index, true);
        return 0;
    }

    return -1;
}

void Sistem::refreshClockAdjust(bool force)
{
    const uint32_t now = millis();
    if (!force && now - lastRtcReadMs < RTC_READ_INTERVAL_MS)
        return;

    lastRtcReadMs = now;

    RtcDateTime currentDateTime;
    const bool currentTimeValid = rtc.read(currentDateTime);
    const bool hourChanged =
        force || currentTimeValid != displayedTimeValid ||
        (currentTimeValid &&
         currentDateTime.hour != displayedDateTime.hour);
    const bool minuteChanged =
        force || currentTimeValid != displayedTimeValid ||
        (currentTimeValid &&
         currentDateTime.minute != displayedDateTime.minute);
    const bool secondChanged =
        force || currentTimeValid != displayedTimeValid ||
        (currentTimeValid &&
         currentDateTime.second != displayedDateTime.second);

    if (currentTimeValid)
        displayedDateTime = currentDateTime;
    displayedTimeValid = currentTimeValid;

    const ClockLayout layout = getClockLayout();

    if (hourChanged)
        drawTimeField(
            TimeField::Hour,
            displayedDateTime.hour,
            layout.hourX,
            currentTimeValid);
    if (minuteChanged)
        drawTimeField(
            TimeField::Minute,
            displayedDateTime.minute,
            layout.minuteX,
            currentTimeValid);
    if (secondChanged)
        drawTimeField(
            TimeField::Second,
            displayedDateTime.second,
            layout.secondX,
            currentTimeValid);
}

void Sistem::drawTimeField(
    TimeField field,
    uint8_t value,
    int x,
    bool valid)
{
    const int fieldWidth = tft.textWidth("00", ITEM_FONT);
    const int fieldHeight = tft.fontHeight(ITEM_FONT);
    const bool selected = selectedTimeField == field;
    const uint16_t background = selected ? TFT_DARKCYAN : COL_BG;
    char text[3];

    if (valid)
        snprintf(text, sizeof(text), "%02u", (unsigned int)value);
    else
        snprintf(text, sizeof(text), "--");

    tft.fillRect(
        x,
        CLOCK_ROW_Y,
        fieldWidth,
        fieldHeight,
        background);
    tft.setTextColor(TFT_WHITE, background);
    tft.drawRightString(
        text,
        x + fieldWidth,
        CLOCK_ROW_Y,
        ITEM_FONT);
}

bool Sistem::processTouch()
{
    const uint16_t touchX = touch.getX();
    const uint16_t touchY = touch.getY();
    const ClockLayout layout = getClockLayout();
    const int firstBoundary =
        layout.firstColonX + layout.colonWidth / 2;
    const int secondBoundary =
        layout.secondColonX + layout.colonWidth / 2;
    const int fieldsLeft =
        layout.hourX - CLOCK_TOUCH_PADDING_X;
    const int fieldsRight =
        layout.secondX + layout.fieldWidth + CLOCK_TOUCH_PADDING_X;
    const int touchTop =
        CLOCK_ROW_Y + CLOCK_TOUCH_OFFSET_Y - CLOCK_TOUCH_PADDING_Y;
    const int touchBottom =
        CLOCK_ROW_Y + CLOCK_TOUCH_OFFSET_Y +
        layout.fieldHeight + CLOCK_TOUCH_PADDING_Y;
    const TouchRect minusHitbox = getMinusButtonHitbox(layout);
    const TouchRect plusHitbox = getPlusButtonHitbox(layout);
    const TouchRect touchCalibrationHitbox =
        getMenuItemHitbox(TOUCH_CAL_ITEM_Y);
    const TouchRect displaySettingsHitbox =
        getMenuItemHitbox(DISPLAY_SETTINGS_ITEM_Y);
    const TouchRect audioSettingsHitbox =
        getMenuItemHitbox(AUDIO_SETTINGS_ITEM_Y);
    const TouchRect factoryResetHitbox =
        getMenuItemHitbox(FACTORY_RESET_ITEM_Y);
    const TouchRect systemInfoHitbox =
        getMenuItemHitbox(SYSTEM_INFO_ITEM_Y);

    const int systemIconAction = handleSystemIconTouch(touchX, touchY);
    if (systemIconAction >= 0)
    {
        if (selectedTimeField != TimeField::None)
        {
            selectedTimeField = TimeField::None;
            refreshClockAdjust(true);
        }

        switch (systemIconAction)
        {
        case 0: // First touch selects only.
            break;
        case 1:
            runTouchCalibration();
            break;
        case 2:
            showDisplaySettingsScreen();
            break;
        case 3:
            setAudio.run();
            drawScreen();
            break;
        case 4:
            showFactoryResetConfirmation();
            break;
        case 5:
            showSystemInformationScreen();
            break;
        case 6:
            record.openThreeHeadTapeEqAutoCal();
            drawScreen();
            break;
        }
        return false;
    }

    // The former textual rows occupied this area.  Keep blank areas between
    // tiles inert so they cannot activate an invisible legacy menu item.
    if (touchY >= TOUCH_CAL_ITEM_Y - MENU_TOUCH_OFFSET_Y &&
        touchY < SYSTEM_ICON_SECOND_ROW_Y + SYSTEM_ICON_SIZE +
                     SYSTEM_ICON_LABEL_OFFSET_Y + tft.fontHeight(1))
    {
        return false;
    }

    const bool configurationItemTouched =
        touchCalibrationHitbox.contains(touchX, touchY) ||
        displaySettingsHitbox.contains(touchX, touchY) ||
        audioSettingsHitbox.contains(touchX, touchY) ||
        factoryResetHitbox.contains(touchX, touchY) ||
        systemInfoHitbox.contains(touchX, touchY);

    if (configurationItemTouched &&
        selectedTimeField != TimeField::None)
    {
        selectedTimeField = TimeField::None;
        refreshClockAdjust(true);
    }

    if (touchCalibrationHitbox.contains(touchX, touchY))
    {
        if (selectedConfigItem == ConfigMenuItem::TouchCalibration)
        {
            runTouchCalibration();
            selectedConfigItem = ConfigMenuItem::None;
        }
        else
        {
            const ConfigMenuItem previous = selectedConfigItem;
            selectedConfigItem = ConfigMenuItem::TouchCalibration;
            if (previous != ConfigMenuItem::None)
            {
                redrawConfigMenuItem("2. TOUCH Calibration", TOUCH_CAL_ITEM_Y, ConfigMenuItem::TouchCalibration);
                if (previous == ConfigMenuItem::DisplaySettings)
                    redrawConfigMenuItem("3. DISPLAY Settings", DISPLAY_SETTINGS_ITEM_Y, ConfigMenuItem::DisplaySettings);
                else if (previous == ConfigMenuItem::AudioSettings)
                    redrawConfigMenuItem("4. AUDIO Settings", AUDIO_SETTINGS_ITEM_Y, ConfigMenuItem::AudioSettings);
                else if (previous == ConfigMenuItem::FactoryReset)
                    redrawConfigMenuItem("5. Factory RESET", FACTORY_RESET_ITEM_Y, ConfigMenuItem::FactoryReset);
                else if (previous == ConfigMenuItem::SystemInformation)
                    redrawConfigMenuItem("6. About Tape-LAB", SYSTEM_INFO_ITEM_Y, ConfigMenuItem::SystemInformation);
            }
            else
            {
                redrawConfigMenuItem("2. TOUCH Calibration", TOUCH_CAL_ITEM_Y, ConfigMenuItem::TouchCalibration);
            }
        }
        return false;
    }
    if (displaySettingsHitbox.contains(touchX, touchY))
    {
        if (selectedConfigItem == ConfigMenuItem::DisplaySettings)
        {
            showDisplaySettingsScreen();
        }
        else
        {
            const ConfigMenuItem previous = selectedConfigItem;
            selectedConfigItem = ConfigMenuItem::DisplaySettings;
            if (previous != ConfigMenuItem::None)
            {
                redrawConfigMenuItem("3. DISPLAY Settings", DISPLAY_SETTINGS_ITEM_Y, ConfigMenuItem::DisplaySettings);
                if (previous == ConfigMenuItem::TouchCalibration)
                    redrawConfigMenuItem("2. TOUCH Calibration", TOUCH_CAL_ITEM_Y, ConfigMenuItem::TouchCalibration);
                else if (previous == ConfigMenuItem::AudioSettings)
                    redrawConfigMenuItem("4. AUDIO Settings", AUDIO_SETTINGS_ITEM_Y, ConfigMenuItem::AudioSettings);
                else if (previous == ConfigMenuItem::FactoryReset)
                    redrawConfigMenuItem("5. Factory RESET", FACTORY_RESET_ITEM_Y, ConfigMenuItem::FactoryReset);
                else if (previous == ConfigMenuItem::SystemInformation)
                    redrawConfigMenuItem("6. About Tape-LAB", SYSTEM_INFO_ITEM_Y, ConfigMenuItem::SystemInformation);
            }
            else
            {
                redrawConfigMenuItem("3. DISPLAY Settings", DISPLAY_SETTINGS_ITEM_Y, ConfigMenuItem::DisplaySettings);
            }
        }
        return false;
    }
    if (audioSettingsHitbox.contains(touchX, touchY))
    {
        if (selectedConfigItem == ConfigMenuItem::AudioSettings)
        {
            setAudio.run();
            drawScreen();
        }
        else
        {
            const ConfigMenuItem previous = selectedConfigItem;
            selectedConfigItem = ConfigMenuItem::AudioSettings;
            if (previous != ConfigMenuItem::None)
            {
                redrawConfigMenuItem("4. AUDIO Settings", AUDIO_SETTINGS_ITEM_Y, ConfigMenuItem::AudioSettings);
                if (previous == ConfigMenuItem::TouchCalibration)
                    redrawConfigMenuItem("2. TOUCH Calibration", TOUCH_CAL_ITEM_Y, ConfigMenuItem::TouchCalibration);
                else if (previous == ConfigMenuItem::DisplaySettings)
                    redrawConfigMenuItem("3. DISPLAY Settings", DISPLAY_SETTINGS_ITEM_Y, ConfigMenuItem::DisplaySettings);
                else if (previous == ConfigMenuItem::FactoryReset)
                    redrawConfigMenuItem("5. Factory RESET", FACTORY_RESET_ITEM_Y, ConfigMenuItem::FactoryReset);
                else if (previous == ConfigMenuItem::SystemInformation)
                    redrawConfigMenuItem("6. About Tape-LAB", SYSTEM_INFO_ITEM_Y, ConfigMenuItem::SystemInformation);
            }
            else
            {
                redrawConfigMenuItem("4. AUDIO Settings", AUDIO_SETTINGS_ITEM_Y, ConfigMenuItem::AudioSettings);
            }
        }
        return false;
    }
    if (factoryResetHitbox.contains(touchX, touchY))
    {
        if (selectedConfigItem == ConfigMenuItem::FactoryReset)
        {
            showFactoryResetConfirmation();
            selectedConfigItem = ConfigMenuItem::None;
        }
        else
        {
            const ConfigMenuItem previous = selectedConfigItem;
            selectedConfigItem = ConfigMenuItem::FactoryReset;
            if (previous != ConfigMenuItem::None)
            {
                redrawConfigMenuItem("5. Factory RESET", FACTORY_RESET_ITEM_Y, ConfigMenuItem::FactoryReset);
                if (previous == ConfigMenuItem::TouchCalibration)
                    redrawConfigMenuItem("2. TOUCH Calibration", TOUCH_CAL_ITEM_Y, ConfigMenuItem::TouchCalibration);
                else if (previous == ConfigMenuItem::DisplaySettings)
                    redrawConfigMenuItem("3. DISPLAY Settings", DISPLAY_SETTINGS_ITEM_Y, ConfigMenuItem::DisplaySettings);
                else if (previous == ConfigMenuItem::AudioSettings)
                    redrawConfigMenuItem("4. AUDIO Settings", AUDIO_SETTINGS_ITEM_Y, ConfigMenuItem::AudioSettings);
                else if (previous == ConfigMenuItem::SystemInformation)
                    redrawConfigMenuItem("6. About Tape-LAB", SYSTEM_INFO_ITEM_Y, ConfigMenuItem::SystemInformation);
            }
            else
            {
                redrawConfigMenuItem("5. Factory RESET", FACTORY_RESET_ITEM_Y, ConfigMenuItem::FactoryReset);
            }
        }
        return false;
    }
    if (systemInfoHitbox.contains(touchX, touchY))
    {
        if (selectedConfigItem == ConfigMenuItem::SystemInformation)
        {
            showSystemInformationScreen();
            selectedConfigItem = ConfigMenuItem::None;
        }
        else
        {
            const ConfigMenuItem previous = selectedConfigItem;
            selectedConfigItem = ConfigMenuItem::SystemInformation;
            if (previous != ConfigMenuItem::None)
            {
                redrawConfigMenuItem("6. About Tape-LAB", SYSTEM_INFO_ITEM_Y, ConfigMenuItem::SystemInformation);
                if (previous == ConfigMenuItem::TouchCalibration)
                    redrawConfigMenuItem("2. TOUCH Calibration", TOUCH_CAL_ITEM_Y, ConfigMenuItem::TouchCalibration);
                else if (previous == ConfigMenuItem::DisplaySettings)
                    redrawConfigMenuItem("3. DISPLAY Settings", DISPLAY_SETTINGS_ITEM_Y, ConfigMenuItem::DisplaySettings);
                else if (previous == ConfigMenuItem::AudioSettings)
                    redrawConfigMenuItem("4. AUDIO Settings", AUDIO_SETTINGS_ITEM_Y, ConfigMenuItem::AudioSettings);
                else if (previous == ConfigMenuItem::FactoryReset)
                    redrawConfigMenuItem("5. Factory RESET", FACTORY_RESET_ITEM_Y, ConfigMenuItem::FactoryReset);
            }
            else
            {
                redrawConfigMenuItem("6. About Tape-LAB", SYSTEM_INFO_ITEM_Y, ConfigMenuItem::SystemInformation);
            }
        }
        return false;
    }

    if (touchY < touchTop || touchY >= touchBottom)
    {
        return true;
    }

    if (touchX >= fieldsLeft && touchX < fieldsRight &&
        selectedConfigItem != ConfigMenuItem::None)
    {
        clearConfigMenuSelection();
    }

    if (touchX >= fieldsLeft && touchX < firstBoundary)
    {
        selectTimeField(TimeField::Hour);
        return false;
    }
    if (touchX >= firstBoundary && touchX < secondBoundary)
    {
        selectTimeField(TimeField::Minute);
        return false;
    }
    if (touchX >= secondBoundary && touchX < fieldsRight)
    {
        selectTimeField(TimeField::Second);
        return false;
    }
    if (minusHitbox.contains(touchX, touchY))
    {
        pressedAdjustButton = AdjustButton::Minus;
        drawAdjustButton(pressedAdjustButton, true);
        adjustSelectedField(-1);
        return false;
    }
    if (plusHitbox.contains(touchX, touchY))
    {
        pressedAdjustButton = AdjustButton::Plus;
        drawAdjustButton(pressedAdjustButton, true);
        adjustSelectedField(1);
        return false;
    }

    // Colons and spacing belong to the item as well; an imprecise touch there
    // must not close the configuration screen.
    return false;
}

void Sistem::selectTimeField(TimeField field)
{
    if (selectedTimeField == field)
        return;

    selectedTimeField = field;
    if (clockSelectionHintVisible)
    {
        display.setAppStatus("TOUCH TO BACK");
        clockSelectionHintVisible = false;
    }
    refreshClockAdjust(true);
}

void Sistem::adjustSelectedField(int8_t direction)
{
    if (selectedTimeField == TimeField::None)
    {
        display.setAppStatus(
            "SELECT ITEM to change HH:MM:SS",
            1,
            220);
        clockSelectionHintVisible = true;
        return;
    }

    RtcDateTime dateTime;
    if (!rtc.read(dateTime))
        return;

    switch (selectedTimeField)
    {
    case TimeField::Hour:
        dateTime.hour =
            (uint8_t)((dateTime.hour + direction + 24) % 24);
        break;
    case TimeField::Minute:
        dateTime.minute =
            (uint8_t)((dateTime.minute + direction + 60) % 60);
        break;
    case TimeField::Second:
        dateTime.second =
            (uint8_t)((dateTime.second + direction + 60) % 60);
        break;
    case TimeField::None:
        return;
    }

    if (!rtc.setTime(
            dateTime.hour,
            dateTime.minute,
            dateTime.second))
    {
        return;
    }

    displayedDateTime = dateTime;
    displayedTimeValid = true;
    refreshClockAdjust(true);
    display.updateClock(true);
}

void Sistem::drawAdjustButton(AdjustButton button, bool pressed)
{
    if (button == AdjustButton::None)
        return;

    const ClockLayout layout = getClockLayout();
    const bool isMinus = button == AdjustButton::Minus;
    const int x = isMinus ? layout.minusX : layout.plusX;
    const char *const text = isMinus ? "-" : "+";
    const TouchRect hitbox =
        isMinus ? getMinusButtonHitbox(layout)
                : getPlusButtonHitbox(layout);
    const uint16_t background = pressed ? TFT_GREEN : COL_CYAN_OFF;
    const uint16_t foreground = pressed ? TFT_BLACK : TFT_WHITE;

    tft.fillRect(
        x,
        CLOCK_ROW_Y,
        isMinus ? layout.minusWidth : layout.plusWidth,
        CLOCK_ADJUST_BUTTON_HEIGHT,
        background);
    tft.drawRect(
        x,
        CLOCK_ROW_Y,
        isMinus ? layout.minusWidth : layout.plusWidth,
        CLOCK_ADJUST_BUTTON_HEIGHT,
        TFT_DARKGREY);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(foreground, background);
    tft.drawString(
        text,
        x + (isMinus ? layout.minusWidth : layout.plusWidth) / 2,
        CLOCK_ROW_Y + CLOCK_ADJUST_BUTTON_HEIGHT / 2,
        ITEM_FONT);
    tft.setTextDatum(TL_DATUM);
}

void Sistem::runTouchCalibration()
{
    TouchCalibration calibration;
    if (touchCal.run(calibration))
    {
        touch.setCalibration(calibration);
        storage.saveTouchCalibration(calibration);
    }

    drawScreen();
}

void Sistem::showFactoryResetConfirmation()
{
    constexpr int MODAL_X = 18;
    constexpr int MODAL_Y = 34;
    constexpr int MODAL_W = 284;
    constexpr int MODAL_H = 170;

    constexpr int BUTTON_Y = MODAL_Y + MODAL_H - 35;
    constexpr int BUTTON_W = 90;
    constexpr int BUTTON_H = 26;

    constexpr int CANCEL_X = MODAL_X + 32;
    constexpr int RESET_X = MODAL_X + MODAL_W - 32 - BUTTON_W;

    const uint16_t background =
        currentBackgroundColor == TFT_DARKNAVY
            ? TFT_BLACK
            : TFT_DARKNAVY;

    auto contains = [](
                        uint16_t x,
                        uint16_t y,
                        int left,
                        int top,
                        int width,
                        int height)
    {
        return x >= left &&
               x < left + width &&
               y >= top &&
               y < top + height;
    };

    auto drawButton = [background](
                          const char *text,
                          int x,
                          bool pressed,
                          uint16_t frameColor)
    {
        const uint16_t buttonBackground =
            pressed ? TFT_DARKGREY : background;

        tft.fillRect(
            x,
            BUTTON_Y,
            BUTTON_W,
            BUTTON_H,
            buttonBackground);

        tft.drawRect(
            x,
            BUTTON_Y,
            BUTTON_W,
            BUTTON_H,
            frameColor);

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, buttonBackground);

        tft.drawString(
            text,
            x + BUTTON_W / 2,
            BUTTON_Y + BUTTON_H / 2,
            2);

        tft.setTextDatum(TL_DATUM);
    };

    // Nu interpretăm atingerea care a deschis modalul.
    while (touch.pressed())
        delay(5);

    tft.fillRect(
        MODAL_X,
        MODAL_Y,
        MODAL_W,
        MODAL_H,
        background);

    tft.drawRect(
        MODAL_X,
        MODAL_Y,
        MODAL_W,
        MODAL_H,
        COL_FRAME);

    tft.drawRect(
        MODAL_X + 2,
        MODAL_Y + 2,
        MODAL_W - 4,
        MODAL_H - 4,
        TFT_DARKGREY);

    tft.setTextDatum(TL_DATUM);

    tft.setTextColor(TFT_ORANGE, background);
    tft.drawString(
        "FACTORY RESET",
        MODAL_X + 12,
        MODAL_Y + 10,
        2);

    tft.drawFastHLine(
        MODAL_X + 10,
        MODAL_Y + 31,
        MODAL_W - 20,
        COL_FRAME);

    tft.setTextColor(TFT_WHITE, background);

    constexpr int TEXT_X = MODAL_X + 12;
    constexpr int TEXT_Y = MODAL_Y + 39;
    constexpr int LINE_H = 14;

    const char *const lines[] =
        {
            "Audio settings, display background",
            "and Touch Calibration return to defaults.",
            "",
            "Clock settings are preserved.",
            "ATENTION:",
            "This action cannot be undone!"};

    for (uint8_t i = 0;
         i < sizeof(lines) / sizeof(lines[0]);
         ++i)
    {
        tft.drawString(
            lines[i],
            TEXT_X,
            TEXT_Y + i * LINE_H,
            2);
    }

    drawButton("CANCEL", CANCEL_X, false, TFT_DARKGREY);
    drawButton("RESET", RESET_X, false, TFT_RED);

    while (true)
    {
        if (!touch.pressed())
        {
            delay(5);
            continue;
        }

        const uint16_t x = touch.getX();
        const uint16_t y = touch.getY();

        const bool cancelTouched =
            contains(
                x,
                y,
                CANCEL_X,
                BUTTON_Y,
                BUTTON_W,
                BUTTON_H);

        const bool resetTouched =
            contains(
                x,
                y,
                RESET_X,
                BUTTON_Y,
                BUTTON_W,
                BUTTON_H);

        if (cancelTouched)
            drawButton("CANCEL", CANCEL_X, true, TFT_DARKGREY);

        if (resetTouched)
            drawButton("RESET", RESET_X, true, TFT_RED);

        while (touch.pressed())
            delay(5);

        if (cancelTouched)
        {
            drawScreen();
            return;
        }

        if (resetTouched)
        {
            executeFactoryReset();
            return;
        }
    }
}

void Sistem::executeFactoryReset()
{
    display.setAppStatus("RESETTING...");
    record.invalidateGeneratorCalibration();
    buzzer.setEnabled(true);

    if (!storage.factoryReset())
    {
        display.openApp("SYSTEM CONFIG", "FACTORY RESET ERROR");
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_RED, COL_BG);
        tft.drawString("FACTORY RESET ERROR", 20, 70, 2);
        tft.drawString("GEN CAL CLEAR FAILED", 20, 96, 2);
        delay(1500);
        drawScreen();
        return;
    }

    showFactoryResetCompleteScreen();
}

void Sistem::showFactoryResetCompleteScreen()
{
    constexpr int BUTTON_Y = 158;
    constexpr int BUTTON_W = 108;
    constexpr int BUTTON_H = 30;
    constexpr int CALIBRATE_X = 42;
    constexpr int RESTART_X = 170;

    display.openApp("SYSTEM CONFIG", "GEN CAL RESET");
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_GREEN, COL_BG);
    tft.drawString("FACTORY DEFAULTS RESTORED", 20, 42, 2);
    tft.setTextColor(TFT_YELLOW, COL_BG);
    tft.drawString("GEN CAL RESET", 20, 72, 2);
    tft.drawString("AUTO CAL RECOMMENDED", 20, 94, 2);

    const auto drawButton = [](const char *text, int x, uint16_t color)
    {
        tft.fillRect(x, BUTTON_Y, BUTTON_W, BUTTON_H, COL_BG);
        tft.drawRect(x, BUTTON_Y, BUTTON_W, BUTTON_H, color);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(color, COL_BG);
        tft.drawString(text, x + BUTTON_W / 2, BUTTON_Y + BUTTON_H / 2, 2);
        tft.setTextDatum(TL_DATUM);
    };
    drawButton("CALIBRATE", CALIBRATE_X, TFT_GREEN);
    drawButton("RESTART", RESTART_X, TFT_CYAN);

    while (touch.pressed())
        delay(5);
    while (true)
    {
        if (!touch.pressed())
        {
            delay(5);
            continue;
        }
        const uint16_t x = touch.getX();
        const uint16_t y = touch.getY();
        const bool calibrate = x >= CALIBRATE_X && x < CALIBRATE_X + BUTTON_W &&
                               y >= BUTTON_Y && y < BUTTON_Y + BUTTON_H;
        const bool restart = x >= RESTART_X && x < RESTART_X + BUTTON_W &&
                             y >= BUTTON_Y && y < BUTTON_Y + BUTTON_H;
        while (touch.pressed())
            delay(5);
        if (calibrate)
        {
            record.openThreeHeadTapeEqAutoCal();
            drawScreen();
            return;
        }
        if (restart)
        {
            NVIC_SystemReset();
            return;
        }
    }
}

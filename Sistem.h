#pragma once

#include <Arduino.h>
#include "Rtc.h"

class Sistem
{
public:
    void run();

private:
    enum class TimeField : uint8_t
    {
        None,
        Hour,
        Minute,
        Second
    };

    enum class AdjustButton : uint8_t
    {
        None,
        Minus,
        Plus
    };

    enum class ConfigMenuItem : uint8_t
    {
        None,
        TouchCalibration,
        DisplaySettings,
        AudioSettings,
        FactoryReset,
        SystemInformation
    };

    void drawScreen();
    void showSystemInformationScreen();
    void showDisplaySettingsScreen();
    void showIconsSettingsScreen();
    void drawSystemIcons();
    void drawSystemIcon(uint8_t index);
    void drawSystemSelectionBorder(uint8_t index, bool selected);
    int handleSystemIconTouch(uint16_t x, uint16_t y);
    void redrawConfigMenuItem(const char *text, int y, ConfigMenuItem item);
    void clearConfigMenuSelection();
    void drawTimeField(
        TimeField field,
        uint8_t value,
        int x,
        bool valid);
    void refreshClockAdjust(bool force = false);
    bool processTouch();
    void selectTimeField(TimeField field);
    void adjustSelectedField(int8_t direction);
    void drawAdjustButton(AdjustButton button, bool pressed);
    void runTouchCalibration();
    
    void showFactoryResetConfirmation();
    void showFactoryResetCompleteScreen();
    void executeFactoryReset();

    RtcDateTime displayedDateTime;
    TimeField selectedTimeField = TimeField::None;
    AdjustButton pressedAdjustButton = AdjustButton::None;
    uint32_t lastRtcReadMs = 0;
    bool displayedTimeValid = false;
    bool clockSelectionHintVisible = false;
    bool systemInfoScreenVisible = false;
    bool displaySettingsScreenVisible = false;
    bool iconsSettingsScreenVisible = false;
    bool displayIconsOptionSelected = false;
    ConfigMenuItem selectedConfigItem = ConfigMenuItem::None;
    uint8_t selectedSystemIndex = 0;
};

extern Sistem sistem;

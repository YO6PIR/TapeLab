#pragma once

#include <Arduino.h>

class Sine
{
public:
    void run();
    void stopForRecord();
    void drawServiceButton(
        int x,
        int y,
        const char *line1,
        const char *line2,
        bool active = false);

    bool isRunning() const;
    uint16_t frequencyHz() const;
    float levelDb() const;

private:
    enum class Button : uint8_t
    {
        None,
        FrequencyDown,
        FrequencyUp,
        LevelDown,
        LevelUp,
        Preset440,
        Preset1000,
        Preset3150,
        Preset10000,
        Preset15000,
        StartStop
    };

    void drawScreen();
    void drawFrequency();
    void drawLevel();
    void drawControls(Button pressed = Button::None);
    void drawControlButton(Button button, bool pressed);
    void drawPresetButton(Button button, bool pressed);
    void drawRunButton(bool pressed = false);
    void drawRunningStatus();
    void drawButton(
        const char *text,
        int x,
        int y,
        int width,
        bool pressed,
        uint8_t font);

    Button buttonAt(uint16_t x, uint16_t y) const;
    bool isAdjustmentButton(Button button) const;
    void apply(Button button);

    uint16_t frequency = 1000;
    int16_t levelTenthsDb = 0;   // 0.0 dB = 1.00 Vpp
    uint16_t displayedFrequency = 0;
    int16_t displayedLevelTenthsDb = 0;
    bool displayedLevelMuted = false;
    bool frequencyDisplayInitialized = false;
    bool levelDisplayInitialized = false;
    bool running = false;
    Button selectedPreset = Button::None;
};

extern Sine sine;

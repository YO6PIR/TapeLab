#pragma once

#include <Arduino.h>

constexpr uint16_t AUDIO_SETTINGS_SIGNATURE = 0x4155;
constexpr uint8_t AUDIO_SETTINGS_VERSION = 3;

struct AudioSettings
{
    uint16_t signature = AUDIO_SETTINGS_SIGNATURE;
    uint8_t version = AUDIO_SETTINGS_VERSION;

    uint16_t rmsReference = 215;
    uint8_t smoothingPercent = 60;
    uint16_t peakHoldMs = 650;
    uint8_t peakDecayHundredths = 69;
    int8_t overLevelDb = 9;
    uint16_t wfNoiseFloorThousandths = 50;
    uint16_t rawFrequency = 3150;

    uint32_t frequencyCalibrationPpm = 1000000;
};

class SetAudio
{
public:
    void begin();
    void run();

    bool bootCheck() const;

    float rmsReference() const;
    float smoothing() const;
    uint16_t peakHoldMs() const;
    float peakDecayDb() const;
    float overLevelDb() const;
    float wfNoiseFloorPercent() const;
    uint16_t rawFrequency() const;

private:
    enum class Field : uint8_t
    {
        None,
        RmsReference,
        Smoothing,
        PeakHold,
        PeakDecay,
        OverLevel,
        WfNoiseFloor,
        Calibration3150Hz
    };

    void drawScreen();
    void redrawScreen();
    void drawField(Field field, bool runPressed = false);
    void drawAdjustmentButtons(bool minusPressed = false, bool plusPressed = false);
    void drawCalibration3150HzFrequency();
    bool processTouch();
    void showHelp(
        int buttonX,
        int buttonY,
        uint16_t idleBackground,
        const char *title,
        const char *const lines[],
        uint8_t lineCount);
    void selectField(Field field);
    void reset3150HzCalibration();
    void adjustSelectedField(int8_t direction);
    void update3150HzCalibrationInput();
    void save();

    AudioSettings settings;
    static constexpr uint16_t FREQUENCY_SAMPLE_COUNT = 1024;
    int16_t frequencySamples[FREQUENCY_SAMPLE_COUNT] = {0};
    Field selectedField = Field::None;
    bool adcCalibrationSelected = false;
    bool calibration3150HzRunning = false;
    bool calibration3150HzConfirmed = false;
    bool calibrationRunPressed = false;
    bool adjustmentButtonPressed = false;
    uint32_t lastCalibrationDisplayMs = 0;
    float calibration3150HzFrequency = 0.0f;
    bool changed = false;
};

extern SetAudio setAudio;

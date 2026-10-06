#pragma once

#include <Arduino.h>

// ATC setup values.  Keep these together so the procedure can be retuned
// without changing its state machine.
constexpr uint16_t ATC_BIAS_FREQ_LOW = 400;
constexpr uint16_t ATC_BIAS_FREQ_HIGH = 10000;
constexpr float ATC_BIAS_LEVEL_DB = -10.0f;
constexpr uint16_t ATC_EQ_FREQ_LOW = 400;
constexpr uint16_t ATC_EQ_FREQ_HIGH = 6000;
constexpr float ATC_EQ_LEVEL_DB = -10.0f;
constexpr uint16_t ATC_LEVEL_FREQ = 3000;
constexpr float ATC_LEVEL_DB = 0.0f;

constexpr float ATC_BIAS_TOLERANCE_DB = 0.5f;
constexpr float ATC_EQ_TOLERANCE_DB = 0.5f;
constexpr float ATC_LEVEL_TOLERANCE_DB = 0.5f;

class Atc
{
public:
    // ATC is deliberately available only from the 2 HEAD record menu.
    void run();

private:
    enum class Stage : uint8_t
    {
        Bias,
        Eq,
        Level,
        Complete
    };
    enum class State : uint8_t
    {
        ReadyToRecord,
        Recording,
        Rewind,
        ReadyToPlay,
        WaitingStartSync,
        MeasuringFirst,
        MeasuringGap,
        MeasuringSecond,
        WaitingEndSync,
        Result,
        WaitingNext
    };

    void resetStage();
    void drawScreen();
    void drawCycleScreen();
    void drawResultScreen();
    void drawCompleteScreen();
    void drawCompleteControl(bool pressed = false);
    void drawPhaseProgress(uint32_t now);
    uint32_t activeCounterDurationMs() const;
    void drawControl(const char *top, const char *bottom, bool pressed = false);
    void drawMessageBox(const char *top, const char *bottom, uint16_t color);
    void drawBottomMessage(const char *top, const char *bottom, uint16_t color);
    void drawBottomSingleLine(const char *text, uint16_t color, uint8_t font);
    bool controlHit(uint16_t x, uint16_t y) const;
    void startRecording();
    void updateRecording(uint32_t now);
    void startPlayback();
    void updatePlayback(uint32_t now);
    bool measureSync();
    void measureTone(uint16_t expectedHz, bool firstTone);
    void finishMeasurement();
    void fail(const char *message);
    bool stageHasSecondTone() const;
    uint16_t firstFrequency() const;
    uint16_t secondFrequency() const;
    float stageLevelDb() const;
    float stageToleranceDb() const;
    const char *stageName() const;

    Stage stage = Stage::Bias;
    State state = State::ReadyToRecord;
    uint32_t segmentStartedMs = 0;
    uint32_t playbackStartedMs = 0;
    uint8_t syncDetections = 0;
    uint8_t syncExitDetections = 0;
    bool syncFound = false;
    uint8_t recordPhase = 0;
    float firstLeftRmsSum = 0.0f;
    float firstRightRmsSum = 0.0f;
    float secondLeftRmsSum = 0.0f;
    float secondRightRmsSum = 0.0f;
    uint8_t firstLeftSampleCount = 0;
    uint8_t firstRightSampleCount = 0;
    uint8_t secondLeftSampleCount = 0;
    uint8_t secondRightSampleCount = 0;
    bool clipped = false;
    float firstLeftDb = NAN;
    float firstRightDb = NAN;
    float secondLeftDb = NAN;
    float secondRightDb = NAN;
    float leftErrorDb = NAN;
    float rightErrorDb = NAN;
    bool resultOk = false;
    const char *errorMessage = nullptr;
    const char *recommendation = nullptr;
    uint32_t progressBarAnchorMs = 0;
    uint32_t progressBarDurationMs = 0;
    uint8_t progressBarValue = 0xFF;
    uint32_t progressBarElapsedSecond = UINT32_MAX;
    uint16_t progressBarOperationKey = UINT16_MAX;
    bool progressBarPhaseActive = false;
    uint8_t completedStages = 0; // bit 0=Bias, 1=EQ, 2=Level
};

extern Atc atc;

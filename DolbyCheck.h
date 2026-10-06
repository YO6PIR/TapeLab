#pragma once

#include <Arduino.h>
#include "GraphMemory.h"

constexpr uint8_t DOLBY_CHECK_FREQUENCY_COUNT = 9;

class DolbyCheck
{
public:
    enum class DeckMode : uint8_t
    {
        TwoHead,
        ThreeHead
    };

    void run(DeckMode deckMode);
    void showMemoryResult(const GraphMemorySlot &slot);

private:
    enum class SweepCurve : uint8_t
    {
        Off,
        B,
        C
    };

    enum class OffSweepState : uint8_t
    {
        Ready,
        PrecheckSettling,
        PrecheckMeasuring,
        PrecheckPassed,
        Settling,
        Measuring,
        Complete,
        Error
    };

    enum class NoiseState : uint8_t
    {
        Inactive,
        Prompt,
        Precheck,
        Settling,
        Measuring,
        MeasuringHf,
        Complete,
        Error
    };

    enum class TwoHeadWorkflowState : uint8_t
    {
        ReadyToRecord,
        Recording,
        Rewind,
        WaitingSync,
        Analyzing,
        WaitingEndSync,
        Complete,
        RecordError,
        PlaybackError
    };

    enum class TwoHeadSequencePhase : uint8_t
    {
        Sync,
        Gap,
        Tone,
        Noise,
        EndSync
    };

    struct CurvePlaceholder
    {
        float measuredDb[DOLBY_CHECK_FREQUENCY_COUNT] = {};
        float relativeDb[DOLBY_CHECK_FREQUENCY_COUNT] = {};
        bool captured[DOLBY_CHECK_FREQUENCY_COUNT] = {};
    };

    struct DeckPlaceholderState
    {
        CurvePlaceholder dolbyOff;
        CurvePlaceholder dolbyB;
        CurvePlaceholder dolbyC;
    };

    void drawScreen(DeckMode deckMode);
    void drawGraphGrid();
    void drawLegend(bool focusActiveCurve = false);
    uint16_t curveColor(SweepCurve curve) const;
    void drawStatus(const char *text, uint16_t color);
    void drawTrackingSummary();
    void drawSnrSummary();
    void drawNoiseControl(
        const char *topText,
        const char *bottomText,
        bool pressed = false);
    void drawTwoHeadControl(
        const char *topText,
        const char *bottomText,
        bool pressed = false);
    void drawDolbyProgress(
        uint8_t step,
        uint8_t totalSteps,
        bool clear,
        uint8_t &previousStep,
        uint8_t &previousBricks);
    void drawThreeHeadSweepProgress(bool clear = false);
    void drawProgressMessage(const char *text, uint16_t color);
    void drawStartButton(
        bool pressed = false,
        SweepCurve curve = SweepCurve::Off);
    bool isStartTouch(uint16_t x, uint16_t y) const;
    bool isDolbyBStartAvailable() const;
    bool isDolbyCStartAvailable() const;
    bool selectStartCurve(SweepCurve &curve) const;
    void clearStartButton();
    void drawCurve(const CurvePlaceholder &curve, uint16_t color);
    void drawStoredCurves();
    bool calculateTrackingErrors(
        const CurvePlaceholder &curve,
        float &averageErrorDb,
        float &maximumErrorDb) const;
    void resetTwoHeadWorkflow();
    void prepareTwoHeadMode(SweepCurve curve);
    void handleTwoHeadAction();
    void startTwoHeadRecording();
    void updateTwoHeadWorkflow();
    void updateTwoHeadRecording(uint32_t now);
    void drawTwoHeadRecordProgress(bool clear = false);
    void startTwoHeadPlayback();
    void updateTwoHeadSync(uint32_t now);
    void startTwoHeadAnalysis(uint32_t now);
    void updateTwoHeadAnalysis(uint32_t now);
    void finishTwoHeadBand(uint32_t now);
    void updateTwoHeadEndSync(uint32_t now);
    bool measureTwoHeadSyncWindow(bool &clipped);
    void completeTwoHeadMode();
    void failTwoHead(const char *status, bool playbackError);
    CurvePlaceholder &activeCurveData();
    const CurvePlaceholder &curveData(SweepCurve curve) const;
    void resetActiveSweep();
    void resetPlaybackPrecheck();
    void startPlaybackPrecheck(
        SweepCurve curve,
        bool forNoise = false);
    void updatePlaybackPrecheck(uint32_t now);
    void finishPlaybackPrecheck();
    void failPlaybackPrecheck(const char *status);
    void startActiveSweep();
    void startOffPoint();
    void updateOffSweep();
    void finishOffPoint();
    bool normalizeActiveCurve();
    void failOffSweep(const char *status = "OFF ERROR");
    void startNoiseWorkflow();
    void startNoisePrecheck();
    void startNoiseMeasurement();
    void updateNoiseMeasurement();
    void finishNoiseMeasurement();
    void startHfNoiseMeasurement();
    void updateHfNoiseMeasurement();
    void finishHfNoiseMeasurement();
    void failNoiseMeasurement(const char *status);
    uint8_t curveIndex(SweepCurve curve) const;
    bool graphResultComplete() const;
    bool askSaveGraphResult();
    void showMemoryFull();

    // Separate result storage for the live 3-head path and the 2-head
    // record/rewind/play workflow; both share the measurement engines.
    DeckPlaceholderState twoHeadState;
    DeckPlaceholderState threeHeadState;
    DeckMode activeDeckMode = DeckMode::ThreeHead;

    SweepCurve activeCurve = SweepCurve::Off;
    OffSweepState offSweepState = OffSweepState::Ready;
    uint8_t offBandIndex = 0;
    uint32_t offPointStartedMs = 0;
    float offSumLeftRms = 0.0f;
    float offSumRightRms = 0.0f;
    uint16_t offLeftWindowCount = 0;
    uint16_t offRightWindowCount = 0;
    bool offPointClipped = false;

    uint32_t precheckStartedMs = 0;
    float precheckSumLeftRms = 0.0f;
    float precheckSumRightRms = 0.0f;
    uint16_t precheckLeftWindowCount = 0;
    uint16_t precheckRightWindowCount = 0;
    uint16_t precheckLeftToneWindowCount = 0;
    uint16_t precheckRightToneWindowCount = 0;
    bool precheckClipped = false;

    NoiseState noiseState = NoiseState::Inactive;
    SweepCurve noiseCurve = SweepCurve::Off;
    bool noisePrecheckActive = false;
    float noiseDb[3] = {NAN, NAN, NAN};
    float hfNoiseDb[3] = {NAN, NAN, NAN};
    uint32_t noiseStartedMs = 0;
    float noiseSumLeftMeanSquare = 0.0f;
    float noiseSumRightMeanSquare = 0.0f;
    float noiseMinimumWindowDb = NAN;
    float noiseMaximumWindowDb = NAN;
    uint16_t noiseWindowCount = 0;
    bool noiseClipped = false;
    float hfNoiseSumLeftPower = 0.0f;
    float hfNoiseSumRightPower = 0.0f;
    float hfNoiseMinimumCaptureDb = NAN;
    float hfNoiseMaximumCaptureDb = NAN;
    uint8_t hfNoiseCaptureCount = 0;
    bool hfNoiseClipped = false;

    TwoHeadWorkflowState twoHeadWorkflowState =
        TwoHeadWorkflowState::ReadyToRecord;
    TwoHeadSequencePhase twoHeadSequencePhase =
        TwoHeadSequencePhase::Sync;
    uint32_t twoHeadSegmentStartedMs = 0;
    uint32_t twoHeadPlaybackStartedMs = 0;
    uint8_t twoHeadSyncDetections = 0;
    uint8_t twoHeadSyncExitDetections = 0;
    bool twoHeadSyncFound = false;
    uint8_t twoHeadRecordProgressStep = 0;
    uint8_t twoHeadRecordProgressBricks = 0xFF;
    uint8_t threeHeadProgressStep = 0;
    uint8_t threeHeadProgressBricks = 0xFF;
};

extern DolbyCheck dolbyCheck;

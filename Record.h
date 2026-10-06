#pragma once

#include <Arduino.h>
#include "GraphMemory.h"

constexpr uint8_t TAPE_RESPONSE_BAND_COUNT = 16;

struct TapeResponseResult
{
    bool valid = false;
    float leftDb[TAPE_RESPONSE_BAND_COUNT] = {};
    float rightDb[TAPE_RESPONSE_BAND_COUNT] = {};
    float leftRelativeDb[TAPE_RESPONSE_BAND_COUNT] = {};
    float rightRelativeDb[TAPE_RESPONSE_BAND_COUNT] = {};
    float combinedDb[TAPE_RESPONSE_BAND_COUNT] = {};
    float combinedRelativeDb[TAPE_RESPONSE_BAND_COUNT] = {};
};

struct TapeResponseCalibration
{
    bool valid = false;
    float measuredDb[TAPE_RESPONSE_BAND_COUNT] = {};
    float relativeDb[TAPE_RESPONSE_BAND_COUNT] = {};
    float generatorCorrectionDb[TAPE_RESPONSE_BAND_COUNT] = {};
};

class Record
{
public:
    void begin();
    void run();
    void openThreeHeadTapeEqAutoCal();
    void openThreeHeadCalibrationWorkflow();
    void invalidateGeneratorCalibration();
    bool setCalibratedTestTone(uint32_t frequencyHz, float nominalLevelDb);
    void stopCalibratedTestTone();
    bool measureTapeResponseWindow(
        float &leftRms,
        float &rightRms,
        bool &leftValid,
        bool &rightValid,
        bool &clipped);
    bool measureTapeResponseToneWindow(
        float &leftRms,
        float &rightRms,
        float &leftFrequencyHz,
        float &rightFrequencyHz,
        bool &leftValid,
        bool &rightValid,
        bool &clipped);
    uint32_t tapeResponsePointSettleMs(bool firstPoint) const;
    uint32_t tapeResponsePointMeasureMs() const;
    float rmsToReferenceDb(float rms) const;

private:
    enum class CalibrationState : uint8_t
    {
        SelectDeckType,
        SelectTest,
        ThreeHeadLevelReady,
        ThreeHeadLevelRunning,
        ThreeHeadLevelMatched,
        ThreeHeadBiasReady,
        ThreeHeadBiasRunning,
        ThreeHeadBiasMatched,
        TwoHeadLevelReadyToRecord,
        TwoHeadLevelRecording,
        TwoHeadLevelRewind,
        TwoHeadLevelReadyToAnalyze,
        TwoHeadLevelAnalyzing,
        TwoHeadLevelResult,
        TwoHeadLevelMatched,
        TwoHeadBiasReadyToRecord,
        TwoHeadBiasRecording,
        TwoHeadBiasRewind,
        TwoHeadBiasReadyToAnalyze,
        TwoHeadBiasAnalyzing,
        TwoHeadBiasResult,
        TwoHeadBiasMatched,
        ThreeHeadLoopbackReady,
        ThreeHeadLoopbackPreflightNoise,
        ThreeHeadLoopbackPreflightSignal,
        ThreeHeadLoopbackPreflightPassed,
        ThreeHeadLoopbackPreflightFailed,
        ThreeHeadLoopbackSignalLost,
        ThreeHeadLoopbackRunning,
        ThreeHeadLoopbackReview,
        ThreeHeadLoopbackVerifying,
        ThreeHeadLoopbackSavePrompt,
        ThreeHeadLoopbackResult,
        ThreeHeadResponseCalibrationRequired,
        ThreeHeadResponseReady,
        ThreeHeadResponseRunning,
        ThreeHeadResponseResult,
        TwoHeadResponseReadyToRecord,
        TwoHeadResponseRecording,
        TwoHeadResponseRewind,
        TwoHeadResponseReadyToAnalyze,
        TwoHeadResponseWaitingSync,
        TwoHeadResponseAnalyzing,
        TwoHeadResponseResult,
        // Guided 3 HEAD workflow screens.  The individual tests keep running in
        // their own states; these two only cover the wizard's own pages.
        ThreeHeadWorkflowIntro,
        ThreeHeadWorkflowSummary
    };

    enum class DeckType : uint8_t
    {
        None,
        ThreeHead,
        TwoHead
    };
    enum class TestType : uint8_t
    {
        None,
        RecordLevel,
        BiasCalibration,
        TapeResponse
    };
    enum class TwoHeadTone : uint8_t
    {
        None,
        Low440,
        High10k
    };
    enum class TapeResponseVerifyRating : uint8_t
    {
        None,
        Excellent,
        Ok,
        Failed
    };
    enum class BiasSequenceSegment : uint8_t
    {
        Sync1,
        TestLow440,
        Sync2,
        TestHigh10k,
        SyncFinal
    };
    enum class TwoHeadBiasPlaybackState : uint8_t
    {
        WaitSync1,
        Measure440,
        WaitSync2,
        Measure10k,
        WaitSync3,
        ShowResult
    };
    enum class TwoHeadEqRecordPhase : uint8_t
    {
        Sync,
        Tone,
        Gap
    };

    // Orchestration layer of the guided 3 HEAD calibration.  It only decides
    // which of the existing tests runs next; the tests themselves are untouched.
    enum class ThreeHeadWorkflowStep : uint8_t
    {
        Inactive,
        CalibrationRequired,
        LevelIntro,
        Level,
        BiasIntro,
        Bias,
        ResponseIntro,
        Response,
        Complete
    };

    struct ThreeHeadWorkflowResult
    {
        bool levelDone = false;
        bool levelMatched = false;
        bool levelValid = false;
        float levelLeftDb = 0.0f;
        float levelRightDb = 0.0f;
        bool biasDone = false;
        bool biasMatched = false;
        bool biasValid = false;
        float biasDiffLeftDb = 0.0f;
        float biasDiffRightDb = 0.0f;
        bool responseDone = false;
        bool responseValid = false;
        float responseMaxDeviationDb = NAN;
    };

    enum class Status : uint8_t
    {
        Ready,
        AdjustRecLevel,
        LevelMatched,
        NoSignal,
        BiasMeasuring,
        AdjustBias,
        BiasMatched,
        Analyzing,
        WaitingFor440,
        WaitingFor10k,
        Capturing440,
        Capturing10k,
        WaitingForSync,
        SyncFound,
        CapturingLow,
        CapturingHigh,
        InvalidTestLevel,
        LevelTooHigh,
        LevelTooLow,
        DecreaseBias,
        IncreaseBias,
        ChannelMismatch,
        BiasBorderline,
        CalibrationPassed,
        CalibrationCompleted
    };

    static constexpr uint16_t SAMPLE_COUNT = 512;

    void resetSession();
    void resetFinalResults();
    void captureFinalRecordLevel();
    void captureFinalBias();
    void resetMeasurements();
    void resetBiasMatchCounters();
    void resetBiasAverages();
    float getGeneratorCalibrationCorrectionDb(float frequencyHz) const;
    void resetLevelTest();
    void resetBiasTest();
    bool isGeneratorRunning() const;
    bool isLevelState() const;
    bool isBiasState() const;
    bool isTapeResponseState() const;
    bool isTwoHeadState() const;
    bool isTwoHeadBiasState() const;
    bool isThreeHeadState() const;

    void drawScreen();
    void drawRecordStaticContent();
    void drawBiasStaticContent();
    void drawCompletionScreen();
    void drawDeckTypeScreen();
    void drawTestScreen();
    void drawRecordHeadIcons();
    void drawGraphMemoryList();
    void drawMemorySavePrompt();
    void showTapeEqMemory(const GraphMemorySlot &slot);
    bool promptAndSaveTapeEq();
    void showMemoryFull();
    void drawRecordActionIcons();
    uint8_t recordActionIconCount() const;
    void recordActionIconPosition(uint8_t index, int &x, int &y) const;
    void drawRecordIconSelection(uint8_t index, bool selected, bool headMenu);
    int handleRecordIconTouch(uint16_t x, uint16_t y, bool headMenu);
    void startTapeTest();
    void drawDeckTypeMenuItem(int y, const char *title, bool selected);
    void drawTestMenuItem(int y, const char *title, bool selected);
    void drawTapeResponseScreen();
    void drawTapeResponseCalibrationRequiredScreen();
    void drawLoopbackReadyScreen();
    void drawLoopbackRunningScreen();
    void drawLoopbackPreflightScreen();
    void drawLoopbackPreflightFailedScreen();
    void drawLoopbackSavePromptScreen();
    void drawLoopbackPhase1ReviewScreen();
    void drawLoopbackMeasuredValues(int firstY);
    void drawLoopbackRunningDynamic(bool force = false);
    void drawLoopbackResultScreen();
    void drawLoopbackResultActionIcons();
    void drawTapeResponseChoiceButton(int y, const char *label);
    void drawTapeResponseReadyScreen();
    void drawTapeResponseRunningScreen();
    void drawTapeResponseRunningDynamic(bool force = false);
    void drawTapeResponseResultScreen(bool liveAnalysis = false);
    void drawTapeResponseResultRetestButton(bool pressed = false);
    void drawTapeResponseToggleViewButton();
    void drawTapeResponseGraph(bool liveAnalysis = false);
    void drawTapeResponseLiveBand(uint8_t bandIndex);
    void drawTwoHeadTapeResponseScreen();
    void drawTwoHeadTapeResponseDynamic(bool force = false);
    void drawTwoHeadScreen();
    void drawTwoHeadDynamic(bool force = false);
    void drawActionButton(bool pressed = false);
    void drawRecordMeasurements(bool force = false);
    void drawBiasMeasurements(bool force = false);
    void drawVuMeters(bool force = false);
    void drawActiveFrequency(bool force = false);
    void drawThreeHeadCalibrationStatus(bool force = false);
    void drawStatus(bool force = false);

    bool measureLevels(bool smooth, bool captureFrequency = false);
    bool handleTapeResponseTouch(
        uint16_t x,
        uint16_t y,
        bool returnToSystemSettingsOnBack);
    void resetTapeResponse();
    void resetTapeResponseCalibration();
    void startLoopbackCalibration(bool verification = false);
    void startLoopbackSweep(bool verification);
    void stopLoopbackCalibration();
    void updateLoopbackCalibration();
    void updateLoopbackPreflight();
    void finishLoopbackBand();
    void failLoopbackSignal();
    bool calculateLoopbackCorrections();
    bool refineLoopbackCorrections();
    bool validateLoopbackVerification();
    void applyLoopbackCandidateCalibration();
    bool saveActiveGeneratorCalibration();
    void startTapeResponse();
    bool startTapeResponseSweep();
    void updateTapeResponsePrecheck();
    void failTapeResponseSignal();
    void stopTapeResponse();
    void updateTapeResponse();
    void finishTapeResponseBand();
    void storeTapeResponseBand(uint8_t bandIndex);
    void normalizeTapeResponse();
    void processBiasMeasurement();
    void updateRecordLevel();
    void storeBiasMeasurement();
    void updateBiasMatchStatus();
    void startBias();
    void stopBias();
    void startThreeHeadLevel();
    void stopThreeHeadLevel();
    bool setBiasFrequency(bool highFrequency);
    void captureFinalThreeHeadBias();
    void completeCalibration();
    void stopGenerator();
    void startTwoHeadLevelRecording();
    void startTwoHeadLevelAnalysis();
    void startTwoHeadBiasRecording();
    void startTwoHeadBiasAnalysis();
    void updateTwoHeadState();
    void updateTwoHeadLevelAnalysis();
    void updateTwoHeadBiasAnalysis();
    void handleTwoHeadAction();
    void enterSelectedTest();
    void returnToTestSelection();
    void startThreeHeadWorkflow();
    void cancelThreeHeadWorkflow();
    void advanceThreeHeadWorkflow();
    void enterWorkflowLevelStep();
    void enterWorkflowBiasStep();
    void enterWorkflowResponseStep();
    void captureWorkflowLevelResult();
    void captureWorkflowBiasResult();
    void captureWorkflowResponseResult();
    bool handleWorkflowTouch(uint16_t x, uint16_t y);
    void drawWorkflowIntroScreen();
    void drawWorkflowCalibrationRequiredScreen();
    void drawWorkflowCompleteScreen();
    void drawWorkflowSummaryLine(int y, const char *label, const char *value,
                                 bool done, bool passed);
    void drawWorkflowButton(int x, int y, const char *label);
    void resetTwoHeadAnalysis();
    void finalizeTwoHeadLevelAnalysis();
    void finalizeTwoHeadBiasAnalysis();
    bool setTwoHeadBiasSegment(BiasSequenceSegment segment);
    void advanceTwoHeadBiasSequence();
    void resetTwoHeadTapeResponse();
    void startTwoHeadTapeResponseRecording();
    void updateTwoHeadTapeResponseRecording();
    void startTwoHeadTapeResponseAnalysis();
    void updateTwoHeadTapeResponseAnalysis();
    void finishTwoHeadTapeResponseBand();
    void handleTwoHeadTapeResponseAction();
    void showHelp();

    CalibrationState state = CalibrationState::SelectDeckType;
    DeckType deckType = DeckType::None;
    DeckType highlightedDeckType = DeckType::None;
    TestType highlightedTestType = TestType::None;
    uint8_t selectedRecordHeadIndex = 0;
    uint8_t selectedRecordActionIndex = 0;
    Status status = Status::Ready;
    Status displayedStatus = Status::Ready;
    bool calibrationCompleted = false;
    bool statusDisplayed = false;

    TapeResponseResult tapeResponseResult;
    bool tapeResponseCombinedView = false;
    TapeResponseCalibration tapeResponseCalibration;
    TapeResponseCalibration loopbackCandidateCalibration;
    uint8_t tapeResponseBandIndex = 0;
    uint32_t tapeResponseBandStartedMs = 0;
    float tapeResponseSumLeftRms = 0.0f;
    float tapeResponseSumRightRms = 0.0f;
    uint16_t tapeResponseLeftWindowCount = 0;
    uint16_t tapeResponseRightWindowCount = 0;
    float tapeResponseLiveLeftDb = 0.0f;
    float tapeResponseLiveRightDb = 0.0f;
    bool tapeResponseLiveLeftValid = false;
    bool tapeResponseLiveRightValid = false;
    bool tapeResponseBandClipped = false;
    bool tapeResponsePrecheckActive = false;
    uint32_t tapeResponsePrecheckStartedMs = 0;
    float tapeResponsePrecheckSumLeftRms = 0.0f;
    float tapeResponsePrecheckSumRightRms = 0.0f;
    uint16_t tapeResponsePrecheckLeftWindowCount = 0;
    uint16_t tapeResponsePrecheckRightWindowCount = 0;
    uint16_t tapeResponsePrecheckLeftToneCount = 0;
    uint16_t tapeResponsePrecheckRightToneCount = 0;
    bool tapeResponsePrecheckClipped = false;
    bool tapeResponseSignalLost = false;
    float loopbackPreflightSumLeftRms = 0.0f;
    float loopbackPreflightSumRightRms = 0.0f;
    uint16_t loopbackPreflightWindowCount = 0;
    float loopbackPreflightNoiseLeftDb = NAN;
    float loopbackPreflightNoiseRightDb = NAN;
    float loopbackPreflightSignalLeftDb = NAN;
    float loopbackPreflightSignalRightDb = NAN;
    bool loopbackPreflightClipped = false;
    uint16_t loopbackPreflightLeftToneCount = 0;
    uint16_t loopbackPreflightRightToneCount = 0;
    bool loopbackPreflightPassed = false;
    bool loopbackAllBandsValid = true;
    uint8_t loopbackFailedBand = 0;
    bool tapeResponseCalibrationFailed = false;
    bool tapeResponseVerificationFailed = false;
    float tapeResponseMaxCorrectionDb = 0.0f;
    uint8_t tapeResponseVerifyPass = 0;
    float tapeResponseMaxVerifyErrorDb = NAN;
    uint8_t tapeResponseMaxVerifyErrorBand = 0;
    TapeResponseVerifyRating tapeResponseVerifyRating =
        TapeResponseVerifyRating::None;
    bool generatorCalibrationLoaded = false;
    float activeGeneratorCalibrationMaxVerifyErrorDb = NAN;
    TapeResponseVerifyRating activeGeneratorCalibrationVerifyRating =
        TapeResponseVerifyRating::None;
    bool generatorCalibrationSaved = false;
    bool generatorCalibrationSaveFailed = false;
    bool loopbackCalibrationNotSaved = false;
    uint8_t displayedTapeResponseBand = 0xFF;
    int16_t displayedTapeResponseLeftTenths = -32768;
    int16_t displayedTapeResponseRightTenths = -32768;
    bool displayedTapeResponseLeftValid = false;
    bool displayedTapeResponseRightValid = false;
    uint8_t displayedLoopbackProgress[3] = {0xFF, 0xFF, 0xFF};

    struct RecordSession
    {
        bool levelTestDone = false;
        bool levelMatched = false;
        bool biasTestDone = false;
        bool biasMatched = false;
        bool responseTestDone = false;
    } session;

    float finalRecordLevelLeftDb = 0.0f;
    float finalRecordLevelRightDb = 0.0f;
    float finalRecordLevelDifferenceDb = 0.0f;
    float finalBiasDiffLeftDb = 0.0f;
    float finalBiasDiffRightDb = 0.0f;
    bool finalRecordLevelValid = false;
    bool finalBiasValid = false;
    DeckType finalDeckType = DeckType::None;
    bool openAutoCalOnRun = false;
    bool tapeEqFlowActive = false;

    ThreeHeadWorkflowStep workflowStep = ThreeHeadWorkflowStep::Inactive;
    ThreeHeadWorkflowResult workflowResult;
    bool workflowActive = false;
    // A step re-run from the summary returns to the summary instead of
    // continuing with the remaining steps.
    bool workflowRetestOnly = false;
    bool openWorkflowOnRun = false;

    TwoHeadEqRecordPhase twoHeadEqRecordPhase = TwoHeadEqRecordPhase::Sync;
    uint32_t twoHeadEqSegmentStartedMs = 0;
    uint32_t twoHeadEqAnalysisStartedMs = 0;
    uint32_t twoHeadBiasRecordingStartedMs = 0;
    bool twoHeadEqSyncFound = false;
    uint8_t twoHeadEqSyncDetections = 0;
    uint8_t twoHeadEqSyncExitDetections = 0;
    bool twoHeadEqAllBandsValid = true;
    bool twoHeadEqNoSync = false;

    bool leftSignalValid = false;
    bool rightSignalValid = false;
    bool smoothingInitialized = false;
    float leftLevelDb = 0.0f;
    float rightLevelDb = 0.0f;
    float smoothedLeftDb = 0.0f;
    float smoothedRightDb = 0.0f;
    float lastLeftRms = 0.0f;
    float lastRightRms = 0.0f;
    float lastFrequencyHz = 0.0f;
    float lastAmplifiedFrequencyHz = 0.0f;
    int16_t frequencySamples[SAMPLE_COUNT] = {};
    int16_t amplifiedFrequencySamples[SAMPLE_COUNT] = {};

    uint8_t biasMatchEntryCount = 0;
    uint8_t biasMatchExitCount = 0;
    uint8_t biasCompletedCycles = 0;
    uint8_t recordMatchEntryCount = 0;
    uint8_t recordMatchExitCount = 0;

    bool biasHighFrequency = false;
    uint32_t biasFrequencySetMs = 0;
    float biasSegmentSumLeftRms = 0.0f;
    float biasSegmentSumRightRms = 0.0f;
    uint16_t biasSegmentMeasurementCount = 0;
    bool bias440Valid = false;
    bool bias10kValid = false;
    bool biasTestLevelValid = false;
    float bias440LeftDb = 0.0f;
    float bias440RightDb = 0.0f;
    float bias10kLeftDb = 0.0f;
    float bias10kRightDb = 0.0f;
    float bias440LeftRms[3] = {};
    float bias440RightRms[3] = {};
    float bias10kLeftRms[3] = {};
    float bias10kRightRms[3] = {};
    uint8_t bias440SampleCount = 0;
    uint8_t bias10kSampleCount = 0;
    uint8_t bias440SampleIndex = 0;
    uint8_t bias10kSampleIndex = 0;

    uint32_t twoHeadStateStartedMs = 0;
    uint32_t twoHeadLastMeasurementMs = 0;
    uint32_t twoHeadValidMeasurementMs = 0;
    uint32_t twoHeadToneStableMs = 0;
    uint32_t twoHeadToneLostMs = 0;
    uint32_t twoHeadToneValidMs = 0;
    uint32_t toneCaptureStartedMs = 0;
    uint32_t toneCaptureWindowStartedMs = 0;
    uint32_t accumulatedValidMs = 0;
    bool waitingForPlaybackTone = true;
    uint8_t consecutiveToneDetections = 0;
    TwoHeadTone expectedTwoHeadTone = TwoHeadTone::Low440;
    uint8_t twoHeadBiasCycle = 0;
    BiasSequenceSegment twoHeadBiasSegment = BiasSequenceSegment::Sync1;
    TwoHeadBiasPlaybackState twoHeadBiasPlaybackState = TwoHeadBiasPlaybackState::WaitSync1;
    uint32_t twoHeadBiasPostSyncStartedMs = 0;
    bool twoHeadBiasAwaitingToneStart = true;
    TwoHeadTone twoHeadDetectedTone = TwoHeadTone::None;
    bool twoHeadToneCaptured = false;
    bool twoHeadSequenceFound = false;
    bool twoHeadResultMatched = false;
    float twoHeadSumLeftRms = 0.0f;
    float twoHeadSumRightRms = 0.0f;
    uint16_t twoHeadRmsMeasurementCount = 0;
    float twoHead440LeftRms[5] = {};
    float twoHead440RightRms[5] = {};
    float twoHead10kLeftRms[5] = {};
    float twoHead10kRightRms[5] = {};
    uint8_t twoHead440Count = 0;
    uint8_t twoHead10kCount = 0;
    bool waitingForBiasSync = true;
    uint8_t biasSyncDetections = 0;
    float biasSyncCandidateLeftRms = 0.0f;
    float biasSyncCandidateRightRms = 0.0f;
    uint16_t biasSyncCandidateMeasurements = 0;
    uint32_t biasPlaybackCycleStartedMs = 0;
    uint32_t biasLowValidMs = 0;
    uint32_t biasHighValidMs = 0;
    float biasCycleLowLeftRms = 0.0f;
    float biasCycleLowRightRms = 0.0f;
    float biasCycleHighLeftRms = 0.0f;
    float biasCycleHighRightRms = 0.0f;
    uint16_t biasCycleLowMeasurements = 0;
    uint16_t biasCycleHighMeasurements = 0;

    int16_t displayedLeftTenths = -32768;
    int16_t displayedRightTenths = -32768;
    int16_t displayedDiffLeftTenths = -32768;
    int16_t displayedDiffRightTenths = -32768;
    int displayedLeftVuPixels = -1;
    int displayedRightVuPixels = -1;
    bool displayedLeftValid = false;
    bool displayedRightValid = false;

    int16_t displayedBias440LeftTenths = -32768;
    int16_t displayedBias440RightTenths = -32768;
    int16_t displayedBias10kLeftTenths = -32768;
    int16_t displayedBias10kRightTenths = -32768;
    int16_t displayedBiasDiffLeftTenths = -32768;
    int16_t displayedBiasDiffRightTenths = -32768;
    bool displayedBias440LeftValid = false;
    bool displayedBias440RightValid = false;
    bool displayedBias10kLeftValid = false;
    bool displayedBias10kRightValid = false;
    bool displayedBiasDiffLeftValid = false;
    bool displayedBiasDiffRightValid = false;
    int8_t displayedActiveFrequency = -1;
    int8_t displayedLevelStatus = -1;
    int8_t displayedBiasStatus = -1;
    int8_t displayedStableCycles = -1;
    int8_t displayedActionButtonMode = -1;
};

extern Record record;

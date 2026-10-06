/***************************************************************************

                            TapeLAB

A professional cassette deck laboratory.

Designed for restoring, calibrating and understanding
analog cassette technology.

The goal is not only to measure,
but to understand what the tape deck is doing.

Accuracy first.
Clear information.
Professional tools.
No unnecessary complexity.

***************************************************************************/
#include "Display.h"
#include "TouchCal.h"
#include "Touch.h"
#include "Storage.h"
#include "Playback.h"
#include "Record.h"
#include "Rtc.h"
#include "Audio.h"
#include "Spec.h"
#include "Sistem.h"
#include "SetAudio.h"
#include "Transport.h"
#include "Wave.h"
#include "Sine.h"
#include "Config.h"
#include "Buzzer.h"
#include "AD9833.h"
#include "Pot.h"
#include "Icons.h"
#include <TFT_eSPI.h>
#include <math.h>
#include <string.h>

uint16_t currentBackgroundColor = TFT_BLACK;

extern Display display;
extern Touch touch;
extern TouchCal touchCal;
extern Spectrum spectrum;
extern TFT_eSPI tft;
extern Icons icons;

namespace
{
    constexpr uint32_t SPLASH_MINIMUM_DISPLAY_MS = 2200;

    void showServiceDiagnostics(const bool results[7])
    {
        static const char *const labels[7] = {
            "TOUCH", "ADC", "AUDIO", "GENERATOR",
            "CALIBRATION", "EEPROM", "CRC"};

        tft.fillScreen(TFT_BLACK);
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawString("SERVICE DIAGNOSTICS", 4, 2, 2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        for (uint8_t i = 0; i < 7; ++i)
        {
            const int y = 27 + i * 28;
            tft.drawString(labels[i], 4, y, 2);
            const bool generatorTest = i == 3;
            tft.setTextColor(
                results[i] ? TFT_GREEN : TFT_RED,
                TFT_BLACK);
            tft.drawRightString(
                results[i] ? "PASS" : (generatorTest ? "NOT PASS" : "FAIL"),
                316, y, 2);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
        }
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawCentreString("TOUCH TO EXIT", LCD_WIDTH / 2, 218, 2);

        touch.waitAnyTouch();
        while (!touch.pressed(false))
            delay(5);
        touch.waitAnyTouch();
        buzzer.beepNow(20);
    }

    bool showServicePreparationScreen(
        bool adcInitialized,
        bool initialAdcResult)
    {
        constexpr int RESET_BUTTON_X = (LCD_WIDTH - 78) / 2;
        constexpr int RESET_BUTTON_Y = 75;
        constexpr int RESET_BUTTON_W = 78;
        constexpr int RESET_BUTTON_H = 40;
        bool adcResult = initialAdcResult;
        bool resetAttempted = false;

        while (true)
        {
            tft.fillScreen(TFT_BLACK);
            tft.setTextDatum(TL_DATUM);
            tft.setTextColor(TFT_CYAN, TFT_BLACK);
            tft.drawString("SERVICE DIAGNOSTICS", 4, 2, 2);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawCentreString("PRESET VALUES", LCD_WIDTH / 2, 35, 2);
            sine.drawServiceButton(
                RESET_BUTTON_X, RESET_BUTTON_Y, "ADC", "RESET");
            if (resetAttempted)
            {
                tft.setTextColor(adcResult ? TFT_GREEN : TFT_RED, TFT_BLACK);
                tft.drawCentreString(
                    adcResult ? "ADC RESET PASS" : "ADC RESET FAIL",
                    LCD_WIDTH / 2,
                    124,
                    2);
            }
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawCentreString("CONNECT OUT -> IN", LCD_WIDTH / 2, 151, 2);
            tft.setTextColor(TFT_CYAN, TFT_BLACK);
            tft.drawCentreString("TOUCH TO CONTINUE", LCD_WIDTH / 2, 218, 2);

            touch.waitAnyTouch();
            while (!touch.pressed(false))
                delay(5);
            const uint16_t x = touch.getX();
            const uint16_t y = touch.getY();
            touch.waitAnyTouch();
            buzzer.beepNow(20);

            if (x >= RESET_BUTTON_X && x < RESET_BUTTON_X + RESET_BUTTON_W &&
                y >= RESET_BUTTON_Y && y < RESET_BUTTON_Y + RESET_BUTTON_H)
            {
                resetAttempted = true;
                adcResult = adcInitialized && audio.calibrate();
                continue;
            }
            if (y >= FOOTER_Y)
                return adcResult;
        }
    }

    struct ServiceMeasurement
    {
        uint16_t adcLeft;
        uint16_t adcRight;
        float leftDb;
        float rightDb;
        float leftFrequencyHz;
        float rightFrequencyHz;
        bool leftValid;
        bool rightValid;
        bool clipped;
    };

    ServiceMeasurement measureServiceInput()
    {
        ServiceMeasurement measurement = {};
        measurement.adcLeft = audio.readLeft();
        measurement.adcRight = audio.readRight();

        float leftRms = 0.0f;
        float rightRms = 0.0f;
        record.measureTapeResponseToneWindow(
            leftRms,
            rightRms,
            measurement.leftFrequencyHz,
            measurement.rightFrequencyHz,
            measurement.leftValid,
            measurement.rightValid,
            measurement.clipped);
        measurement.leftDb = measurement.leftValid
                                 ? record.rmsToReferenceDb(leftRms)
                                 : NAN;
        measurement.rightDb = measurement.rightValid
                                  ? record.rmsToReferenceDb(rightRms)
                                  : NAN;
        return measurement;
    }

    float serviceLevelDifference(const ServiceMeasurement &measurement)
    {
        return measurement.leftValid && measurement.rightValid
                   ? fabsf(measurement.leftDb - measurement.rightDb)
                   : NAN;
    }

    bool serviceToneDetected(
        const ServiceMeasurement &measurement,
        uint32_t expectedFrequencyHz)
    {
        const float toleranceHz = expectedFrequencyHz * 0.05f;
        return !measurement.clipped &&
               measurement.leftValid && measurement.rightValid &&
               fabsf(measurement.leftFrequencyHz - expectedFrequencyHz) <= toleranceHz &&
               fabsf(measurement.rightFrequencyHz - expectedFrequencyHz) <= toleranceHz;
    }

    struct ServiceDisplayValue
    {
        bool initialized = false;
        bool valid = false;
        int32_t displayedValue = 0;
        uint16_t color = TFT_CYAN;
        char text[20] = {};
    };

    void drawServiceStaticLabel(const char *label, int y)
    {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(label, 4, y, 2);
    }

    void drawServiceStaticPair(const char *label, const char *unit, int y)
    {
        drawServiceStaticLabel(label, y);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("/", 194, y, 2);
        if (unit[0])
        {
            tft.drawString(unit, 184, y, 2);
            tft.drawString(unit, 274, y, 2);
        }
    }

    void drawServiceFloat(
        ServiceDisplayValue &state,
        float value,
        bool valid,
        uint8_t decimals,
        int x,
        int y,
        int width,
        const char *invalidText = "N/A")
    {
        if (!isfinite(value))
            valid = false;
        const int32_t scale = decimals ? 10 : 1;
        const int32_t displayedValue = valid
                                           ? static_cast<int32_t>(lroundf(value * scale))
                                           : 0;
        if (state.initialized && state.valid == valid &&
            (!valid || state.displayedValue == displayedValue))
            return;

        state.initialized = true;
        state.valid = valid;
        state.displayedValue = displayedValue;
        tft.fillRect(x, y, width, 18, TFT_BLACK);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        if (valid)
            tft.drawFloat(
                static_cast<float>(displayedValue) / scale,
                decimals, x, y, 2);
        else
            tft.drawString(invalidText, x, y, 2);
    }

    void drawServiceText(
        ServiceDisplayValue &state,
        const char *value,
        uint16_t color,
        int x,
        int y,
        int width)
    {
        if (state.initialized && state.color == color &&
            strcmp(state.text, value) == 0)
            return;

        state.initialized = true;
        state.color = color;
        strncpy(state.text, value, sizeof(state.text) - 1);
        state.text[sizeof(state.text) - 1] = '\0';
        tft.fillRect(x, y, width, 18, TFT_BLACK);
        tft.setTextColor(color, TFT_BLACK);
        tft.drawString(value, x, y, 2);
    }

    bool showServiceToneTest(
        const char *title,
        uint32_t frequencyHz,
        bool generatorTest)
    {
        constexpr int BUTTON_X = 240;
        constexpr int BUTTON_Y = 29;
        constexpr int BUTTON_W = 78;
        constexpr int BUTTON_H = 40;
        bool generatorOn = record.setCalibratedTestTone(frequencyHz, -10.0f);
        bool passed = false;
        tft.fillScreen(TFT_BLACK);
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawString("SERVICE DIAGNOSTICS", 4, 2, 2);
        tft.drawCentreString(title, LCD_WIDTH / 2, 22, 2);
        tft.drawCentreString("TOUCH TO CONTINUE", LCD_WIDTH / 2, 218, 2);

        const int rowStep = generatorTest ? 20 : 17;
        const int firstRow = generatorTest ? 44 : 40;
        const int genFreqY = firstRow;
        const int genLevelY = genFreqY + rowStep;
        const int adcY = genLevelY + rowStep;
        const int audioY = adcY + rowStep;
        const int freqLrY = audioY + rowStep;
        const int peakFreqY = freqLrY + rowStep;
        const int noiseY = peakFreqY + rowStep;
        const int differenceY = generatorTest ? 144 : noiseY + rowStep;
        const int signalY = generatorTest ? 164 : differenceY + rowStep;
        const int resultY = generatorTest ? 184 : signalY + rowStep;

        drawServiceStaticLabel("GEN FREQ:", genFreqY);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawNumber(frequencyHz, 150, genFreqY, 2);
        tft.drawString("Hz", 194, genFreqY, 2);
        drawServiceStaticLabel("GEN LEVEL:", genLevelY);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawString("-10.0 dB", 150, genLevelY, 2);

        drawServiceStaticPair("ADC L/R:", "", adcY);
        drawServiceStaticPair("AUDIO L/R:", "dB", audioY);
        drawServiceStaticPair("FREQ L/R:", "Hz", freqLrY);
        if (!generatorTest)
        {
            drawServiceStaticLabel("NOISE FREQ:", peakFreqY);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawString("Hz", 218, peakFreqY, 2);
            drawServiceStaticLabel("NOISE LEVEL:", noiseY);
            tft.drawString("dB", 210, noiseY, 2);
        }
        drawServiceStaticLabel("L-R:", differenceY);
        tft.drawString("dB", 194, differenceY, 2);
        drawServiceStaticLabel("SIGNAL:", signalY);
        drawServiceStaticLabel(
            generatorTest ? "GENERATOR:" : "AUDIO:", resultY);

        if (!generatorTest)
            sine.drawServiceButton(
                BUTTON_X, BUTTON_Y, "GEN", "ON/OFF", generatorOn);

        ServiceDisplayValue adcLeftDisplay;
        ServiceDisplayValue adcRightDisplay;
        ServiceDisplayValue audioLeftDisplay;
        ServiceDisplayValue audioRightDisplay;
        ServiceDisplayValue frequencyLeftDisplay;
        ServiceDisplayValue frequencyRightDisplay;
        ServiceDisplayValue noiseFrequencyDisplay;
        ServiceDisplayValue noiseLevelDisplay;
        ServiceDisplayValue differenceDisplay;
        ServiceDisplayValue signalDisplay;
        ServiceDisplayValue resultDisplay;

        while (true)
        {
            ServiceMeasurement measurement = measureServiceInput();
            float fftPeakFrequencyHz = 0.0f;
            float fftPeakLevelDb = 0.0f;
            const bool fftPeakValid = !generatorTest &&
                                      spectrum.updateDiagnosticPeak(
                                          fftPeakFrequencyHz,
                                          fftPeakLevelDb);
            const bool detected = generatorOn &&
                                  serviceToneDetected(measurement, frequencyHz);
            passed = passed || detected;

            drawServiceFloat(
                adcLeftDisplay, measurement.adcLeft, true, 0,
                145, adcY, 46);
            drawServiceFloat(
                adcRightDisplay, measurement.adcRight, true, 0,
                218, adcY, 52);
            drawServiceFloat(
                audioLeftDisplay, measurement.leftDb, measurement.leftValid, 1,
                145, audioY, 39);
            drawServiceFloat(
                audioRightDisplay, measurement.rightDb, measurement.rightValid, 1,
                218, audioY, 52);
            drawServiceFloat(
                frequencyLeftDisplay, measurement.leftFrequencyHz,
                measurement.leftValid, 0, 145, freqLrY, 39);
            drawServiceFloat(
                frequencyRightDisplay, measurement.rightFrequencyHz,
                measurement.rightValid, 0, 218, freqLrY, 52);

            if (!generatorTest)
            {
                drawServiceFloat(
                    noiseFrequencyDisplay, fftPeakFrequencyHz, fftPeakValid, 0,
                    150, peakFreqY, 64, "----");
                drawServiceFloat(
                    noiseLevelDisplay, fftPeakLevelDb, fftPeakValid, 1,
                    150, noiseY, 54, "N/A");
            }

            const float differenceDb = serviceLevelDifference(measurement);
            drawServiceFloat(
                differenceDisplay, differenceDb, isfinite(differenceDb), 1,
                150, differenceY, 40);

            const uint16_t signalColor = detected ? TFT_GREEN : TFT_RED;
            drawServiceText(
                signalDisplay, detected ? "DETECTED" : "NOT DETECTED",
                signalColor, 150, signalY, 170);
            drawServiceText(
                resultDisplay, passed ? "PASS" : "FAIL",
                passed ? TFT_GREEN : TFT_RED, 150, resultY, 80);

            if (touch.pressed(false))
            {
                const uint16_t x = touch.getX();
                const uint16_t y = touch.getY();
                touch.waitAnyTouch();
                buzzer.beepNow(20);
                if (!generatorTest && x >= BUTTON_X &&
                    x < BUTTON_X + BUTTON_W && y >= BUTTON_Y &&
                    y < BUTTON_Y + BUTTON_H)
                {
                    if (generatorOn)
                    {
                        record.stopCalibratedTestTone();
                        generatorOn = false;
                    }
                    else
                        generatorOn = record.setCalibratedTestTone(
                            frequencyHz, -10.0f);
                    sine.drawServiceButton(
                        BUTTON_X, BUTTON_Y, "GEN", "ON/OFF", generatorOn);
                    continue;
                }
                record.stopCalibratedTestTone();
                return passed;
            }
        }
    }

    void runServiceDiagnosticsPhase2(
        bool adcInitialized,
        bool &adcPassed,
        bool &touchPassed,
        bool &audioPassed,
        bool &generatorPassed)
    {
        const bool initialAdcResult = adcPassed;
        adcPassed = showServicePreparationScreen(
            adcInitialized, initialAdcResult);
        TouchCalibration candidateCalibration;
        touchPassed = touchCal.run(candidateCalibration, true);
        audioPassed = showServiceToneTest("AUDIO TEST", 1000, false);
        generatorPassed = showServiceToneTest("GENERATOR TEST", 3150, true);
        record.stopCalibratedTestTone();
    }

    bool showGeneratorCalibrationWarning(GeneratorCalibrationBootStatus status)
    {
        if (status == GeneratorCalibrationBootStatus::Ok)
            return false;

        constexpr int BUTTON_Y = 158;
        constexpr int BUTTON_W = 108;
        constexpr int BUTTON_H = 30;
        constexpr int CALIBRATE_X = 42;
        constexpr int CONTINUE_X = 170;
        const bool notCalibrated =
            status == GeneratorCalibrationBootStatus::NotCalibrated;
        const uint16_t color = notCalibrated ? TFT_YELLOW : TFT_RED;

        display.openApp("GENERATOR CAL", notCalibrated ? "NOT CAL" : "FAIL");
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(color, COL_BG);
        tft.drawString(
            notCalibrated ? "GENERATOR NOT CALIBRATED" : "GENERATOR CAL DATA ERROR",
            20, 42, 2);
        tft.setTextColor(notCalibrated ? COL_TEXT : TFT_RED, COL_BG);
        if (notCalibrated)
        {
            tft.drawString("NO VALID GEN CAL", 20, 72, 2);
            tft.drawString("ACCURACY MAY BE REDUCED", 20, 94, 2);
            tft.drawString("RUN AUTO CAL NOW?", 20, 120, 2);
        }
        else
        {
            tft.drawString("GEN CAL INVALID", 20, 72, 2);
            tft.drawString("RUN AUTO CAL AGAIN", 20, 98, 2);
        }

        const auto drawButton = [](const char *text, int x, uint16_t buttonColor)
        {
            tft.fillRect(x, BUTTON_Y, BUTTON_W, BUTTON_H, COL_BG);
            tft.drawRect(x, BUTTON_Y, BUTTON_W, BUTTON_H, buttonColor);
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(buttonColor, COL_BG);
            tft.drawString(text, x + BUTTON_W / 2, BUTTON_Y + BUTTON_H / 2, 2);
            tft.setTextDatum(TL_DATUM);
        };
        drawButton("CALIBRATE", CALIBRATE_X, color);
        drawButton("CONTINUE", CONTINUE_X, TFT_CYAN);

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
            const bool calibrate = x >= CALIBRATE_X &&
                                   x < CALIBRATE_X + BUTTON_W && y >= BUTTON_Y &&
                                   y < BUTTON_Y + BUTTON_H;
            const bool proceed = x >= CONTINUE_X && x < CONTINUE_X + BUTTON_W &&
                                 y >= BUTTON_Y && y < BUTTON_Y + BUTTON_H;
            while (touch.pressed())
                delay(5);
            if (calibrate)
                return true;
            if (proceed)
                return false;
        }
    }
}

Playback playback;
Display display;
Touch touch;
TouchCal touchCal;
Storage storage;
Spectrum spectrum;
Transport transport;
Waveform waveform;

void setup()
{
    analogReadResolution(12);
    pinMode(PA0, INPUT_ANALOG);
    pinMode(PA1, INPUT_ANALOG);

    display.begin();
    touch.begin();
    const uint32_t splashStartedMs = millis();
    display.splash();
    bool serviceDiagnosticsRequested = touch.pressed(false);
    const auto pollSplashTouch = [&serviceDiagnosticsRequested]()
    {
        serviceDiagnosticsRequested =
            touch.pressed(false) || serviceDiagnosticsRequested;
    };

    storage.begin();
    pollSplashTouch();
    buzzer.begin();

    bool storedBeepEnabled = true;
    if (storage.loadBeepEnabled(storedBeepEnabled))
        buzzer.setEnabled(storedBeepEnabled);
    record.begin();
    pollSplashTouch();
    uint16_t storedBackgroundColor;
    if (storage.loadBackgroundColor(storedBackgroundColor) &&
        (storedBackgroundColor == TFT_BLACK ||
         storedBackgroundColor == TFT_DARKNAVY))
    {
        currentBackgroundColor = storedBackgroundColor;
    }

    ad9833.begin();
    pot.begin();
    pollSplashTouch();

    const bool adcInitialized = audio.begin();
    // Asteapta stabilizarea Audio Engine
    const uint32_t audioStabilizationStartedMs = millis();
    while (millis() - audioStabilizationStartedMs < 500)
    {
        pollSplashTouch();
        delay(1);
    }

    // Executa calibrarea Zero ADC
    const bool adcZeroValid = adcInitialized && audio.calibrate();

    setAudio.begin();

    const bool audioSettingsValid = setAudio.bootCheck();

    const bool audioOK =
        adcInitialized &&
        adcZeroValid &&
        audioSettingsValid;
    pollSplashTouch();

    const bool rtcOK = rtc.begin();
    playback.begin();

    TouchCalibration cal;
    if (storage.loadTouchCalibration(cal))
    {
        touch.setCalibration(cal);
    }
    pollSplashTouch();

    if (!serviceDiagnosticsRequested && touch.checkCalibrationRequest())
    {
        TouchCalibration cal;
        if (touchCal.run(cal))
        {
            if (storage.saveTouchCalibration(cal))
            {
                // mai târziu putem afișa "EEPROM SAVE OK"
            }
            NVIC_SystemReset();
        }
    }

    const BootTestResults bootTests =
        {
            true,
            true,
            storage.isPresent(),
            rtcOK,
            audioOK,
            static_cast<uint8_t>(storage.generatorCalibrationBootStatus())};

    if (!serviceDiagnosticsRequested && bootTests.allPassed() &&
        storage.generatorCalibrationBootStatus() ==
            GeneratorCalibrationBootStatus::Ok)
    {
        while (millis() - splashStartedMs < SPLASH_MINIMUM_DISPLAY_MS)
        {
            pollSplashTouch();
            delay(1);
        }
    }
    else if (!serviceDiagnosticsRequested)
    {
        display.post(bootTests);
    }
    pollSplashTouch();

    if (serviceDiagnosticsRequested)
    {
        const GeneratorCalibrationBootStatus calibrationStatus =
            storage.generatorCalibrationBootStatus();
        const bool generatorReady = calibrationStatus ==
                                    GeneratorCalibrationBootStatus::Ok;
        const bool diagnostics[7] = {
            true,
            adcInitialized && adcZeroValid,
            audioOK,
            // AD9833 has no output readback or feedback path in this hardware.
            // Do not claim a physical generator PASS from command state alone.
            false,
            generatorReady,
            storage.isPresent() && storage.checkReadAccess(),
            generatorReady};
        bool audioLoopbackPassed = false;
        bool generatorLoopbackPassed = false;
        bool adcResetPassed = diagnostics[1];
        bool touchTestPassed = false;
        runServiceDiagnosticsPhase2(
            adcInitialized,
            adcResetPassed,
            touchTestPassed,
            audioLoopbackPassed,
            generatorLoopbackPassed);
        bool finalDiagnostics[7];
        for (uint8_t i = 0; i < 7; ++i)
            finalDiagnostics[i] = diagnostics[i];
        finalDiagnostics[0] = touchTestPassed;
        finalDiagnostics[1] = adcResetPassed;
        finalDiagnostics[2] = audioLoopbackPassed;
        finalDiagnostics[3] = generatorLoopbackPassed;
        showServiceDiagnostics(finalDiagnostics);
    }

    if (showGeneratorCalibrationWarning(
            storage.generatorCalibrationBootStatus()))
    {
        record.openThreeHeadTapeEqAutoCal();
    }

    // display.mainMenu();
    icons.setBackgroundColor(currentBackgroundColor);
    icons.showMainScreen();
    // daca totul este OK la bootare, genereaza sunet Succes
    buzzer.beepTripleNow();
}

void loop()
{
    buzzer.update();
    display.updateClock();

    if (touch.pressed(false))
    {
        const uint16_t touchX = touch.getX();
        const uint16_t touchY = touch.getY();

        const int selectedApp =
            icons.handleMainTouch(touchX, touchY);

        if (selectedApp >= 0)
            buzzer.beepNow(BEEP_TOUCH_MS);

        touch.waitAnyTouch();

        if (selectedApp <= 0)
            return; // No app was entered.

        switch (selectedApp)
        {
        case 1:
            playback.run();
            break;

        case 2:
            record.run();
            break;

        case 3:
            spectrum.run();
            break;

        case 4:
            sine.run();
            break;

        case 5:
            waveform.run();
            break;

        case 6:
            transport.run();
            break;

        case 7:
            sistem.run();
            break;

        case 8:
            // ALIGNMENT: folosește funcționalitatea de azimut din Playback
            playback.runAlignment();
            break;
        }

        // Ajungem aici NUMAI dupa iesirea dintr-o aplicatie
        icons.setBackgroundColor(currentBackgroundColor);
        icons.showMainScreen();
    }
}

#include "Spec.h"
#include "Display.h"
#include "Touch.h"
#include "Config.h"
#include "Buzzer.h"
#include "SetAudio.h"

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <math.h>
#include <string.h>

extern TFT_eSPI tft;
extern Display display;
extern Touch touch;
extern Spectrum spectrum;
extern SetAudio setAudio;
static TFT_eSprite spectrumSprite(&tft);
static TFT_eSprite peakMeasurementSprite(&tft);

namespace
{
    constexpr int SPECTRUM_X = 32;
    constexpr int SPECTRUM_Y = 37;
    constexpr int SPECTRUM_W = FFTAnalyzer::BIN_COUNT;
    constexpr int SPECTRUM_H = 161;
    constexpr int SPECTRUM_BOTTOM = SPECTRUM_H - 1;
    constexpr uint32_t FFT_NYQUIST_HZ =
        FFTAnalyzer::SAMPLE_RATE_HZ / 2;
    constexpr uint32_t FFT_ZOOM_SPAN_HZ = 2000;
    constexpr uint8_t FREQUENCY_TICK_COUNT = 5;
    constexpr float SPECTRUM_MIN_DB = -60.0f;
    constexpr float SPECTRUM_MAX_DB = 10.0f;
    constexpr float HAMMING_RMS_GAIN = 0.62979f;
    constexpr float FFT_TO_RMS = 1.41421356f /
                                 (FFTAnalyzer::SAMPLES * HAMMING_RMS_GAIN);
    constexpr float PEAK_SMOOTHING_ALPHA = 0.25f;
    constexpr int FFT_PEAK_X = SPECTRUM_X;
    constexpr int FFT_PEAK_Y = 27;
    constexpr int FFT_PEAK_W = SPECTRUM_W;
    constexpr int FFT_PEAK_H = 9;
    constexpr uint8_t SPRITE_BACKGROUND = 0;
    constexpr uint8_t SPRITE_FOREGROUND = 1;
    constexpr uint8_t PEAK_TEXT_BACKGROUND = 0;
    constexpr uint8_t PEAK_TEXT_LABEL = 1;
    constexpr uint8_t PEAK_TEXT_VALUE = 2;

    constexpr int8_t DB_TICKS[] = {-60, -40, -20, 0, 10};

    int frequencyTickToSpriteX(uint8_t tick)
    {
        return static_cast<int>(
            (static_cast<uint32_t>(SPECTRUM_W - 1) * tick +
             (FREQUENCY_TICK_COUNT - 1) / 2) /
            (FREQUENCY_TICK_COUNT - 1));
    }

    void formatFrequencyLabel(
        uint32_t frequencyHz,
        char *label,
        size_t labelSize)
    {
        if (frequencyHz < 1000)
        {
            snprintf(
                label,
                labelSize,
                "%lu",
                static_cast<unsigned long>(frequencyHz));
            return;
        }

        const uint32_t roundedTenths = (frequencyHz + 50) / 100;
        const uint32_t kilohertz = roundedTenths / 10;
        const uint32_t tenths = roundedTenths % 10;
        if (tenths == 0)
        {
            snprintf(
                label,
                labelSize,
                "%luk",
                static_cast<unsigned long>(kilohertz));
        }
        else
        {
            snprintf(
                label,
                labelSize,
                "%lu.%luk",
                static_cast<unsigned long>(kilohertz),
                static_cast<unsigned long>(tenths));
        }
    }

    bool formatUnsignedFixed(
        float value,
        uint8_t decimalPlaces,
        char *text,
        size_t textSize)
    {
        if (text == nullptr || textSize == 0 ||
            !isfinite(value) || value < 0.0f ||
            decimalPlaces < 1 || decimalPlaces > 2)
        {
            return false;
        }

        const uint32_t scale = decimalPlaces == 2 ? 100UL : 10UL;
        const uint32_t scaled = static_cast<uint32_t>(
            value * static_cast<float>(scale) + 0.5f);
        const uint32_t whole = scaled / scale;
        const uint32_t fraction = scaled % scale;

        if (decimalPlaces == 2)
        {
            snprintf(text, textSize, "%lu.%02lu",
                     static_cast<unsigned long>(whole),
                     static_cast<unsigned long>(fraction));
        }
        else
        {
            snprintf(text, textSize, "%lu.%01lu",
                     static_cast<unsigned long>(whole),
                     static_cast<unsigned long>(fraction));
        }

        return true;
    }

    bool formatSignedTenths(
        float value,
        char *text,
        size_t textSize)
    {
        if (text == nullptr || textSize == 0 || !isfinite(value))
            return false;

        const bool negative = value < 0.0f;
        const float magnitude = negative ? -value : value;
        const uint32_t scaled = static_cast<uint32_t>(
            magnitude * 10.0f + 0.5f);

        snprintf(
            text,
            textSize,
            "%c%lu.%01lu",
            negative ? '-' : '+',
            static_cast<unsigned long>(scaled / 10UL),
            static_cast<unsigned long>(scaled % 10UL));
        return true;
    }

    int dbToSpriteY(int db)
    {
        const int rangeDb =
            static_cast<int>(SPECTRUM_MAX_DB - SPECTRUM_MIN_DB);
        return SPECTRUM_BOTTOM -
               (((db - static_cast<int>(SPECTRUM_MIN_DB)) *
                 SPECTRUM_BOTTOM) +
                rangeDb / 2) /
                   rangeDb;
    }

}

// Constante
static const int GRID_X1 = 26;
static const int GRID_X2 = 302;

const uint16_t COL_CYAN_OFF = tft.color565(0, 20, 40);
const uint16_t COL_RED_OFF = tft.color565(40, 0, 0);

static const int GRID_Y_TOP = 26;
static const int GRID_Y_BOTTOM = 202;

static const int GRID_LINES = 10;
constexpr int FFT_BASE_Y = GRID_Y_BOTTOM - 4;

const int dbValue[9] = {30, 25, 20, 15, 10, 5, 0, 5, 10};
constexpr int SCALE_LEFT_X = 20;
constexpr int SCALE_RIGHT_X = 301;
// Repere pentru cele 16 benzi logaritmice, de la 150 Hz la 18 kHz.
// Etichetele alternează cu cel mult un spațiu liber, pentru o scală uniformă.
const char *freqLabel[16] = {"150", "", "270", "", "500", "", "900", "",
                             "1k6", "", "3k", "", "5k4", "", "10k", "18k"};

// constante folosite de FFT
constexpr int FFT_BAR_GAP = 3; // spațiu între bare
constexpr int FFT_BAR_W =
    ((GRID_X2 - GRID_X1) - (FFT_BANDS - 1) * FFT_BAR_GAP) / FFT_BANDS;
constexpr int FFT_BAR_H = FFT_BASE_Y - GRID_Y_TOP;
int bandWidth = (GRID_X2 - GRID_X1) / FFT_BANDS;

constexpr int FFT_TOP = GRID_Y_TOP;
constexpr int FFT_BOTTOM = FFT_BASE_Y;
constexpr int FFT_HEIGHT = FFT_BOTTOM - FFT_TOP;

static const int BAR_TOP = GRID_Y_TOP;
static const int BAR_BOTTOM = GRID_Y_BOTTOM;

static const int BAR_W = 14;
static const int BAR_GAP = 3;

static const int BAR_MAX_H = BAR_BOTTOM - BAR_TOP;
extern uint8_t peakBands[FFT_BANDS];

// În Spec.cpp, în zona de constante:
constexpr int BLOCK_H = 4;
constexpr int BLOCK_GAP = 1;
constexpr int BRICKS_PER_INTERVAL = 4;                                     // 4 brick-uri între două linii consecutive
constexpr int STEP_PER_LINE = BRICKS_PER_INTERVAL * (BLOCK_H + BLOCK_GAP); // 4 * 5 = 20 pixeli per interval

//===================================================================================================
void Spectrum::begin()
{
    resetSpectrumLevels();
    resetFFTView();
    resetPeakMeasurement();
    fftGraphTouchPending = false;
    spectrumSpriteReady =
        activeView == View::FFT &&
        createSpectrumSprite();
}

void Spectrum::run()
{
    begin();
    drawScreen();

    while (true)
    {
        updateDisplay();
        display.updateClock();

        if (processTouch())
            break;

        FFTGraphTouch graphTouch;
        if (takeFFTGraphTouch(graphTouch))
        {
            recenterFFTView(graphTouch.x);
            updatePeakMeasurement(displayMagnitudes);
            drawScreen();
            drawHighResolutionSpectrum();
        }
    }

    audio.stopCapture();
    releaseSpectrumSprite();
}

void Spectrum::clearFFT()
{
    tft.fillRect(
        GRID_X1,
        GRID_Y_TOP,
        GRID_X2 - GRID_X1 + 1,
        FFT_HEIGHT,
        COL_BG);
}

void Spectrum::drawScreen()
{
    display.openApp(activeTitle(), "TOUCH TO BACK");

    if (activeView == View::Spectrum)
    {
        drawFFTBackground();
        drawGrid();
        drawScale();
        return;
    }

    drawSpectrumAxes();
    drawPeakMeasurement();

    if (spectrumSpriteReady)
    {
        drawSpectrumGridToSprite();
        spectrumSprite.pushSprite(SPECTRUM_X, SPECTRUM_Y);
    }
    else
    {
        tft.fillRect(
            SPECTRUM_X,
            SPECTRUM_Y,
            SPECTRUM_W,
            SPECTRUM_H,
            COL_BG);
    }
}

const char *Spectrum::activeTitle() const
{
    return activeView == View::Spectrum
               ? "SPECTRUM ANALYZER"
               : "FFT ANALYZER";
}

bool Spectrum::isTitleTouch(uint16_t x, uint16_t y) const
{
    constexpr uint16_t TITLE_X = 12;
    constexpr uint16_t TITLE_TOUCH_PADDING = 8;
    constexpr uint16_t TITLE_TOUCH_Y_MAX = HEADER_HEIGHT + 4;

    // Use the longer title for both views so the toggle zone never moves.
    const uint16_t titleRight = static_cast<uint16_t>(
        TITLE_X +
        tft.textWidth("SPECTRUM ANALYZER", 2) +
        TITLE_TOUCH_PADDING);

    return x >= TITLE_X - TITLE_TOUCH_PADDING &&
           x <= titleRight &&
           y <= TITLE_TOUCH_Y_MAX;
}

bool Spectrum::isBackTouch(uint16_t y) const
{
    return y >= FOOTER_Y;
}

bool Spectrum::isFFTGraphTouch(uint16_t x, uint16_t y) const
{
    return x >= SPECTRUM_X &&
           x < SPECTRUM_X + SPECTRUM_W &&
           y >= SPECTRUM_Y &&
           y < SPECTRUM_Y + SPECTRUM_H;
}

void Spectrum::captureFFTGraphTouch(uint16_t x, uint16_t y)
{
    pendingFFTGraphTouch.x = static_cast<uint16_t>(x - SPECTRUM_X);
    pendingFFTGraphTouch.y = static_cast<uint16_t>(y - SPECTRUM_Y);
    fftGraphTouchPending = true;
}

bool Spectrum::takeFFTGraphTouch(FFTGraphTouch &coordinates)
{
    if (!fftGraphTouchPending)
        return false;

    coordinates = pendingFFTGraphTouch;
    fftGraphTouchPending = false;
    return true;
}

bool Spectrum::processTouch()
{
    if (!touch.pressed())
        return false;

    const uint16_t x = touch.getX();
    const uint16_t y = touch.getY();
    touch.waitAnyTouch();

    if (isTitleTouch(x, y))
    {
        toggleView();
        drawScreen();
        return false;
    }

    if (isBackTouch(y))
        return true;

    if (activeView == View::FFT && isFFTGraphTouch(x, y))
        captureFFTGraphTouch(x, y);

    return false;
}

void Spectrum::toggleView()
{
    resetFFTView();
    resetPeakMeasurement();
    activeView =
        activeView == View::Spectrum
            ? View::FFT
            : View::Spectrum;

    if (activeView == View::FFT && !spectrumSpriteReady)
        spectrumSpriteReady = createSpectrumSprite();
}

void Spectrum::resetFFTView()
{
    fftZoomActive = false;
    fftViewStartHz = 0;
    fftViewStopHz = FFT_NYQUIST_HZ;
    fftCenterHz = FFT_NYQUIST_HZ / 2;
}

uint32_t Spectrum::fftColumnToFrequency(uint16_t column) const
{
    const uint32_t viewSpanHz = fftViewStopHz - fftViewStartHz;
    return fftViewStartHz +
           (viewSpanHz * column + (SPECTRUM_W - 1) / 2) /
               (SPECTRUM_W - 1);
}

void Spectrum::recenterFFTView(uint16_t graphX)
{
    const uint32_t requestedCenterHz =
        fftColumnToFrequency(graphX);
    constexpr uint32_t HALF_SPAN_HZ = FFT_ZOOM_SPAN_HZ / 2;

    if (requestedCenterHz < HALF_SPAN_HZ)
    {
        fftViewStartHz = 0;
        fftViewStopHz = FFT_ZOOM_SPAN_HZ;
    }
    else if (requestedCenterHz > FFT_NYQUIST_HZ - HALF_SPAN_HZ)
    {
        fftViewStartHz = FFT_NYQUIST_HZ - FFT_ZOOM_SPAN_HZ;
        fftViewStopHz = FFT_NYQUIST_HZ;
    }
    else
    {
        fftViewStartHz = requestedCenterHz - HALF_SPAN_HZ;
        fftViewStopHz = requestedCenterHz + HALF_SPAN_HZ;
    }

    fftCenterHz = (fftViewStartHz + fftViewStopHz) / 2;
    fftZoomActive = true;
    resetPeakMeasurement();
}

uint8_t Spectrum::fftLevelForColumn(uint16_t column) const
{
    if (!fftZoomActive)
        return smoothedLevels[column];

    const uint32_t frequencyHz = fftColumnToFrequency(column);
    uint32_t bin =
        (frequencyHz * FFTAnalyzer::SAMPLES +
         FFTAnalyzer::SAMPLE_RATE_HZ / 2) /
        FFTAnalyzer::SAMPLE_RATE_HZ;
    if (bin >= FFTAnalyzer::BIN_COUNT)
        bin = FFTAnalyzer::BIN_COUNT - 1;

    return smoothedLevels[bin];
}

uint32_t Spectrum::fftTickFrequency(uint8_t tick) const
{
    if (tick == FREQUENCY_TICK_COUNT / 2)
        return fftCenterHz;

    const uint32_t viewSpanHz = fftViewStopHz - fftViewStartHz;
    return fftViewStartHz +
           (viewSpanHz * tick + (FREQUENCY_TICK_COUNT - 1) / 2) /
               (FREQUENCY_TICK_COUNT - 1);
}

// Functie comuna pentru pozitia pe Y
int Spectrum::gridY(uint8_t line)
{
    // line 0 = -30 (baza)
    // line 1 = -25
    // line 2 = -20
    // line 3 = -15
    // line 4 = -10
    // line 5 = -5
    // line 6 = 0
    // line 7 = +5
    // line 8 = +10
    // (iar linia 9 corespunzătoare lui +15 nu se va desena)

    return GRID_Y_BOTTOM - (line * STEP_PER_LINE) - BLOCK_H - BLOCK_GAP;
}

void Spectrum::drawGrid()
{
    // Desenăm de la i = 0 (-30) până la i = 8 (+10). Linia 9 (+15) este oprită.
    for (int i = 0; i < 9; i++)
    {
        int y = gridY(i);

        // Poți evidenția linia de 0 dB (care e la indexul 6: -30,-25,-20,-15,-10,-5,0 -> i=6)
        if (i == 6)
        {
            for (int x = GRID_X1 - 4; x <= GRID_X2; x += 8)
                tft.drawFastHLine(x, y, 5, TFT_WHITE);
        }
        else
        {
            for (int x = GRID_X1 - 4; x <= GRID_X2; x += 4)
                tft.drawFastHLine(x, y, 2, COL_FRAME);
        }
    }
}

void Spectrum::drawScale()
{
    tft.setTextFont(1);

    tft.setTextColor(TFT_GREEN, COL_BG);
    for (int i = 0; i < 9; i++)
    {
        int y = gridY(i);
        char txt[5];
        sprintf(txt, "%d", dbValue[i]);
        tft.drawRightString(txt, SCALE_LEFT_X, y - 3, 1); // scrie cifra LEFT
        tft.drawString(txt, SCALE_RIGHT_X, y - 3, 1);     // scrie cifra Right
    }
    tft.setTextColor(TFT_CYAN, COL_BG);
    int x = 0;
    for (int i = 0; i < FFT_BANDS; i++)
    {
        if (freqLabel[i][0] == '\0')
            continue;
        if (i == FFT_BANDS - 1)
            x = GRID_X1 + i * bandWidth + bandWidth / 2 + 5;
        else
            x = GRID_X1 + i * bandWidth + bandWidth / 2;
        tft.setTextDatum(MC_DATUM);
        tft.drawCentreString(freqLabel[i], x, GRID_Y_BOTTOM, 1);
    }

    tft.setTextDatum(TL_DATUM);
}

void Spectrum::drawFrequencyScale()
{
}

void Spectrum::drawFFTBackground()
{
    for (int i = 0; i < FFT_BANDS; i++)
    {
        int x = GRID_X1 + i * (FFT_BAR_W + FFT_BAR_GAP);

        tft.fillRect(
            x,
            GRID_Y_TOP,
            FFT_BAR_W,
            FFT_BAR_H,
            COL_CYAN_OFF);
    }
}

void Spectrum::updateDisplay()
{
    uint8_t realBands[FFT_BANDS];

    // 1. Actualizăm datele audio (FFT brut rămâne neschimbat)
    audio.update();

    // 2. Preluăm benzile calculate
    for (uint8_t i = 0; i < FFT_BANDS; i++)
    {
        realBands[i] = audio.getBand(i);
    }

    // 3. Construim copia display-only a celor 256 de magnitudini.
    //    fftAnalyzer.binMagnitudes() (fftBins[]) este doar citit aici; nu e
    //    scris, deci nicio cale de măsură (Dolby Check integratedRms etc.)
    //    nu este afectată. Nicio procesare suplimentară nu este aplicată.
    buildDisplayMagnitudes(fftAnalyzer.binMagnitudes());

    // 4. Trimitem la funcția de desenare pe ecran
    if (activeView == View::Spectrum)
    {
        drawBars(realBands, peakBands);
        return;
    }

    updateSpectrumLevels(displayMagnitudes);
    updatePeakMeasurement(displayMagnitudes);
    drawHighResolutionSpectrum();
    drawPeakMeasurement();
}

bool Spectrum::updateDiagnosticPeak(float &frequencyHz, float &levelDb)
{
    bool clipped = false;
    if (!audio.calculateFFTChannel(false, clipped))
    {
        resetPeakMeasurement();
        frequencyHz = 0.0f;
        levelDb = 0.0f;
        return false;
    }

    (void)clipped;
    buildDisplayMagnitudes(fftAnalyzer.binMagnitudes());
    updatePeakMeasurement(displayMagnitudes);
    if (!peakMeasurementValid)
    {
        frequencyHz = 0.0f;
        levelDb = 0.0f;
        return false;
    }

    frequencyHz = peakFrequencyHz;
    levelDb = peakLevelDb;
    return true;
}

bool Spectrum::createSpectrumSprite()
{
    if (!spectrumSprite.created())
    {
        spectrumSprite.setColorDepth(1);
        if (spectrumSprite.createSprite(
                SPECTRUM_W,
                SPECTRUM_H) == nullptr)
            return false;

        spectrumSprite.setBitmapColor(TFT_CYAN, COL_BG);
        spectrumSprite.fillSprite(SPRITE_BACKGROUND);
    }

    if (!peakMeasurementSprite.created())
    {
        peakMeasurementSprite.setColorDepth(4);
        if (peakMeasurementSprite.createSprite(
                FFT_PEAK_W,
                FFT_PEAK_H) != nullptr)
        {
            peakMeasurementSprite.setPaletteColor(
                PEAK_TEXT_BACKGROUND,
                COL_BG);
            peakMeasurementSprite.setPaletteColor(
                PEAK_TEXT_LABEL,
                TFT_ORANGE);
            peakMeasurementSprite.setPaletteColor(
                PEAK_TEXT_VALUE,
                TFT_CYAN);
            peakMeasurementSprite.setTextFont(1);
            peakMeasurementSprite.setTextDatum(TL_DATUM);
            peakMeasurementSprite.fillSprite(PEAK_TEXT_BACKGROUND);
        }
    }

    return true;
}

void Spectrum::releaseSpectrumSprite()
{
    if (spectrumSprite.created())
        spectrumSprite.deleteSprite();
    if (peakMeasurementSprite.created())
        peakMeasurementSprite.deleteSprite();

    spectrumSpriteReady = false;
}

void Spectrum::resetSpectrumLevels()
{
    memset(smoothedLevels, 0, sizeof(smoothedLevels));
}

uint8_t Spectrum::magnitudeToLevel(
    float magnitude,
    float rmsReference) const
{
    float db = SPECTRUM_MIN_DB;
    const float rms = magnitude * FFT_TO_RMS;

    if (rms > 0.0001f &&
        rmsReference > 0.0f &&
        isfinite(rms) &&
        isfinite(rmsReference))
    {
        db = 20.0f * log10f(rms / rmsReference);
    }

    if (!isfinite(db))
        db = SPECTRUM_MIN_DB;
    if (db < SPECTRUM_MIN_DB)
        db = SPECTRUM_MIN_DB;
    if (db > SPECTRUM_MAX_DB)
        db = SPECTRUM_MAX_DB;

    return static_cast<uint8_t>(
        ((db - SPECTRUM_MIN_DB) * SPECTRUM_BOTTOM /
         (SPECTRUM_MAX_DB - SPECTRUM_MIN_DB)) +
        0.5f);
}

void Spectrum::updateSpectrumLevels(const float *magnitudes)
{
    if (magnitudes == nullptr)
        return;

    const float rmsReference = setAudio.rmsReference();
    for (uint16_t bin = 0; bin < FFTAnalyzer::BIN_COUNT; ++bin)
    {
        const uint8_t target =
            magnitudeToLevel(
                magnitudes[bin],
                rmsReference);
        const uint8_t previous = smoothedLevels[bin];

        // Fast attack keeps moving tones responsive; slower release calms jitter.
        if (target >= previous)
        {
            smoothedLevels[bin] = static_cast<uint8_t>(
                (previous + 3U * target + 2U) / 4U);
        }
        else
        {
            smoothedLevels[bin] = static_cast<uint8_t>(
                (3U * previous + target + 2U) / 4U);
        }
    }
}

void Spectrum::buildDisplayMagnitudes(const float *rawMagnitudes)
{
    if (rawMagnitudes == nullptr)
    {
        memset(displayMagnitudes, 0, sizeof(displayMagnitudes));
        return;
    }

    // Keep the FFT display copy identical to the raw magnitudes. No display-side
    // processing is applied; rendering uses the raw magnitudes directly.
    for (uint16_t bin = 0; bin < FFTAnalyzer::BIN_COUNT; ++bin)
        displayMagnitudes[bin] = rawMagnitudes[bin];
}

void Spectrum::resetPeakMeasurement()
{
    peakFrequencyHz = 0.0f;
    peakLevelDb = 0.0f;
    peakMeasurementValid = false;
    peakMeasurementInitialized = false;
    displayedPeakText[0] = '\0';
}

void Spectrum::updatePeakMeasurement(const float *magnitudes)
{
    if (magnitudes == nullptr)
    {
        resetPeakMeasurement();
        return;
    }

    uint32_t firstBin =
        (fftViewStartHz * FFTAnalyzer::SAMPLES +
         FFTAnalyzer::SAMPLE_RATE_HZ / 2) /
        FFTAnalyzer::SAMPLE_RATE_HZ;
    uint32_t lastBin =
        (fftViewStopHz * FFTAnalyzer::SAMPLES +
         FFTAnalyzer::SAMPLE_RATE_HZ / 2) /
        FFTAnalyzer::SAMPLE_RATE_HZ;

    if (firstBin < 1)
        firstBin = 1;
    if (lastBin >= FFTAnalyzer::BIN_COUNT)
        lastBin = FFTAnalyzer::BIN_COUNT - 1;
    if (firstBin > lastBin)
    {
        resetPeakMeasurement();
        return;
    }

    uint16_t peakBin = static_cast<uint16_t>(firstBin);
    float peakMagnitude = magnitudes[peakBin];
    for (uint16_t bin = peakBin + 1;
         bin <= static_cast<uint16_t>(lastBin);
         ++bin)
    {
        if (magnitudes[bin] > peakMagnitude)
        {
            peakMagnitude = magnitudes[bin];
            peakBin = bin;
        }
    }

    if (!isfinite(peakMagnitude) || peakMagnitude <= 0.0001f)
    {
        resetPeakMeasurement();
        return;
    }
    float delta = 0.0f;
    if (peakBin > 0 && peakBin + 1 < FFTAnalyzer::BIN_COUNT)
    {
        const float left = magnitudes[peakBin - 1];
        const float center = magnitudes[peakBin];
        const float right = magnitudes[peakBin + 1];
        const float denominator = left - 2.0f * center + right;

        if (isfinite(left) && isfinite(center) && isfinite(right) &&
            fabsf(denominator) > 0.000001f)
        {
            delta = 0.5f * (left - right) / denominator;
            if (!isfinite(delta))
                delta = 0.0f;
            if (delta < -0.5f)
                delta = -0.5f;
            if (delta > 0.5f)
                delta = 0.5f;
        }
    }

    float measuredFrequencyHz =
        (static_cast<float>(peakBin) + delta) *
        FFTAnalyzer::BIN_WIDTH_HZ;
    if (measuredFrequencyHz < static_cast<float>(fftViewStartHz))
        measuredFrequencyHz = static_cast<float>(fftViewStartHz);
    if (measuredFrequencyHz > static_cast<float>(fftViewStopHz))
        measuredFrequencyHz = static_cast<float>(fftViewStopHz);

    const float rmsReference = setAudio.rmsReference();
    const float rms = peakMagnitude * FFT_TO_RMS;
    if (rmsReference <= 0.0f || !isfinite(rmsReference) ||
        rms <= 0.0001f || !isfinite(rms))
    {
        resetPeakMeasurement();
        return;
    }

    float measuredDb = 20.0f * log10f(rms / rmsReference);
    if (!isfinite(measuredDb) || measuredDb <= SPECTRUM_MIN_DB)
    {
        resetPeakMeasurement();
        return;
    }
    if (measuredDb > SPECTRUM_MAX_DB)
        measuredDb = SPECTRUM_MAX_DB;

    if (peakMeasurementInitialized)
    {
        peakFrequencyHz += PEAK_SMOOTHING_ALPHA *
                           (measuredFrequencyHz - peakFrequencyHz);
        peakLevelDb += PEAK_SMOOTHING_ALPHA *
                       (measuredDb - peakLevelDb);
    }
    else
    {
        peakFrequencyHz = measuredFrequencyHz;
        peakLevelDb = measuredDb;
        peakMeasurementInitialized = true;
    }

    peakMeasurementValid =
        isfinite(peakFrequencyHz) &&
        isfinite(peakLevelDb);
}

void Spectrum::drawPeakMeasurement()
{
    char line[40];
    char frequencyValue[16];
    const char *frequencyUnit = "Hz";
    char levelValue[12];
    bool lineFormatted = false;

    if (peakMeasurementValid)
    {
        if (peakFrequencyHz >= 1000.0f)
        {
            lineFormatted = formatUnsignedFixed(
                peakFrequencyHz / 1000.0f,
                2,
                frequencyValue,
                sizeof(frequencyValue));
            frequencyUnit = "kHz";
        }
        else
        {
            lineFormatted = formatUnsignedFixed(
                peakFrequencyHz,
                1,
                frequencyValue,
                sizeof(frequencyValue));
        }

        const bool levelFormatted = formatSignedTenths(
            peakLevelDb,
            levelValue,
            sizeof(levelValue));

        lineFormatted = lineFormatted && levelFormatted;
        if (lineFormatted)
            snprintf(
                line,
                sizeof(line),
                "PEAK: %s %s %s dB",
                frequencyValue,
                frequencyUnit,
                levelValue);
        else
            snprintf(line, sizeof(line), "PEAK: ---  ---");
    }
    else
    {
        snprintf(line, sizeof(line), "PEAK: ---  ---");
    }

    struct ColoredTextPart
    {
        const char *text;
        uint8_t color;
    };

    const char *displayFrequency = lineFormatted ? frequencyValue : "---";
    const char *displayFrequencyUnit = lineFormatted ? frequencyUnit : "";
    const char *displayLevel = lineFormatted ? levelValue : "---";
    const char *displayDbUnit = lineFormatted ? "dB" : "";

    const ColoredTextPart parts[] = {
        {"PEAK: ", PEAK_TEXT_LABEL},
        {displayFrequency, PEAK_TEXT_VALUE},
        {" ", PEAK_TEXT_LABEL},
        {displayFrequencyUnit, PEAK_TEXT_LABEL},
        {"  ", PEAK_TEXT_LABEL},
        {displayLevel, PEAK_TEXT_VALUE},
        {" ", PEAK_TEXT_LABEL},
        {displayDbUnit, PEAK_TEXT_LABEL}};

    int totalWidth = 0;
    for (uint8_t i = 0; i < sizeof(parts) / sizeof(parts[0]); ++i)
        totalWidth += tft.textWidth(parts[i].text, 1);

    int x = (FFT_PEAK_W - totalWidth) / 2;
    if (x < 0)
        x = 0;

    if (peakMeasurementSprite.created())
    {
        // The application background can be changed by the system settings.
        // Keep the paletted text sprite synchronized before clearing it; an
        // old palette/background otherwise leaves stale pixels in this strip.
        peakMeasurementSprite.setPaletteColor(
            PEAK_TEXT_BACKGROUND,
            COL_BG);
        peakMeasurementSprite.setPaletteColor(
            PEAK_TEXT_LABEL,
            TFT_ORANGE);
        peakMeasurementSprite.setPaletteColor(
            PEAK_TEXT_VALUE,
            TFT_CYAN);
        peakMeasurementSprite.fillSprite(PEAK_TEXT_BACKGROUND);
        for (uint8_t i = 0; i < sizeof(parts) / sizeof(parts[0]); ++i)
        {
            peakMeasurementSprite.setTextColor(
                parts[i].color,
                PEAK_TEXT_BACKGROUND);
            peakMeasurementSprite.drawString(
                parts[i].text,
                x,
                0,
                1);
            x += tft.textWidth(parts[i].text, 1);
        }
        peakMeasurementSprite.pushSprite(FFT_PEAK_X, FFT_PEAK_Y);
    }
    else
    {
        tft.fillRect(
            FFT_PEAK_X,
            FFT_PEAK_Y,
            FFT_PEAK_W,
            FFT_PEAK_H,
            COL_BG);
        x += FFT_PEAK_X;
        for (uint8_t i = 0; i < sizeof(parts) / sizeof(parts[0]); ++i)
        {
            tft.setTextColor(
                parts[i].color == PEAK_TEXT_VALUE ? TFT_CYAN : TFT_ORANGE,
                COL_BG);
            tft.drawString(parts[i].text, x, FFT_PEAK_Y, 1);
            x += tft.textWidth(parts[i].text, 1);
        }
    }

    snprintf(
        displayedPeakText,
        sizeof(displayedPeakText),
        "%s",
        line);
}

void Spectrum::drawSpectrumGridToSprite()
{
    spectrumSprite.fillSprite(SPRITE_BACKGROUND);

    for (uint8_t i = 0; i < sizeof(DB_TICKS); ++i)
    {
        const int y = dbToSpriteY(DB_TICKS[i]);
        for (int x = 0; x < SPECTRUM_W; x += 4)
            spectrumSprite.drawPixel(x, y, SPRITE_FOREGROUND);
    }

    for (uint8_t i = 0; i < FREQUENCY_TICK_COUNT; ++i)
    {
        const int x = frequencyTickToSpriteX(i);
        for (int y = 0; y < SPECTRUM_H; y += 4)
            spectrumSprite.drawPixel(x, y, SPRITE_FOREGROUND);
    }
}

void Spectrum::drawHighResolutionSpectrum()
{
    if (!spectrumSpriteReady)
    {
        tft.fillRect(
            SPECTRUM_X,
            SPECTRUM_Y,
            SPECTRUM_W,
            SPECTRUM_H,
            COL_BG);

        tft.startWrite();
        for (uint16_t column = 0; column < SPECTRUM_W; ++column)
        {
            const uint8_t level = fftLevelForColumn(column);
            if (level == 0)
                continue;

            tft.drawFastVLine(
                SPECTRUM_X + column,
                SPECTRUM_Y + SPECTRUM_BOTTOM - level,
                level + 1,
                TFT_CYAN);
        }
        tft.endWrite();
        return;
    }

    drawSpectrumGridToSprite();

    for (uint16_t column = 0; column < SPECTRUM_W; ++column)
    {
        const uint8_t level = fftLevelForColumn(column);
        if (level == 0)
            continue;

        spectrumSprite.drawFastVLine(
            column,
            SPECTRUM_BOTTOM - level,
            level + 1,
            SPRITE_FOREGROUND);
    }

    spectrumSprite.pushSprite(SPECTRUM_X, SPECTRUM_Y);
}

void Spectrum::drawSpectrumAxes()
{
    tft.setTextFont(1);
    tft.setTextColor(TFT_GREEN, COL_BG);

    for (uint8_t i = 0; i < sizeof(DB_TICKS); ++i)
    {
        char label[5];
        snprintf(label, sizeof(label), "%d", DB_TICKS[i]);
        const int y = SPECTRUM_Y + dbToSpriteY(DB_TICKS[i]);
        tft.drawRightString(label, SPECTRUM_X - 3, y - 3, 1);
    }

    tft.setTextColor(TFT_CYAN, COL_BG);
    for (uint8_t i = 0; i < FREQUENCY_TICK_COUNT; ++i)
    {
        const int x = SPECTRUM_X + frequencyTickToSpriteX(i);
        const int y = SPECTRUM_Y + SPECTRUM_H + 3;
        char label[8];
        formatFrequencyLabel(
            fftTickFrequency(i),
            label,
            sizeof(label));

        if (i == 0)
            tft.drawString(label, x, y, 1);
        else if (i == FREQUENCY_TICK_COUNT - 1)
            tft.drawRightString(label, x, y, 1);
        else
            tft.drawCentreString(label, x, y, 1);
    }

    tft.setTextDatum(TL_DATUM);
}

void Spectrum::drawBarBackground()
{
    for (int i = 0; i < FFT_BANDS; i++)
    {
        int x = GRID_X1 + i * (BAR_W + BAR_GAP);

        tft.fillRect(
            x,
            BAR_TOP,
            BAR_W,
            BAR_MAX_H,
            COL_CYAN_OFF);
    }
}

/*void Spectrum::drawBars(const uint8_t barValues[FFT_BANDS])
{
    // Setăm dimensiunile unui brick pe verticală (poți ajusta după preferință)
    constexpr uint8_t BLOCK_H   = 4;   // Înălțimea unui pătrățel
    constexpr uint8_t BLOCK_GAP = 1;   // Spațiul vertical între pătrățele

    // Câte blocuri încap pe înălțimea totală a barei
    // 8 intervale de 5 dB, a câte 4 brick-uri: -30…+10 dB.
    const int totalBlocks = BRICKS_PER_INTERVAL * 8;

    for (int i = 0; i < FFT_BANDS; i++) {
        // Coordonata X a barei curente (folosind lățimea calculată de tine: FFT_BAR_W)
        int x = GRID_X1 + i * (FFT_BAR_W + FFT_BAR_GAP);

        // Mapăm valoarea primită (de la 0 la 255 sau nivelul tău maxim) în număr de blocuri active
        // int activeBlocks = map(barValues[i], 0, 100, 0, totalBlocks);
        int activeBlocks = barValues[i];
        if (activeBlocks > totalBlocks) activeBlocks = totalBlocks;
        if (activeBlocks < 0) activeBlocks = 0;

        // Desenăm blocurile de jos în sus
        for (int j = 0; j < totalBlocks; j++) {
            // Calculăm coordonata Y de jos în sus (pornind de la FFT_BASE_Y)
            int y = FFT_BASE_Y - ((j + 1) * (BLOCK_H + BLOCK_GAP));

            uint16_t brickColor;

            if (j < activeBlocks) {
                // Dacă blocul este aprins (părțile de sus le facem roșii/galbene, restul cyan)
                if (j > (totalBlocks * 0.8))      brickColor = TFT_RED;
                else if (j > (totalBlocks * 0.56))  brickColor = TFT_GREEN;
                else                               brickColor = TFT_CYAN;
            } else {
                // Dacă blocul este stins, folosیم culorile de fundal stins pe care le-ai definit
                if (j > (totalBlocks * 0.8))      brickColor = COL_RED_OFF;
                else if (j > (totalBlocks * 0.56))  brickColor = tft.color565(40, 40, 0); // galben stins
                else                               brickColor = COL_CYAN_OFF;
            }

            tft.fillRect(x, y, FFT_BAR_W, BLOCK_H, brickColor);
        }
    }
}*/

void Spectrum::drawBars(const uint8_t barValues[FFT_BANDS], const uint8_t peakValues[FFT_BANDS])
{
    // Setăm dimensiunile unui brick pe verticală
    constexpr uint8_t BLOCK_H = 4;   // Înălțimea unui pătrățel
    constexpr uint8_t BLOCK_GAP = 1; // Spațiul vertical între pătrățele

    // Câte blocuri încap pe înălțimea totală a barei
    // 8 intervale de 5 dB, a câte 4 brick-uri: -30…+10 dB.
    const int totalBlocks = BRICKS_PER_INTERVAL * 8;

    for (int i = 0; i < FFT_BANDS; i++)
    {
        int x = GRID_X1 + i * (FFT_BAR_W + FFT_BAR_GAP);

        int activeBlocks = barValues[i];
        if (activeBlocks > totalBlocks)
            activeBlocks = totalBlocks;
        if (activeBlocks < 0)
            activeBlocks = 0;

        int peakBlock = peakValues[i];
        if (peakBlock > totalBlocks)
            peakBlock = totalBlocks;
        if (peakBlock < 0)
            peakBlock = 0;

        // Desenăm blocurile de jos în sus
        for (int j = 0; j < totalBlocks; j++)
        {
            int y = FFT_BASE_Y - ((j + 1) * (BLOCK_H + BLOCK_GAP));

            uint16_t brickColor;

            // Verificăm dacă acest bloc este vârful (peak-ul) suspendat
            // (Desenăm vârful ca un brick distinct, de ex. alb sau roșu aprins, dacă nu e acoperit de bară)
            if (j == (peakBlock - 1) && peakBlock > activeBlocks)
            {
                brickColor = TFT_WHITE; // Sau TFT_RED - culoarea vârfului plutitor
            }
            else if (j < activeBlocks)
            {
                // Dacă blocul este aprins în bara principală
                if (j > (totalBlocks * 0.8))
                    brickColor = TFT_RED;
                else if (j > (totalBlocks * 0.56))
                    brickColor = TFT_GREEN;
                else
                    brickColor = TFT_CYAN;
            }
            else
            {
                // Dacă blocul este stins
                if (j > (totalBlocks * 0.8))
                    brickColor = COL_RED_OFF;
                else if (j > (totalBlocks * 0.56))
                    brickColor = tft.color565(40, 40, 0);
                else
                    brickColor = COL_CYAN_OFF;
            }

            tft.fillRect(x, y, FFT_BAR_W, BLOCK_H, brickColor);
        }
    }
}

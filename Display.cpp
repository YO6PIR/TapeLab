//====== Display.cpp ============
#include "Display.h"
#include "Config.h"
#include "Storage.h"
#include "Rtc.h"
#include "Sine.h"
#include "Touch.h"
#include <TFT_eSPI.h>
#include <string.h>
#include "SplashRle.h"

extern Touch touch;
TFT_eSPI tft;

static const char *menuText[] =
{
    "PLAYBACK Analyzer",
    "RECORD Calibration",
    "SPECTRUM Analyzer",
    "WAVEFORM Analyzer",
    "AUDIO Generator",
    "TRANSPORT Analysis",
    "SISTEM Configuration"
};

static const uint16_t menuY[] =
{
    MENU_Y,
    MENU_Y + MENU_STEP,
    MENU_Y + 2 * MENU_STEP,
    MENU_Y + 3 * MENU_STEP,
    MENU_Y + 4 * MENU_STEP,
    MENU_Y + 5 * MENU_STEP,
    MENU_Y + 6 * MENU_STEP
};

//================= MENU LAYOUT =================
constexpr uint16_t MENU_ITEM_HEIGHT  = MENU_STEP;
constexpr uint16_t MENU_HIGHLIGHT_HEIGHT = MENU_ITEM_HEIGHT + 3;
const uint16_t MENU_SELECTION_BACKGROUND = tft.color565(0, 20, 40);
//===============================================

void Display::begin()
{
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
}

void Display::clear()
{
    tft.fillScreen(COL_BG);
}

void Display::drawProgressBricks(
    int x,
    int y,
    uint8_t progress,
    uint8_t previousProgress)
{
    const uint8_t firstBrick = previousProgress == 0xFF
                                   ? 0
                                   : min(previousProgress, progress);
    const uint8_t lastBrick = previousProgress == 0xFF
                                  ? PROGRESS_BAR_BRICK_COUNT
                                  : max(previousProgress, progress);
    const uint16_t darkBlue = tft.color565(0, 0, 127);

    for (uint8_t brick = firstBrick; brick < lastBrick; ++brick)
    {
        const int brickX = x + brick *
            (PROGRESS_BAR_BRICK_WIDTH + PROGRESS_BAR_BRICK_GAP);
        tft.fillRect(
            brickX,
            y,
            PROGRESS_BAR_BRICK_WIDTH,
            PROGRESS_BAR_BRICK_HEIGHT,
            brick < progress ? TFT_CYAN : darkBlue);
    }
}

void Display::splash()
{
    tft.fillScreen(TFT_BLACK);
    drawSplashRle();
}

void Display::mainMenu()
{
    tft.setTextDatum(TL_DATUM);
    clear();
    drawFrame();
    drawHeader("Tape-LAB");
    drawFooter("MAIN MENU", STATUS_READY);
    refreshMenuSelection();
}

void Display::post(const BootTestResults &results)
{
    clear();
    drawFrame();
    drawHeader("POWER ON SELF TEST");
    drawFooter("BOOTING...", STATUS_BUSY);

    const char *name[] =
    {
        "DISPLAY",
        "TOUCH",
        "STORAGE",
        "RTC",
        "AUDIO"
    };

    bool state[] =
    {
        results.displayOK,
        results.touchOK,
        results.storageOK,
        results.rtcOK,
        results.audioOK
    };

    const int total = 5;

    for(int i = 0; i < total; i++)
    {
        int y = 45 + i * 25;
        drawBootItem(name[i], state[i], y);
    }

    const GeneratorCalibrationBootStatus generatorStatus =
        static_cast<GeneratorCalibrationBootStatus>(
            results.generatorCalibrationStatus);
    const char *generatorText = "FAIL";
    uint16_t generatorColor = TFT_RED;
    if (generatorStatus == GeneratorCalibrationBootStatus::Ok)
    {
        generatorText = "OK";
        generatorColor = TFT_GREEN;
    }
    else if (generatorStatus == GeneratorCalibrationBootStatus::NotCalibrated)
    {
        generatorText = "NOT CAL";
        generatorColor = TFT_YELLOW;
    }
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("GEN CAL", 20, 170, 2);
    tft.setTextColor(generatorColor, COL_BG);
    tft.drawRightString(generatorText, 300, 170, 2);

    drawFooter("TOUCH TO CONTINUE", STATUS_WARNING);
    touch.waitAnyTouch();
}

void Display::drawFrame()
{
    tft.drawRect(
        FRAME_MARGIN,
        FRAME_MARGIN,
        LCD_WIDTH - FRAME_MARGIN * 2,
        LCD_HEIGHT - FRAME_MARGIN * 2,
        COL_FRAME);

    tft.drawRect(
        FRAME_MARGIN + 2,
        FRAME_MARGIN + 2,
        LCD_WIDTH - FRAME_MARGIN * 2 - 4,
        LCD_HEIGHT - FRAME_MARGIN * 2 - 4,
        TFT_DARKGREY);
}

void Display::drawHeader(const char *title)
{
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_TITLE, COL_BG);
    tft.drawString(title,12,8,2);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawRightString(FIRMWARE_VERSION,308,8,2);

    if (sine.isRunning())
    {
        const bool generatorScreen =
            strcmp(title, "AUDIO GENERATOR") == 0;
        const char *status =
            generatorScreen ? "running..." : "GEN-RUN";
        const int titleEnd =
            12 + tft.textWidth(title, 2) + 5;
        const int versionLeft =
            308 - tft.textWidth(FIRMWARE_VERSION, 2);
        const int statusWidth = tft.textWidth(status, 2);
        int statusX =
            titleEnd + (versionLeft - titleEnd - statusWidth) / 2;
        if (strcmp(title, "TRANSPORT ANALYSIS") == 0)
            statusX += 10;

        if (statusX >= titleEnd && statusX + statusWidth <= versionLeft)
        {
            tft.setTextColor(TFT_GREEN, COL_BG);
            tft.drawString(status, statusX, 8, 2);
        }
    }

    tft.drawFastHLine(
        8,
        HEADER_HEIGHT,
        304,
        COL_FRAME);
}

void Display::drawFooter(
    const char *status,
    uint16_t color,
    uint8_t statusFont,
    int statusY)
{
    tft.setTextDatum(TL_DATUM);
    // Șterge complet zona footer
    clearFooter();
    // Redesenează linia de separare
    tft.drawFastHLine(
        8,
        LCD_HEIGHT - FOOTER_HEIGHT - 2,
        304,
        COL_FRAME);

    // Mesajul din stânga
    tft.setTextColor(color, COL_BG);
    tft.drawString(status, 12, statusY, statusFont);

    // Informația din dreapta
    updateClock(true);
}

void Display::updateClock(bool force)
{
    constexpr uint32_t CLOCK_READ_INTERVAL_MS = 250;
    const uint32_t now = millis();

    if (!force && now - lastClockReadMs < CLOCK_READ_INTERVAL_MS)
        return;

    lastClockReadMs = now;

    char currentClock[9];
    rtc.formatTime(currentClock, sizeof(currentClock));

    if (!force && strcmp(currentClock, displayedClock) == 0)
        return;

    const int clockWidth = tft.textWidth("00:00:00", 2);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.setTextPadding(clockWidth);
    tft.drawRightString(currentClock, 308, 215, 2);
    tft.setTextPadding(0);

    strncpy(displayedClock, currentClock, sizeof(displayedClock));
    displayedClock[sizeof(displayedClock) - 1] = '\0';
}

void Display::drawBootItem(const char *name, bool status, int y)
{
    tft.setTextColor(COL_TEXT, COL_BG);
    
    tft.drawString(name,20,y,2);

    if(status)
    {
        tft.setTextColor(TFT_GREEN,COL_BG);
        tft.drawRightString("OK",300,y,2);
    }
    else
    {
        tft.setTextColor(TFT_RED,COL_BG);
        tft.drawRightString("FAIL",300,y,2);
    }

}

void Display::clearFooter()
{
      tft.fillRect(
    10,
    214,
    300,
    16,
    COL_BG);
}

void Display::drawMenuItem(const char *text, int y, bool selected)
{
    // Șterge exact linia pe care o redesenăm
    const uint16_t background =
        selected ? MENU_SELECTION_BACKGROUND : COL_BG;
    tft.fillRect(MENU_X, y - 3, MENU_WIDTH, MENU_HIGHLIGHT_HEIGHT, background);

    if(selected)
        tft.setTextColor(COL_SELECT, background);
    else
        tft.setTextColor(COL_TEXT, background);

    tft.setTextDatum(MC_DATUM);
    tft.drawString(text, LCD_WIDTH / 2, y + MENU_ITEM_HEIGHT / 2, 4);
    tft.setTextDatum(TL_DATUM);
}


void Display::setSelectedItem(int item)
{
    if(item == selectedItem)
        return;

    drawMenuItem(
        menuText[selectedItem],
        menuY[selectedItem],
        false);

    selectedItem = item;

    drawMenuItem(
        menuText[selectedItem],
        menuY[selectedItem],
        true);
}

void Display::debugValue(const char *name, uint32_t value)
{
    char buf[40];
    sprintf(buf, "%s=%lu", name, value);
    drawHeader(buf);
}


void Display::refreshMenuSelection()
{
    for(int i = 0; i < MENU_COUNT; i++)
    {
        drawMenuItem(menuText[i], menuY[i], i == selectedItem);
    }
}

void Display::openApp(
    const char *title,
    const char *status,
    uint8_t statusFont,
    int statusY)
{
    clear();
    drawFrame();
    drawHeader(title);
    drawFooter(status, STATUS_READY, statusFont, statusY);
}

void Display::setAppStatus(
    const char *status,
    uint8_t statusFont,
    int statusY)
{
    drawFooter(status, STATUS_READY, statusFont, statusY);
}

void Display::printCenter(
    const char *text,
    int y,
    int font)
{
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(text,160,y,font);
}

void Display::printLeft(
    const char *text,
    int x,
    int y,
    int font)
{
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(text,x,y,font);
}

int Display::getSelectedItem() const
{
    return selectedItem;
}

void Display::printInt(int xRight, int y, int width, int value, uint8_t font)
{
    char buf[16];
    sprintf(buf, "%*d",width, value);
   
    // Acum textWidth va fi mereu aproape la fel (pt 4 caractere)
    int w = tft.textWidth(buf, font);
    
    // Desenăm aliniat la dreapta în interiorul spațiului alocat
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(buf, xRight - w, y, font);
}

void Display::printFloatDB(int xRight, int y, int width, float value, uint8_t font)
{
    (void)width;
    char buf[16];
    
    int val = (int)(value * 10.0f);
    int whole = abs(val / 10);
    int dec = abs(val % 10);
    char sign = (val >= 0) ? '+' : '-';

    // Dacă whole < 10, adăugăm spațiu înainte de semn
    if (whole < 10) {
        sprintf(buf, "   %c%d.%d", sign, whole, dec);
    } else {
        sprintf(buf, "%c%d.%d", sign, whole, dec);
    }

    int w = tft.textWidth(buf, font);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(buf, xRight - w, y, font);
}

void Display::drawSplashRle()
{
    tft.startWrite();
    tft.setAddrWindow(
        0,
        0,
        SPLASH_RLE_WIDTH,
        SPLASH_RLE_HEIGHT);

    uint32_t position = 0;

    while (position + 1 < SPLASH_RLE_DATA_SIZE)
    {
        const uint8_t count =
            splashRleData[position++];

        const uint8_t paletteIndex =
            splashRleData[position++];

        if (paletteIndex >= SPLASH_RLE_PALETTE_SIZE)
            break;

        const uint16_t color =
            splashRlePalette[paletteIndex];

        tft.pushColor(color, count);
    }

    tft.endWrite();
}

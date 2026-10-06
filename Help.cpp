#include "Help.h"

#include "Config.h"
#include "Touch.h"

#include <TFT_eSPI.h>

extern TFT_eSPI tft;
extern Touch touch;

Help help;

namespace
{
    constexpr uint8_t BUTTON_FONT = 2;
    constexpr uint8_t ICON_FONT = 1;
    constexpr int16_t BUTTON_HEIGHT = 16;
    constexpr int16_t BUTTON_PADDING_X = 5;
    constexpr int16_t ICON_RADIUS = BUTTON_HEIGHT / 2 - 1;

    constexpr int16_t MODAL_X = 16;
    constexpr int16_t MODAL_Y = 27;
    constexpr int16_t MODAL_WIDTH = 288;
    constexpr int16_t MODAL_HEIGHT = 184;
    constexpr int16_t MODAL_INNER_X = MODAL_X + 10;
    constexpr int16_t MODAL_TITLE_Y = MODAL_Y + 8;
    constexpr int16_t MODAL_SEPARATOR_Y = MODAL_Y + 27;
    constexpr int16_t MODAL_TEXT_Y = MODAL_Y + 33;
    constexpr int16_t MODAL_LINE_HEIGHT = 10;
    constexpr uint8_t MODAL_TITLE_FONT = 2;
    constexpr uint8_t MODAL_TEXT_FONT = 1;
    constexpr int16_t OK_WIDTH = 46;
    constexpr int16_t OK_HEIGHT = 22;
    constexpr int16_t OK_X = MODAL_X + (MODAL_WIDTH - OK_WIDTH) / 2;
    constexpr int16_t OK_Y = MODAL_Y + MODAL_HEIGHT - OK_HEIGHT - 5;
    bool contains(
        uint16_t x,
        uint16_t y,
        int16_t left,
        int16_t top,
        int16_t width,
        int16_t height)
    {
        return x >= left && x < left + width &&
               y >= top && y < top + height;
    }

    void drawOkButton(bool pressed, uint16_t modalBackground)
    {
        const uint16_t background =
            pressed ? TFT_DARKGREY : modalBackground;

        tft.fillRect(OK_X, OK_Y, OK_WIDTH, OK_HEIGHT, background);
        tft.drawRect(OK_X, OK_Y, OK_WIDTH, OK_HEIGHT, COL_FRAME);
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_WHITE, background);
        tft.drawCentreString(
            "OK",
            OK_X + OK_WIDTH / 2,
            OK_Y + (OK_HEIGHT - tft.fontHeight(BUTTON_FONT)) / 2,
            BUTTON_FONT);
    }
}

int16_t Help::buttonWidth() const
{
    return tft.textWidth("[?]", BUTTON_FONT) +
           2 * BUTTON_PADDING_X;
}

int16_t Help::buttonHeight() const
{
    return BUTTON_HEIGHT;
}

void Help::drawButton(
    int16_t x,
    int16_t y,
    bool pressed)
{
    drawButton(x, y, pressed, COL_BG);
}

void Help::drawButton(
    int16_t x,
    int16_t y,
    bool pressed,
    uint16_t idleBackground)
{
    const int16_t width = buttonWidth();
    const int16_t centerX = x + width / 2;
    const int16_t centerY = y + BUTTON_HEIGHT / 2;
    const uint16_t iconBackground =
        pressed ? TFT_GREEN : TFT_NAVY;
    const uint16_t iconBorder =
        pressed ? TFT_WHITE : TFT_DARKGREY;
    const uint16_t iconText =
        pressed ? TFT_BLACK : TFT_WHITE;

    tft.fillRect(x, y, width, BUTTON_HEIGHT, idleBackground);
    tft.fillCircle(centerX, centerY, ICON_RADIUS, iconBackground);
    tft.drawCircle(centerX, centerY, ICON_RADIUS, iconBorder);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(iconText, iconBackground);
    tft.drawString("?", centerX, centerY, ICON_FONT);
    tft.setTextDatum(TL_DATUM);
}

bool Help::hitTest(
    uint16_t touchX,
    uint16_t touchY,
    int16_t buttonX,
    int16_t buttonY) const
{
    return contains(
        touchX,
        touchY,
        buttonX,
        buttonY,
        buttonWidth(),
        buttonHeight());
}

void Help::showModal(
    const char *title,
    const char *const lines[],
    uint8_t lineCount)
{
    const uint16_t modalBackground =
        currentBackgroundColor == TFT_DARKNAVY
            ? TFT_BLACK
            : TFT_DARKNAVY;

    tft.fillRect(
        MODAL_X,
        MODAL_Y,
        MODAL_WIDTH,
        MODAL_HEIGHT,
        modalBackground);
    tft.drawRect(
        MODAL_X,
        MODAL_Y,
        MODAL_WIDTH,
        MODAL_HEIGHT,
        COL_FRAME);
    tft.drawRect(
        MODAL_X + 2,
        MODAL_Y + 2,
        MODAL_WIDTH - 4,
        MODAL_HEIGHT - 4,
        TFT_DARKGREY);

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_TITLE, modalBackground);
    tft.drawString(title, MODAL_INNER_X, MODAL_TITLE_Y, MODAL_TITLE_FONT);
    tft.drawFastHLine(
        MODAL_INNER_X,
        MODAL_SEPARATOR_Y,
        MODAL_WIDTH - 20,
        COL_FRAME);

    tft.setTextColor(COL_TEXT, modalBackground);
    for (uint8_t i = 0; i < lineCount; ++i)
    {
        tft.drawString(
            lines[i],
            MODAL_INNER_X,
            MODAL_TEXT_Y + i * MODAL_LINE_HEIGHT,
            MODAL_TEXT_FONT);
    }

    drawOkButton(false, modalBackground);

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
        const bool okTouched =
            contains(x, y, OK_X, OK_Y, OK_WIDTH, OK_HEIGHT);

        if (okTouched)
            drawOkButton(true, modalBackground);

        while (touch.pressed())
            delay(5);

        if (okTouched)
            return;
    }
}

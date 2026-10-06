#include "Icons.h"

#include "Config.h"
#include "Display.h"
#include "Buzzer.h"
#include "Touch.h"
#include <TFT_eSPI.h>
#include <math.h>

extern Display display;
extern TFT_eSPI tft;
extern Touch touch;
extern Buzzer buzzer;

Icons icons;

namespace
{
    constexpr uint8_t ITEM_FONT = 4;
    constexpr int ICON_BUTTON_COLUMNS = 4;
    constexpr int ICON_BUTTON_ROWS = 2;
    constexpr int ICON_BUTTON_GAP = 8;
    constexpr int ICON_BUTTON_WIDTH = 64;
    constexpr int ICON_BUTTON_HEIGHT = 64;
    constexpr int ICON_BUTTON_ROW_GAP = 8 + 10;
    constexpr int ICON_BUTTON_START_X = CONTENT_LEFT + 10;
    constexpr int ICON_BUTTON_START_Y = CONTENT_TOP + 16;
    constexpr int ICON_SECOND_ROW_OFFSET_Y = 5;

    bool isInsideButton(uint16_t touchX, uint16_t touchY, int index)
    {
        const int row = (index - 1) / ICON_BUTTON_COLUMNS;
        const int col = (index - 1) % ICON_BUTTON_COLUMNS;
        const int x = ICON_BUTTON_START_X + col * (ICON_BUTTON_WIDTH + ICON_BUTTON_GAP);
        const int y = ICON_BUTTON_START_Y + row * (ICON_BUTTON_HEIGHT + ICON_BUTTON_ROW_GAP);
        return touchX >= x && touchX < x + ICON_BUTTON_WIDTH &&
               touchY >= y && touchY < y + ICON_BUTTON_HEIGHT;
    }

    void drawIconFrame(int x, int y, int size, uint16_t backgroundColor, uint16_t borderColor)
    {
        tft.fillRect(x, y, size, size, backgroundColor);
        tft.drawRect(x, y, size, size, borderColor);
    }
}

// Deseneaza TOUCH CALIBRATION Icon
void Icons::drawTouchCalibrationIcon(
    int x,
    int y,
    int size,
    uint16_t foregroundColor,
    uint16_t backgroundColor)
{
    drawIconFrame(
        x,
        y,
        size,
        backgroundColor,
        foregroundColor);

    const uint16_t iconColor = TFT_BLUE;

    const int cx = x + size / 2;
    const int cy = y + size / 2;

    const int arm = size / 3;
    const int radius = size / 5;

    // cerc tinta
    tft.drawCircle(
        cx,
        cy,
        radius,
        iconColor);

    // linie orizontala continua
    tft.drawLine(
        cx - arm,
        cy,
        cx + arm,
        cy,
        iconColor);

    // linie verticala continua
    tft.drawLine(
        cx,
        cy - arm,
        cx,
        cy + arm,
        iconColor);

    // punct central
    tft.fillCircle(
        cx,
        cy,
        2,
        TFT_CYAN);
}

// Deseneaza DISPLAY SETTINGS Icon
void Icons::drawDisplaySettingsIcon(
    int x,
    int y,
    int size,
    uint16_t foregroundColor,
    uint16_t backgroundColor)
{
    drawIconFrame(
        x,
        y,
        size,
        backgroundColor,
        foregroundColor);

    const uint16_t frameColor = TFT_CYAN;
    const uint16_t sunColor = TFT_ORANGE;

    const int centerX = x + size / 2;
    const int centerY = y + size / 2;

    const int screenW = size * 3 / 4;
    const int screenH = size * 2 / 3;

    const int screenX = centerX - screenW / 2;
    const int screenY = centerY - screenH / 2 - 2;

    // rama monitorului
    tft.drawRoundRect(
        screenX,
        screenY,
        screenW,
        screenH,
        3,
        frameColor);

    tft.drawRoundRect(
        screenX + 1,
        screenY + 1,
        screenW - 2,
        screenH - 2,
        3,
        frameColor);

    // interior display
    tft.fillRect(
        screenX + 3,
        screenY + 3,
        screenW - 6,
        screenH - 6,
        backgroundColor);

    // soare
    const int sunX = centerX;
    const int sunY = screenY + screenH / 2;
    const int sunR = size / 10;

    tft.fillCircle(
        sunX,
        sunY,
        sunR,
        sunColor);

    const int ray1 = sunR + 2;
    const int ray2 = sunR + 6;

    // raze verticale
    tft.drawLine(sunX, sunY - ray1, sunX, sunY - ray2, sunColor);
    tft.drawLine(sunX, sunY + ray1, sunX, sunY + ray2, sunColor);

    // raze orizontale
    tft.drawLine(sunX - ray1, sunY, sunX - ray2, sunY, sunColor);
    tft.drawLine(sunX + ray1, sunY, sunX + ray2, sunY, sunColor);

    // raze diagonale
    tft.drawLine(sunX - ray1, sunY - ray1,
                 sunX - ray2, sunY - ray2, sunColor);

    tft.drawLine(sunX + ray1, sunY - ray1,
                 sunX + ray2, sunY - ray2, sunColor);

    tft.drawLine(sunX - ray1, sunY + ray1,
                 sunX - ray2, sunY + ray2, sunColor);

    tft.drawLine(sunX + ray1, sunY + ray1,
                 sunX + ray2, sunY + ray2, sunColor);

    // picior monitor
    tft.drawLine(
        centerX,
        screenY + screenH,
        centerX,
        screenY + screenH + size / 8,
        frameColor);

    // baza
    tft.drawLine(
        centerX - size / 6,
        screenY + screenH + size / 8,
        centerX + size / 6,
        screenY + screenH + size / 8,
        frameColor);
}

// Deseneaza AUDIO SETTINGS Icon
void Icons::drawAudioSettingsIcon(
    int x,
    int y,
    int size,
    uint16_t foregroundColor,
    uint16_t backgroundColor)
{
    drawIconFrame(
        x,
        y,
        size,
        backgroundColor,
        foregroundColor);

    const uint16_t speakerColor = TFT_YELLOW;
    const uint16_t waveColor = TFT_CYAN;

    const int centerX = x + size / 2;
    const int centerY = y + size / 2;
    const int inner = size - 14;

    // corp difuzor
    tft.fillRect(
        centerX - inner / 3,
        centerY - inner / 7,
        inner / 8,
        inner / 3,
        speakerColor);

    // con difuzor - ingust in stanga, larg in dreapta
    const int coneLeft = centerX - inner / 5;
    const int coneRight = centerX + inner / 10;

    tft.fillTriangle(
        coneLeft,
        centerY - inner / 8,
        coneLeft,
        centerY + inner / 8,
        coneRight,
        centerY - inner / 3,
        speakerColor);

    tft.fillTriangle(
        coneLeft,
        centerY + inner / 8,
        coneRight,
        centerY - inner / 4,
        coneRight,
        centerY + inner / 3,
        speakerColor);

    // prima unda
    tft.drawLine(
        centerX + inner / 6,
        centerY - inner / 5,
        centerX + inner / 4,
        centerY - inner / 8,
        waveColor);

    tft.drawLine(
        centerX + inner / 4,
        centerY - inner / 8,
        centerX + inner / 4,
        centerY + inner / 8,
        waveColor);

    tft.drawLine(
        centerX + inner / 4,
        centerY + inner / 8,
        centerX + inner / 6,
        centerY + inner / 5,
        waveColor);

    // a doua unda
    tft.drawLine(
        centerX + inner / 3,
        centerY - inner / 3,
        centerX + inner / 2,
        centerY - inner / 5,
        waveColor);

    tft.drawLine(
        centerX + inner / 2,
        centerY - inner / 5,
        centerX + inner / 2,
        centerY + inner / 5,
        waveColor);

    tft.drawLine(
        centerX + inner / 2,
        centerY + inner / 5,
        centerX + inner / 3,
        centerY + inner / 3,
        waveColor);
}

// Deseneaza RESET Factory Icon
void Icons::drawFactoryResetIcon(
    int x,
    int y,
    int size,
    uint16_t foregroundColor,
    uint16_t backgroundColor)
{
    drawIconFrame(
        x,
        y,
        size,
        backgroundColor,
        foregroundColor);

    const uint16_t iconColor = TFT_RED;

    const int cx = x + size / 2;
    const int cy = y + size / 2 + 2;
    const int r = (size - 16) / 3;

    // cerc principal
    tft.drawCircle(
        cx,
        cy,
        r,
        iconColor);
    tft.drawCircle(cx, cy, r - 1, iconColor);

    // intrerupere in partea de sus
    tft.fillRect(
        cx - 4,
        cy - r - 2,
        9,
        9,
        backgroundColor);

    // linia verticala ON/OFF
    tft.drawLine(
        cx,
        cy - r - 5,
        cx,
        cy - r / 3,
        iconColor);

    // o facem putin mai groasa
    tft.drawLine(
        cx + 1,
        cy - r - 5,
        cx + 1,
        cy - r / 3,
        iconColor);
}
// Deseneaza ABOUT Icon
void Icons::drawAboutIcon(
    int x,
    int y,
    int size,
    uint16_t foregroundColor,
    uint16_t backgroundColor)
{
    drawIconFrame(
        x,
        y,
        size,
        backgroundColor,
        foregroundColor);

    const uint16_t iconColor = TFT_GREEN;

    const int cx = x + size / 2;
    const int cy = y + size / 2;

    // partea de sus a ?
    tft.drawLine(cx - 10, cy - 15, cx - 5, cy - 20, iconColor);
    tft.drawLine(cx - 5, cy - 20, cx + 7, cy - 20, iconColor);
    tft.drawLine(cx + 7, cy - 20, cx + 12, cy - 15, iconColor);
    tft.drawLine(cx + 12, cy - 15, cx + 12, cy - 8, iconColor);
    tft.drawLine(cx + 12, cy - 8, cx + 3, cy, iconColor);
    tft.drawLine(cx + 3, cy, cx + 3, cy + 6, iconColor);

    // dublare la 1 px pentru grosime
    tft.drawLine(cx - 10, cy - 14, cx - 5, cy - 19, iconColor);
    tft.drawLine(cx - 5, cy - 19, cx + 7, cy - 19, iconColor);
    tft.drawLine(cx + 7, cy - 19, cx + 11, cy - 14, iconColor);
    tft.drawLine(cx + 11, cy - 14, cx + 11, cy - 8, iconColor);
    tft.drawLine(cx + 11, cy - 8, cx + 2, cy, iconColor);
    tft.drawLine(cx + 2, cy, cx + 2, cy + 6, iconColor);

    // punct
    tft.fillCircle(
        cx + 3,
        cy + 14,
        3,
        iconColor);
}

// Deseneaza 2 HEAD Icon
void Icons::draw2HeadIcon(
    int x,
    int y,
    int size,
    uint16_t foregroundColor,
    uint16_t backgroundColor)
{
    drawIconFrame(x, y, size, backgroundColor, foregroundColor);

    const uint16_t tapeColor = TFT_LIGHTGREY;
    const uint16_t headColor = TFT_CYAN;
    const uint16_t textColor = TFT_YELLOW;

    const int cx = x + size / 2;

    // banda mai sus
    const int tapeY = y + size / 6;

    tft.drawLine(
        x + size / 7,
        tapeY,
        x + size - size / 7,
        tapeY,
        tapeColor);

    tft.drawLine(
        x + size / 7,
        tapeY + 1,
        x + size - size / 7,
        tapeY + 1,
        tapeColor);

    // cap mai mare si aproape de banda
    const int r = size / 5; // înainte era /7
    const int headY = tapeY + r + 3;

    tft.drawCircle(cx, headY, r, headColor);
    tft.drawCircle(cx, headY, r - 1, headColor);

    // intrerupere SCURTA sus
    tft.fillRect(
        cx - 2,
        headY - r - 2,
        5,
        5,
        backgroundColor);

    // R/P sub cap
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(textColor, backgroundColor);

    tft.drawString(
        "R/P",
        cx,
        y + size - 13,
        2);

    tft.setTextDatum(TL_DATUM);
}

// Icon 3 HEAD
void Icons::draw3HeadIcon(
    int x,
    int y,
    int size,
    uint16_t foregroundColor,
    uint16_t backgroundColor)
{
    drawIconFrame(x, y, size, backgroundColor, foregroundColor);

    const uint16_t tapeColor = TFT_LIGHTGREY;
    const uint16_t headColor = TFT_CYAN;
    const uint16_t textColor = TFT_YELLOW;

    const int cx = x + size / 2;

    // banda mai sus
    const int tapeY = y + size / 6;

    tft.drawLine(
        x + size / 7,
        tapeY,
        x + size - size / 7,
        tapeY,
        tapeColor);

    tft.drawLine(
        x + size / 7,
        tapeY + 1,
        x + size - size / 7,
        tapeY + 1,
        tapeColor);

    // capete mai mari
    const int r = size / 6;

    // apropiate intre ele
    const int leftX = cx - r;
    const int rightX = cx + r;

    // apropiate de banda
    const int headY = tapeY + r + 3;

    // RECORD HEAD
    tft.drawCircle(leftX, headY, r, headColor);
    tft.drawCircle(leftX, headY, r - 1, headColor);

    tft.fillRect(
        leftX - 2,
        headY - r - 2,
        5,
        5,
        backgroundColor);

    // PLAY HEAD
    tft.drawCircle(rightX, headY, r, headColor);
    tft.drawCircle(rightX, headY, r - 1, headColor);

    tft.fillRect(
        rightX - 2,
        headY - r - 2,
        5,
        5,
        backgroundColor);

    // R si P
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(textColor, backgroundColor);

    tft.drawString(
        "R",
        leftX,
        y + size - 13,
        2);

    tft.drawString(
        "P",
        rightX,
        y + size - 13,
        2);

    tft.setTextDatum(TL_DATUM);
}

void Icons::drawMemoryChecksIcon(
    int x,
    int y,
    int size,
    uint16_t foregroundColor,
    uint16_t backgroundColor)
{
    drawIconFrame(x, y, size, backgroundColor, foregroundColor);
    const int left = x + size / 4;
    const int right = x + size - size / 5;
    const int top = y + size / 4;
    const int step = size / 5;
    for (uint8_t i = 0; i < 4; ++i)
    {
        const int cy = top + i * step;
        tft.drawLine(left, cy, left + 3, cy + 3, TFT_GREEN);
        tft.drawLine(left + 3, cy + 3, left + 8, cy - 3, TFT_GREEN);
        tft.drawFastHLine(left + 11, cy, right - left - 11, TFT_CYAN);
    }
}

// Deseneaza REC LEVEL Icon
void Icons::drawRecordLevelIcon(
    int x,
    int y,
    int size,
    uint16_t foregroundColor,
    uint16_t backgroundColor)
{
    drawIconFrame(
        x,
        y,
        size,
        backgroundColor,
        foregroundColor);

    const int centerX = x + size / 2;
    const int centerY = y + size / 2;

    const int brickW = size / 12;
    const int brickH = size / 9;
    const int gap = 2;

    const int bricks = 8;

    const int totalWidth =
        bricks * brickW +
        (bricks - 1) * gap;

    const int startX =
        centerX - totalWidth / 2;

    const int vuL_Y =
        centerY - brickH - 5;

    const int vuR_Y =
        centerY + 5;

    // VU LEFT
    for (int i = 0; i < bricks; ++i)
    {
        const uint16_t color =
            (i < 6) ? TFT_CYAN : TFT_RED;

        tft.fillRect(
            startX + i * (brickW + gap),
            vuL_Y,
            brickW,
            brickH,
            color);
    }

    // linie separatoare centru
    tft.drawLine(
        startX,
        centerY,
        startX + totalWidth,
        centerY,
        TFT_LIGHTGREY);

    // VU RIGHT
    for (int i = 0; i < bricks; ++i)
    {
        const uint16_t color =
            (i < 6) ? TFT_CYAN : TFT_RED;

        tft.fillRect(
            startX + i * (brickW + gap),
            vuR_Y,
            brickW,
            brickH,
            color);
    }
}

// Deseneaza BIAS Icon
void Icons::drawBiasIcon(
    int x,
    int y,
    int size,
    uint16_t foregroundColor,
    uint16_t backgroundColor)
{
    drawIconFrame(
        x,
        y,
        size,
        backgroundColor,
        foregroundColor);

    const uint16_t biasColor = TFT_MAGENTA;

    const int centerX = x + size / 2;
    const int centerY = y + size / 2;

    const int waveWidth = size - 16;
    const int waveHeight = size / 4;

    const int startX =
        centerX - waveWidth / 2;

    // axa
    tft.drawLine(
        startX,
        centerY,
        startX + waveWidth,
        centerY,
        TFT_DARKGREY);

    // doua perioade sinusoidale
    int prevX = startX;
    int prevY = centerY;

    for (int i = 1; i <= waveWidth; ++i)
    {
        const float angle =
            (4.0f * PI * i) /
            waveWidth;

        const int xPos =
            startX + i;

        const int yPos =
            centerY -
            static_cast<int>(
                sinf(angle) * waveHeight);

        tft.drawLine(
            prevX,
            prevY,
            xPos,
            yPos,
            biasColor);

        prevX = xPos;
        prevY = yPos;
    }
}

// Draws the Dolby double-D mark.
void Icons::drawDolbyCheckIcon(
    int x,
    int y,
    int size,
    uint16_t foregroundColor,
    uint16_t backgroundColor)
{
    drawIconFrame(
        x,
        y,
        size,
        backgroundColor,
        foregroundColor);

    const uint16_t markColor = TFT_CYAN;
    const int boxLeft = x + size / 8;
    const int boxRight = x + size - size / 8 - 1;
    const int originalBoxTop = y + size / 5;
    const int originalBoxBottom = y + size - size / 5 - 1;
    const int originalBoxHeight = originalBoxBottom - originalBoxTop + 1;
    const int baseBoxHeight = originalBoxHeight * 2 / 3;
    const int boxHeight = baseBoxHeight + 6;
    const float glyphScale =
        static_cast<float>(boxHeight) / baseBoxHeight;
    const int boxTop = y + size / 8;
    const int boxBottom = boxTop + boxHeight - 1;
    const int originalGlyphTop = originalBoxTop + size / 10;
    const int originalGlyphBottom = originalBoxBottom - size / 10;
    const int originalGlyphHeight = originalGlyphBottom - originalGlyphTop + 1;
    const int glyphHeight = static_cast<int>(roundf(
        originalGlyphHeight * 2.0f / 3.0f * glyphScale));
    const int glyphTop = boxTop + (boxHeight - glyphHeight) / 2;
    const int glyphBottom = glyphTop + glyphHeight - 1;
    const int markCenterX = (boxLeft + boxRight) / 2;
    const int originalLeftStem = boxLeft + size / 8;
    const int originalRightStem = boxRight - size / 8;
    const int leftStem = markCenterX - static_cast<int>(roundf(
                                           (markCenterX - originalLeftStem) * glyphScale));
    const int rightStem = markCenterX + static_cast<int>(roundf(
                                            (originalRightStem - markCenterX) * glyphScale));
    const int curveRadiusX = static_cast<int>(roundf(size / 8.0f * glyphScale));
    const int capWidth = static_cast<int>(roundf(size / 12.0f * glyphScale));
    const int leftCurveCenter = leftStem + capWidth;
    const int rightCurveCenter = rightStem - capWidth;

    tft.fillRect(boxLeft, boxTop,
                 boxRight - boxLeft + 1, boxBottom - boxTop + 1, markColor);

    for (int row = glyphTop; row <= glyphBottom; ++row)
    {
        const float normalizedY =
            2.0f * (row - glyphTop) / (glyphBottom - glyphTop) - 1.0f;
        const int curveOffset = static_cast<int>(roundf(
            curveRadiusX * sqrtf(1.0f - normalizedY * normalizedY)));
        const int leftEdge = leftCurveCenter + curveOffset;
        const int rightEdge = rightCurveCenter - curveOffset;

        tft.drawFastHLine(
            leftStem, row, leftEdge - leftStem + 1, backgroundColor);
        tft.drawFastHLine(
            rightEdge, row, rightStem - rightEdge + 1, backgroundColor);
    }

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(markColor, backgroundColor);
    tft.drawString(
        "DOLBY",
        x + size / 2,
        (boxBottom + y + size) / 2,
        2);
    tft.setTextDatum(TL_DATUM);
}

void Icons::drawAtcIcon(
    int x,
    int y,
    int size,
    uint16_t foregroundColor,
    uint16_t backgroundColor)
{
    drawIconFrame(x, y, size, backgroundColor, foregroundColor);

    // Three equal vertical calibration stages.
    const uint16_t colors[] = {TFT_MAGENTA, TFT_YELLOW, TFT_CYAN};
    const char *labels[] = {"BIAS", "EQ", "LEVEL"};
    tft.setTextDatum(MC_DATUM);
    for (uint8_t i = 0; i < 3; ++i)
    {
        const int lineY = y + size * (i + 1) / 4;
        tft.setTextColor(colors[i], backgroundColor);
        tft.drawString(labels[i], x + size / 2, lineY, 2);
    }
    tft.setTextDatum(TL_DATUM);
}

//========== Deseneaza TAPE EQ Icon =================
void Icons::drawTapeEqIcon(
    int x,
    int y,
    int size,
    uint16_t foregroundColor,
    uint16_t backgroundColor)
{
    drawIconFrame(
        x,
        y,
        size,
        backgroundColor,
        foregroundColor);

    const uint16_t axisColor = TFT_LIGHTGREY;
    const uint16_t barColor1 = TFT_CYAN;
    const uint16_t barColor2 = TFT_YELLOW;

    const int centerX = x + size / 2;
    const int centerY = y + size / 2;
    const int inner = size - 16;

    const int barWidth = 4;
    const int gap = 3;

    const int heights[] =
        {
            inner / 5,
            inner / 3,
            inner / 2,
            inner / 4,
            inner * 2 / 3,
            inner / 2,
            inner / 3};

    constexpr int barCount =
        sizeof(heights) / sizeof(heights[0]);

    const int totalWidth =
        barCount * barWidth +
        (barCount - 1) * gap;

    int barX = centerX - totalWidth / 2;

    // linia de baza
    const int baseY = centerY + inner / 3;

    tft.drawLine(
        barX - 3,
        baseY,
        barX + totalWidth + 3,
        baseY,
        axisColor);

    // barele
    for (int i = 0; i < barCount; ++i)
    {
        const int h = heights[i];

        const uint16_t color =
            (i >= barCount - 2)
                ? barColor2
                : barColor1;

        tft.fillRect(
            barX,
            baseY - h,
            barWidth,
            h,
            color);

        barX += barWidth + gap;
    }
}

//============== deseneaza AUTO CAL Icon ====================
void Icons::drawAutoCalIcon(
    int x,
    int y,
    int size,
    uint16_t foregroundColor,
    uint16_t backgroundColor)
{
    drawIconFrame(
        x,
        y,
        size,
        backgroundColor,
        foregroundColor);

    const int centerX = x + size / 2;
    const int centerY = y + size / 2;

    const uint16_t beamColor = TFT_CYAN;
    const uint16_t panColor = TFT_YELLOW;
    const uint16_t baseColor = TFT_ORANGE;

    const int beamHalf = size / 3;
    const int beamY = centerY - size / 7;

    // Bara balanței - dublată pentru grosime
    tft.drawLine(
        centerX - beamHalf,
        beamY,
        centerX + beamHalf,
        beamY,
        beamColor);

    tft.drawLine(
        centerX - beamHalf,
        beamY + 1,
        centerX + beamHalf,
        beamY + 1,
        beamColor);

    // Pivot central
    tft.fillCircle(
        centerX,
        beamY,
        3,
        TFT_YELLOW);

    // Firele talerelor
    const int leftX = centerX - beamHalf + 5;
    const int rightX = centerX + beamHalf - 5;

    const int panY = centerY + size / 10;

    tft.drawLine(
        leftX,
        beamY + 2,
        leftX - 7,
        panY,
        beamColor);

    tft.drawLine(
        leftX,
        beamY + 2,
        leftX + 7,
        panY,
        beamColor);

    tft.drawLine(
        rightX,
        beamY + 2,
        rightX - 7,
        panY,
        beamColor);

    tft.drawLine(
        rightX,
        beamY + 2,
        rightX + 7,
        panY,
        beamColor);

    // Talere
    tft.drawLine(
        leftX - 9,
        panY,
        leftX + 9,
        panY,
        panColor);

    tft.drawLine(
        leftX - 7,
        panY + 1,
        leftX + 7,
        panY + 1,
        panColor);

    tft.drawLine(
        rightX - 9,
        panY,
        rightX + 9,
        panY,
        panColor);

    tft.drawLine(
        rightX - 7,
        panY + 1,
        rightX + 7,
        panY + 1,
        panColor);

    // Suport triunghiular
    const int baseY = centerY + size / 3;

    tft.fillTriangle(
        centerX,
        beamY + 5,
        centerX - size / 7,
        baseY,
        centerX + size / 7,
        baseY,
        TFT_MAROON); // baseColor

    tft.drawTriangle(
        centerX,
        beamY + 5,
        centerX - size / 7,
        baseY,
        centerX + size / 7,
        baseY,
        baseColor); //

    // Mic pivot vizibil peste suport
    tft.fillCircle(
        centerX,
        beamY,
        2,
        panColor);
}

//=============== Deseneaza TAPE EQ TEST Icon ==============
void Icons::drawTapeTestIcon(
    int x,
    int y,
    int size,
    uint16_t foregroundColor,
    uint16_t backgroundColor)
{
    drawIconFrame(
        x,
        y,
        size,
        backgroundColor,
        foregroundColor);

    const uint16_t bodyColor = TFT_MAGENTA;
    const uint16_t reelColor = TFT_CYAN;
    const uint16_t hubColor = TFT_YELLOW;
    const uint16_t mechColor = TFT_MAGENTA;

    const int centerX = x + size / 2;
    const int centerY = y + size / 2;

    const int left = x + 9;
    const int right = x + size - 9;
    const int top = y + 13;
    const int bottom = y + size - 13;

    // Corp caseta
    tft.drawRoundRect(
        left,
        top,
        right - left,
        bottom - top,
        3,
        TFT_BLUE);

    tft.drawRoundRect(
        left + 1,
        top + 1,
        right - left - 2,
        bottom - top - 2,
        3,
        bodyColor);

    // Role
    const int reelY = centerY - 5;
    const int reelRadius = size / 7;

    const int leftReelX = centerX - size / 5;
    const int rightReelX = centerX + size / 5;

    tft.drawCircle(
        leftReelX,
        reelY,
        reelRadius,
        reelColor);

    tft.drawCircle(
        rightReelX,
        reelY,
        reelRadius,
        reelColor);

    // Butuci centrali
    const int hubRadius = 4;

    tft.fillCircle(
        leftReelX,
        reelY,
        hubRadius,
        hubColor);

    tft.fillCircle(
        rightReelX,
        reelY,
        hubRadius,
        hubColor);

    // 3 spite la 120 grade - rola stanga
    for (int i = 0; i < 3; ++i)
    {
        const float angle =
            (i * 120.0f - 90.0f) * PI / 180.0f;

        const int x1 =
            leftReelX +
            static_cast<int>(cosf(angle) * hubRadius);

        const int y1 =
            reelY +
            static_cast<int>(sinf(angle) * hubRadius);

        const int x2 =
            leftReelX +
            static_cast<int>(cosf(angle) * (reelRadius - 1));

        const int y2 =
            reelY +
            static_cast<int>(sinf(angle) * (reelRadius - 1));

        tft.drawLine(
            x1,
            y1,
            x2,
            y2,
            hubColor);
    }

    // 3 spite la 120 grade - rola dreapta
    for (int i = 0; i < 3; ++i)
    {
        const float angle =
            (i * 120.0f - 90.0f) * PI / 180.0f;

        const int x1 =
            rightReelX +
            static_cast<int>(cosf(angle) * hubRadius);

        const int y1 =
            reelY +
            static_cast<int>(sinf(angle) * hubRadius);

        const int x2 =
            rightReelX +
            static_cast<int>(cosf(angle) * (reelRadius - 1));

        const int y2 =
            reelY +
            static_cast<int>(sinf(angle) * (reelRadius - 1));

        tft.drawLine(
            x1,
            y1,
            x2,
            y2,
            hubColor);
    }

    // Banda intre role
    tft.drawLine(
        leftReelX + reelRadius,
        reelY,
        rightReelX - reelRadius,
        reelY,
        reelColor);

    // Trapezul inferior al casetei
    // baza mica sus, baza mare jos
    const int trapTopY = centerY + size / 7;
    const int trapBottomY = bottom - 4;

    const int trapTopHalf = size / 7;
    const int trapBottomHalf = size / 4;

    tft.drawLine(
        centerX - trapTopHalf,
        trapTopY,
        centerX + trapTopHalf,
        trapTopY,
        mechColor);

    tft.drawLine(
        centerX - trapTopHalf,
        trapTopY,
        centerX - trapBottomHalf,
        trapBottomY,
        mechColor);

    tft.drawLine(
        centerX + trapTopHalf,
        trapTopY,
        centerX + trapBottomHalf,
        trapBottomY,
        mechColor);

    tft.drawLine(
        centerX - trapBottomHalf,
        trapBottomY,
        centerX + trapBottomHalf,
        trapBottomY,
        mechColor);

    // Bulinele de fixare
    tft.fillCircle(
        centerX - size / 10,
        trapBottomY - 4,
        2,
        mechColor);

    tft.fillCircle(
        centerX + size / 10,
        trapBottomY - 4,
        2,
        mechColor);
}

void Icons::drawButton(int index, int x, int y, int size, bool pressed)
{
    const uint16_t idleBackground = getAdaptiveIconBackground();

    const uint16_t background =
        pressed ? TFT_DARKGREY : idleBackground;

    uint16_t iconColor = TFT_WHITE;

    switch (index)
    {
    case 1:
        iconColor = TFT_GREEN;
        break; // PLAYBACK
    case 2:
        iconColor = TFT_RED;
        break; // RECORD
    case 3:
        iconColor = TFT_ORANGE;
        break; // SPECTRUM
    case 4:
        iconColor = TFT_YELLOW;
        break; // GENERATOR
    case 5:
        iconColor = TFT_CYAN;
        break; // SCOPE PLACEHOLDER
    case 6:
        iconColor = TFT_LIGHTGREY;
        break; // TRANSPORT
    case 7:
        iconColor = TFT_CYAN;
        break; // SETTINGS
    case 8:
        iconColor = TFT_MAGENTA;
        break; // ALIGNMENT
    }

    const uint16_t foreground = pressed ? TFT_WHITE : iconColor;
    const uint16_t border = pressed ? iconColor : TFT_DARKGREY;

    tft.fillRect(x, y, size, size, background);
    tft.drawRect(x, y, size, size, border);

    const int centerX = x + size / 2;
    const int centerY = y + size / 2;
    const int inner = size - 14;

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(foreground, background);

    switch (index)
    {
    case 7: // Roata settings
    {
        // Gear / Settings icon
        constexpr int TEETH = 8;

        const float rootRadius = inner * 0.30f;
        const float outerRadius = inner * 0.46f;
        const float toothHalf = 0.16f; // half angular width of each tooth

        // Corpul central al rotii
        tft.fillCircle(centerX, centerY, rootRadius + 2, foreground);

        // 8 dinti reali, fiecare desenat ca un trapez
        for (int i = 0; i < TEETH; ++i)
        {
            const float angle = i * (2.0f * PI / TEETH);

            const float a1 = angle - toothHalf;
            const float a2 = angle + toothHalf;

            const int x1 = centerX + cosf(a1) * rootRadius;
            const int y1 = centerY + sinf(a1) * rootRadius;

            const int x2 = centerX + cosf(a1) * outerRadius;
            const int y2 = centerY + sinf(a1) * outerRadius;

            const int x3 = centerX + cosf(a2) * outerRadius;
            const int y3 = centerY + sinf(a2) * outerRadius;

            const int x4 = centerX + cosf(a2) * rootRadius;
            const int y4 = centerY + sinf(a2) * rootRadius;

            // Trapez = doua triunghiuri
            tft.fillTriangle(x1, y1, x2, y2, x3, y3, foreground);
            tft.fillTriangle(x1, y1, x3, y3, x4, y4, foreground);
        }

        // Gaura centrala
        tft.fillCircle(centerX, centerY, inner / 5, background);

        // Contur interior pentru definitie mai buna
        tft.drawCircle(centerX, centerY, inner / 5, foreground);

        break;
    }

    case 1:
    {
        // PLAY icon
        const int halfHeight = inner / 3;
        const int leftX = centerX - inner / 5;
        const int rightX = centerX + inner / 3;

        tft.fillTriangle(
            leftX, centerY - halfHeight,
            leftX, centerY + halfHeight,
            rightX, centerY,
            foreground);
        tft.drawTriangle(
            leftX, centerY - halfHeight,
            leftX, centerY + halfHeight,
            rightX, centerY,
            TFT_LIGHTGREY);

        break;
    }
    case 2:
    {
        // REC icon
        const int radius = inner / 4;

        tft.fillCircle(
            centerX,
            centerY,
            radius,
            foreground);

        tft.drawCircle(
            centerX,
            centerY,
            radius,
            TFT_LIGHTGREY);

        break;
    }

    case 4:
    {
        // GENERATOR icon design
        const int waveWidth = size - 16;
        const int waveHeight = size / 4;
        const int startX = centerX - waveWidth / 2;

        tft.drawLine(
            startX,
            centerY,
            startX + waveWidth,
            centerY,
            TFT_DARKGREY);

        int prevX = startX;
        int prevY = centerY;

        for (int i = 1; i <= waveWidth; ++i)
        {
            const float angle = (4.0f * PI * i) / waveWidth;
            const int x = startX + i;
            const int y = centerY - static_cast<int>(sinf(angle) * waveHeight);

            tft.drawLine(
                prevX,
                prevY,
                x,
                y,
                iconColor);

            prevX = x;
            prevY = y;
        }

        break;
    }
    case 3:
    {
        // Spectrum Analyzer icon
        const int barWidth = 4;
        const int gap = 3;

        const int heights[] =
            {
                inner / 5,
                inner / 3,
                inner / 2,
                inner / 4,
                inner * 2 / 3,
                inner / 2,
                inner / 3};

        constexpr int barCount = sizeof(heights) / sizeof(heights[0]);

        const int totalWidth =
            barCount * barWidth +
            (barCount - 1) * gap;

        int x = centerX - totalWidth / 2;

        // Linia de baza
        const int baseY = centerY + inner / 3;

        tft.drawLine(
            x - 3,
            baseY,
            x + totalWidth + 3,
            baseY,
            TFT_LIGHTGREY);

        // Barele spectrului
        for (int i = 0; i < barCount; ++i)
        {
            const int h = heights[i];

            tft.fillRect(
                x,
                baseY - h,
                barWidth,
                h,
                foreground);

            x += barWidth + gap;
        }

        break;
    }
    case 5:
    {
        // Oscilloscope icon - square CRT with dotted grid
        const int scopeSize = inner;
        const int left = centerX - scopeSize / 2;
        const int top = centerY - scopeSize / 2;
        const int right = left + scopeSize - 1;
        const int bottom = top + scopeSize - 1;

        // Rama CRT
        tft.drawRoundRect(left, top, scopeSize, scopeSize, 4, TFT_WHITE);

        tft.drawLine(
            left,
            centerY,
            right,
            centerY,
            TFT_DARKGREY);

        // 2 linii verticale punctate
        const int v1 = left + scopeSize / 3;
        const int v2 = left + (scopeSize * 2) / 3;

        for (int y = top + 2; y < bottom - 1; y += 4)
        {
            tft.drawPixel(v1, y, TFT_CYAN);
            tft.drawPixel(v2, y, TFT_CYAN);
        }

        // 2 linii orizontale punctate
        const int h1 = top + scopeSize / 3;
        const int h2 = top + (scopeSize * 2) / 3;

        for (int x = left + 2; x < right - 1; x += 4)
        {
            tft.drawPixel(x, h1, TFT_CYAN);
            tft.drawPixel(x, h2, TFT_CYAN);
        }

        // Unda ramane in interior
        const int x1 = left + 5;
        const int x2 = left + scopeSize / 4;
        const int x3 = centerX;
        const int x4 = right - scopeSize / 4;
        const int x5 = right - 5;

        tft.drawLine(x1, centerY, x2, centerY - scopeSize / 5, TFT_CYAN);
        tft.drawLine(x2, centerY - scopeSize / 5, x3, centerY + scopeSize / 5, TFT_CYAN);
        tft.drawLine(x3, centerY + scopeSize / 5, x4, centerY - scopeSize / 6, TFT_CYAN);
        tft.drawLine(x4, centerY - scopeSize / 6, x5, centerY, TFT_CYAN);

        break;
    }
    case 6:
    {
        // TRANSPORT / Sony-style reel with 3 trapezoidal spokes

        const int outerR = inner / 2 - 2;
        const int hubR = inner / 8 + 1;

        const int spokeInnerR = hubR + 4;
        const int spokeOuterR = outerR - 4;

        const float innerHalfWidth = 4.0f;
        const float outerHalfWidth = 8.0f;

        // inel exterior
        tft.drawCircle(centerX, centerY, outerR, foreground);
        tft.drawCircle(centerX, centerY, outerR - 1, foreground);
        tft.drawCircle(centerX, centerY, outerR - 6, TFT_WHITE);

        // 3 spite
        for (int i = 0; i < 3; ++i)
        {
            const float a = -PI / 2.0f + i * (2.0f * PI / 3.0f);

            const float ca = cosf(a);
            const float sa = sinf(a);

            const float px = -sinf(a);
            const float py = cosf(a);

            const int x1 = centerX + (int)(ca * spokeInnerR + px * innerHalfWidth);
            const int y1 = centerY + (int)(sa * spokeInnerR + py * innerHalfWidth);

            const int x2 = centerX + (int)(ca * spokeInnerR - px * innerHalfWidth);
            const int y2 = centerY + (int)(sa * spokeInnerR - py * innerHalfWidth);

            const int x3 = centerX + (int)(ca * spokeOuterR - px * outerHalfWidth);
            const int y3 = centerY + (int)(sa * spokeOuterR - py * outerHalfWidth);

            const int x4 = centerX + (int)(ca * spokeOuterR + px * outerHalfWidth);
            const int y4 = centerY + (int)(sa * spokeOuterR + py * outerHalfWidth);

            tft.fillTriangle(x1, y1, x2, y2, x3, y3, TFT_MAROON);
            tft.fillTriangle(x1, y1, x3, y3, x4, y4, TFT_MAROON);
            tft.drawLine(x4, y4, x1, y1, foreground);
            tft.drawLine(x1, y1, x2, y2, foreground);
            tft.drawLine(x3, y3, x2, y2, foreground);
        }

        // hub central
        tft.fillCircle(centerX, centerY, hubR, background);
        tft.drawCircle(centerX, centerY, hubR, foreground);
        tft.fillCircle(centerX, centerY, 2, foreground);

        break;
    }
    case 8:
    {
        // CALIBRATION / ALIGNMENT icon

        const int leftX = centerX - inner / 5;
        const int rightX = centerX + inner / 5;

        const int topY = centerY - inner / 3;
        const int bottomY = centerY + inner / 3;

        // sinele celor doua reglaje
        tft.drawLine(leftX, topY, leftX, bottomY, TFT_MAGENTA);
        tft.drawLine(rightX, topY, rightX, bottomY, TFT_MAGENTA);

        // cursoare
        const int knobW = inner / 4;
        const int knobH = 6;

        tft.fillRect(
            leftX - knobW / 2,
            centerY - inner / 6,
            knobW,
            knobH,
            foreground);

        tft.fillRect(
            rightX - knobW / 2,
            centerY + inner / 8,
            knobW,
            knobH,
            foreground);

        tft.drawRect(
            leftX - knobW / 2,
            centerY - inner / 6,
            knobW,
            knobH,
            TFT_WHITE);

        tft.drawRect(
            rightX - knobW / 2,
            centerY + inner / 8,
            knobW,
            knobH,
            TFT_WHITE);

        break;
    }
    }

    tft.setTextDatum(TL_DATUM);
}

void Icons::showSettingsScreen()
{
    display.openApp("ICONS SETTINGS", "TOUCH TO BACK");

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, COL_BG);

    // Textele pentru cele 8 iconuri
    const char *labels[] =
        {
            "PLAYBACK",
            "RECORD",
            "SPECTR.",
            "GEN.",
            "SCOPE",
            "TRANSP.",
            "SETTINGS",
            "ALIGN."};

    for (int row = 0; row < ICON_BUTTON_ROWS; ++row)
    {
        for (int col = 0; col < ICON_BUTTON_COLUMNS; ++col)
        {
            const int index = row * ICON_BUTTON_COLUMNS + col + 1;

            const int x =
                ICON_BUTTON_START_X +
                col * (ICON_BUTTON_WIDTH + ICON_BUTTON_GAP);

            const int y =
                ICON_BUTTON_START_Y +
                row * (ICON_BUTTON_HEIGHT + ICON_BUTTON_ROW_GAP) +
                (row == 1 ? ICON_SECOND_ROW_OFFSET_Y : 0);

            // Deseneaza iconul
            drawButton(
                index,
                x,
                y,
                ICON_BUTTON_WIDTH,
                selectedIndex == index);

            // Text sub icon
            tft.setTextDatum(TC_DATUM);
            tft.setTextFont(2);
            tft.setTextColor(TFT_WHITE, COL_BG);

            tft.drawString(
                labels[index - 1],
                x + ICON_BUTTON_WIDTH / 2,
                y + ICON_BUTTON_HEIGHT + 3);
        }
    }

    // Revenim la datum-ul standard al proiectului
    tft.setTextDatum(TL_DATUM);
}

bool Icons::handleTouch(uint16_t touchX, uint16_t touchY)
{
    for (int index = 1;
         index <= ICON_BUTTON_COLUMNS * ICON_BUTTON_ROWS;
         ++index)
    {
        if (isInsideButton(touchX, touchY, index))
        {
            const int oldIndex = selectedIndex;
            // Daca apasam acelasi icon, nu mai avem nimic de redesenat
            if (oldIndex == index)
                return true;

            selectedIndex = index;
            return true;
        }
    }

    return false;
}

void Icons::setBackgroundColor(uint16_t backgroundColor)
{
    pageBackgroundColor = backgroundColor;
}

uint16_t Icons::getAdaptiveIconBackground() const
{
    return (pageBackgroundColor == TFT_DARKNAVY) ? 0x000F : TFT_DARKNAVY;
}

int Icons::handleMainTouch(uint16_t touchX, uint16_t touchY)
{
    for (int index = 1;
         index <= ICON_BUTTON_COLUMNS * ICON_BUTTON_ROWS;
         ++index)
    {
        if (isInsideButton(touchX, touchY, index))
        {
            // A doua atingere pe acelasi icon = ENTER
            if (selectedIndex == index)
            {
                return index;
            }

            const int oldIndex = selectedIndex;
            selectedIndex = index;

            // Scoatem rama de selectie de pe iconul anterior
            if (oldIndex >= 1 && oldIndex <= 8)
            {
                drawSelectionBorder(oldIndex, false);
            }

            // Evidentiem iconul nou
            drawSelectionBorder(index, true);

            // Prima atingere = numai SELECT
            return 0;
        }
    }

    return 0;
}

void Icons::showMainScreen()
{

    display.openApp("TAPELAB", "READY");

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, COL_BG);

    const char *labels[] =
        {
            "PLAYBACK",
            "RECORD",
            "SPECTR.",
            "GEN.",
            "SCOPE",
            "TRANSP.",
            "SETTINGS",
            "ALIGN."};

    for (int row = 0; row < ICON_BUTTON_ROWS; ++row)
    {
        for (int col = 0; col < ICON_BUTTON_COLUMNS; ++col)
        {
            const int index =
                row * ICON_BUTTON_COLUMNS + col + 1;

            const int x =
                ICON_BUTTON_START_X +
                col * (ICON_BUTTON_WIDTH + ICON_BUTTON_GAP);

            const int y =
                ICON_BUTTON_START_Y +
                row * (ICON_BUTTON_HEIGHT + ICON_BUTTON_ROW_GAP) +
                (row == 1 ? ICON_SECOND_ROW_OFFSET_Y : 0);

            drawButton(
                index,
                x,
                y,
                ICON_BUTTON_WIDTH,
                false);

            tft.setTextDatum(TC_DATUM);
            tft.setTextFont(2);
            tft.setTextColor(TFT_WHITE, COL_BG);

            tft.drawString(
                labels[index - 1],
                x + ICON_BUTTON_WIDTH / 2,
                y + ICON_BUTTON_HEIGHT + 3);
        }
    }

    tft.setTextDatum(TL_DATUM);
}

void Icons::drawSelectionBorder(int index, bool selected)
{
    if (index < 1 || index > 8)
        return;

    const int row =
        (index - 1) / ICON_BUTTON_COLUMNS;

    const int col =
        (index - 1) % ICON_BUTTON_COLUMNS;

    const int x =
        ICON_BUTTON_START_X +
        col * (ICON_BUTTON_WIDTH + ICON_BUTTON_GAP);

    const int y =
        ICON_BUTTON_START_Y +
        row * (ICON_BUTTON_HEIGHT + ICON_BUTTON_ROW_GAP) +
        (row == 1 ? ICON_SECOND_ROW_OFFSET_Y : 0);

    const uint16_t idleBackground =
        (pageBackgroundColor == TFT_DARKNAVY)
            ? 0x000F
            : TFT_DARKNAVY;

    const uint16_t color =
        selected ? TFT_GREEN : idleBackground;

    tft.drawRect(
        x,
        y,
        ICON_BUTTON_WIDTH,
        ICON_BUTTON_WIDTH,
        color);

    tft.drawRect(
        x + 1,
        y + 1,
        ICON_BUTTON_WIDTH - 2,
        ICON_BUTTON_WIDTH - 2,
        color);
}

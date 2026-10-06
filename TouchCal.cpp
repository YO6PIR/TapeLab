#include "TouchCal.h"
#include "Config.h"
#include "Storage.h"
#include "Buzzer.h"
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

extern TFT_eSPI tft;
extern XPT2046_Touchscreen ts;
extern Touch touch;
extern Buzzer buzzer;

namespace
{
    constexpr int INTRO_CENTER_X = 160;
    constexpr int INTRO_START_LINE_1_Y = 90;
    constexpr int INTRO_START_LINE_2_Y = 110;
    constexpr int INTRO_START_PADDING_X = 12;
    constexpr int INTRO_START_PADDING_Y = 10;
    constexpr uint8_t INTRO_FONT = 4;
}

void TouchCal::drawIntro()
{
    tft.fillScreen(COL_BG);
    tft.setTextColor(TFT_CYAN, COL_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("TOUCH CALIBRATION",160,40,4);
    tft.setTextColor(TFT_WHITE,COL_BG);
    tft.drawString(
        "Touch the center",
        INTRO_CENTER_X,
        INTRO_START_LINE_1_Y,
        INTRO_FONT);
    tft.drawString(
        "to begin",
        INTRO_CENTER_X,
        INTRO_START_LINE_2_Y,
        INTRO_FONT);
    tft.drawString("Touch anywhere",160,170,4);
    tft.drawString("to EXIT",160,190,4);
    tft.setTextDatum(TL_DATUM);
}

void TouchCal::drawTarget(int x,int y)
{
    tft.drawFastHLine(x-12,y,24,TFT_WHITE);
    tft.drawFastVLine(x,y-12,24,TFT_WHITE);
    tft.drawCircle(x,y,6,TFT_YELLOW);
    tft.fillCircle(x,y,2,TFT_YELLOW);
}

void TouchCal::clearTarget(int x,int y)
{
    tft.fillRect(x-15,y-15,30,30,COL_BG);
}

void TouchCal::drawMessage(const char *msg)
{
    tft.fillRect(0,210,320,30,COL_BG);
    tft.setTextColor(TFT_GREEN,COL_BG);
    tft.drawCentreString(msg,160,215,2);
}

bool TouchCal::waitTouch(uint16_t &rawX, uint16_t &rawY)
{
    while(!ts.touched());
    TS_Point p = ts.getPoint();

    rawX = p.x;
    rawY = p.y;

    while(ts.touched());

    delay(150);
    if (touchFeedbackEnabled)
        buzzer.beepNow(20);

    return true;
}

bool TouchCal::run(TouchCalibration &cal, bool touchFeedback)
{
    touchFeedbackEnabled = touchFeedback;
    drawIntro();
    waitRelease();
    if (!confirmCalibrationStart())
        return false;

    tft.fillScreen(COL_BG);

    //==========================
    // Colectare puncte
    //==========================
    drawTarget(TOUCH_CAL_TARGET_LEFT, TOUCH_CAL_TARGET_TOP);
    waitTouch(rawX[0],rawY[0]);
    clearTarget(TOUCH_CAL_TARGET_LEFT, TOUCH_CAL_TARGET_TOP);

    drawTarget(TOUCH_CAL_TARGET_RIGHT, TOUCH_CAL_TARGET_TOP);
    waitTouch(rawX[1],rawY[1]);
    clearTarget(TOUCH_CAL_TARGET_RIGHT, TOUCH_CAL_TARGET_TOP);

    drawTarget(TOUCH_CAL_TARGET_RIGHT, TOUCH_CAL_TARGET_BOTTOM);
    waitTouch(rawX[2],rawY[2]);
    clearTarget(TOUCH_CAL_TARGET_RIGHT, TOUCH_CAL_TARGET_BOTTOM);

    drawTarget(TOUCH_CAL_TARGET_LEFT, TOUCH_CAL_TARGET_BOTTOM);
    waitTouch(rawX[3],rawY[3]);
    clearTarget(TOUCH_CAL_TARGET_LEFT, TOUCH_CAL_TARGET_BOTTOM);

    drawTarget(160,120);
    waitTouch(rawX[4],rawY[4]);
    clearTarget(160,120);

    //==========================
    // Calculează calibrarea
    //==========================
    cal = calculateCalibration();
    bool passed = checkCalibration(cal);

    //==========================
    // Afișează rezultatele
    //==========================
    tft.fillScreen(COL_BG);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, COL_BG);
    tft.drawString("TOUCH CALIBRATION",10,10,2);
    tft.drawString("XMIN :",10,50,2);
    tft.drawNumber(cal.xmin,120,50,2);
    tft.drawString("XMAX :",10,75,2);
    tft.drawNumber(cal.xmax,120,75,2);
    tft.drawString("YMIN :",10,100,2);
    tft.drawNumber(cal.ymin,120,100,2);
    tft.drawString("YMAX :",10,125,2);
    tft.drawNumber(cal.ymax,120,125,2);

    if(passed)
    {
        tft.setTextColor(COL_BG, TFT_GREEN);
        tft.drawString("STATUS : PASSED",10,155,2);
    }
    else
    {
        tft.setTextColor(COL_BG, TFT_RED);
        tft.drawString("STATUS : FAILED",10,155,2);
    }

    tft.setTextColor(TFT_WHITE, COL_BG);
    tft.drawString("Touch to continue...",10,205,2);

    //==========================
    // Așteaptă utilizatorul
    //==========================
    uint16_t dummyX, dummyY;
    waitTouch(dummyX, dummyY);

    return passed;
}

void TouchCal::waitRelease()
{
    while (ts.touched());
    delay(150);
}

bool TouchCal::confirmCalibrationStart()
{
    uint16_t rawTouchX;
    uint16_t rawTouchY;
    waitTouch(rawTouchX, rawTouchY);

    const int touchX = touch.mapX(rawTouchX);
    const int touchY = touch.mapY(rawTouchY);
    const int firstLineWidth =
        tft.textWidth("Touch the center", INTRO_FONT);
    const int secondLineWidth =
        tft.textWidth("to begin", INTRO_FONT);
    const int textWidth =
        firstLineWidth > secondLineWidth
            ? firstLineWidth
            : secondLineWidth;
    const int fontHeight = tft.fontHeight(INTRO_FONT);
    const int hitboxLeft =
        INTRO_CENTER_X - textWidth / 2 - INTRO_START_PADDING_X;
    const int hitboxRight =
        INTRO_CENTER_X + textWidth / 2 + INTRO_START_PADDING_X;
    const int hitboxTop =
        INTRO_START_LINE_1_Y - fontHeight / 2 -
        INTRO_START_PADDING_Y;
    const int hitboxBottom =
        INTRO_START_LINE_2_Y + fontHeight / 2 +
        INTRO_START_PADDING_Y;

    return touchX >= hitboxLeft && touchX <= hitboxRight &&
           touchY >= hitboxTop && touchY <= hitboxBottom;
}

TouchCalibration TouchCal::calculateCalibration()
{
    TouchCalibration cal;
    cal.signature = TOUCH_SIGNATURE;

    // Media punctelor din stânga
    cal.xmin = (rawX[0] + rawX[3]) / 2;

    // Media punctelor din dreapta
    cal.xmax = (rawX[1] + rawX[2]) / 2;

    // Media punctelor de sus
    cal.ymin = (rawY[0] + rawY[1]) / 2;

    // Media punctelor de jos
    cal.ymax = (rawY[2] + rawY[3]) / 2;

    return cal;
}

bool TouchCal::checkCalibration(const TouchCalibration &cal)
{
    uint16_t dx = abs((int)cal.xmax - (int)cal.xmin);
    uint16_t dy = abs((int)cal.ymax - (int)cal.ymin);

    return (dx > 2000) && (dy > 2000);
}

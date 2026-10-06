#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

class Icons
{
public:
    void showSettingsScreen();
    bool handleTouch(uint16_t touchX, uint16_t touchY);
    void setBackgroundColor(uint16_t backgroundColor);
    uint16_t getAdaptiveIconBackground() const;
    void showMainScreen();
    // Returns 0 when no app was entered, or the entered app index.
    int handleMainTouch(uint16_t touchX, uint16_t touchY);

    void drawTouchCalibrationIcon(int x, int y, int size, uint16_t foregroundColor, uint16_t backgroundColor);
    void drawDisplaySettingsIcon(int x, int y, int size, uint16_t foregroundColor, uint16_t backgroundColor);
    void drawAudioSettingsIcon(int x, int y, int size, uint16_t foregroundColor, uint16_t backgroundColor);
    void drawFactoryResetIcon(int x, int y, int size, uint16_t foregroundColor, uint16_t backgroundColor);
    void drawAboutIcon(int x, int y, int size, uint16_t foregroundColor, uint16_t backgroundColor);

    void draw2HeadIcon(int x, int y, int size, uint16_t foregroundColor, uint16_t backgroundColor);
    void draw3HeadIcon(int x, int y, int size, uint16_t foregroundColor, uint16_t backgroundColor);
    void drawMemoryChecksIcon(int x, int y, int size, uint16_t foregroundColor, uint16_t backgroundColor);
    void drawRecordLevelIcon(int x, int y, int size, uint16_t foregroundColor, uint16_t backgroundColor);
    void drawBiasIcon(int x, int y, int size, uint16_t foregroundColor, uint16_t backgroundColor);
    void drawDolbyCheckIcon(int x, int y, int size, uint16_t foregroundColor, uint16_t backgroundColor);
    void drawTapeEqIcon(int x, int y, int size, uint16_t foregroundColor, uint16_t backgroundColor);
    void drawAutoCalIcon(int x, int y, int size, uint16_t foregroundColor, uint16_t backgroundColor);
    void drawTapeTestIcon(int x, int y, int size, uint16_t foregroundColor, uint16_t backgroundColor);
    void drawAtcIcon(int x, int y, int size, uint16_t foregroundColor, uint16_t backgroundColor);

private:
    void drawButton(int index, int x, int y, int size, bool pressed);
    void drawSelectionBorder(int index, bool selected);
    uint16_t pageBackgroundColor = TFT_BLACK;
    int selectedIndex = -1;
     
};

extern Icons icons;

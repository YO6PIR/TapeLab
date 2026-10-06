//==== Display.h =============
#pragma once
#include <Arduino.h>

struct BootTestResults
{
    bool displayOK;
    bool touchOK;
    bool storageOK;
    bool rtcOK;
    bool audioOK;
    uint8_t generatorCalibrationStatus;

    bool allPassed() const
    {
        return displayOK && touchOK && storageOK && rtcOK && audioOK;
    }
};

class Display
{
public:
    // Initializare TFT
    void begin();

    // Ecran logo
    void splash();

    // Boot hardware
   
    void post(const BootTestResults &results);

    // Meniu principal
    void mainMenu();

    void drawMenuItem(
    const char *text,
    int y,
    bool selected);

    // Șterge ecranul
    void clear();

    void setSelectedItem(int item);
    void refreshMenuSelection();
    void debugTouch(uint16_t x, uint16_t y);

    //Deschide Apps
    void openApp(
        const char *title,
        const char *status,
        uint8_t statusFont = 2,
        int statusY = 215);
    void setAppStatus(
        const char *status,
        uint8_t statusFont = 2,
        int statusY = 215);
    void updateClock(bool force = false);
    void drawProgressBricks(
        int x,
        int y,
        uint8_t progress,
        uint8_t previousProgress);

    void printCenter(
    const char *text,
    int y,
    int font = 2);

    void printLeft(
    const char *text,
    int x,
    int y,
    int font = 2);
    
    int getSelectedItem() const;

    void debugValue(const char *name, uint32_t value);

   void printInt(
    int xRight,
    int y,
    int fieldWidth,
    int value,
    uint8_t font);

    void printFloatDB(int xRight, int y, int width, float value, uint8_t font);

private:
    int selectedItem = 0;
    int previousSelectedItem = 0;
    uint32_t lastClockReadMs = 0;
    char displayedClock[9] = "";

    // Rama principală
    void drawFrame();

    // Bara superioară
    void drawHeader(const char *title);
   
    // Bara inferioară
    void drawFooter(
        const char *status,
        uint16_t color,
        uint8_t statusFont = 2,
        int statusY = 215);
    void clearFooter();

    void drawBootItem(const char *name, bool status, int y);
    void drawSplashRle();   //deseneaza splash-screen cu cseta

};

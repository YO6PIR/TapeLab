#pragma once
#include <Arduino.h>
#include "Touch.h"

class TouchCal
{
public:
    // Inițializează modulul de calibrare
    void begin();

    // Rulează rutina completă de calibrare
    bool run(TouchCalibration &cal, bool touchFeedback = false);

private:
    // Ecranul introductiv
    void drawIntro();

    // Desenează ținta de calibrare
    void drawTarget(int x, int y);

    // Șterge ținta
    void clearTarget(int x, int y);

    // Așteaptă atingerea țintei
   bool waitTouch(uint16_t &rawX, uint16_t &rawY);

    // Desenează mesajele de progres
    void drawMessage(const char *msg);

    // Coordonatele brute citite
    uint16_t rawX[5];
    uint16_t rawY[5];
    bool touchFeedbackEnabled = false;

    //Asteapta eliberae Touch
    void waitRelease();
    bool confirmCalibrationStart();

    //Calculeaza coordonatele de calibrare
    TouchCalibration calculateCalibration();
    bool checkCalibration(const TouchCalibration &cal);
};

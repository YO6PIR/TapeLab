#pragma once
#include <Arduino.h>

constexpr uint16_t TOUCH_SIGNATURE = 0x544C;

// Screen coordinates used by the four calibration targets. The raw values
// measured at these points must be mapped back to these same coordinates.
constexpr uint16_t TOUCH_CAL_TARGET_LEFT = 25;
constexpr uint16_t TOUCH_CAL_TARGET_RIGHT = 300;
constexpr uint16_t TOUCH_CAL_TARGET_TOP = 25;
constexpr uint16_t TOUCH_CAL_TARGET_BOTTOM = 220;

//======  Touch variables ======
struct TouchCalibration
{
    uint16_t signature;
    uint16_t xmin;
    uint16_t xmax;
    uint16_t ymin;
    uint16_t ymax;
};

class Touch
{
public:
    void begin();
    bool pressed(bool feedback = true);
    bool isDown() const;
    bool checkCalibrationRequest();
    uint16_t getX();
    uint16_t getY();
    uint16_t mapX(uint16_t raw);
    uint16_t mapY(uint16_t raw);
    void setCalibration(const TouchCalibration &newCal);
    const TouchCalibration &getCalibration() const;
    void waitAnyTouch();

private:
    TouchCalibration calibration;
    uint16_t x;
    uint16_t y;
    bool wasPressed = false;
};

#include "Touch.h"
#include "Config.h"
#include <XPT2046_Touchscreen.h>
#include "Buzzer.h"

XPT2046_Touchscreen ts(TOUCH_CS);

void Touch::begin()
{
    ts.begin();
    ts.setRotation(1);
    calibration.signature = TOUCH_SIGNATURE;
    calibration.xmin = 3390;
    calibration.xmax = 409;
    calibration.ymin = 3444;
    calibration.ymax = 565;
}

bool Touch::pressed(bool feedback)
{
    bool nowPressed = ts.touched();

    if (nowPressed && !wasPressed)
    {
        wasPressed = true;

        if (feedback)
            buzzer.beepNow(BEEP_TOUCH_MS); // un singur tick la front

        TS_Point p = ts.getPoint();
        x = p.x;
        y = p.y;

        return true;
    }

    if (!nowPressed)
    {
        wasPressed = false;
    }

    return false;
}

bool Touch::isDown() const
{
    return ts.touched();
}

uint16_t Touch::getX()
{
    return mapX(x);
}

uint16_t Touch::getY()
{
    return mapY(y);
}

bool Touch::checkCalibrationRequest()
{
    return ts.touched();
}

uint16_t Touch::mapX(uint16_t raw)
{
    const long mapped = map(raw,
                            calibration.xmin,
                            calibration.xmax,
                            TOUCH_CAL_TARGET_LEFT,
                            TOUCH_CAL_TARGET_RIGHT);
    return (uint16_t)constrain(mapped, 0L, (long)LCD_WIDTH - 1);
}

uint16_t Touch::mapY(uint16_t raw)
{
    const long mapped = map(raw,
                            calibration.ymin,
                            calibration.ymax,
                            TOUCH_CAL_TARGET_TOP,
                            TOUCH_CAL_TARGET_BOTTOM);
    return (uint16_t)constrain(mapped, 0L, (long)LCD_HEIGHT - 1);
}

void Touch::setCalibration(const TouchCalibration &newCal)
{
    calibration = newCal;
}

const TouchCalibration &Touch::getCalibration() const
{
    return calibration;
}

void Touch::waitAnyTouch()
{
    // Dacă degetul este deja pe ecran, așteptăm doar eliberarea.
    while (ts.touched())
    {
        yield();
    }

    // Rearmăm detectorul de front pentru următoarea apăsare.
    wasPressed = false;
}

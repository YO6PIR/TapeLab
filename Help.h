#pragma once

#include <Arduino.h>

class Help
{
public:
    void drawButton(
        int16_t x,
        int16_t y,
        bool pressed = false);
    void drawButton(
        int16_t x,
        int16_t y,
        bool pressed,
        uint16_t idleBackground);

    bool hitTest(
        uint16_t touchX,
        uint16_t touchY,
        int16_t buttonX,
        int16_t buttonY) const;

    void showModal(
        const char *title,
        const char *const lines[],
        uint8_t lineCount);

    int16_t buttonWidth() const;
    int16_t buttonHeight() const;
};

extern Help help;

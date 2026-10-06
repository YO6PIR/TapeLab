#pragma once

#include <Arduino.h>

class Pot
{
public:
    bool begin();

    // Scriere directă: 0...127
    bool setWiper(uint8_t value);
    bool readWiper(uint8_t &value);

    // Nivel relativ:
    // maximumDb corespunde cursorului 127.
    bool setLevelDb(float levelDb, float maximumDb = 10.0f);

    uint8_t wiper() const;
    bool isPresent() const;

private:
    static constexpr uint8_t I2C_ADDRESS = 0x2E;
    static constexpr uint8_t MAX_WIPER = 127;

    uint8_t currentWiper = 0;
    bool devicePresent = false;
};

extern Pot pot;

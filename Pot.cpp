#include "Pot.h"

#include <Wire.h>
#include <math.h>

Pot pot;

bool Pot::begin()
{
    // Wire este inițializat o singură dată, global, în TapeLab.ino.
    Wire.beginTransmission(I2C_ADDRESS);
    devicePresent = (Wire.endTransmission() == 0);

    if (!devicePresent)
        return false;

    return setWiper(0);
}

bool Pot::setWiper(uint8_t value)
{
    if (value > 127)
        value = 127;

    Wire.beginTransmission(0x2E);

    Wire.write(0x00);         // registrul WIPER
    Wire.write(value & 0x7F); // poziția 0...127

    const uint8_t error =
        Wire.endTransmission();

    if (error != 0)
    {
        devicePresent = false;
        return false;
    }

    currentWiper = value;
    devicePresent = true;
    return true;
}

bool Pot::readWiper(uint8_t &value)
{
    Wire.beginTransmission(I2C_ADDRESS);
    Wire.write(0x00);

    if (Wire.endTransmission(false) != 0)
        return false;

    if (Wire.requestFrom(
            static_cast<uint8_t>(I2C_ADDRESS),
            static_cast<uint8_t>(1)) != 1)
    {
        return false;
    }

    value = Wire.read() & 0x7F;
    return true;
}

bool Pot::setLevelDb(float levelDb, float maximumDb)
{
    if (levelDb >= maximumDb)
        return setWiper(MAX_WIPER);

    const float attenuationDb =
        levelDb - maximumDb;

    const float linearRatio =
        powf(10.0f, attenuationDb / 20.0f);

    int calculatedWiper =
        static_cast<int>(
            linearRatio *
                static_cast<float>(MAX_WIPER) +
            0.5f);

    if (calculatedWiper < 0)
        calculatedWiper = 0;

    if (calculatedWiper > MAX_WIPER)
        calculatedWiper = MAX_WIPER;

    return setWiper(
        static_cast<uint8_t>(calculatedWiper));
}

uint8_t Pot::wiper() const
{
    return currentWiper;
}

bool Pot::isPresent() const
{
    return devicePresent;
}

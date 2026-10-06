#pragma once

#include <Arduino.h>

struct RtcDateTime
{
    uint16_t year = 2000;
    uint8_t month = 1;
    uint8_t day = 1;
    uint8_t dayOfWeek = 1;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;
};

class Rtc
{
public:
    // Detects the DS1307 and initializes an invalid/stopped clock from the
    // firmware build time.
    bool begin();
    bool isPresent() const;
    bool isRunning();

    bool read(RtcDateTime &dateTime);
    bool set(const RtcDateTime &dateTime);
    bool setTime(uint8_t hour, uint8_t minute, uint8_t second);
    bool setFromCompileTime();

    // Writes HH:MM:SS or --:--:-- when the RTC cannot be read.
    bool formatTime(char *buffer, size_t bufferSize);

private:
    static uint8_t fromBcd(uint8_t value);
    static uint8_t toBcd(uint8_t value);
    static bool isValid(const RtcDateTime &dateTime);
    static uint8_t calculateDayOfWeek(
        uint16_t year,
        uint8_t month,
        uint8_t day);
    static uint8_t compileMonth(const char *month);

    bool present = false;
};

extern Rtc rtc;

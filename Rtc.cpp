#include "Rtc.h"

#include "Config.h"

#include <Wire.h>

Rtc rtc;

uint8_t Rtc::fromBcd(uint8_t value)
{
    return (value >> 4) * 10 + (value & 0x0F);
}

uint8_t Rtc::toBcd(uint8_t value)
{
    return ((value / 10) << 4) | (value % 10);
}

bool Rtc::begin()
{
    Wire.beginTransmission(RTC_ADDRESS);
    present = (Wire.endTransmission() == 0);

    if (!present)
        return false;

    RtcDateTime dateTime;
    if (!isRunning() || !read(dateTime))
        return setFromCompileTime();

    return true;
}

bool Rtc::isPresent() const
{
    return present;
}

bool Rtc::isRunning()
{
    if (!present)
        return false;

    Wire.beginTransmission(RTC_ADDRESS);
    Wire.write((uint8_t)0x00);
    if (Wire.endTransmission(false) != 0)
        return false;

    if (Wire.requestFrom((uint8_t)RTC_ADDRESS, (uint8_t)1) != 1 ||
        !Wire.available())
    {
        return false;
    }

    // CH (Clock Halt), bit 7 of the seconds register, must be clear.
    return (Wire.read() & 0x80) == 0;
}

bool Rtc::read(RtcDateTime &dateTime)
{
    if (!present)
        return false;

    Wire.beginTransmission(RTC_ADDRESS);
    Wire.write((uint8_t)0x00);
    if (Wire.endTransmission(false) != 0)
        return false;

    if (Wire.requestFrom((uint8_t)RTC_ADDRESS, (uint8_t)7) != 7)
        return false;

    uint8_t registers[7];
    for (uint8_t i = 0; i < 7; i++)
    {
        if (!Wire.available())
            return false;
        registers[i] = Wire.read();
    }

    dateTime.second = fromBcd(registers[0] & 0x7F);
    dateTime.minute = fromBcd(registers[1] & 0x7F);

    if (registers[2] & 0x40)
    {
        // Convert the DS1307 12-hour representation to 24-hour time.
        uint8_t hour = fromBcd(registers[2] & 0x1F);
        const bool isPm = (registers[2] & 0x20) != 0;
        if (hour == 12)
            hour = 0;
        dateTime.hour = hour + (isPm ? 12 : 0);
    }
    else
    {
        dateTime.hour = fromBcd(registers[2] & 0x3F);
    }

    dateTime.dayOfWeek = fromBcd(registers[3] & 0x07);
    dateTime.day = fromBcd(registers[4] & 0x3F);
    dateTime.month = fromBcd(registers[5] & 0x1F);
    dateTime.year = 2000 + fromBcd(registers[6]);

    return isValid(dateTime);
}

bool Rtc::set(const RtcDateTime &dateTime)
{
    if (!present || !isValid(dateTime))
        return false;

    Wire.beginTransmission(RTC_ADDRESS);
    Wire.write((uint8_t)0x00);
    Wire.write(toBcd(dateTime.second) & 0x7F);
    Wire.write(toBcd(dateTime.minute));
    Wire.write(toBcd(dateTime.hour)); // Always store 24-hour time.
    Wire.write(toBcd(dateTime.dayOfWeek));
    Wire.write(toBcd(dateTime.day));
    Wire.write(toBcd(dateTime.month));
    Wire.write(toBcd((uint8_t)(dateTime.year - 2000)));

    return Wire.endTransmission() == 0;
}

bool Rtc::setTime(uint8_t hour, uint8_t minute, uint8_t second)
{
    RtcDateTime dateTime;
    if (!read(dateTime))
        return false;

    dateTime.hour = hour;
    dateTime.minute = minute;
    dateTime.second = second;
    return set(dateTime);
}

bool Rtc::setFromCompileTime()
{
    const char *date = __DATE__;
    const char *time = __TIME__;

    RtcDateTime dateTime;
    dateTime.month = compileMonth(date);
    dateTime.day =
        (uint8_t)((date[4] == ' ' ? 0 : date[4] - '0') * 10 +
                  (date[5] - '0'));
    dateTime.year =
        (uint16_t)((date[7] - '0') * 1000 +
                   (date[8] - '0') * 100 +
                   (date[9] - '0') * 10 +
                   (date[10] - '0'));
    dateTime.hour =
        (uint8_t)((time[0] - '0') * 10 + (time[1] - '0'));
    dateTime.minute =
        (uint8_t)((time[3] - '0') * 10 + (time[4] - '0'));
    dateTime.second =
        (uint8_t)((time[6] - '0') * 10 + (time[7] - '0'));
    dateTime.dayOfWeek =
        calculateDayOfWeek(dateTime.year, dateTime.month, dateTime.day);

    return set(dateTime);
}

bool Rtc::formatTime(char *buffer, size_t bufferSize)
{
    if (buffer == nullptr || bufferSize < 9)
        return false;

    RtcDateTime dateTime;
    if (!read(dateTime))
    {
        snprintf(buffer, bufferSize, "--:--:--");
        return false;
    }

    snprintf(
        buffer,
        bufferSize,
        "%02u:%02u:%02u",
        (unsigned int)dateTime.hour,
        (unsigned int)dateTime.minute,
        (unsigned int)dateTime.second);
    return true;
}

bool Rtc::isValid(const RtcDateTime &dateTime)
{
    if (dateTime.year < 2000 || dateTime.year > 2099 ||
        dateTime.month < 1 || dateTime.month > 12 ||
        dateTime.day < 1 || dateTime.day > 31 ||
        dateTime.dayOfWeek < 1 || dateTime.dayOfWeek > 7 ||
        dateTime.hour > 23 || dateTime.minute > 59 ||
        dateTime.second > 59)
    {
        return false;
    }

    static const uint8_t daysPerMonth[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint8_t maximumDay = daysPerMonth[dateTime.month - 1];
    const bool leapYear =
        (dateTime.year % 4 == 0 && dateTime.year % 100 != 0) ||
        (dateTime.year % 400 == 0);

    if (dateTime.month == 2 && leapYear)
        maximumDay = 29;

    return dateTime.day <= maximumDay;
}

uint8_t Rtc::calculateDayOfWeek(
    uint16_t year,
    uint8_t month,
    uint8_t day)
{
    static const uint8_t monthOffsets[12] = {
        0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};

    if (month < 3)
        year--;

    // Sakamoto algorithm: 0 = Sunday. DS1307 uses values 1...7.
    return (uint8_t)
        ((year + year / 4 - year / 100 + year / 400 +
          monthOffsets[month - 1] + day) %
         7) +
        1;
}

uint8_t Rtc::compileMonth(const char *month)
{
    static const char *const months[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    for (uint8_t i = 0; i < 12; i++)
    {
        if (month[0] == months[i][0] &&
            month[1] == months[i][1] &&
            month[2] == months[i][2])
        {
            return i + 1;
        }
    }

    return 1;
}

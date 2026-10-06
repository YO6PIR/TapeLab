#pragma once

#include <Arduino.h>

class AD9833
{
public:
    void begin();

    void setFrequency(uint32_t frequencyHz);

    void enable();
    void disable();

    bool isEnabled() const;
    uint32_t frequencyHz() const;

private:
    void writeRegister(uint16_t value);
    void applyFrequency();

    static constexpr uint8_t FSYNC_PIN = PB12;
    static constexpr uint8_t CLOCK_PIN = PB13;
    static constexpr uint8_t DATA_PIN = PB15;

    static constexpr uint32_t MASTER_CLOCK_HZ = 18000000UL;

    uint32_t currentFrequencyHz = 1000;
    bool enabled = false;
};

extern AD9833 ad9833;
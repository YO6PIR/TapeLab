#include "AD9833.h"

AD9833 ad9833;

void AD9833::begin()
{
    pinMode(FSYNC_PIN, OUTPUT);
    pinMode(CLOCK_PIN, OUTPUT);
    pinMode(DATA_PIN, OUTPUT);

    digitalWrite(FSYNC_PIN, HIGH);
    digitalWrite(CLOCK_PIN, HIGH);
    digitalWrite(DATA_PIN, LOW);

    delay(10);

    // RESET activ, încărcare pe 28 biți.
    writeRegister(0x2100);

    applyFrequency();

    // Inițial rămâne oprit până la START.
    disable();
}

void AD9833::setFrequency(uint32_t frequencyHz)
{
    if (frequencyHz < 1)
        frequencyHz = 1;

    if (frequencyHz > 20000)
        frequencyHz = 20000;

    currentFrequencyHz = frequencyHz;
    applyFrequency();
}

void AD9833::enable()
{
    enabled = true;
    // B28 = 1, RESET = 0, ieșire sinus activă.
    writeRegister(0x2000);
}

void AD9833::disable()
{
    enabled = false;
    // B28 = 1, RESET = 1.
    writeRegister(0x2100);
}

bool AD9833::isEnabled() const
{
    return enabled;
}

uint32_t AD9833::frequencyHz() const
{
    return currentFrequencyHz;
}

void AD9833::applyFrequency()
{
    const uint64_t tuningWord =
        (static_cast<uint64_t>(currentFrequencyHz) << 28) /
        MASTER_CLOCK_HZ;

    const uint16_t frequencyLsb =
        0x4000 |
        static_cast<uint16_t>(tuningWord & 0x3FFF);

    const uint16_t frequencyMsb =
        0x4000 |
        static_cast<uint16_t>((tuningWord >> 14) & 0x3FFF);

    // RESET activ cât timp încărcăm registrele.
    writeRegister(0x2100);

    writeRegister(frequencyLsb);
    writeRegister(frequencyMsb);

    // Fază zero.
    writeRegister(0xC000);

    if (enabled)
        writeRegister(0x2000);
    else
        writeRegister(0x2100);
}

void AD9833::writeRegister(uint16_t value)
{
    digitalWrite(FSYNC_PIN, LOW);

    for (int8_t bit = 15; bit >= 0; --bit)
    {
        // Datele trebuie să fie stabile înaintea frontului descendent.
        digitalWrite(
            DATA_PIN,
            (value & (1U << bit)) ? HIGH : LOW);

        digitalWrite(CLOCK_PIN, LOW);
        delayMicroseconds(1);

        digitalWrite(CLOCK_PIN, HIGH);
        delayMicroseconds(1);
    }

    digitalWrite(FSYNC_PIN, HIGH);
    delayMicroseconds(1);
}
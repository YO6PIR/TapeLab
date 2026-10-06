#include "Storage.h"
#include <Wire.h>
#include "Config.h"
#include <math.h>
#include <string.h>

namespace
{
    constexpr uint8_t EEPROM_PAGE_SIZE = 32;
    constexpr uint8_t EEPROM_I2C_DATA_LIMIT = 30;
    constexpr uint16_t BACKGROUND_SETTINGS_SIGNATURE = 0x4247;
    constexpr uint8_t BACKGROUND_SETTINGS_VERSION = 2;

    struct BackgroundSettings
    {
        uint16_t signature;
        uint8_t version;
        uint16_t color;
        uint8_t beepEnabled;
    };

    // Version 1 contained only the background colour.  Keep this exact
    // layout so existing installations safely default the new setting to ON.
    struct BackgroundSettingsV1
    {
        uint16_t signature;
        uint8_t version;
        uint16_t color;
    };

}

void Storage::begin()
{
    Wire.setSDA(PB7);
    Wire.setSCL(PB6);
    Wire.begin();
}

bool Storage::isPresent()
{
    Wire.beginTransmission(EEPROM_ADDRESS);
    return (Wire.endTransmission() == 0);
}

bool Storage::checkReadAccess()
{
    uint8_t value;
    return readBlock(EEPROM_GENERATOR_CAL_ADDR, &value, sizeof(value));
}

bool Storage::writeBlock(uint16_t address,
                         const uint8_t *data,
                         uint16_t length)
{
    while (length > 0)
    {
        const uint8_t pageRemaining = EEPROM_PAGE_SIZE -
            (address % EEPROM_PAGE_SIZE);
        uint16_t chunk = length;
        const uint8_t pageChunk = pageRemaining < EEPROM_I2C_DATA_LIMIT
            ? pageRemaining : EEPROM_I2C_DATA_LIMIT;
        if (chunk > pageChunk)
            chunk = pageChunk;
        Wire.beginTransmission(EEPROM_ADDRESS);
        Wire.write(highByte(address));
        Wire.write(lowByte(address));
        for (uint16_t i = 0; i < chunk; ++i)
            Wire.write(data[i]);

        if (Wire.endTransmission() != 0)
            return false;
        delay(5);
        address += chunk;
        data += chunk;
        length -= chunk;
    }
    return true;
}                       

//=== Rutina de citire din EEprom ========
bool Storage::readBlock(uint16_t address,
                        uint8_t *data,
                        uint16_t length)
{
    while (length > 0)
    {
        const uint8_t chunk = length > EEPROM_I2C_DATA_LIMIT
            ? EEPROM_I2C_DATA_LIMIT : length;
        Wire.beginTransmission(EEPROM_ADDRESS);
        Wire.write(highByte(address));
        Wire.write(lowByte(address));
        if (Wire.endTransmission(false) != 0)
            return false;

        if (Wire.requestFrom(EEPROM_ADDRESS, chunk) != chunk)
            return false;
        for (uint8_t i = 0; i < chunk; ++i)
        {
            if (!Wire.available())
                return false;
            data[i] = Wire.read();
        }
        address += chunk;
        data += chunk;
        length -= chunk;
    }
    return true;
}

uint32_t Storage::calculateGeneratorCalibrationCrc(
    const GeneratorCalibrationStorage &calibration)
{
    const uint8_t *data = reinterpret_cast<const uint8_t *>(&calibration);
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < offsetof(GeneratorCalibrationStorage, crc32); ++i)
    {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320UL & -(crc & 1));
    }
    return ~crc;
}

bool Storage::validateGeneratorCalibration(
    const GeneratorCalibrationStorage &calibration)
{
    if (calibration.magic != GENERATOR_CAL_MAGIC ||
        calibration.version != GENERATOR_CAL_VERSION ||
        calibration.bandCount != TAPE_RESPONSE_BAND_COUNT ||
        (calibration.verifyGrade != GENERATOR_CAL_GRADE_EXCELLENT &&
         calibration.verifyGrade != GENERATOR_CAL_GRADE_OK) ||
        calibration.crc32 != calculateGeneratorCalibrationCrc(calibration) ||
        !isfinite(calibration.maxVerifyErrorDb) ||
        calibration.maxVerifyErrorDb < 0.0f ||
        calibration.maxVerifyErrorDb > GENERATOR_CAL_MAX_VERIFY_ERROR_DB)
        return false;

    for (uint8_t i = 0; i < TAPE_RESPONSE_BAND_COUNT; ++i)
    {
        if (!isfinite(calibration.correctionDb[i]) ||
            fabsf(calibration.correctionDb[i]) >
                GENERATOR_CAL_MAX_CORRECTION_DB)
            return false;
    }
    return true;
}

bool Storage::isEmptyGeneratorCalibration(
    const GeneratorCalibrationStorage &calibration)
{
    const uint8_t *data = reinterpret_cast<const uint8_t *>(&calibration);
    bool allZero = true;
    bool allFF = true;
    for (size_t i = 0; i < sizeof(calibration); ++i)
    {
        allZero = allZero && data[i] == 0x00;
        allFF = allFF && data[i] == 0xFF;
    }
    return allZero || allFF;
}

GeneratorCalibrationBootStatus Storage::loadGeneratorCalibration(
    GeneratorCalibrationStorage &calibration)
{
    GeneratorCalibrationStorage primary;
    GeneratorCalibrationStorage backup;
    const bool primaryRead = readBlock(
        EEPROM_GENERATOR_CAL_ADDR,
        reinterpret_cast<uint8_t *>(&primary), sizeof(primary));
    const bool backupRead = readBlock(
        EEPROM_GENERATOR_CAL_BACKUP_ADDR,
        reinterpret_cast<uint8_t *>(&backup), sizeof(backup));
    const bool primaryValid = primaryRead && validateGeneratorCalibration(primary);
    const bool backupValid = backupRead && validateGeneratorCalibration(backup);
    if (primaryValid || backupValid)
    {
        calibration = (!backupValid ||
                       (primaryValid && primary.calibrationCounter >=
                                          backup.calibrationCounter))
            ? primary : backup;
        generatorCalibrationStatus = GeneratorCalibrationBootStatus::Ok;
        return generatorCalibrationStatus;
    }

    generatorCalibrationStatus = (!primaryRead || !backupRead ||
        !isEmptyGeneratorCalibration(primary) ||
        !isEmptyGeneratorCalibration(backup))
        ? GeneratorCalibrationBootStatus::Failed
        : GeneratorCalibrationBootStatus::NotCalibrated;
    return generatorCalibrationStatus;
}

GeneratorCalibrationBootStatus Storage::generatorCalibrationBootStatus() const
{
    return generatorCalibrationStatus;
}

bool Storage::saveGeneratorCalibration(GeneratorCalibrationStorage &calibration)
{
    GeneratorCalibrationStorage current;
    const bool haveCurrent = loadGeneratorCalibration(current) ==
        GeneratorCalibrationBootStatus::Ok;
    calibration.magic = GENERATOR_CAL_MAGIC;
    calibration.version = GENERATOR_CAL_VERSION;
    calibration.bandCount = TAPE_RESPONSE_BAND_COUNT;
    calibration.calibrationCounter = haveCurrent
        ? current.calibrationCounter + 1 : 1;
    calibration.crc32 = calculateGeneratorCalibrationCrc(calibration);
    if (!validateGeneratorCalibration(calibration))
        return false;

    const uint16_t target = !haveCurrent ||
        current.calibrationCounter % 2 == 0
        ? EEPROM_GENERATOR_CAL_ADDR : EEPROM_GENERATOR_CAL_BACKUP_ADDR;
    if (!writeBlock(target, reinterpret_cast<const uint8_t *>(&calibration),
                    sizeof(calibration)))
        return false;

    GeneratorCalibrationStorage readBack;
    const bool saved = readBlock(
        target,
        reinterpret_cast<uint8_t *>(&readBack),
        sizeof(readBack)) &&
        validateGeneratorCalibration(readBack) &&
        readBack.calibrationCounter == calibration.calibrationCounter;
    if (saved)
        generatorCalibrationStatus = GeneratorCalibrationBootStatus::Ok;
    return saved;
}

//== Rutina de salvare in memorie a TouchCalibration =========
bool Storage::saveTouchCalibration(const TouchCalibration &cal)
{
    return writeBlock(
        EEPROM_TOUCH_ADDR,
        (const uint8_t*)&cal,
        sizeof(TouchCalibration));
}

//==== Rutina de citire din memorie TouchCalibration ======
bool Storage::loadTouchCalibration(TouchCalibration &cal)
{
    if(!readBlock(
        EEPROM_TOUCH_ADDR,
        (uint8_t*)&cal,
        sizeof(TouchCalibration)))
        return false;

    if (cal.signature != TOUCH_SIGNATURE)
        return false;

    constexpr uint16_t TOUCH_ADC_MAX = 4095;
    if (cal.xmin > TOUCH_ADC_MAX || cal.xmax > TOUCH_ADC_MAX ||
        cal.ymin > TOUCH_ADC_MAX || cal.ymax > TOUCH_ADC_MAX)
        return false;

    const uint16_t dx = abs(static_cast<int>(cal.xmax) -
                            static_cast<int>(cal.xmin));
    const uint16_t dy = abs(static_cast<int>(cal.ymax) -
                            static_cast<int>(cal.ymin));
    return dx > 2000 && dy > 2000;
}

bool Storage::saveAudioSettings(const AudioSettings &settings)
{
    return writeBlock(
        EEPROM_AUDIO_ADDR,
        (const uint8_t *)&settings,
        sizeof(AudioSettings));
}

bool Storage::loadAudioSettings(AudioSettings &settings)
{
    return readBlock(
        EEPROM_AUDIO_ADDR,
        (uint8_t *)&settings,
        sizeof(AudioSettings));
}

bool Storage::saveBackgroundColor(uint16_t color)
{
    BackgroundSettings settings = {
        BACKGROUND_SETTINGS_SIGNATURE,
        BACKGROUND_SETTINGS_VERSION,
        color,
        1};
    bool enabled;
    if (loadBeepEnabled(enabled))
        settings.beepEnabled = enabled ? 1 : 0;

    return writeBlock(
        EEPROM_SYSTEM_ADDR,
        (const uint8_t *)&settings,
        sizeof(BackgroundSettings));
}

bool Storage::loadBackgroundColor(uint16_t &color)
{
    BackgroundSettings settings;
    if (!readBlock(EEPROM_SYSTEM_ADDR,
                   reinterpret_cast<uint8_t *>(&settings),
                   sizeof(settings)) ||
        settings.signature != BACKGROUND_SETTINGS_SIGNATURE ||
        (settings.version != BACKGROUND_SETTINGS_VERSION &&
         settings.version != 1))
        return false;

    color = settings.color;
    return true;
}

bool Storage::saveBeepEnabled(bool enabled)
{
    BackgroundSettings settings = {
        BACKGROUND_SETTINGS_SIGNATURE,
        BACKGROUND_SETTINGS_VERSION,
        0x0000,
        enabled ? 1 : 0};
    uint16_t color;
    if (loadBackgroundColor(color))
        settings.color = color;

    return writeBlock(EEPROM_SYSTEM_ADDR,
                      reinterpret_cast<const uint8_t *>(&settings),
                      sizeof(settings));
}

bool Storage::loadBeepEnabled(bool &enabled)
{
    BackgroundSettings settings;
    if (!readBlock(EEPROM_SYSTEM_ADDR,
                   reinterpret_cast<uint8_t *>(&settings),
                   sizeof(settings)) ||
        settings.signature != BACKGROUND_SETTINGS_SIGNATURE ||
        (settings.version != BACKGROUND_SETTINGS_VERSION &&
         settings.version != 1))
        return false;

    if (settings.version == 1)
    {
        enabled = true;
        return true;
    }
    enabled = settings.beepEnabled == 1;
    return settings.beepEnabled == 0 || settings.beepEnabled == 1;
}

bool Storage::factoryReset()
{
    const uint16_t invalidSignature = 0x0000;

    // Invalidează calibrarea Touch.
    if (!writeBlock(
            EEPROM_TOUCH_ADDR,
            reinterpret_cast<const uint8_t *>(&invalidSignature),
            sizeof(invalidSignature)))
    {
        return false;
    }

    // Rescrie setările audio cu valorile implicite din firmware.
    const AudioSettings defaultAudioSettings;
    if (!saveAudioSettings(defaultAudioSettings))
    {
        return false;
    }

    // Invalidează setarea temei/fundalului.
    if (!writeBlock(
            EEPROM_SYSTEM_ADDR,
            reinterpret_cast<const uint8_t *>(&invalidSignature),
            sizeof(invalidSignature)))
    {
        return false;
    }

    uint8_t erasedGeneratorCalibration[sizeof(GeneratorCalibrationStorage)];
    memset(erasedGeneratorCalibration, 0xFF, sizeof(erasedGeneratorCalibration));
    if (!writeBlock(
            EEPROM_GENERATOR_CAL_ADDR,
            erasedGeneratorCalibration,
            sizeof(erasedGeneratorCalibration)) ||
        !writeBlock(
            EEPROM_GENERATOR_CAL_BACKUP_ADDR,
            erasedGeneratorCalibration,
            sizeof(erasedGeneratorCalibration)))
    {
        return false;
    }

    generatorCalibrationStatus = GeneratorCalibrationBootStatus::NotCalibrated;

    return true;
}

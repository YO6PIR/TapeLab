#pragma once

#include "Touch.h"
#include "SetAudio.h"
#include "Record.h"

//======== EEprom Map =======================
constexpr uint16_t EEPROM_TOUCH_ADDR  = 0x0000;
constexpr uint16_t EEPROM_USER_ADDR   = 0x0020;
constexpr uint16_t EEPROM_AUDIO_ADDR  = 0x0060;
constexpr uint16_t EEPROM_SYSTEM_ADDR = 0x00A0;
constexpr uint16_t EEPROM_GENERATOR_CAL_ADDR = 0x00C0;
constexpr uint16_t EEPROM_GENERATOR_CAL_BACKUP_ADDR = 0x0114;
//==========================================

constexpr uint32_t GENERATOR_CAL_MAGIC = 0x4743414CUL; // "GCAL"
constexpr uint16_t GENERATOR_CAL_VERSION = 1;
constexpr uint8_t GENERATOR_CAL_GRADE_EXCELLENT = 1;
constexpr uint8_t GENERATOR_CAL_GRADE_OK = 2;
constexpr float GENERATOR_CAL_MAX_CORRECTION_DB = 2.0f;
constexpr float GENERATOR_CAL_MAX_VERIFY_ERROR_DB = 0.5f;

enum class GeneratorCalibrationBootStatus : uint8_t
{
    Ok,
    NotCalibrated,
    Failed
};

struct GeneratorCalibrationStorage
{
    uint32_t magic;
    uint16_t version;
    uint8_t bandCount;
    uint8_t verifyGrade;
    float correctionDb[TAPE_RESPONSE_BAND_COUNT];
    float maxVerifyErrorDb;
    uint32_t calibrationCounter;
    uint32_t crc32;
};

static_assert(sizeof(GeneratorCalibrationStorage) == 84,
              "Generator calibration EEPROM record size changed");
static_assert(EEPROM_GENERATOR_CAL_BACKUP_ADDR +
                  sizeof(GeneratorCalibrationStorage) <= 0x1000,
              "Generator calibration exceeds 24C32 EEPROM");

class Storage
{
public:
    void begin();
    bool isPresent();          
    bool checkReadAccess();

    bool loadTouchCalibration(TouchCalibration &cal);
    bool saveTouchCalibration(const TouchCalibration &cal);

    bool loadAudioSettings(AudioSettings &settings);
    bool saveAudioSettings(const AudioSettings &settings);

    bool loadBackgroundColor(uint16_t &color);
    bool saveBackgroundColor(uint16_t color);
    bool loadBeepEnabled(bool &enabled);
    bool saveBeepEnabled(bool enabled);

    GeneratorCalibrationBootStatus loadGeneratorCalibration(
        GeneratorCalibrationStorage &calibration);
    bool saveGeneratorCalibration(GeneratorCalibrationStorage &calibration);
    GeneratorCalibrationBootStatus generatorCalibrationBootStatus() const;

    bool factoryReset();

private:    
    bool writeBlock(uint16_t address,
                    const uint8_t *data,
                    uint16_t length);

    bool readBlock(uint16_t address,
                   uint8_t *data,
                   uint16_t length);
    static uint32_t calculateGeneratorCalibrationCrc(
        const GeneratorCalibrationStorage &calibration);
    static bool validateGeneratorCalibration(
        const GeneratorCalibrationStorage &calibration);
    static bool isEmptyGeneratorCalibration(
        const GeneratorCalibrationStorage &calibration);
    GeneratorCalibrationBootStatus generatorCalibrationStatus =
        GeneratorCalibrationBootStatus::NotCalibrated;
};

extern Storage storage;

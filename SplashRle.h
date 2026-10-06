#pragma once
#include <Arduino.h>

constexpr uint16_t SPLASH_RLE_WIDTH  = 320;
constexpr uint16_t SPLASH_RLE_HEIGHT = 240;
constexpr uint8_t  SPLASH_RLE_PALETTE_SIZE = 32;
constexpr uint32_t SPLASH_RLE_DATA_SIZE = 60546;

extern const uint16_t splashRlePalette[SPLASH_RLE_PALETTE_SIZE];
extern const uint8_t splashRleData[SPLASH_RLE_DATA_SIZE];

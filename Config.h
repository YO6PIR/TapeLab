#pragma once
#include <Arduino.h>
#include "TFT_eSPI.h"

// Increment this value for every completed functional firmware milestone.
constexpr char FIRMWARE_VERSION[] = "FW 1.11";

//================ LCD =================
constexpr uint16_t LCD_WIDTH = 320;
constexpr uint16_t LCD_HEIGHT = 240;
// Configurare pini de comanda DDS AD9833
constexpr uint8_t AD9833_FSYNC_PIN = PB12;
constexpr uint8_t AD9833_CLOCK_PIN = PB13;
constexpr uint8_t AD9833_DATA_PIN = PB15;

// Active buzzer UI feedback on digital output PB10.
constexpr uint32_t BUZZER_PIN = PB10;
constexpr uint16_t BEEP_TOUCH_MS = 15;
constexpr uint16_t BEEP_SUCCESS_MS = 90;
constexpr uint16_t BEEP_FAILURE_MS = 60;
constexpr uint16_t BEEP_GAP_MS = 70;

//================ Layout =================
constexpr uint16_t FRAME_MARGIN = 4;

constexpr uint16_t HEADER_HEIGHT = 24;
constexpr uint16_t FOOTER_HEIGHT = 24;

constexpr uint16_t HEADER_Y = 0;
constexpr uint16_t FOOTER_Y = LCD_HEIGHT - FOOTER_HEIGHT;

constexpr uint16_t CONTENT_LEFT = FRAME_MARGIN + 3;
constexpr uint16_t CONTENT_RIGHT = LCD_WIDTH - FRAME_MARGIN - 4;

constexpr uint16_t CONTENT_TOP = HEADER_HEIGHT;
constexpr uint16_t CONTENT_BOTTOM = FOOTER_Y - 1;

constexpr uint16_t CONTENT_WIDTH = CONTENT_RIGHT - CONTENT_LEFT + 1;
constexpr uint16_t CONTENT_HEIGHT = CONTENT_BOTTOM - CONTENT_TOP + 1;

// Shared calibration progress bar, matching RECORD CALIBRATION 2 HEAD.
constexpr uint8_t PROGRESS_BAR_BRICK_COUNT = 16;
constexpr int PROGRESS_BAR_BRICK_WIDTH = 8;
constexpr int PROGRESS_BAR_BRICK_HEIGHT = 10;
constexpr int PROGRESS_BAR_BRICK_GAP = 2;
constexpr int PROGRESS_BAR_WIDTH =
    PROGRESS_BAR_BRICK_COUNT *
        (PROGRESS_BAR_BRICK_WIDTH + PROGRESS_BAR_BRICK_GAP) -
    PROGRESS_BAR_BRICK_GAP;
constexpr int PROGRESS_BAR_RIGHT = LCD_WIDTH - 1 - 20;
constexpr int PROGRESS_BAR_X = PROGRESS_BAR_RIGHT - PROGRESS_BAR_WIDTH + 1;

//================ Menu =================
constexpr uint8_t MENU_COUNT = 7;

constexpr uint16_t MENU_X = 10;
constexpr uint16_t MENU_WIDTH = 300;

constexpr uint16_t MENU_Y = 30;
constexpr uint16_t MENU_STEP = 26;

//================ TFT =================
#define TFT_CS PA4
#define TFT_DC PB0
#define TFT_RST PB1

//================ Touch =================
#define TOUCH_CS PA3

//================ I2C =================
#define I2C_SDA PB7
#define I2C_SCL PB6

//================ EEPROM =================
#define EEPROM_ADDRESS 0x50

//================ RTC =================
#define RTC_ADDRESS 0x68

//================ Colors =================
constexpr uint16_t TFT_DARKNAVY = 0x0008;
constexpr uint16_t COL_DARKGREEN = 0x0320;
extern uint16_t currentBackgroundColor;
#define COL_BG currentBackgroundColor
#define COL_FRAME TFT_DARKGREY
#define COL_TEXT TFT_WHITE
#define COL_TITLE TFT_CYAN
#define COL_SELECT TFT_GREEN
#define COL_STATUS TFT_YELLOW
#define COL_ERROR TFT_RED

//================ Status =================
#define STATUS_READY TFT_GREEN
#define STATUS_BUSY TFT_YELLOW
#define STATUS_ERROR TFT_RED
#define STATUS_INFO TFT_CYAN
#define STATUS_WARNING TFT_ORANGE

//======================================================
// Playback Analyzer UI
//======================================================

constexpr int UI_FREQ_TITLE_Y = 34;
constexpr int UI_FREQ_VALUE_Y = 52;

constexpr int UI_LEVEL_Y = 82;

constexpr int UI_BAR_X = 22;
constexpr int UI_BAR_W = 276;
constexpr int UI_BAR_H = 8;

constexpr int UI_BAR_L_Y = 118;
constexpr int UI_BAR_R_Y = 142;

constexpr int UI_OFFSET_Y = 182;

//======================================================
// Spectrum Analyzer Parameters
//======================================================

// Number of FFT frames between decay steps.
// 1 = decay on every frame; larger values make the bars fall more slowly.
constexpr uint16_t SPECTRUM_BAR_DECAY_FRAMES = 1;

// Number of bricks removed at each decay step.
// Increase this value for a faster, livelier fall (for example 2...4).
constexpr uint8_t SPECTRUM_BAR_DECAY_BRICKS = 3;

// Number of FFT frames for which the peak marker remains fixed.
// cât timp rămâne nemișcat;
constexpr uint16_t SPECTRUM_PEAK_HOLD_FRAMES = 5;

// Number of FFT frames between peak-marker decay steps.
// intervalul dintre pașii de coborâre;
constexpr uint16_t SPECTRUM_PEAK_DECAY_FRAMES = 1;

// Number of bricks removed from the peak marker at each decay step.
// câte brick-uri coboară la fiecare pas.
constexpr uint8_t SPECTRUM_PEAK_DECAY_BRICKS = 1;

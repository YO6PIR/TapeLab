#pragma once

#include "Config.h"

inline void drawCalibrationProgressBar(
    TFT_eSPI &tft,
    int x,
    int y,
    uint8_t progress,
    uint8_t previousProgress)
{
    progress = min(progress, PROGRESS_BAR_BRICK_COUNT);
    const uint8_t firstBrick = previousProgress == 0xFF
                                   ? 0
                                   : min(previousProgress, progress);
    const uint8_t lastBrick = previousProgress == 0xFF
                                  ? PROGRESS_BAR_BRICK_COUNT
                                  : max(previousProgress, progress);
    const uint16_t darkBlue = tft.color565(0, 0, 127);

    for (uint8_t brick = firstBrick; brick < lastBrick; ++brick)
    {
        const int brickX = x + brick *
                                   (PROGRESS_BAR_BRICK_WIDTH + PROGRESS_BAR_BRICK_GAP);
        tft.fillRect(
            brickX,
            y,
            PROGRESS_BAR_BRICK_WIDTH,
            PROGRESS_BAR_BRICK_HEIGHT,
            brick < progress ? TFT_CYAN : darkBlue);
    }
}
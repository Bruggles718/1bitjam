#pragma once

#include "pd_api.h"

constexpr int PIXEL_SCALE = 1;

constexpr float NEAR_PLANE = 0.1f;

constexpr int SCREEN_WIDTH = (LCD_COLUMNS / PIXEL_SCALE);
constexpr int SCREEN_HEIGHT = (LCD_ROWS / PIXEL_SCALE);
#pragma once

#include <vector>
#include "pd_api.h"

#define samplepixel(data, x, y, rowbytes) (((data[(y)*rowbytes+(x)/8] & (1 << (uint8_t)(7 - ((x) % 8)))) != 0) ? kColorWhite : kColorBlack)

// Set the pixel at x, y to black.
#define setpixel(data, x, y, rowbytes) (data[(y)*rowbytes+(x)/8] &= ~(1 << (uint8_t)(7 - ((x) % 8))))

// Set the pixel at x, y to white.
#define clearpixel(data, x, y, rowbytes) (data[(y)*rowbytes+(x)/8] |= (1 << (uint8_t)(7 - ((x) % 8))))

// Set the pixel at x, y to the specified color.
#define drawpixel(data, x, y, rowbytes, color) (((color) == kColorBlack) ? setpixel((data), (x), (y), (rowbytes)) : clearpixel((data), (x), (y), (rowbytes)))

void setPixel(PlaydateAPI* pd, int x, int y, int color);

float my_lerp(float a, float b, float t);

float calc_percentage(float start, float end, float x);

bool bad_float(float f);

float clamp_to_screen_x(float f);

std::vector<std::vector<float>> bayerMatrix(int n);
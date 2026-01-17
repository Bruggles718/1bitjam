#include "utils.h"
#include "ScreenGlobals.h"

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float calc_percentage(float start, float end, float x) {
    if (fabsf(end - start) < 1e-6f) return 0;
    return (x - start) / (end - start);
}

bool bad_float(float f) {
    return (isnan(f) || !isfinite(f));
}

float clamp_to_screen_x(float f) {
    return fmaxf(0, fminf(f, SCREEN_WIDTH - 1));
}
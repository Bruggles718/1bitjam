#include "utils.h"

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float calc_percentage(float start, float end, float x) {
    return (x - start) / (end - start);
}
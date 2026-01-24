#include "utils.hpp"
#include "ScreenGlobals.hpp"

float my_lerp(float a, float b, float t) {
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

std::vector<std::vector<float>> bayerMatrix(int n) {
    std::vector<std::vector<int>> B = {{0, 2}, {3, 1}};
    int size = 2;

    while (size < n) {
        std::vector<std::vector<int>> newB(size * 2, std::vector<int>(size * 2));
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                int v = B[y][x];
                newB[y][x] = 4 * v;
                newB[y][x + size] = 4 * v + 2;
                newB[y + size][x] = 4 * v + 3;
                newB[y + size][x + size] = 4 * v + 1;
            }
        }
        B = std::move(newB);
        size *= 2;
    }

    // Normalize to [0,1)
    std::vector<std::vector<float>> normalized(n, std::vector<float>(n));
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            normalized[y][x] = B[y][x] / float(n * n);
        }
    }
    return normalized;
}
#ifndef SBUFFERRENDERER_H
#define SBUFFERRENDERER_H

#include "cglm/cglm.h"
#include "cglm/struct.h"
#include "vector.h"
#include "utils.h"

typedef struct Span {
    float x_start;
    float x_end;
    float z_start;
    float z_end;
    vec3s normal_start;
    vec3s normal_end;
} span_t;

void insert_span(Vector *list, span_t *s);

void insert_span_intersect_override(Vector* list, span_t* s, bool ov);

float orient(vec2s a, vec2s b, vec2s c);

bool intersection(vec2s a1, vec2s a2, vec2s b1, vec2s b2, vec2s *out);

void binary_insert(Vector *list, span_t *s, int low, int high, bool ov);

bool clip_span(span_t *s, float new_x_start, float new_x_end);

void draw_span(span_t *span, int y, PlaydateAPI* pd, BayerMatrix *T);

void draw_scanline(Vector* list, int y, PlaydateAPI* pd, BayerMatrix *T);

#endif
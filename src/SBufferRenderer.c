#include "SBufferRenderer.h"
#include "ScreenGlobals.h"
#include <math.h>
#include "utils.h"

float orient(vec2s a, vec2s b, vec2s c) {
    return glms_vec2_cross(glms_vec2_sub(b, a), glms_vec2_sub(c, a));
}

bool intersection(vec2s a1, vec2s a2, vec2s b1, vec2s b2, vec2s *out) {
    float oa = orient(b1,b2,a1), 
        ob = orient(b1,b2,a2),            
        oc = orient(a1,a2,b1),            
        od = orient(a1,a2,b2);      
    // Proper intersection exists iff opposite signs  
    if (oa*ob < 0 && oc*od < 0) {
        float denom = (ob - oa);
        if (fabsf(denom) < 1e-6f) return false;
        *out = glms_vec2_scale((glms_vec2_sub(glms_vec2_scale(a1, ob), glms_vec2_scale(a2, oa))), 1 / denom); 
        return true;
    }
    return false; 
}

void clip_span(span_t *s, float new_x_start, float new_x_end) {
    // add normal interpolation eventually
    if (new_x_start > s->x_start) {
        float old_x_start = s->x_start;
        float new_x_percentage = calc_percentage(old_x_start, s->x_end, new_x_start);
        s->z_start = lerp(s->z_start, s->z_end, new_x_percentage);
        s->x_start = new_x_start;
    }
    if (new_x_end < s->x_end) {
        float old_x_end = s->x_end;
        float new_x_percentage = calc_percentage(old_x_end, s->x_start, new_x_end);
        s->z_end = lerp(s->z_end, s->z_start, new_x_percentage);
        s->x_end = new_x_end;
    }
    
}

void attempt_span_insert(Vector *list, span_t *s, int index) {
    if (index > list->size) return;
    span_t *current = &VECTOR_GET_AS(span_t, list, index);
    if (s->x_start >= current->x_start) {
        // case1:
        // cccccc
        //    ssssss
        // case 2:
        // cccccc
        //  ssss

        // if no intersection:
        //   determine which is in front, 
        // if s in front:
        //   clamp the left side, add a new right segment 
        //   from s->x_end to current.x_end assuming current.x_end > s->x_end.
        //   interpolate z value and normal
        // if current in front:
        //   add segment to the right of current going from current.x_end to s->x_end
        //   assuming s->x_end > current.x_end.
        //   interpolate z value and normal

        vec2s s1 = {s->x_start, s->z_start};
        vec2s s2 = {s->x_end, s->z_end};
        vec2s current1 = {current->x_start, current->z_start};
        vec2s current2 = {current->x_end, current->z_end};
        vec2s p;

        bool lines_intersect = intersection(s1, s2, current1, current2, &p);

        if (!lines_intersect || glms_vec2_eqv_eps(p, s1) || glms_vec2_eqv_eps(p, s2) || glms_vec2_eqv_eps(p, current1) || glms_vec2_eqv_eps(p, current2)) {
            if (s->z_start < current->z_start) { // s in front
                if (s->x_end <= current->x_end) {
                    // split current into left and right and place s in the middle
                    span_t right = *current;
                    clip_span(&right, s->x_end, right.x_end);
                    clip_span(current, current->x_start, s->x_start);
                    int new_idx = index + 1;
                    vector_insert(list, new_idx, &right);
                    vector_insert(list, new_idx, s);
                    return;
                } else {
                    clip_span(current, current->x_start, s->x_start);
                    attempt_span_insert(list, s, index + 1);
                    return;
                }
            } else {
                if (s->x_end <= current->x_end) {
                    // s fully behind, throw it out
                    return;
                } else {
                    // clip span to end of current and re attempt insertion
                    clip_span(s, current->x_end, s->x_end);
                    attempt_span_insert(list, s, index + 1);
                    return;
                }
            }
        } else { // intersection
            if (s->z_start < current->z_start) { // s in front
                span_t right_s = *s;
                float new_x_start = current->x_end;
                clip_span(s, s->x_start, p.x);
                attempt_span_insert(list, s, index);
                int new_idx = index + 3;
                if (right_s.x_end > current->x_end) {
                    clip_span(&right_s, new_x_start, right_s.x_end);
                    attempt_span_insert(list, &right_s, new_idx);
                    return;
                }
                return;
            }
        }
        
    } else { // s->x_start < current.x_start
        // case1:
        // ssssss
        //   cccccc
        // case2:
        // ssssss
        //  cccc
        attempt_span_insert(list, s, index + 1);
    }
}

void binary_insert(Vector *list, span_t *s, int low, int high) {
    span_t *current;
    while (true) {
        if (low > high) {
            // low is now the index to insert at
            // s start is greater than low-1 end
            // s end is less than low start
            // just insert

            vector_insert(list, low, s);
            return;
        }

        int mid = (low + high) / 2;
        current = &VECTOR_GET_AS(span_t, list, mid);

        if (s->x_start > current->x_end) {
            low = mid + 1;
        } else if (s->x_end < current->x_start) {
            high = mid - 1;
        } else {
            attempt_span_insert(list, s, mid);
            return;
        }
    }
}

void insert_span(Vector *list, span_t *s) {
    if (s->x_start > SCREEN_WIDTH) {
        return;
    }
    if (s->x_end < 0) {
        return;
    }
    binary_insert(list, s, 0, list->size - 1);
}

static inline int max_int(int a, int b) {
    return a > b ? a : b;
}

static inline int min_int(int a, int b) {
    return a < b ? a : b;
}

void draw_span(span_t *span, int y, PlaydateAPI* pd, BayerMatrix *T) {
    int x_start = max_int(0, min_int(SCREEN_WIDTH - 1, (int)ceilf(span->x_start)));
    int x_end = max_int(0, min_int(SCREEN_WIDTH - 1, (int)floorf(span->x_end)));
    // draw start and end black always
    setPixel(pd, x_start, y, kColorBlack);
    setPixel(pd, x_end, y, kColorBlack);
    vec3s norm = span->normal;
    float len = sqrtf(norm.x * norm.x + norm.y * norm.y + norm.z * norm.z);
    float brightness = (norm.y + len) * 0.5f / len;
    int bayer_y = y & 7;
    int bayer_x = x_start & 7;
    for (int x = x_start + 1; x < x_end - 1; x += 1, bayer_x = (bayer_x + 1) & 7) {
        // draw pixel
        int color = (brightness > T->data[bayer_y][bayer_x]) ? kColorWhite : kColorBlack;
        setPixel(pd, x, y, color);
    }
}

void draw_scanline(Vector* list, int y, PlaydateAPI* pd, BayerMatrix *T) {
    int size = list->size;
    span_t *data = (span_t*)list->data;

    for (int i = 0; i < size; i += 1) {
        draw_span(&(data[i]), y, pd, T);
    }
}
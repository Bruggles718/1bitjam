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

float calc_z_at_x(span_t* s, float x) {
    float x_percentage = calc_percentage(s->x_start, s->x_end, x);
    return lerp(s->z_start, s->z_end, x_percentage);
}

bool is_fully_front(span_t* a, span_t* b) {
    return ((a->z_start > b->z_start) && (a->z_end > b->z_end));
}

bool span_in_bounds(span_t* s) {
    return (s->x_start < SCREEN_WIDTH&& s->x_end >= 0);
}

// should be called only when determined that there is full overlap, but no intersection point
// cccccc
//  ssss
void simple_insert(Vector* list, span_t* s, span_t* current, int index) {
    span_t left;
    span_t middle;
    span_t right;
    
    //vector_insert(list, index, &right);
    //vector_insert(list, index, &middle);
    //vector_insert(list, index, &left);
    bool s_in_current = (s->x_start >= current->x_start) && (s->x_end <= current->x_end);
    bool current_in_s = (current->x_start >= s->x_start) && (current->x_end <= s->x_end);
    bool s_in_front = is_fully_front(s, current);
    if (s_in_front && s_in_current) { // s fully in front
        left = *current;
        right = left;
        clip_span(&left, left.x_start, s->x_start);
        clip_span(&right, s->x_end, right.x_end);
        middle = *s;
        vector_erase(list, index);
        insert_span(list, &right);
        insert_span(list, &middle);
        insert_span(list, &left);
    }
    else if (!s_in_front && current_in_s) {
        left = *s;
        right = left;
        clip_span(&left, left.x_start, current->x_start);
        clip_span(&right, current->x_end, right.x_end);
        middle = *current;
        vector_erase(list, index);
        //vector_insert(list, index, &right);
        //vector_insert(list, index, &middle);
        //vector_insert(list, index, &left);
        insert_span(list, &right);
        insert_span(list, &middle);
        insert_span(list, &left);
    }
    else if (!s_in_front && s_in_current) {
        return;
    }
    else if (s_in_front && current_in_s) {
        /**current = *s;*/
        vector_erase(list, index);
        insert_span(list, s);
    }
    
    

    //insert_span(list, &right);
    //insert_span(list, &middle);
    //insert_span(list, &left);
}

void overlap_right_insert(Vector* list, span_t* s, span_t *current, int index) {
    span_t left = *current;
    span_t right = *s;
    if (is_fully_front(s, current)) { // s fully in front
        clip_span(&left, left.x_start, s->x_start);
    }
    else {
        clip_span(&right, current->x_end, right.x_end);
    }
    vector_erase(list, index);
    insert_span(list, &right);
    insert_span(list, &left);
}

void intersect_split_insert(Vector *list, span_t *s, span_t* current, int index, bool ov) {
    vec2s s1 = { s->x_start, s->z_start };
    vec2s s2 = { s->x_end, s->z_end };
    vec2s current1 = { current->x_start, current->z_start };
    vec2s current2 = { current->x_end, current->z_end };
    vec2s p;
    bool lines_intersect = intersection(s1, s2, current1, current2, &p);
    if (ov || !lines_intersect) {
        bool s_in_current = (s->x_start >= current->x_start) && (s->x_end <= current->x_end);
        bool current_in_s = (current->x_start >= s->x_start) && (current->x_end <= s->x_end);
        if (s_in_current || current_in_s) { // full overlap
            simple_insert(list, s, current, index);
        }
        else { // not full overlap
            if (s->x_start >= current->x_start) {
                overlap_right_insert(list, s, current, index);
            }
            else {
                overlap_right_insert(list, current, s, index);
            }
        }
    }
    else if (lines_intersect) {
        span_t sleft = *s;
        span_t sright = sleft;
        span_t cleft = *current;
        span_t cright = cleft;
        clip_span(&sleft, sleft.x_start, p.x);
        clip_span(&sright, p.x, sright.x_end);
        clip_span(&cleft, cleft.x_start, p.x);
        clip_span(&cright, p.x, cright.x_end);
        vector_erase(list, index);
        insert_span_intersect_override(list, &cleft, true);
        insert_span_intersect_override(list, &cright, true);
        insert_span_intersect_override(list, &sleft, true);
        insert_span_intersect_override(list, &sright, true);
    }
}

void binary_insert(Vector *list, span_t *s, int low, int high, bool ov) {
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

        if (s->x_start >= current->x_end) {
            low = mid + 1;
        } else if (s->x_end <= current->x_start) {
            high = mid - 1;
        } else {
            intersect_split_insert(list, s, current, mid, ov);
            return;
        }
    }
}

bool bad_float(float f) {
    return (isnan(f) || !isfinite(f));
}

float clamp_to_screen_x(float f) {
    return fmaxf(0, fminf(f, SCREEN_WIDTH - 1));
}

void insert_span_intersect_override(Vector* list, span_t* s, bool ov) {
    if (s->x_start >= SCREEN_WIDTH) {
        return;
    }
    if (s->x_end < 0) {
        return;
    }
    clip_span(s, clamp_to_screen_x(s->x_start), clamp_to_screen_x(s->x_end));
    if (fabsf(s->x_end - s->x_start) < 1e-2f) {
        return;
    }
    if (s->x_start >= s->x_end) {
        return;
    }
    if (bad_float(s->x_start) || bad_float(s->x_end) || bad_float(s->z_start) || bad_float(s->z_end)) return;
    binary_insert(list, s, 0, list->size - 1, ov);
}

void insert_span(Vector *list, span_t *s) {
    insert_span_intersect_override(list, s, false);
}

static inline int max_int(int a, int b) {
    return a > b ? a : b;
}

static inline int min_int(int a, int b) {
    return a < b ? a : b;
}

void draw_span(span_t *span, int y, PlaydateAPI* pd, BayerMatrix *T) {
    int line_thickness = 1;

    int x_start = max_int(0, min_int(SCREEN_WIDTH - 1, (int)ceilf(span->x_start)));
    int x_end = max_int(0, min_int(SCREEN_WIDTH - 1, (int)floorf(span->x_end)));
    // draw start and end black always
    for (int i = 0; i < line_thickness; i++) {
        setPixel(pd, x_start + i, y, kColorBlack);
        setPixel(pd, x_end - i, y, kColorBlack);
    }
    vec3s norm = span->normal;
    float len = sqrtf(norm.x * norm.x + norm.y * norm.y + norm.z * norm.z);
    float brightness = 1;
    if (len >= 1e-6f) {
        brightness = (norm.y + len) * 0.5f / len;
    }
    int bayer_y = y & 7;
    int bayer_x = x_start & 7;
    for (int x = x_start + line_thickness; x < x_end - line_thickness; x += 1, bayer_x = (bayer_x + 1) & 7) {
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
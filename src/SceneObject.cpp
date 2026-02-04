#include "SceneObject.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <algorithm>
#include "ScreenGlobals.hpp"
#include "utils.hpp"

static inline float min3(float a, float b, float c) {
    float min_val = a;
    if (b < min_val) min_val = b;
    if (c < min_val) min_val = c;
    return min_val;
}

static inline float max3(float a, float b, float c) {
    float max_val = a;
    if (b > max_val) max_val = b;
    if (c > max_val) max_val = c;
    return max_val;
}

static inline int max_int(int a, int b) {
    return a > b ? a : b;
}

static inline int min_int(int a, int b) {
    return a < b ? a : b;
}

static inline int abs_int(int x) {
    return x < 0 ? -x : x;
}

VertexData::VertexData(std::vector<float> i_vertex_buffer) {
    m_vertex_buffer = i_vertex_buffer;
}

VertexData::~VertexData() {
    
}

void VertexData::add_to_vertex_buffer(float i_f) {
    m_vertex_buffer.push_back(i_f);
}

static inline float edge(
    float ax, float ay,
    float bx, float by,
    float px, float py)
{
    return (px - ax) * (by - ay)
         - (py - ay) * (bx - ax);
}

void drawLineZ(PlaydateAPI* pd, 
               std::vector<float>& depth_buffer,
               int x0, int y0, float iz0,
               int x1, int y1, float iz1, 
               uint8_t color)
{
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    int x = x0;
    int y = y0;

    int steps = std::max(dx, dy);
    for (int i = 0; i <= steps; i++) {
        // Linear interpolation along the line
        float t = steps == 0 ? 0 : (float)i / steps;
        float iz = iz0 * (1.0f - t) + iz1 * t;

        int idx = y * SCREEN_WIDTH + x;
        if (iz > depth_buffer[idx]) {
            depth_buffer[idx] = iz;
            pd->graphics->setPixel(x, y, color);
        }

        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx)  { err += dx; y += sy; }
    }
}

void drawLineZThick(PlaydateAPI* pd,
                    std::vector<float>& depth_buffer,
                    int x0, int y0, float iz0,
                    int x1, int y1, float iz1,
                    uint8_t color, int thickness)
{
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    int steps = std::max(dx, dy);
    for (int i = 0; i <= steps; i++) {
        float tline = steps == 0 ? 0 : (float)i / steps;
        float iz = iz0 * (1.0f - tline) + iz1 * tline;

        // Compute main pixel along the line
        int xi = x0;
        int yi = y0;

        // Draw thick pixels perpendicular
        for (int ox = -thickness; ox <= thickness; ox++) {
            for (int oy = -thickness; oy <= thickness; oy++) {
                int px = xi + ox;
                int py = yi + oy;
                if (px < 0 || px >= SCREEN_WIDTH || py < 0 || py >= SCREEN_HEIGHT)
                    continue;

                int idx = py * SCREEN_WIDTH + px;
                if (iz > depth_buffer[idx]) {
                    depth_buffer[idx] = iz;
                    pd->graphics->setPixel(px, py, color);
                }
            }
        }

        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

typedef struct {
    int x;           /* Current x position */
    int z;           /* Current z */
    int dx;
    int dy;
    int dz;
    int sx;
    int sy;
    int sz;
    int error;
    int error_z;
    int y_start;       /* Starting scanline */
    int y_end;         /* Ending scanline */
    int x_start;
    int x_end;
    int z_start;
    int z_end;
    glm::vec3 normal_start;
    glm::vec3 normal_end;
    glm::vec3 normal;
} EdgeData;

typedef struct ClipVert {
    float x, y, z;
    float nx, ny, nz;
} ClipVert;

static inline void sort_clipvert_by_y(ClipVert *v1, ClipVert *v2, ClipVert *v3) {
    /* Bubble sort is fine for 3 elements */
    if (v1->y > v2->y) {
        ClipVert temp = *v1;
        *v1 = *v2;
        *v2 = temp;
    }
    if (v2->y > v3->y) {
        ClipVert temp = *v2;
        *v2 = *v3;
        *v3 = temp;
    }
    if (v1->y > v2->y) {
        ClipVert temp = *v1;
        *v1 = *v2;
        *v2 = temp;
    }
}

static inline void setup_edge_clipvert(EdgeData* edge, ClipVert* v1, ClipVert* v2) {
    edge->y_start = (int)ceilf(v1->y);
    edge->y_end = (int)ceilf(v2->y);
    edge->x_start = (int)ceilf(v1->x);
    edge->x_end = (int)ceilf(v2->x);
    int z_scale = INT16_MAX;
    edge->z_start = (int)(v1->z * z_scale);
    edge->z_end = (int)(v2->z * z_scale);


    edge->normal_start = {v1->nx, v1->ny, v1->nz};
    edge->normal_end = {v2->nx, v2->ny, v2->nz};

    edge->x = edge->x_start;
    edge->z = edge->z_start;
    edge->normal = { v1->nx, v1->ny, v1->nz };

    edge->dx = abs(edge->x_end - edge->x_start);
    edge->sx = edge->x_start < edge->x_end ? 1 : -1;
    edge->dy = -abs(edge->y_end - edge->y_start);
    edge->sy = edge->y_start < edge->y_end ? 1 : -1;
    edge->dz = abs(edge->z_end - edge->z_start);
    edge->sz = edge->z_start < edge->z_end ? 1 : -1;

    edge->error = edge->dx + edge->dy;
    edge->error_z = edge->dz + edge->dy;
}

static inline void step_edge(EdgeData& edge, int y) {
    while (true) {
        if (edge.x == edge.x_end && y == edge.y_end) {
            break;
        }
        int e2 = 2 * edge.error;
        if (e2 >= edge.dy) {
            if (edge.x == edge.x_end) {
                break;
            }
            edge.error += edge.dy;
            edge.x += edge.sx;
        }
        if (e2 <= edge.dx) {
            if (y == edge.y_end) {
                break;
            }
            edge.error += edge.dx;
            break;
        }
    }

    while (true) {
        if (edge.z == edge.z_end && y == edge.y_end) {
            break;
        }
        int e2 = 2 * edge.error_z;
        if (e2 >= edge.dy) {
            if (edge.z == edge.z_end) {
                break;
            }
            edge.error_z += edge.dy;
            edge.z += edge.sz;
        }
        if (e2 <= edge.dz) {
            if (y == edge.y_end) {
                break;
            }
            edge.error_z += edge.dz;
            break;
        }
    }
}

typedef struct {
    uint32_t low;
    uint32_t high;
} uint64_emulated;

static inline uint64_emulated multiply_32x32_to_64(uint32_t a, uint32_t b) {
    uint64_emulated result;
    
    uint32_t a_low = a & 0xFFFF;
    uint32_t a_high = a >> 16;
    uint32_t b_low = b & 0xFFFF;
    uint32_t b_high = b >> 16;
    
    uint32_t low_low = a_low * b_low;
    uint32_t low_high = a_low * b_high;
    uint32_t high_low = a_high * b_low;
    uint32_t high_high = a_high * b_high;
    
    uint32_t middle = low_high + high_low;
    uint32_t carry = (middle < low_high) ? 1 : 0;
    
    result.low = low_low + (middle << 16);
    result.high = high_high + (middle >> 16) + carry + ((result.low < low_low) ? 1 : 0);
    
    return result;
}

static inline uint32_t divide_64_by_32_to_32(uint64_emulated dividend, uint32_t divisor) {
    uint32_t remainder = dividend.high % divisor;
    uint32_t quotient = 0;
    
    for (int i = 31; i >= 0; i--) {
        remainder = (remainder << 1) | ((dividend.low >> i) & 1);
        if (remainder >= divisor) {
            remainder -= divisor;
            quotient |= (1U << i);
        }
    }
    
    return quotient;
}

static inline void step_edge_constant(EdgeData& edge, int target_y) {
    int dy_steps = target_y - edge.y_start;
    if (dy_steps <= 0) return;

    int total_dy = -edge.dy;
    if (total_dy == 0) return;

    int total_dx = edge.dx;
    int total_dz = edge.dz;

    int x_steps = divide_64_by_32_to_32(multiply_32x32_to_64(total_dx, dy_steps), total_dy);
    int z_steps = divide_64_by_32_to_32(multiply_32x32_to_64(total_dz, dy_steps), total_dy);

    edge.x = edge.x_start + x_steps * edge.sx;
    edge.z = edge.z_start + z_steps * edge.sz;
}

static inline void clamp_edge(EdgeData& edge, int y_top) {
    int y_diff = y_top - edge.y_start;
    if (y_diff > 0) {
        int y_span = (edge.y_end - edge.y_start);
        float lt = y_span != 0 ? (float)(y_top - edge.y_start) / y_span : 0.0f;
        edge.x = my_lerp(edge.x_start, edge.x_end, lt);
        edge.z = my_lerp(edge.z_start, edge.z_end, lt);
        edge.normal = glm::mix(edge.normal_start, edge.normal_end, lt);
    }
}

static ClipVert intersect_near(ClipVert a, ClipVert b, float near_z)
{
    float t = (near_z - a.z) / (b.z - a.z);

    ClipVert r;
    r.x = a.x + (b.x - a.x) * t;
    r.y = a.y + (b.y - a.y) * t;
    r.z = near_z;
    r.nx = a.nx + (b.nx - a.nx) * t;
    r.ny = a.ny + (b.ny - a.ny) * t;
    r.nz = a.nz + (b.nz - a.nz) * t;
    return r;
}

int clip_triangle_near(
    ClipVert in[3],
    ClipVert out[6],
    float near_z)
{
    int inside[3];
    for (int i = 0; i < 3; i++)
        inside[i] = (in[i].z <= near_z);

    int count =
        inside[0] +
        inside[1] +
        inside[2];

    /* Fully outside */
    if (count == 0)
        return 0;

    /* Fully inside */
    if (count == 3) {
        out[0] = in[0];
        out[1] = in[1];
        out[2] = in[2];
        return 1; /* one triangle */
    }

    /* One inside */
    if (count == 1) {
        int i0 = inside[0] ? 0 : inside[1] ? 1 : 2;
        int i1 = (i0 + 1) % 3;
        int i2 = (i0 + 2) % 3;

        ClipVert v0 = in[i0];
        ClipVert v1 = intersect_near(v0, in[i1], near_z);
        ClipVert v2 = intersect_near(v0, in[i2], near_z);

        out[0] = v0;
        out[1] = v1;
        out[2] = v2;
        return 1;
    }

    /* Two inside */
    int i0 = !inside[0] ? 0 : !inside[1] ? 1 : 2;
    int i1 = (i0 + 1) % 3;
    int i2 = (i0 + 2) % 3;

    ClipVert v0 = in[i1]; // real v0
    ClipVert v1 = in[i2]; // real v1
    ClipVert v2 = intersect_near(v0, in[i0], near_z); // new v2
    ClipVert v3 = intersect_near(v1, in[i0], near_z); // new v3

    /* split quad into 2 triangles */
    out[0] = v0;
    out[1] = v1;
    out[2] = v3;

    out[3] = v0;
    out[4] = v2;
    out[5] = v3;

    return 2;
}

float Q_rsqrt(float number)
{
    long i;
    float x2, y;
    const float threehalfs = 1.5F;

    x2 = number * 0.5F;
    y = number;
    i = *(long*)&y;                       // evil floating point bit level hacking
    i = 0x5f3759df - (i >> 1);               // what the fuck?
    y = *(float*)&i;
    y = y * (threehalfs - (x2 * y * y));   // 1st iteration
//	y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed

    return y;
}

void normalize_clipvert(ClipVert* v) {
    float mag_sqr = v->nx * v->nx + v->ny * v->ny + v->nz * v->nz;
    float inv_sqrt = Q_rsqrt(mag_sqr);
    //vec3s norm = { nx * inv_sqrt, ny * inv_sqrt, nz * inv_sqrt };
    v->nx = v->nx * inv_sqrt;
    v->ny = v->ny * inv_sqrt;
    v->nz = v->nz * inv_sqrt;
}

/* Fill a horizontal span with integrated edge drawing */
static inline void fill_span(PlaydateAPI* pd, int* depth_buffer, 
    std::vector<std::vector<int>>& bayer_matrix,
    int y, EdgeData& left, EdgeData& right) {

    int x_start = max_int(0, left.x);
    int x_end = min_int(SCREEN_WIDTH - 1, right.x);

    if (x_start > x_end) return;

    float span_width = right.x - left.x;
    if (fabsf(span_width) < 0.001f) return;

    // float dz = (right.z - left.z) / span_width;
    // float prestep = x_start - left.x;
    // float z = left.z + dz * prestep;
    int total_dz = right.z - left.z;
    int prestep = x_start - left.x;

    int idx = y * SCREEN_WIDTH + x_start;
    int bayer_y = y & 7;
    int bayer_x = x_start & 7;

    int* bayer_row = bayer_matrix[bayer_y].data();

    const int edge_width = 1;  /* Change this to adjust edge thickness */
    const float edge_depth_offset = 1;  /* Bring edges slightly closer */

    float norm_diff = left.normal.y - right.normal.y;
    float dn = norm_diff / span_width;
    float current_normal = left.normal.y + dn * prestep;

    for (int x = x_start; x <= x_end; x++, idx++, bayer_x = (bayer_x + 1) & 7) {
        int total_dx = x - left.x;
        int z = left.z + ((total_dz * total_dx) / span_width);

        /* Draw outer 2 pixels on each edge in black */
        float brightness = (current_normal + 1) * 0.5f;
        int lum = brightness * 255;
        int dist_from_left = x - x_start;
        int dist_from_right = x_end - x;

        bool is_left_edge = (dist_from_left < edge_width);
        bool is_right_edge = (dist_from_right < edge_width);

        /* Edges use offset depth to always appear on top */
        float z_to_test = (is_left_edge || is_right_edge) ?
            z + edge_depth_offset : z;
        if (idx < 0 || idx >= SCREEN_WIDTH * SCREEN_HEIGHT) continue;
        if (z_to_test < depth_buffer[idx]) {
            depth_buffer[idx] = z_to_test;

            int color;
            if (is_left_edge || is_right_edge) {
                color = kColorBlack;
            }
            else {
                color = (lum > bayer_row[bayer_x]) ? kColorWhite : kColorBlack;
            }
            setPixel(pd, x, y, color);
        }
        current_normal += dn;
    }
}

static inline void fill_spans_y(PlaydateAPI* pd, int* depth_buffer,
    std::vector<std::vector<int>>& bayer_matrix,
    int y_start, int y_end, EdgeData& left, EdgeData& right) {
    float lt_mul = 1.0f / (left.y_end - left.y_start);
    float rt_mul = 1.0f / (right.y_end - right.y_start);
    for (int y = y_start; y < y_end; y += 1) {
        step_edge_constant(left, y);
        step_edge_constant(right, y);

        float lt = ((float)y - left.y_start) * lt_mul;
        // left.x = my_lerp(left.x_start, left.x_end, lt);
        // left.z = my_lerp(left.z_start, left.z_end, lt);
        left.normal = glm::mix(left.normal_start, left.normal_end, lt);
        float rt = ((float)y - right.y_start) * rt_mul;
        // right.x = my_lerp(right.x_start, right.x_end, rt);
        // right.z = my_lerp(right.z_start, right.z_end, rt);
        right.normal = glm::mix(right.normal_start, right.normal_end, rt);

        fill_span(pd, depth_buffer, bayer_matrix, y, left, right);
        //step_edge(left, y);
        //step_edge(right, y);
    }
}

void VertexData::draw(PlaydateAPI* pd, glm::mat4& model, glm::mat4& view, glm::mat4& projection, 
    int* depth_buffer, std::vector<std::vector<int>>& bayer_matrix) {
    glm::mat4 mv = view * model;
    glm::mat4 mvp = projection * mv;

    glm::mat3 normal_mat = glm::mat3(model);
    normal_mat = glm::inverse(normal_mat);
    normal_mat = glm::transpose(normal_mat);

    float* buf = m_vertex_buffer.data();
    size_t buf_size = m_vertex_buffer.size();

    const float hw = SCREEN_WIDTH * 0.5f;
    const float hh = SCREEN_HEIGHT * 0.5f;

    for (size_t i = 0; i < buf_size; i += 18) {
        glm::vec4 pos1 = { buf[i], buf[i + 1], buf[i + 2], 1.0f };
        glm::vec3 n1 = { buf[i + 3], buf[i + 4], buf[i + 5] };

        glm::vec4 pos2 = { buf[i + 6], buf[i + 7], buf[i + 8], 1.0f };
        glm::vec3 n2 = { buf[i + 9], buf[i + 10], buf[i + 11] };

        glm::vec4 pos3 = { buf[i + 12], buf[i + 13], buf[i + 14], 1.0f };
        glm::vec3 n3 = { buf[i + 15], buf[i + 16], buf[i + 17] };

        glm::vec4 view_pos1, view_pos2, view_pos3;
        view_pos1 = mv * pos1;
        view_pos2 = mv * pos2;
        view_pos3 = mv * pos3;

        /* Z-clip */
        if (view_pos1.z >= 0 && view_pos2.z >= 0 && view_pos3.z >= 0) continue;

        /* Backface culling */
        float e1x = view_pos2.x - view_pos1.x;
        float e1y = view_pos2.y - view_pos1.y;
        float e1z = view_pos2.z - view_pos1.z;
        float e2x = view_pos3.x - view_pos1.x;
        float e2y = view_pos3.y - view_pos1.y;
        float e2z = view_pos3.z - view_pos1.z;

        float view_dir_x = -view_pos1.x;  // Camera at origin
        float view_dir_y = -view_pos1.y;
        float view_dir_z = -view_pos1.z;

        float nx = e1y * e2z - e1z * e2y;
        float ny = e1z * e2x - e1x * e2z;
        float nz = e1x * e2y - e1y * e2x;
        float facing = nx * view_dir_x + ny * view_dir_y + nz * view_dir_z;

        if (facing < 0) continue;

        /* Lighting */

        n1 = normal_mat * n1;
        n2 = normal_mat * n2;
        n3 = normal_mat * n3;

        /* Fixed world-space light direction (pointing up) */
        const float light_y = 1.0f;

        /* Calculate brightness from world-space normal */
        //float len = sqrtf(nx * nx + ny * ny + nz * nz);
        //if (len < 0.001f) continue;

        //float brightness = (ny + len) * 0.5f / len;

        ClipVert vin[3] = {
            { view_pos1.x, view_pos1.y, view_pos1.z, n1.x, n1.y, n1.z },
            { view_pos2.x, view_pos2.y, view_pos2.z, n2.x, n2.y, n2.z },
            { view_pos3.x, view_pos3.y, view_pos3.z, n3.x, n3.y, n3.z }
        };

        ClipVert vout[6];
        int tri_count = clip_triangle_near(vin, vout, -NEAR_PLANE); // your near plane

        if (tri_count == 0)
            continue;

        for (int t = 0; t < tri_count; t++) {

            ClipVert a = vout[t * 3 + 0];
            ClipVert b = vout[t * 3 + 1];
            ClipVert c = vout[t * 3 + 2];

            /* Use a,b,c as your new triangle */
            /* Continue pipeline from here */

            view_pos1 = { a.x, a.y, a.z, 1.0f };
            view_pos2 = { b.x, b.y, b.z, 1.0f };
            view_pos3 = { c.x, c.y, c.z, 1.0f };

            /* Project to screen space */
            glm::vec4 clip1, clip2, clip3;
            clip1 = projection * view_pos1;
            clip2 = projection * view_pos2;
            clip3 = projection * view_pos3;

            float inv_w1 = 1.0f / clip1.w;
            float inv_w2 = 1.0f / clip2.w;
            float inv_w3 = 1.0f / clip3.w;

            float x1 = (clip1.x * inv_w1 + 1.0f) * hw;
            float y1 = (1.0f - clip1.y * inv_w1) * hh;
            float x2 = (clip2.x * inv_w2 + 1.0f) * hw;
            float y2 = (1.0f - clip2.y * inv_w2) * hh;
            float x3 = (clip3.x * inv_w3 + 1.0f) * hw;
            float y3 = (1.0f - clip3.y * inv_w3) * hh;

            // After projection and screen transform
            //float z1 = 1.0f / -view_pos1.z;  // Use reciprocal of view Z
            //float z2 = 1.0f / -view_pos2.z;
            //float z3 = 1.0f / -view_pos3.z;
            float z1 = clip1.z * inv_w1;
            float z2 = clip2.z * inv_w2;
            float z3 = clip3.z * inv_w3;

            z1 = z1 * 0.5f + 0.5f;
            z2 = z2 * 0.5f + 0.5f;
            z3 = z3 * 0.5f + 0.5f;


            float min_x = min3(x1, x2, x3);
            float max_x = max3(x1, x2, x3);
            float min_y = min3(y1, y2, y3);
            float max_y = max3(y1, y2, y3);

            if (max_x < 0 || min_x >= SCREEN_WIDTH ||
                max_y < 0 || min_y >= SCREEN_HEIGHT) {
                continue;  /* Triangle completely off-screen */
            }

            ClipVert v1 = { x1, y1, z1, a.nx, a.ny, a.nz };
            ClipVert v2 = { x2, y2, z2, b.nx, b.ny, b.nz };
            ClipVert v3 = { x3, y3, z3, c.nx, c.ny, c.nz };

            // normalize_clipvert(&v1);
            // normalize_clipvert(&v2);
            // normalize_clipvert(&v3);

            /* Sort vertices by Y coordinate */
            sort_clipvert_by_y(&v1, &v2, &v3);
            //sort_vertices_by_y(&x1, &y1, &z1, &x2, &y2, &z2, &x3, &y3, &z3);

            /* Check for degenerate triangle */
            if (fabsf(v3.y - v1.y) < 0.001f) continue;

            /* Setup edges */
            EdgeData edge_long;   /* v1 to v3 (long edge) */
            EdgeData edge_short1; /* v1 to v2 (short edge 1) */
            EdgeData edge_short2; /* v2 to v3 (short edge 2) */

            /* Don't clamp y_min/y_max initially - use original values */
            int y_top = max_int(0, (int)ceilf(v1.y));
            int y_bottom = min_int(SCREEN_HEIGHT - 1, (int)floorf(v3.y));

            /* Setup edges with original values */
            setup_edge_clipvert(&edge_long, &v1, &v3);
            setup_edge_clipvert(&edge_short1, &v1, &v2);
            setup_edge_clipvert(&edge_short2, &v2, &v3);

            /* Determine which side the middle vertex is on */
            float mid_x_on_long = v1.x + (v3.x - v1.x) * (v2.y - v1.y) / (v3.y - v1.y);
            int middle_is_right = (v2.x > mid_x_on_long);

            /* Rasterize top half (v1 to v2) */
            int y_mid = min_int(SCREEN_HEIGHT - 1, max_int(0, (int)ceilf(v2.y)));

            //if (y_top > edge_long.y_start) {
            //    clamp_edge(edge_long, y_top);
            //}
            //if (y_top > edge_short1.y_start) {
            //    clamp_edge(edge_short1, y_top);
            //}

            EdgeData* left_edge = middle_is_right ? &edge_long : &edge_short1;
            EdgeData* right_edge = middle_is_right ? &edge_short1 : &edge_long;

            fill_spans_y(pd, depth_buffer, bayer_matrix, y_top, y_mid, *left_edge, *right_edge);

            //if (y_mid > edge_short2.y_start) {
            //    clamp_edge(edge_short2, y_mid);
            //}

            /* Rasterize bottom half (v2 to v3) */
            left_edge = middle_is_right ? &edge_long : &edge_short2;
            right_edge = middle_is_right ? &edge_short2 : &edge_long;

            fill_spans_y(pd, depth_buffer, bayer_matrix, y_mid, y_bottom + 1, *left_edge, *right_edge);
        }
    }
}

void VertexData::print_vertex_buffer() {
    for (float f : m_vertex_buffer) {
        std::cout << f << std::endl;
    }
}

SceneObject::SceneObject(std::shared_ptr<VertexData> i_vertex_data) {
    m_vertex_data = i_vertex_data;
    
}

SceneObject::SceneObject() {
    m_vertex_data = nullptr;
}

SceneObject::~SceneObject() {
    // Delete our Graphics pipeline
    
}

void SceneObject::send_vertex_data_to_gpu() {
    m_vertex_data->send_to_gpu();
}

void SceneObject::draw(const Camera& i_camera, PlaydateAPI* pd, int* depth_buffer,
    std::vector<std::vector<int>>& bayer_matrix) {
    // glm::mat4 model = glm::translate(glm::mat4(1.0f), m_transform.m_position);
    // model = model * glm::mat4_cast(m_transform.m_rotation);
    // model = glm::scale(model, m_transform.m_scale);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), m_transform.m_position)
                * glm::mat4_cast(m_transform.m_rotation)
                * glm::scale(glm::mat4(1.0f), m_transform.m_scale);

    glm::mat4 view = i_camera.GetViewMatrix();
    glm::mat4 perspective = glm::perspective(
        glm::radians(45.0f),
        (float)SCREEN_WIDTH/(float)SCREEN_HEIGHT,
        NEAR_PLANE,
        1000.0f);
    m_vertex_data->draw(pd, model, view, perspective, depth_buffer, bayer_matrix);
}

void SceneObject::set_transform(Transform i_tf) {
    m_transform = i_tf;
}
void SceneObject::set_position(glm::vec3 i_position) {
    m_transform.m_position = i_position;
}
void SceneObject::set_scale(glm::vec3 i_scale) {
    m_transform.m_scale = i_scale;
}
void SceneObject::set_rotation(glm::quat i_rotation) {
    m_transform.m_rotation = i_rotation;
}
void SceneObject::rotate(float i_angle_degrees, glm::vec3 i_axis) {
    glm::quat rotation_quat = glm::angleAxis(glm::radians(i_angle_degrees), glm::normalize(i_axis));
    m_transform.m_rotation = rotation_quat * m_transform.m_rotation;
}
void SceneObject::set_diffuse_color(glm::vec3 i_diffuse_color) {
    m_diffuse_color = i_diffuse_color;
}
void SceneObject::set_specular_strength(float i_specular_strength) {
    m_specular_strength = i_specular_strength;
}

Transform SceneObject::get_transform() {
    return m_transform;
}
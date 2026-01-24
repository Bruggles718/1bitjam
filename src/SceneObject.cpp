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
    float x;           /* Current x position */
    float dx;          /* x increment per scanline */
    float z;           /* Current z (inverse depth) */
    float dz;          /* z increment per scanline */
    int y_start;       /* Starting scanline */
    int y_end;         /* Ending scanline */
    int x_start;
    int x_end;
    float z_start;
    float z_end;
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
    edge->z_start = v1->z;
    edge->z_end = v2->z;

    edge->normal_start = {v1->nx, v1->ny, v1->nz};
    edge->normal_end = {v2->nx, v2->ny, v2->nz};

    edge->x = v1->x;
    edge->z = v1->z;
    edge->normal = { v1->nx, v1->ny, v1->nz };
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
static inline void fill_span(PlaydateAPI* pd, std::vector<float>& depth_buffer, 
    std::vector<std::vector<float>>& bayer_matrix,
    int y, float x_left, float x_right,
    float z_left, float z_right, glm::vec3 n_left, glm::vec3 n_right,
    int draw_left_edge, int draw_right_edge) {

    int x_start = std::max(0, (int)ceilf(x_left));
    int x_end = std::min(SCREEN_WIDTH - 1, (int)floorf(x_right));

    if (x_start > x_end) return;

    float span_width = x_right - x_left;
    if (fabsf(span_width) < 0.001f) return;

    float dz = (z_right - z_left) / span_width;
    float prestep = x_start - x_left;
    float z = z_left + dz * prestep;

    int idx = y * SCREEN_WIDTH + x_start;
    int bayer_y = y & 7;
    int bayer_x = x_start & 7;

    const int edge_width = 1;  /* Change this to adjust edge thickness */
    const float edge_depth_offset = 0.0001f;  /* Bring edges slightly closer */

    float norm_diff = n_left.y - n_right.y;
    float dn = norm_diff / span_width;
    float current_normal = n_left.y + dn * prestep;
    float brightness = (current_normal + 1) * 0.5f;

    for (int x = x_start; x <= x_end; x++, idx++, bayer_x = (bayer_x + 1) & 7) {
        /* Draw outer 2 pixels on each edge in black */
        int dist_from_left = x - x_start;
        int dist_from_right = x_end - x;

        int is_left_edge = (dist_from_left < edge_width) && draw_left_edge;
        int is_right_edge = (dist_from_right < edge_width) && draw_right_edge;

        /* Edges use offset depth to always appear on top */
        float z_to_test = (is_left_edge || is_right_edge) ?
            z + edge_depth_offset : z;
        if (idx < 0 || idx >= SCREEN_WIDTH * SCREEN_HEIGHT) continue;
        if (z_to_test > depth_buffer[idx]) {
            depth_buffer[idx] = z_to_test;

            int color;
            if (is_left_edge || is_right_edge) {
                color = kColorBlack;
            }
            else {
                color = (brightness > bayer_matrix[bayer_y][bayer_x]) ? kColorWhite : kColorBlack;
            }
            setPixel(pd, x, y, color);
        }
        z += dz;
        current_normal += dn;
    }
}

void VertexData::draw(PlaydateAPI* pd, glm::mat4& model, glm::mat4& view, glm::mat4& projection, 
    std::vector<float>& depth_buffer, std::vector<std::vector<float>>& bayer_matrix) {
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
            { view_pos1[0], view_pos1[1], view_pos1[2], n1[0], n1[1], n1[2]},
            { view_pos2[0], view_pos2[1], view_pos2[2], n2[0], n2[1], n2[2] },
            { view_pos3[0], view_pos3[1], view_pos3[2], n3[0], n3[1], n3[2] }
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

            float inv_w1 = 1.0f / clip1[3];
            float inv_w2 = 1.0f / clip2[3];
            float inv_w3 = 1.0f / clip3[3];

            float x1 = (clip1[0] * inv_w1 + 1.0f) * hw;
            float y1 = (1.0f - clip1[1] * inv_w1) * hh;
            float x2 = (clip2[0] * inv_w2 + 1.0f) * hw;
            float y2 = (1.0f - clip2[1] * inv_w2) * hh;
            float x3 = (clip3[0] * inv_w3 + 1.0f) * hw;
            float y3 = (1.0f - clip3[1] * inv_w3) * hh;

            // After projection and screen transform
            float z1 = 1.0f / -view_pos1[2];  // Use reciprocal of view Z
            float z2 = 1.0f / -view_pos2[2];
            float z3 = 1.0f / -view_pos3[2];

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

            normalize_clipvert(&v1);
            normalize_clipvert(&v2);
            normalize_clipvert(&v3);

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
            EdgeData* left_edge = middle_is_right ? &edge_long : &edge_short1;
            EdgeData* right_edge = middle_is_right ? &edge_short1 : &edge_long;

            for (int y = y_top; y < y_mid; y++) {
                
                float lt = ((float)y - left_edge->y_start) / (left_edge->y_end - left_edge->y_start);
                left_edge->x = my_lerp((float)left_edge->x_start, (float)left_edge->x_end, lt);
                left_edge->z = my_lerp((float)left_edge->z_start, (float)left_edge->z_end, lt);
                //left_edge->z += left_edge->dz;
                float rt = ((float)y - right_edge->y_start) / (right_edge->y_end - right_edge->y_start);
                right_edge->x = my_lerp((float)right_edge->x_start, (float)right_edge->x_end, rt);
                right_edge->z = my_lerp((float)right_edge->z_start, (float)right_edge->z_end, rt);
                //right_edge->z += right_edge->dz;
                
                /* Draw left edge if it's the short edge, right edge if it's the short edge */
                int draw_left = !middle_is_right;  /* short edge on left */
                int draw_right = middle_is_right;   /* short edge on right */
                /* ALWAYS draw both edges - left is always an edge, right is always an edge */
                if (y >= 0 && y < SCREEN_HEIGHT) {
                    fill_span(pd, depth_buffer, bayer_matrix, y,
                        left_edge->x, right_edge->x,
                        left_edge->z, right_edge->z, left_edge->normal, right_edge->normal,
                        1, 1);
                }
            }

            /* Rasterize bottom half (v2 to v3) */
            left_edge = middle_is_right ? &edge_long : &edge_short2;
            right_edge = middle_is_right ? &edge_short2 : &edge_long;

            for (int y = y_mid; y <= y_bottom; y++) {

                float lt = ((float)y - left_edge->y_start) / (left_edge->y_end - left_edge->y_start);
                left_edge->x = my_lerp((float)left_edge->x_start, (float)left_edge->x_end, lt);
                left_edge->z = my_lerp((float)left_edge->z_start, (float)left_edge->z_end, lt);
                //left_edge->z += left_edge->dz;
                float rt = ((float)y - right_edge->y_start) / (right_edge->y_end - right_edge->y_start);
                right_edge->x = my_lerp((float)right_edge->x_start, (float)right_edge->x_end, rt);
                right_edge->z = my_lerp((float)right_edge->z_start, (float)right_edge->z_end, rt);
                //right_edge->z += right_edge->dz;


                /* Draw left edge if it's the short edge, right edge if it's the short edge */
                int draw_left = !middle_is_right;  /* short edge on left */
                int draw_right = middle_is_right;   /* short edge on right */
                /* ALWAYS draw both edges - left is always an edge, right is always an edge */
                if (y >= 0 && y < SCREEN_HEIGHT) {
                    fill_span(pd, depth_buffer, bayer_matrix, y,
                        left_edge->x, right_edge->x,
                        left_edge->z, right_edge->z, left_edge->normal, right_edge->normal,
                        1, 1);
                }
            }
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

void SceneObject::draw(const Camera& i_camera, PlaydateAPI* pd, std::vector<float>& depth_buffer,
    std::vector<std::vector<float>>& bayer_matrix) {
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
        0.1f,
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
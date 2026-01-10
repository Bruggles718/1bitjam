#include "SceneObject.h"
// #include <cglm/gtc/matrix_transform.h>
#include <cglm/mat4.h>
// #include <cglm/gtc/type_ptr.h>
#include <math.h>
#include <assert.h>

/* ===== Helper Functions ===== */

static inline float edge(float ax, float ay, float bx, float by, float px, float py) {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

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

/* ===== Bayer Matrix Functions ===== */

BayerMatrix bayer_matrix_create(int n) {
    assert((n & (n - 1)) == 0 && "Size must be a power of 2");
    
    BayerMatrix result;
    result.size = n;
    result.data = (float**)malloc(n * sizeof(float*));
    for (int i = 0; i < n; i++) {
        result.data[i] = (float*)malloc(n * sizeof(float));
    }
    
    /* Start with 2x2 base matrix */
    int** B = (int**)malloc(2 * sizeof(int*));
    for (int i = 0; i < 2; i++) {
        B[i] = (int*)malloc(2 * sizeof(int));
    }
    B[0][0] = 0; B[0][1] = 2;
    B[1][0] = 3; B[1][1] = 1;
    int size = 2;
    
    /* Grow to desired size */
    while (size < n) {
        int new_size = size * 2;
        int** newB = (int**)malloc(new_size * sizeof(int*));
        for (int i = 0; i < new_size; i++) {
            newB[i] = (int*)malloc(new_size * sizeof(int));
        }
        
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                int v = B[y][x];
                newB[y][x] = 4 * v;
                newB[y][x + size] = 4 * v + 2;
                newB[y + size][x] = 4 * v + 3;
                newB[y + size][x + size] = 4 * v + 1;
            }
        }
        
        /* Free old B */
        for (int i = 0; i < size; i++) {
            free(B[i]);
        }
        free(B);
        
        B = newB;
        size = new_size;
    }
    
    /* Normalize to [0,1) */
    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            result.data[y][x] = B[y][x] / (float)(n * n);
        }
    }
    
    /* Free B */
    for (int i = 0; i < size; i++) {
        free(B[i]);
    }
    free(B);
    
    return result;
}

void bayer_matrix_destroy(BayerMatrix* matrix) {
    for (int i = 0; i < matrix->size; i++) {
        free(matrix->data[i]);
    }
    free(matrix->data);
}

/* ===== Drawing Functions ===== */

static void draw_line_z_thick(PlaydateAPI* pd, Vector* depth_buffer,
                               int x0, int y0, float iz0,
                               int x1, int y1, float iz1,
                               uint8_t color, int thickness) {
    if (thickness == 0) return;
    int dx = abs_int(x1 - x0);
    int dy = abs_int(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    
    int steps = max_int(dx, dy);
    for (int i = 0; i <= steps; i++) {
        float tline = steps == 0 ? 0 : (float)i / steps;
        float iz = iz0 * (1.0f - tline) + iz1 * tline;
        
        int xi = x0;
        int yi = y0;
        
        /* Draw thick pixels perpendicular */
        for (int ox = -thickness; ox <= thickness; ox++) {
            for (int oy = -thickness; oy <= thickness; oy++) {
                int px = xi + ox;
                int py = yi + oy;
                if (px < 0 || px >= SCREEN_WIDTH || py < 0 || py >= SCREEN_HEIGHT)
                    continue;
                
                int idx = py * SCREEN_WIDTH + px;
                float depth_val = VECTOR_GET_AS(float, depth_buffer, idx);
                if (iz > depth_val) {
                    depth_val = iz;
                    pd->graphics->setPixel(px, py, color);
                }
            }
        }
        
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

/* ===== VertexData Implementation ===== */

void vertex_data_init(SimpleVertexData* vd, Vector* vertex_buffer) {
    if (!vd || !vertex_buffer) return;
    vd->m_vertex_buffer.data = NULL;
    
    /* Copy the vertex buffer */
    vector_copy(&vd->m_vertex_buffer, vertex_buffer);
    //vd->m_vertex_buffer = *vertex_buffer;
}

void vertex_data_destroy(SimpleVertexData* vd) {
    if (!vd) return;
    vector_destroy(&vd->m_vertex_buffer);
}

void vertex_data_add_to_vertex_buffer(SimpleVertexData* vd, float value) {
    if (!vd) return;
    vector_push_back(&vd->m_vertex_buffer, &value);
}

/* Scanline rasterization - fills horizontal spans between edges */

typedef struct {
    float x;           /* Current x position */
    float dx;          /* x increment per scanline */
    float z;           /* Current z (inverse depth) */
    float dz;          /* z increment per scanline */
    int y_start;       /* Starting scanline */
    int y_end;         /* Ending scanline */
} EdgeData;

/* Sort vertices by Y coordinate */
static inline void sort_vertices_by_y(float* v1x, float* v1y, float* v1z,
    float* v2x, float* v2y, float* v2z,
    float* v3x, float* v3y, float* v3z) {
    /* Bubble sort is fine for 3 elements */
    if (*v1y > *v2y) {
        float tx = *v1x, ty = *v1y, tz = *v1z;
        *v1x = *v2x; *v1y = *v2y; *v1z = *v2z;
        *v2x = tx; *v2y = ty; *v2z = tz;
    }
    if (*v2y > *v3y) {
        float tx = *v2x, ty = *v2y, tz = *v2z;
        *v2x = *v3x; *v2y = *v3y; *v2z = *v3z;
        *v3x = tx; *v3y = ty; *v3z = tz;
    }
    if (*v1y > *v2y) {
        float tx = *v1x, ty = *v1y, tz = *v1z;
        *v1x = *v2x; *v1y = *v2y; *v1z = *v2z;
        *v2x = tx; *v2y = ty; *v2z = tz;
    }
}

/* Initialize edge data */
static inline void setup_edge(EdgeData* edge, float x1, float y1, float z1,
    float x2, float y2, float z2) {
    edge->y_start = (int)ceilf(y1);
    edge->y_end = (int)ceilf(y2);

    float dy = y2 - y1;
    if (fabsf(dy) < 0.001f) {
        edge->dx = 0;
        edge->dz = 0;
        edge->x = x1;
        edge->z = z1;
        return;
    }

    edge->dx = (x2 - x1) / dy;
    edge->dz = (z2 - z1) / dy;

    /* Prestep to first scanline center */
    float prestep = edge->y_start - y1;
    edge->x = x1 + edge->dx * prestep;
    edge->z = z1 + edge->dz * prestep;
}

/* Fill a horizontal span with integrated edge drawing */
static inline void fill_span(PlaydateAPI* pd, float* depth_data, BayerMatrix* T,
    int y, float x_left, float x_right,
    float z_left, float z_right, float brightness,
    int draw_left_edge, int draw_right_edge) {
    if (x_left > x_right) {
        float tmp = x_left; x_left = x_right; x_right = tmp;
        tmp = z_left; z_left = z_right; z_right = tmp;
        int tmp_edge = draw_left_edge;
        draw_left_edge = draw_right_edge;
        draw_right_edge = tmp_edge;
    }

    int x_start = max_int(0, (int)ceilf(x_left));
    int x_end = min_int(SCREEN_WIDTH - 1, (int)floorf(x_right));

    if (x_start > x_end) return;

    float span_width = x_right - x_left;
    if (fabsf(span_width) < 0.001f) return;

    float dz = (z_right - z_left) / span_width;
    float prestep = x_start - x_left;
    float z = z_left + dz * prestep;

    int idx = y * SCREEN_WIDTH + x_start;
    int bayer_y = y & 7;
    int bayer_x = x_start & 7;

    for (int x = x_start; x <= x_end; x++, idx++, bayer_x = (bayer_x + 1) & 7) {
        if (z > depth_data[idx]) {
            depth_data[idx] = z;

            /* Draw edge pixels in black, interior pixels with dithering */
            int is_edge = (x == x_start && draw_left_edge) ||
                (x == x_end && draw_right_edge);

            int color;
            if (is_edge) {
                color = kColorBlack;
            }
            else {
                color = (brightness > T->data[bayer_y][bayer_x]) ? kColorWhite : kColorBlack;
            }
            pd->graphics->setPixel(x, y, color);
        }
        z += dz;
    }
}

void vertex_data_draw_scanline(SimpleVertexData* vd, PlaydateAPI* pd, mat4 model, mat4 view,
    mat4 projection, Vector* depth_buffer, BayerMatrix* T) {
    if (!vd) return;

    mat4 mv, mvp;
    glm_mat4_mul(view, model, mv);
    glm_mat4_mul(projection, mv, mvp);

    float* buf = (float*)vd->m_vertex_buffer.data;
    size_t buf_size = vd->m_vertex_buffer.size;
    float* depth_data = (float*)depth_buffer->data;

    const float hw = SCREEN_WIDTH * 0.5f;
    const float hh = SCREEN_HEIGHT * 0.5f;

    for (size_t i = 0; i < buf_size; i += 9) {
        vec4 pos1 = { buf[i], buf[i + 1], buf[i + 2], 1.0f };
        vec4 pos2 = { buf[i + 3], buf[i + 4], buf[i + 5], 1.0f };
        vec4 pos3 = { buf[i + 6], buf[i + 7], buf[i + 8], 1.0f };

        vec4 view_pos1, view_pos2, view_pos3;
        glm_mat4_mulv(mv, pos1, view_pos1);
        glm_mat4_mulv(mv, pos2, view_pos2);
        glm_mat4_mulv(mv, pos3, view_pos3);

        /* Z-clip */
        if (view_pos1[2] >= 0 && view_pos2[2] >= 0 && view_pos3[2] >= 0) continue;

        /* Backface culling */
        float e1x = view_pos2[0] - view_pos1[0];
        float e1y = view_pos2[1] - view_pos1[1];
        float e1z = view_pos2[2] - view_pos1[2];
        float e2x = view_pos3[0] - view_pos1[0];
        float e2y = view_pos3[1] - view_pos1[1];
        float e2z = view_pos3[2] - view_pos1[2];

        float cross_z = e1x * e2y - e1y * e2x;
        if (cross_z < 0) continue;

        /* Lighting */
        float nx = e1y * e2z - e1z * e2y;
        float ny = e1z * e2x - e1x * e2z;
        float nz = cross_z;
        float len = sqrtf(nx * nx + ny * ny + nz * nz);
        if (len < 0.001f) continue;
        float brightness = (ny + len) * 0.5f / len;

        /* Project to screen space */
        vec4 clip1, clip2, clip3;
        glm_mat4_mulv(mvp, pos1, clip1);
        glm_mat4_mulv(mvp, pos2, clip2);
        glm_mat4_mulv(mvp, pos3, clip3);

        float inv_w1 = 1.0f / clip1[3];
        float inv_w2 = 1.0f / clip2[3];
        float inv_w3 = 1.0f / clip3[3];

        float x1 = (clip1[0] * inv_w1 + 1.0f) * hw;
        float y1 = (1.0f - clip1[1] * inv_w1) * hh;
        float x2 = (clip2[0] * inv_w2 + 1.0f) * hw;
        float y2 = (1.0f - clip2[1] * inv_w2) * hh;
        float x3 = (clip3[0] * inv_w3 + 1.0f) * hw;
        float y3 = (1.0f - clip3[1] * inv_w3) * hh;

        float z1 = -inv_w1 / view_pos1[2];
        float z2 = -inv_w2 / view_pos2[2];
        float z3 = -inv_w3 / view_pos3[2];

        /* Sort vertices by Y coordinate */
        sort_vertices_by_y(&x1, &y1, &z1, &x2, &y2, &z2, &x3, &y3, &z3);

        /* Clamp to screen bounds */
        int y_min = max_int(0, (int)ceilf(y1));
        int y_max = min_int(SCREEN_HEIGHT - 1, (int)floorf(y3));

        if (y_min > y_max) continue;

        /* Check for degenerate triangle */
        if (fabsf(y3 - y1) < 0.001f) continue;

        /* Setup edges */
        EdgeData edge_long;   /* v1 to v3 (long edge) */
        EdgeData edge_short1; /* v1 to v2 (short edge 1) */
        EdgeData edge_short2; /* v2 to v3 (short edge 2) */

        setup_edge(&edge_long, x1, y1, z1, x3, y3, z3);
        setup_edge(&edge_short1, x1, y1, z1, x2, y2, z2);
        setup_edge(&edge_short2, x2, y2, z2, x3, y3, z3);

        /* Determine which side the middle vertex is on */
        float mid_x_on_long = x1 + (x3 - x1) * (y2 - y1) / (y3 - y1);
        int middle_is_right = (x2 > mid_x_on_long);

        /* Rasterize top half (v1 to v2) */
        int y_mid = (int)ceilf(y2);
        EdgeData* left_edge = middle_is_right ? &edge_long : &edge_short1;
        EdgeData* right_edge = middle_is_right ? &edge_short1 : &edge_long;

        for (int y = y_min; y < y_mid && y <= y_max; y++) {
            /* Draw left edge if it's the short edge, right edge if it's the short edge */
            int draw_left = !middle_is_right;  /* short edge on left */
            int draw_right = middle_is_right;   /* short edge on right */

            fill_span(pd, depth_data, T, y,
                left_edge->x, right_edge->x,
                left_edge->z, right_edge->z, brightness,
                draw_left, draw_right);

            left_edge->x += left_edge->dx;
            left_edge->z += left_edge->dz;
            right_edge->x += right_edge->dx;
            right_edge->z += right_edge->dz;
        }

        /* Rasterize bottom half (v2 to v3) */
        left_edge = middle_is_right ? &edge_long : &edge_short2;
        right_edge = middle_is_right ? &edge_short2 : &edge_long;

        for (int y = y_mid; y <= y_max; y++) {
            /* Draw left edge if it's the short edge, right edge if it's the short edge */
            int draw_left = !middle_is_right;  /* short edge on left */
            int draw_right = middle_is_right;   /* short edge on right */

            fill_span(pd, depth_data, T, y,
                left_edge->x, right_edge->x,
                left_edge->z, right_edge->z, brightness,
                draw_left, draw_right);

            left_edge->x += left_edge->dx;
            left_edge->z += left_edge->dz;
            right_edge->x += right_edge->dx;
            right_edge->z += right_edge->dz;
        }
    }
}

void vertex_data_draw(SimpleVertexData* vd, PlaydateAPI* pd, mat4 model, mat4 view,
                      mat4 projection, Vector* depth_buffer, BayerMatrix *T) {
    if (!vd) return;

    /* Pre-compute combined MVP matrix */
    mat4 mv, mvp;
    glm_mat4_mul(view, model, mv);
    glm_mat4_mul(projection, mv, mvp);

    /* Cache pointer to buffer data */
    float* buf = (float*)vd->m_vertex_buffer.data;
    size_t buf_size = vd->m_vertex_buffer.size;

    /* Pre-compute screen transform constants */
    const float hw = SCREEN_WIDTH * 0.5f;
    const float hh = SCREEN_HEIGHT * 0.5f;
    const int screen_w_minus_1 = SCREEN_WIDTH - 1;
    const int screen_h_minus_1 = SCREEN_HEIGHT - 1;
    const int line_thickness = 0;

    /* Cache depth buffer data pointer */
    float* depth_data = (float*)depth_buffer->data;

    /* Light direction (pre-normalized) */
    const vec3 light_dir = { 0.0f, 1.0f, 0.0f };

    /* Process triangles (9 floats per triangle) */
    for (size_t i = 0; i < buf_size; i += 9) {
        /* Load vertices */
        vec4 pos1 = { buf[i], buf[i + 1], buf[i + 2], 1.0f };
        vec4 pos2 = { buf[i + 3], buf[i + 4], buf[i + 5], 1.0f };
        vec4 pos3 = { buf[i + 6], buf[i + 7], buf[i + 8], 1.0f };

        /* Transform to view space for backface culling */
        vec4 view_pos1, view_pos2, view_pos3;
        glm_mat4_mulv(mv, pos1, view_pos1);
        glm_mat4_mulv(mv, pos2, view_pos2);
        glm_mat4_mulv(mv, pos3, view_pos3);

        /* Early z-clip test */
        if (view_pos1[2] >= 0 && view_pos2[2] >= 0 && view_pos3[2] >= 0) {
            continue;
        }

        /* Backface culling (cross product z-component only) */
        float e1x = view_pos2[0] - view_pos1[0];
        float e1y = view_pos2[1] - view_pos1[1];
        float e1z = view_pos2[2] - view_pos1[2];
        float e2x = view_pos3[0] - view_pos1[0];
        float e2y = view_pos3[1] - view_pos1[1];
        float e2z = view_pos3[2] - view_pos1[2];

        float cross_z = e1x * e2y - e1y * e2x;
        if (cross_z < 0) continue;

        /* Calculate lighting (using already computed edges) */
        float nx = e1y * e2z - e1z * e2y;
        float ny = e1z * e2x - e1x * e2z;
        float nz = e1x * e2y - e1y * e2x;
        float brightness = (ny + sqrtf(nx * nx + ny * ny + nz * nz)) * 0.5f / sqrtf(nx * nx + ny * ny + nz * nz);

        /* Transform to clip space using MVP */
        vec4 clip1, clip2, clip3;
        glm_mat4_mulv(mvp, pos1, clip1);
        glm_mat4_mulv(mvp, pos2, clip2);
        glm_mat4_mulv(mvp, pos3, clip3);

        /* Perspective divide and screen transform */
        float inv_w1 = 1.0f / clip1[3];
        float inv_w2 = 1.0f / clip2[3];
        float inv_w3 = 1.0f / clip3[3];

        float sx1 = (clip1[0] * inv_w1 + 1.0f) * hw;
        float sy1 = (1.0f - clip1[1] * inv_w1) * hh;
        float sx2 = (clip2[0] * inv_w2 + 1.0f) * hw;
        float sy2 = (1.0f - clip2[1] * inv_w2) * hh;
        float sx3 = (clip3[0] * inv_w3 + 1.0f) * hw;
        float sy3 = (1.0f - clip3[1] * inv_w3) * hh;

        /* Compute inverse depth values */
        float iz0 = -inv_w1 / view_pos1[2];
        float iz1 = -inv_w2 / view_pos2[2];
        float iz2 = -inv_w3 / view_pos3[2];

        /* Bounding box */
        float min_x = min3(sx1, sx2, sx3);
        float max_x = max3(sx1, sx2, sx3);
        float min_y = min3(sy1, sy2, sy3);
        float max_y = max3(sy1, sy2, sy3);

        int minX = max_int(0, (int)floorf(min_x + line_thickness));
        int maxX = min_int(screen_w_minus_1, (int)ceilf(max_x - line_thickness));
        int minY = max_int(0, (int)floorf(min_y + line_thickness));
        int maxY = min_int(screen_h_minus_1, (int)ceilf(max_y - line_thickness));

        /* Pre-compute barycentric setup */
        float denom = (sy2 - sy3) * (sx1 - sx3) + (sx3 - sx2) * (sy1 - sy3);
        if (fabsf(denom) < 1e-6f) continue; /* Degenerate triangle */

        float inv_denom = 1.0f / denom;
        float a_dx = (sy2 - sy3) * inv_denom;
        float a_dy = (sx3 - sx2) * inv_denom;
        float b_dx = (sy3 - sy1) * inv_denom;
        float b_dy = (sx1 - sx3) * inv_denom;

        /* Barycentric at minX, minY */
        float px_start = minX + 0.5f;
        float py_start = minY + 0.5f;
        float alpha_base = a_dx * (px_start - sx3) + a_dy * (py_start - sy3);
        float beta_base = b_dx * (px_start - sx3) + b_dy * (py_start - sy3);

        /* Get Bayer threshold offset */
        int bayer_y_offset = minY % 8;
        int bayer_x_base = minX % 8;

        /* Rasterize with incremental barycentric */
        for (int j = minY; j <= maxY; j++) {
            float alpha = alpha_base;
            float beta = beta_base;
            int idx = j * SCREEN_WIDTH + minX;
            int bayer_x = bayer_x_base;

            for (int i = minX; i <= maxX; i++, idx++, bayer_x = (bayer_x + 1) & 7) {
                float gamma = 1.0f - alpha - beta;

                if (alpha >= 0 && beta >= 0 && gamma >= 0) {
                    float iz = alpha * iz0 + beta * iz1 + gamma * iz2;

                    /* Depth test */
                    if (iz > depth_data[idx]) {
                        depth_data[idx] = iz;
                        int color = (brightness > T->data[bayer_y_offset][bayer_x]) ? kColorWhite : kColorBlack;
                        pd->graphics->setPixel(i, j, color);
                    }
                }

                alpha += a_dx;
                beta += b_dx;
            }

            alpha_base += a_dy;
            beta_base += b_dy;
            bayer_y_offset = (bayer_y_offset + 1) & 7;
        }

        /* Draw triangle edges */
        draw_line_z_thick(pd, depth_buffer, (int)sx1, (int)sy1, iz0,
            (int)sx2, (int)sy2, iz1, kColorBlack, line_thickness);
        draw_line_z_thick(pd, depth_buffer, (int)sx2, (int)sy2, iz1,
            (int)sx3, (int)sy3, iz2, kColorBlack, line_thickness);
        draw_line_z_thick(pd, depth_buffer, (int)sx3, (int)sy3, iz2,
            (int)sx1, (int)sy1, iz0, kColorBlack, line_thickness);
    }
}

void vertex_data_print_vertex_buffer(SimpleVertexData* vd) {
    if (!vd) return;
    
    for (size_t i = 0; i < vd->m_vertex_buffer.size; i++) {
        float f = VECTOR_GET_AS(float, &vd->m_vertex_buffer, i);
        printf("%f\n", f);
    }
}

/* ===== SimpleVertexData Implementation ===== */

void simple_vertex_data_init(SimpleVertexData* svd, Vector* vertex_buffer,
                             int stride, int* offsets, int offsets_count) {
    if (!svd) return;
    
    /* Initialize base class */
    vertex_data_init(svd, vertex_buffer);
    
    /* Copy derived class data */
    svd->m_stride = stride;
    svd->m_offsets = offsets;
    svd->m_offsets_count = offsets_count;
}

void simple_vertex_data_destroy(SimpleVertexData* svd) {
    if (!svd) return;
    
    /* Free offsets array */
    if (svd->m_offsets) {
        free(svd->m_offsets);
        svd->m_offsets = NULL;
    }
    
    /* Destroy base class */
    vertex_data_destroy(svd);
}

void simple_vertex_data_send_to_gpu(SimpleVertexData* vd) {
    /* Cast to derived type */
    SimpleVertexData* svd = (SimpleVertexData*)vd;
    /* Implementation would go here */
    (void)svd;
}

int simple_vertex_data_get_attribute_size(SimpleVertexData* svd, int idx) {
    if (!svd) return 0;
    
    if (idx + 1 < svd->m_offsets_count) {
        return svd->m_offsets[idx + 1] - svd->m_offsets[idx];
    }
    return svd->m_stride - svd->m_offsets[idx];
}

/* ===== Transform Implementation ===== */

Transform transform_create(void) {
    Transform t;
    t.m_position[0] = 0.0f; t.m_position[1] = 0.0f; t.m_position[2] = 0.0f;
    t.m_scale[0] = 1.0f; t.m_scale[1] = 1.0f; t.m_scale[2] = 1.0f;
    glm_quat_identity(t.m_rotation);
    return t;
}

/* ===== SceneObject Implementation ===== */

void scene_object_init(SceneObject* obj, SimpleVertexData* vertex_data) {
    if (!obj) return;
    
    obj->m_vertex_data = vertex_data;
    obj->m_transform = transform_create();
    obj->m_diffuse_color[0] = 1.0f;
    obj->m_diffuse_color[1] = 1.0f;
    obj->m_diffuse_color[2] = 1.0f;
    obj->m_specular_strength = 1.0f;
}

void scene_object_destroy(SceneObject* obj) {
    if (!obj) return;
    /* Vertex data is not owned by scene object, don't free it */
}

void scene_object_draw(SceneObject* obj, const Camera* camera, PlaydateAPI* pd,
                       Vector* depth_buffer, BayerMatrix* T) {
    if (!obj) return;
    
    /* Build model matrix */
    mat4 model;
    glm_mat4_identity(model);
    glm_translate(model, obj->m_transform.m_position);
    
    mat4 rotation_matrix;
    glm_quat_mat4(obj->m_transform.m_rotation, rotation_matrix);
    glm_mat4_mul(model, rotation_matrix, model);
    
    glm_scale(model, obj->m_transform.m_scale);
    
    /* Get view matrix */
    mat4 view;
    camera_get_view_matrix(camera, view);
    
    /* Create perspective matrix */
    mat4 perspective;
    glm_perspective(glm_rad(45.0f), (float)SCREEN_WIDTH/(float)SCREEN_HEIGHT,
                   0.1f, 1000.0f, perspective);
    
    /* Draw */
    vertex_data_draw_scanline(obj->m_vertex_data, pd, model, view, perspective, depth_buffer, T);
}

void scene_object_set_transform(SceneObject* obj, Transform transform) {
    if (!obj) return;
    obj->m_transform = transform;
}

void scene_object_set_position(SceneObject* obj, vec3 position) {
    if (!obj) return;
    glm_vec3_copy(position, obj->m_transform.m_position);
}

void scene_object_set_scale(SceneObject* obj, vec3 scale) {
    if (!obj) return;
    glm_vec3_copy(scale, obj->m_transform.m_scale);
}

void scene_object_set_rotation(SceneObject* obj, versor rotation) {
    if (!obj) return;
    glm_quat_copy(rotation, obj->m_transform.m_rotation);
}

void scene_object_rotate(SceneObject* obj, float angle_degrees, vec3 axis) {
    if (!obj) return;
    
    vec3 normalized_axis;
    glm_vec3_normalize_to(axis, normalized_axis);
    
    versor rotation_quat;
    glm_quatv(rotation_quat, glm_rad(angle_degrees), normalized_axis);
    
    versor result;
    glm_quat_mul(rotation_quat, obj->m_transform.m_rotation, result);
    glm_quat_copy(result, obj->m_transform.m_rotation);
}

void scene_object_set_diffuse_color(SceneObject* obj, vec3 diffuse_color) {
    if (!obj) return;
    glm_vec3_copy(diffuse_color, obj->m_diffuse_color);
}

void scene_object_set_specular_strength(SceneObject* obj, float specular_strength) {
    if (!obj) return;
    obj->m_specular_strength = specular_strength;
}

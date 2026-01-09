#include "SceneObject.h"
// #include <cglm/gtc/matrix_transform.h>
#include <cglm/mat4.h>
// #include <cglm/gtc/type_ptr.h>
#include <stdio.h>
#include <stdlib.h>
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

typedef struct BayerMatrix {
    float** data;
    int size;
} BayerMatrix;

static BayerMatrix bayer_matrix_create(int n) {
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

static void bayer_matrix_destroy(BayerMatrix* matrix) {
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

void vertex_data_draw(SimpleVertexData* vd, PlaydateAPI* pd, mat4 model, mat4 view,
                      mat4 projection, Vector* depth_buffer) {
    if (!vd) return;
    
    //pd->system->logToConsole("Drawing VertexData with %zu floats", vd->m_vertex_buffer.size);
    
    mat4 mv;
    glm_mat4_mul(view, model, mv);
    
    /* Process triangles (9 floats per triangle: 3 vertices * 3 coords) */
    for (size_t i = 0; i < vd->m_vertex_buffer.size; i += 9) {
        float* buf = (float*)vd->m_vertex_buffer.data;
        float x1 = buf[i], y1 = buf[i+1], z1 = buf[i+2];
        float x2 = buf[i+3], y2 = buf[i+4], z2 = buf[i+5];
        float x3 = buf[i+6], y3 = buf[i+7], z3 = buf[i+8];
        //pd->system->logToConsole("verts: %f, %f, %f, %f, %f, %f, %f, %f, %f", x1, y1, z1, x2, y2, z2, x3, y3, z3);
        
        vec4 pos1 = {x1, y1, z1, 1.0f};
        vec4 pos2 = {x2, y2, z2, 1.0f};
        vec4 pos3 = {x3, y3, z3, 1.0f};
        
        vec4 view_pos1, view_pos2, view_pos3;
        glm_mat4_mulv(mv, pos1, view_pos1);
        glm_mat4_mulv(mv, pos2, view_pos2);
        glm_mat4_mulv(mv, pos3, view_pos3);
        
        /* Backface culling */
        vec3 edge1 = {view_pos2[0] - view_pos1[0], view_pos2[1] - view_pos1[1], view_pos2[2] - view_pos1[2]};
        vec3 edge2 = {view_pos3[0] - view_pos1[0], view_pos3[1] - view_pos1[1], view_pos3[2] - view_pos1[2]};
        vec3 cross_result;
        glm_vec3_cross(edge1, edge2, cross_result);
        if (cross_result[2] < 0) continue;
        
        /* Project to clip space */
        vec4 clip1_v4, clip2_v4, clip3_v4;
        glm_mat4_mulv(projection, view_pos1, clip1_v4);
        glm_mat4_mulv(projection, view_pos2, clip2_v4);
        glm_mat4_mulv(projection, view_pos3, clip3_v4);
        
        /* Perspective divide */
        vec3 clip1_v3 = {clip1_v4[0]/clip1_v4[3], clip1_v4[1]/clip1_v4[3], clip1_v4[2]/clip1_v4[3]};
        vec3 clip2_v3 = {clip2_v4[0]/clip2_v4[3], clip2_v4[1]/clip2_v4[3], clip2_v4[2]/clip2_v4[3]};
        vec3 clip3_v3 = {clip3_v4[0]/clip3_v4[3], clip3_v4[1]/clip3_v4[3], clip3_v4[2]/clip3_v4[3]};
        
        /* Screen space coordinates */
        vec2 clip1 = {(clip1_v3[0] + 1) * SCREEN_WIDTH * 0.5f, (1 - clip1_v3[1]) * SCREEN_HEIGHT * 0.5f};
        vec2 clip2 = {(clip2_v3[0] + 1) * SCREEN_WIDTH * 0.5f, (1 - clip2_v3[1]) * SCREEN_HEIGHT * 0.5f};
        vec2 clip3 = {(clip3_v3[0] + 1) * SCREEN_WIDTH * 0.5f, (1 - clip3_v3[1]) * SCREEN_HEIGHT * 0.5f};
        
        int line_thickness = 1;

        /* Bounding box */
        int minX = max_int(0, (int)floorf(min3(clip1[0], clip2[0], clip3[0]) + line_thickness));
        int maxX = min_int(SCREEN_WIDTH - 1, (int)ceilf(max3(clip1[0], clip2[0], clip3[0]) - line_thickness));
        int minY = max_int(0, (int)floorf(min3(clip1[1], clip2[1], clip3[1]) + line_thickness));
        int maxY = min_int(SCREEN_HEIGHT - 1, (int)ceilf(max3(clip1[1], clip2[1], clip3[1]) - line_thickness));

        //pd->system->logToConsole("bounding box: %i, %i, %i, %i", minX, minY, maxX, maxY);
        
        float vz0 = -view_pos1[2];
        float vz1 = -view_pos2[2];
        float vz2 = -view_pos3[2];
        
        float iz0 = 1.0f / vz0;
        float iz1 = 1.0f / vz1;
        float iz2 = 1.0f / vz2;
        
        /* Calculate normal for lighting */
        vec3 normal;
        glm_vec3_cross(edge1, edge2, normal);
        
        vec3 light_dir = {0.0f, 1.0f, 0.0f};
        float dot = glm_vec3_dot(normal, light_dir);
        float brightness = (dot + 1.0f) * 0.5f;
        
        int width = maxX - minX;
        int height = maxY - minY;
        int n = 1;
        while (n < max_int(width, height)) n *= 2;
        
        BayerMatrix T = bayer_matrix_create(n);
        
        /* Rasterize triangle */
        for (int i = minX; i <= maxX; i++) {
            for (int j = minY; j <= maxY; j++) {
                float px = i + 0.5f;
                float py = j + 0.5f;
                
                /* Barycentric coordinates */
                float alpha = ((clip2[1] - clip3[1]) * (px - clip3[0]) +
                              (clip3[0] - clip2[0]) * (py - clip3[1])) /
                             ((clip2[1] - clip3[1]) * (clip1[0] - clip3[0]) +
                              (clip3[0] - clip2[0]) * (clip1[1] - clip3[1]));
                
                float beta = ((clip3[1] - clip1[1]) * (px - clip3[0]) +
                             (clip1[0] - clip3[0]) * (py - clip3[1])) /
                            ((clip3[1] - clip1[1]) * (clip2[0] - clip3[0]) +
                             (clip1[0] - clip3[0]) * (clip2[1] - clip3[1]));
                
                float gamma = 1.0f - alpha - beta;
                
                if (alpha < 0 || beta < 0 || gamma < 0) continue;
                
                float iz = alpha * iz0 + beta * iz1 + gamma * iz2;
                int idx = j * SCREEN_WIDTH + i;
                
                /* Depth test */
                float depth_val = VECTOR_GET_AS(float, depth_buffer, idx);
                if (iz > depth_val) {
                    depth_val = iz;
                    int y = j - minY;
                    int x = i - minX;
                    int color = (brightness > T.data[y % n][x % n]) ? kColorWhite : kColorBlack;
                    pd->graphics->setPixel(i, j, color);
                }
            }
        }
        
        /* Draw triangle edges */
        draw_line_z_thick(pd, depth_buffer, (int)clip1[0], (int)clip1[1], iz0,
                         (int)clip2[0], (int)clip2[1], iz1, kColorBlack, line_thickness);
        draw_line_z_thick(pd, depth_buffer, (int)clip2[0], (int)clip2[1], iz1,
                         (int)clip3[0], (int)clip3[1], iz2, kColorBlack, line_thickness);
        draw_line_z_thick(pd, depth_buffer, (int)clip3[0], (int)clip3[1], iz2,
                         (int)clip1[0], (int)clip1[1], iz0, kColorBlack, line_thickness);
        
        bayer_matrix_destroy(&T);
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
                       Vector* depth_buffer) {
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
    vertex_data_draw(obj->m_vertex_data, pd, model, view, perspective, depth_buffer);
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

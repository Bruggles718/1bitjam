#ifndef SCENE_OBJECT_H
#define SCENE_OBJECT_H

#include <cglm/cglm.h>
#include <cglm/quat.h>
#include "vector.h"
#include "Camera.h"
#include "pd_api.h"

#define PIXEL_SCALE 1

#define SCREEN_WIDTH (LCD_COLUMNS / PIXEL_SCALE)
#define SCREEN_HEIGHT (LCD_ROWS / PIXEL_SCALE)

void setPixel(PlaydateAPI* pd, int x, int y, int color);

typedef struct BayerMatrix {
    float** data;
    int size;
} BayerMatrix;

void bayer_matrix_destroy(BayerMatrix* matrix);
BayerMatrix bayer_matrix_create(int n);

/* Forward declarations */
typedef struct Camera Camera;
typedef struct SimpleVertexData SimpleVertexData;

/* ===== VertexData Base "Class" ===== */

/* Virtual function pointer type for send_to_gpu */
typedef void (*send_to_gpu_fn)(SimpleVertexData* vertex_data);

/* Base constructor */
void vertex_data_init(SimpleVertexData* vd, Vector* vertex_buffer);

/* Base destructor */
void vertex_data_destroy(SimpleVertexData* vd);

/* Base methods */
void vertex_data_add_to_vertex_buffer(SimpleVertexData* vd, float value);
void vertex_data_draw(SimpleVertexData* vd, PlaydateAPI* pd, mat4 model, mat4 view, 
                      mat4 projection, Vector* depth_buffer, BayerMatrix *T);
void vertex_data_print_vertex_buffer(SimpleVertexData* vd);


/* ===== SimpleVertexData "Derived Class" ===== */

typedef struct SimpleVertexData {
    Vector m_vertex_buffer; 
    
    int m_stride;
    int* m_offsets;  /* Dynamic array of offsets */
    int m_offsets_count; /* Number of offsets */
} SimpleVertexData;

/* Constructor - takes ownership of offsets array */
void simple_vertex_data_init(
    SimpleVertexData* svd,
    Vector* vertex_buffer,
    int stride,
    int* offsets,
    int offsets_count
);

/* Destructor */
void simple_vertex_data_destroy(SimpleVertexData* svd);

/* Implementation of virtual function */
void simple_vertex_data_send_to_gpu(SimpleVertexData* vd);

/* Private helper methods */
void simple_vertex_data_setup_for_send(SimpleVertexData* svd);
void simple_vertex_data_send_buffer(SimpleVertexData* svd);
void simple_vertex_data_send_attributes(SimpleVertexData* svd);
int simple_vertex_data_get_attribute_size(SimpleVertexData* svd, int idx);


/* ===== Transform Struct ===== */

typedef struct Transform {
    vec3 m_position;
    vec3 m_scale;
    versor m_rotation; /* Quaternion (glm uses versor for quaternions in C) */
} Transform;

/* Transform helper functions */
Transform transform_create(void);
Transform transform_create_with_values(vec3 position, vec3 scale, versor rotation);


/* ===== SceneObject "Class" ===== */

typedef struct SceneObject {
    SimpleVertexData* m_vertex_data; /* Pointer to allow polymorphism */
    
    Transform m_transform;
    
    vec3 m_diffuse_color;
    float m_specular_strength;
} SceneObject;

/* Constructor */
void scene_object_init(SceneObject* obj, SimpleVertexData* vertex_data);

/* Destructor */
void scene_object_destroy(SceneObject* obj);

/* Public methods */
void scene_object_draw(SceneObject* obj, const Camera* camera, PlaydateAPI* pd, 
                       Vector* depth_buffer, BayerMatrix* T);
void scene_object_set_transform(SceneObject* obj, Transform transform);
void scene_object_set_position(SceneObject* obj, vec3 position);
void scene_object_set_rotation(SceneObject* obj, versor rotation);
void scene_object_rotate(SceneObject* obj, float angle_degrees, vec3 axis);
void scene_object_set_scale(SceneObject* obj, vec3 scale);
void scene_object_set_diffuse_color(SceneObject* obj, vec3 diffuse_color);
void scene_object_set_specular_strength(SceneObject* obj, float specular_strength);

/* Private helper methods */
void scene_object_send_vertex_data_to_gpu(SceneObject* obj);
void scene_object_pre_draw(SceneObject* obj, const Camera* camera, 
                           int screen_width, int screen_height);

bool scene_object_colliding_with_point(SceneObject* obj, vec3 point, float distance);

#endif /* SCENE_OBJECT_H */

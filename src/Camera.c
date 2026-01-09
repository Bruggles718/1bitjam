#include "Camera.h"
#include <cglm/affine.h>
// #include <glm/gtx/rotate_vector.h>
#include <stdio.h>

void camera_init(Camera* camera) {
    if (!camera) return;
    
    printf("Camera.c: (Constructor) Created a Camera!\n");
    
    /* Position us at the origin */
    camera->m_eye_position[0] = 0.0f;
    camera->m_eye_position[1] = 0.0f;
    camera->m_eye_position[2] = 5.0f;
    
    /* Looking down along the z-axis initially */
    /* Remember, this is negative because we are looking 'into' the scene */
    camera->m_view_direction[0] = 0.0f;
    camera->m_view_direction[1] = 0.0f;
    camera->m_view_direction[2] = -1.0f;
    
    /* For now--our upVector always points up along the y-axis */
    camera->m_up_vector[0] = 0.0f;
    camera->m_up_vector[1] = 1.0f;
    camera->m_up_vector[2] = 0.0f;
    
    /* Initialize old mouse position */
    camera->m_old_mouse_position[0] = 0.0f;
    camera->m_old_mouse_position[1] = 0.0f;
}

void camera_destroy(Camera* camera) {
    /* Nothing to clean up for basic camera */
    (void)camera;
}

void camera_get_view_matrix(const Camera* camera, mat4 out) {
    vec3 center;
    vec3 eye_copy;
    vec3 up_copy;
    
    /* Copy eye position */
    glm_vec3_copy(camera->m_eye_position, eye_copy);
    
    /* Calculate center = eye + viewDirection */
    glm_vec3_add(camera->m_eye_position, camera->m_view_direction, center);
    
    /* Copy up vector */
    glm_vec3_copy(camera->m_up_vector, up_copy);
    
    /* Create view matrix */
    glm_lookat(eye_copy, center, up_copy, out);
    
    // return view_matrix;
}

void camera_mouse_look(Camera* camera, int mouse_x, int mouse_y) {
    /* Record our new position as a vector */
    vec2 new_mouse_position;
    new_mouse_position[0] = (float)mouse_x;
    new_mouse_position[1] = (float)mouse_y;
    
    /* Little hack for our 'mouse look function' */
    /* We need this so that we can move our camera for the first time */
    static bool first_look = true;
    if (first_look) {
        first_look = false;
        glm_vec2_copy(new_mouse_position, camera->m_old_mouse_position);
    }
    
    /* Detect how much the mouse has moved since the last time */
    int mouse_delta_x = camera->m_old_mouse_position[0] - new_mouse_position[0];
    
    /* Rotate about the upVector */
    vec3 rotation_axis = {0.0f, 1.0f, 0.0f};
    vec3 rotated_view;
    glm_vec3_rotate(camera->m_view_direction, glm_rad((float)mouse_delta_x), rotation_axis);
    //glm_vec3_copy(rotated_view, camera->m_view_direction);
    
    /* Update our old position after we have made changes */
    glm_vec2_copy(new_mouse_position, camera->m_old_mouse_position);
}

void camera_move_forward(Camera* camera, float speed) {
    camera->m_eye_position[0] += camera->m_view_direction[0] * speed;
    camera->m_eye_position[1] += camera->m_view_direction[1] * speed;
    camera->m_eye_position[2] += camera->m_view_direction[2] * speed;
}

void camera_move_backward(Camera* camera, float speed) {
    camera->m_eye_position[0] -= camera->m_view_direction[0] * speed;
    camera->m_eye_position[1] -= camera->m_view_direction[1] * speed;
    camera->m_eye_position[2] -= camera->m_view_direction[2] * speed;
}

void camera_move_left(Camera* camera, float speed) {
    float angle = 90.0f;
    vec3 rotation_axis = {0.0f, 1.0f, 0.0f};
    vec3 move_direction;
    glm_vec3_copy(camera->m_view_direction, move_direction);
    
    glm_vec3_rotate(move_direction, glm_rad(angle), rotation_axis);
    glm_vec3_scale(move_direction, speed, move_direction);
    glm_vec3_add(camera->m_eye_position, move_direction, camera->m_eye_position);
}

void camera_move_right(Camera* camera, float speed) {
    float angle = -90.0f;
    vec3 rotation_axis = {0.0f, 1.0f, 0.0f};
    vec3 move_direction;
    
    glm_vec3_copy(camera->m_view_direction, move_direction);
    
    glm_vec3_rotate(move_direction, glm_rad(angle), rotation_axis);
    glm_vec3_scale(move_direction, speed, move_direction);
    glm_vec3_add(camera->m_eye_position, move_direction, camera->m_eye_position);
}

void camera_move_up(Camera* camera, float speed) {
    camera->m_eye_position[1] += speed;
}

void camera_move_down(Camera* camera, float speed) {
    camera->m_eye_position[1] -= speed;
}

void camera_set_eye_position(Camera* camera, float x, float y, float z) {
    camera->m_eye_position[0] = x;
    camera->m_eye_position[1] = y;
    camera->m_eye_position[2] = z;
}

float camera_get_eye_x_position(const Camera* camera) {
    return camera->m_eye_position[0];
}

float camera_get_eye_y_position(const Camera* camera) {
    return camera->m_eye_position[1];
}

float camera_get_eye_z_position(const Camera* camera) {
    return camera->m_eye_position[2];
}

float camera_get_view_x_direction(const Camera* camera) {
    return camera->m_view_direction[0];
}

float camera_get_view_y_direction(const Camera* camera) {
    return camera->m_view_direction[1];
}

float camera_get_view_z_direction(const Camera* camera) {
    return camera->m_view_direction[2];
}

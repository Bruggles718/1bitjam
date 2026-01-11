#include "Camera.h"
#include <cglm/affine.h>
#include <stdio.h>

void camera_init(Camera* camera) {
    if (!camera) return;

    printf("Camera.c: (Constructor) Created a Camera!\n");

    /* Position us at the origin */
    camera->m_eye_position[0] = 0.0f;
    camera->m_eye_position[1] = 0.0f;
    camera->m_eye_position[2] = 5.0f;

    camera_move_up(camera, 1.0f);

    /* Initialize rotation to identity (no rotation) */
    glm_quat_identity(camera->m_rotation);
    /*vec3 axis = { 1.0f,0.0f,0.0f };
    camera_rotate(camera, 89.0f, axis);*/
}

void camera_destroy(Camera* camera) {
    /* Nothing to clean up for basic camera */
    (void)camera;
}

void camera_get_view_matrix(const Camera* camera, mat4 out) {
    /* Invert the camera's orientation quaternion */
    versor quat_inv;
    glm_quat_conjugate(camera->m_rotation, quat_inv);

    /* Convert inverted quaternion to matrix */
    glm_quat_mat4(quat_inv, out);

    /* Apply negative translation */
    vec3 neg_eye = { -camera->m_eye_position[0], -camera->m_eye_position[1], -camera->m_eye_position[2] };
    glm_translate(out, neg_eye);
}

void camera_mouse_look(Camera* camera, int mouse_x, int mouse_y) {
    /* Static variables to track mouse state */
    static vec2 old_mouse_position = { 0.0f, 0.0f };
    static bool first_look = true;

    /* Record our new position as a vector */
    vec2 new_mouse_position;
    new_mouse_position[0] = (float)mouse_x;
    new_mouse_position[1] = (float)mouse_y;

    /* Little hack for our 'mouse look function' */
    /* We need this so that we can move our camera for the first time */
    if (first_look) {
        first_look = false;
        glm_vec2_copy(new_mouse_position, old_mouse_position);
        return;
    }

    /* Detect how much the mouse has moved since the last time */
    float mouse_delta_x = old_mouse_position[0] - new_mouse_position[0];
    float mouse_delta_y = old_mouse_position[1] - new_mouse_position[1];

    /* Sensitivity factor */
    float sensitivity = 0.1f;

    /* Rotate about the Y-axis (yaw) */
    vec3 y_axis = { 0.0f, 1.0f, 0.0f };
    versor yaw_rotation;
    glm_quatv(yaw_rotation, glm_rad(mouse_delta_x * sensitivity), y_axis);
    glm_quat_mul(yaw_rotation, camera->m_rotation, camera->m_rotation);

    /* Rotate about the local X-axis (pitch) */
    vec3 x_axis = { 1.0f, 0.0f, 0.0f };
    versor pitch_rotation;
    glm_quatv(pitch_rotation, glm_rad(mouse_delta_y * sensitivity), x_axis);
    glm_quat_mul(camera->m_rotation, pitch_rotation, camera->m_rotation);

    /* Normalize to prevent drift */
    glm_quat_normalize(camera->m_rotation);

    /* Update our old position after we have made changes */
    glm_vec2_copy(new_mouse_position, old_mouse_position);
}

void camera_move_forward(Camera* camera, float speed) {
    /* Get forward direction from rotation quaternion */
    vec3 forward = { 0.0f, 0.0f, -1.0f };  /* Default forward is -Z */
    mat4 rotation_matrix;
    glm_quat_mat4(camera->m_rotation, rotation_matrix);
    glm_mat4_mulv3(rotation_matrix, forward, 1.0f, forward);

    /* Move in forward direction */
    glm_vec3_scale(forward, speed, forward);
    glm_vec3_add(camera->m_eye_position, forward, camera->m_eye_position);
}

void camera_move_backward(Camera* camera, float speed) {
    camera_move_forward(camera, -speed);
}

void camera_move_left(Camera* camera, float speed) {
    /* Get right direction from rotation quaternion */
    vec3 right = { 1.0f, 0.0f, 0.0f };  /* Default right is +X */
    mat4 rotation_matrix;
    glm_quat_mat4(camera->m_rotation, rotation_matrix);
    glm_mat4_mulv3(rotation_matrix, right, 1.0f, right);

    /* Move in left direction (negative right) */
    glm_vec3_scale(right, -speed, right);
    glm_vec3_add(camera->m_eye_position, right, camera->m_eye_position);
}

void camera_move_right(Camera* camera, float speed) {
    /* Get right direction from rotation quaternion */
    vec3 right = { 1.0f, 0.0f, 0.0f };  /* Default right is +X */
    mat4 rotation_matrix;
    glm_quat_mat4(camera->m_rotation, rotation_matrix);
    glm_mat4_mulv3(rotation_matrix, right, 1.0f, right);

    /* Move in right direction */
    glm_vec3_scale(right, speed, right);
    glm_vec3_add(camera->m_eye_position, right, camera->m_eye_position);
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
    vec3 forward = { 0.0f, 0.0f, -1.0f };
    mat4 rotation_matrix;
    glm_quat_mat4(camera->m_rotation, rotation_matrix);
    glm_mat4_mulv3(rotation_matrix, forward, 1.0f, forward);
    return forward[0];
}

float camera_get_view_y_direction(const Camera* camera) {
    vec3 forward = { 0.0f, 0.0f, -1.0f };
    mat4 rotation_matrix;
    glm_quat_mat4(camera->m_rotation, rotation_matrix);
    glm_mat4_mulv3(rotation_matrix, forward, 1.0f, forward);
    return forward[1];
}

float camera_get_view_z_direction(const Camera* camera) {
    vec3 forward = { 0.0f, 0.0f, -1.0f };
    mat4 rotation_matrix;
    glm_quat_mat4(camera->m_rotation, rotation_matrix);
    glm_mat4_mulv3(rotation_matrix, forward, 1.0f, forward);
    return forward[2];
}

void camera_set_rotation(Camera* camera, versor rotation) {
    glm_quat_copy(rotation, camera->m_rotation);
}

void camera_rotate(Camera* camera, float angle_degrees, vec3 axis) {
    versor rotation;
    glm_quatv(rotation, glm_rad(angle_degrees), axis);
    glm_quat_mul(camera->m_rotation, rotation, camera->m_rotation);
    glm_quat_normalize(camera->m_rotation);
}
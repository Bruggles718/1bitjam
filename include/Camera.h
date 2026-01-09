/** @file Camera.h
 *  @brief Sets up an OpenGL camera.
 *  
 *  Sets up an OpenGL Camera. The camera is what
 *  sets up our 'view' matrix.
 *
 *  @author Mike
 *  @bug No known bugs.
 */
#ifndef CAMERA_H
#define CAMERA_H

#include <cglm/cglm.h>

typedef struct Camera {
    /* Track the old mouse position */
    vec2 m_old_mouse_position;
    /* Where is our camera positioned */
    vec3 m_eye_position;
    /* What direction is the camera looking */
    vec3 m_view_direction;
    /* Which direction is 'up' in our world
     * Generally this is constant, but if you wanted
     * to 'rock' or 'rattle' the camera you might play
     * with modifying this value.
     */
    vec3 m_up_vector;
} Camera;

/* Constructor to create a camera */
void camera_init(Camera* camera);

/* Destructor */
void camera_destroy(Camera* camera);

/* Return a 'view' matrix with our camera transformation applied. */
void camera_get_view_matrix(const Camera* camera, mat4 out);

/* Move the camera around */
void camera_mouse_look(Camera* camera, int mouse_x, int mouse_y);
void camera_move_forward(Camera* camera, float speed);
void camera_move_backward(Camera* camera, float speed);
void camera_move_left(Camera* camera, float speed);
void camera_move_right(Camera* camera, float speed);
void camera_move_up(Camera* camera, float speed);
void camera_move_down(Camera* camera, float speed);

/* Set the position for the camera */
void camera_set_eye_position(Camera* camera, float x, float y, float z);

/* Returns the Camera X Position where the eye is */
float camera_get_eye_x_position(const Camera* camera);

/* Returns the Camera Y Position where the eye is */
float camera_get_eye_y_position(const Camera* camera);

/* Returns the Camera Z Position where the eye is */
float camera_get_eye_z_position(const Camera* camera);

/* Returns the X 'view' direction */
float camera_get_view_x_direction(const Camera* camera);

/* Returns the Y 'view' direction */
float camera_get_view_y_direction(const Camera* camera);

/* Returns the Z 'view' direction */
float camera_get_view_z_direction(const Camera* camera);

#endif /* CAMERA_H */

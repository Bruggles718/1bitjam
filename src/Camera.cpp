#include "Camera.hpp"

#include "glm/gtx/transform.hpp"
#include "glm/gtx/rotate_vector.hpp"
#include "glm/gtx/quaternion.hpp"

// OPTIONAL TODO: 
//               The camera could really be improved by
//               updating the eye position along the m_viewDirection.
//               Think about how you can do this for a better camera!

void Camera::MoveForward(float speed){
    //    m_eyePosition.z -= speed;
    glm::vec4 forward = { 0.0f, 0.0f, -1.0f, 1.0f };  /* Default forward is -Z */
    glm::mat4 rotation_matrix = glm::toMat4(m_rotation);
    forward = m_rotation * forward;

    /* Move in forward direction */
    forward = forward * speed;
    m_eyePosition += forward;
}

void Camera::MoveBackward(float speed){
   this->MoveForward(-speed);
}

void Camera::MoveLeft(float speed){
    //   TODO -- Note: You are updating the 'eye' position, but consider
    //        	       that you will need to update the 'rightVector'
    // Compute the right vector and update your 'eye' accordingly
    glm::vec4 right = { -1.0f, 0.0f, 0.0f, 1.0f };  /* Default forward is -Z */
    glm::mat4 rotation_matrix = glm::toMat4(m_rotation);
    right = m_rotation * right;

    /* Move in forward direction */
    right = right * speed;
    m_eyePosition += right;
}

void Camera::MoveRight(float speed){
    this->MoveLeft(-speed);
}

void Camera::MoveUp(float speed){
    m_eyePosition.y += speed;
}

void Camera::MoveDown(float speed){
    m_eyePosition.y -= speed;
}

// Set the position for the camera
void Camera::SetCameraEyePosition(float x, float y, float z){
    m_eyePosition.x = x;
    m_eyePosition.y = y;
    m_eyePosition.z = z;
}

float Camera::GetEyeXPosition(){
    return m_eyePosition.x;
}

float Camera::GetEyeYPosition(){
    return m_eyePosition.y;
}

float Camera::GetEyeZPosition(){
    return m_eyePosition.z;
}

void Camera::SetRotation(glm::quat rotation) {
    m_rotation = rotation;
}

void Camera::Rotate(float angle_degrees, glm::vec3 axis) {
    glm::quat rot = glm::angleAxis(glm::radians(angle_degrees), axis);
    m_rotation = rot * m_rotation;
    m_rotation = glm::normalize(m_rotation);
}

Camera::Camera(){
    //std::cout << "Camera.cpp: (Constructor) Created a Camera!\n";
	// Position us at the origin.
    m_eyePosition = glm::vec3(0.0f,0.0f, 5.0f);
	// Looking down along the z-axis initially.
	// Remember, this is negative because we are looking 'into' the scene.
    m_rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

glm::mat4 Camera::GetViewMatrix() const{
    // Think about the second argument and why that is
    // setup as it is.
    glm::quat quat_inv = glm::conjugate(m_rotation);
    glm::mat4 out = glm::toMat4(quat_inv);
    glm::vec3 negative_eye = {-m_eyePosition.x, -m_eyePosition.y, -m_eyePosition.z};
    return glm::translate(out, negative_eye);
}

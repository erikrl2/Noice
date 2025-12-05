#pragma once
#include "camera.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

glm::mat4 Camera::GetView() const {
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    return glm::lookAt(position, position + glm::normalize(front), glm::vec3(0, 1, 0));
}

glm::mat4 Camera::GetProjection(float aspect) const {
    return glm::perspective(glm::radians(fov), aspect, 0.1f, 100.0f);
}

void Camera::ProcessKeyboard(const glm::vec3& dir, float speed) { position += dir * speed; }

void Camera::ProcessMouseDelta(float dx, float dy, float sensitivity = 0.1f) {
    yaw += dx * sensitivity; pitch += dy * sensitivity;
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}
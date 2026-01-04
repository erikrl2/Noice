#include "camera.hpp"

#include "util.hpp"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

static bool mouseDown = false;

void Camera::Update(float dt) {
  if (ImGui::GetIO().WantCaptureKeyboard) return;

  float camSpeed = 50.0f * dt;
  glm::vec3 front = GetFront();
  glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
  glm::vec3 right = glm::normalize(glm::cross(front, worldUp));

  glm::vec3 moveDir(0.0f);
  if (util::IsKeyPressed(GLFW_KEY_W)) moveDir += front;
  if (util::IsKeyPressed(GLFW_KEY_S)) moveDir -= front;
  if (util::IsKeyPressed(GLFW_KEY_A)) moveDir -= right;
  if (util::IsKeyPressed(GLFW_KEY_D)) moveDir += right;
  if (util::IsKeyPressed(GLFW_KEY_SPACE)) moveDir += worldUp;
  if (util::IsKeyPressed(GLFW_KEY_LEFT_CONTROL)) moveDir -= worldUp;
  if (util::IsKeyPressed(GLFW_KEY_LEFT_SHIFT)) camSpeed *= 0.5f;

  if (glm::length(moveDir) > 0.001f) {
    moveDir = glm::normalize(moveDir);
    position += moveDir * camSpeed;
  }
}

void Camera::OnMouseClicked(int button, int action) {
  switch (button) {
  case GLFW_MOUSE_BUTTON_LEFT: {
    if (action == GLFW_PRESS) {
      mouseDown = true;
      util::SetCursorDisabled(true);
    } else if (action == GLFW_RELEASE) {
      mouseDown = false;
      util::SetCursorDisabled(false);
    }
    break;
  }
  }
}

void Camera::OnMouseMoved(double xpos, double ypos) {
  static bool firstMouse = true;
  static double lastX, lastY;

  if (!mouseDown) {
    firstMouse = true;
    return;
  }
  if (firstMouse) {
    lastX = xpos;
    lastY = ypos;
    firstMouse = false;
  }

  double dx = xpos - lastX;
  double dy = lastY - ypos;
  lastX = xpos;
  lastY = ypos;

  float mouseSensitivity = 0.12f;

  // ProcessMouseDelta
  yaw += (float)dx * mouseSensitivity;
  pitch += (float)dy * mouseSensitivity;
  if (pitch > 89.0f) pitch = 89.0f;
  if (pitch < -89.0f) pitch = -89.0f;
}

glm::mat4 Camera::GetView() const {
  glm::vec3 front = GetFront();
  return glm::lookAt(position, position + front, glm::vec3(0, 1, 0));
}

glm::mat4 Camera::GetProjection(float aspect) const {
  return glm::perspective(glm::radians(fov), aspect, 0.1f, 1000.0f);
}

glm::vec3 Camera::GetFront() const {
  glm::vec3 front;
  front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
  front.y = sin(glm::radians(pitch));
  front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
  return glm::normalize(front);
}

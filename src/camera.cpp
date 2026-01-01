#include "camera.hpp"

#include "GLFW/glfw3.h"

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

static bool mouseDown = false;

void Camera::Update(float dt) {
  if (ImGui::GetIO().WantCaptureKeyboard) return;
  GLFWwindow* win = glfwGetCurrentContext();

  float camSpeed = 50.0f * dt;
  glm::vec3 front = GetFront();
  glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
  glm::vec3 right = glm::normalize(glm::cross(front, worldUp));

  glm::vec3 moveDir(0.0f);
  if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) moveDir += front;
  if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) moveDir -= front;
  if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) moveDir -= right;
  if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) moveDir += right;
  if (glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS) moveDir += worldUp;
  if (glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) moveDir -= worldUp;
  if (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) camSpeed *= 0.5f;

  if (glm::length(moveDir) > 0.001f) {
    moveDir = glm::normalize(moveDir);
    position += moveDir * camSpeed;
  }
}

void Camera::OnMouseClicked(int button, int action) {
  GLFWwindow* win = glfwGetCurrentContext();
  switch (button) {
  case GLFW_MOUSE_BUTTON_LEFT: {
    if (action == GLFW_PRESS) {
      mouseDown = true;
      glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else if (action == GLFW_RELEASE) {
      mouseDown = false;
      glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
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
  return glm::perspective(glm::radians(fov), aspect, 0.1f, 100.0f);
}

glm::vec3 Camera::GetFront() const {
  glm::vec3 front;
  front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
  front.y = sin(glm::radians(pitch));
  front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
  return glm::normalize(front);
}

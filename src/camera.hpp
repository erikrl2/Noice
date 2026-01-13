#pragma once
#include <glm/glm.hpp>

class Camera {
public:
  void Update(float dt);
  void OnMouseClicked(int button, int action);
  void OnMouseMoved(double xpos, double ypos);

  glm::mat4 GetView() const;
  glm::mat4 GetProjection(float aspect, bool ortho = false) const;

  glm::vec3 GetPosition() const { return position; }

private:
  glm::vec3 GetFront() const;

private:
  glm::vec3 position = {0.0f, 10.0f, 0.0f};
  float yaw = -90.0f;
  float pitch = 0.0f;
  float fov = 60.0f;
};

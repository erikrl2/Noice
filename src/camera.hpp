#pragma once
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

class Camera {
public:
    void UpdateCamera(GLFWwindow* win, float dt);
    void OnMouseClicked(GLFWwindow* win, int button, int action);
    void OnMouseMoved(double xpos, double ypos);

    glm::mat4 GetView() const;
    glm::mat4 GetProjection(float aspect) const;

private:
    glm::vec3 GetFront() const;

private:
    glm::vec3 position = { 0.0f, 1.0f, 5.0f };
    float yaw = -90.0f;
    float pitch = 0.0f;
    float fov = 60.0f;
};
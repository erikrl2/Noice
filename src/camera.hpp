#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Camera {
    glm::vec3 position = {0.0f, 0.0f, 3.0f};
    float yaw = -90.0f;
    float pitch = 0.0f;
    float fov = 60.0f;

    glm::mat4 GetView() const;
    glm::mat4 GetProjection(float aspect) const;

    // Liefert die Blickrichtung (normalisiert)
    glm::vec3 GetFront() const;

    // dir wird als Richtungsvektor interpretiert (lokal in Weltkoordinaten)
    void ProcessKeyboard(const glm::vec3& dir, float speed);
    void ProcessMouseDelta(float dx, float dy, float sensitivity);
};
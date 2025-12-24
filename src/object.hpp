#pragma once
#include "shader.hpp"
#include "framebuffer.hpp"
#include "mesh.hpp"
#include "camera.hpp"
#include "mode.hpp"

#include <GLFW/glfw3.h>

class ObjectMode : public Mode {
public:
    void Init(int width, int height);
    void Destroy();

    void UpdateImGui();
    void Update(float dt);

    void OnResize(int width, int height);
    void OnMouseClicked(int button, int action);
    void OnMouseMoved(double xpos, double ypos);

    Framebuffer& GetResultFB() { return objectFB; }
    const MvpState* GetMvpState() { return &mvpState; }

private:
    void UpdateTransformMatrices(float dt);
    void RenderObject();

    SimpleMesh& SelectedMesh();

private:
    enum class MeshType { Car, Spider, Dragon, Alien };
    MeshType meshSelect = MeshType::Car;

    SimpleMesh carMesh;
    SimpleMesh spiderMesh;
    SimpleMesh dragonMesh;

    Shader objectShader;
    Framebuffer objectFB;

    Camera camera;
    MvpState mvpState;
    bool hasValidPrevMvp = false;
};

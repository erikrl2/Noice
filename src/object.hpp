#pragma once
#include "shader.hpp"
#include "framebuffer.hpp"
#include "mesh.hpp"
#include "camera.hpp"
#include "mode.hpp"

#include <GLFW/glfw3.h>

#include <array>

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
    enum class MeshType { Car = 0, Spider, Dragon, Alien, Head, Count };
    MeshType meshSelect = MeshType::Head;

    std::array<SimpleMesh, (size_t)MeshType::Count> meshes;

    Shader objectShader;
    Framebuffer objectFB;

    Camera camera;
    MvpState mvpState;
    bool hasValidPrevMvp = false;
};

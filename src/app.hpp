#pragma once
#include "shader.hpp"
#include "framebuffer.hpp"
#include "mesh.hpp"
#include "camera.hpp"
#include "effect.hpp"

#include <GLFW/glfw3.h>

class App {
public:
    App();
    ~App();

    void Run();

private:
    void InitWindow();
    void InitOpenGL();
    void InitImGui();
    void SetupResources();
    void DestroyResources();

    void Update(float dt);

    void UpdateImGui();
    void UpdateTransformMatrices(float dt);

    void RenderObject();
    void RenderTexToScreen(Texture& resultTex);

    static void OnFramebufferResized(GLFWwindow* window, int w, int h);
    static void OnMouseClicked(GLFWwindow* window, int button, int action, int mods);
    static void OnMouseMoved(GLFWwindow* window, double xpos, double ypos);

    SimpleMesh& SelectedMesh();
    Effect& SelectedEffect();

    void CheckWindowSize();

private:
    GLFWwindow* win = nullptr;
    int width = 1280;
    int height = 720;
    bool minimized = false;

    enum class MeshType { Car, Spider, Dragon, Alien };
    MeshType meshSelect = MeshType::Car;

    bool hasValidPrevMvp = false;
    MvpState mvpState;

    Camera camera;

    Effect effect3D;
    // TODO: list of 2D effects associated with imgui windows

    SimpleMesh quadMesh;
    SimpleMesh carMesh;
    SimpleMesh spiderMesh;
    SimpleMesh dragonMesh;
    Shader objectShader;
    Shader postShader;
    Framebuffer objectFB;
};

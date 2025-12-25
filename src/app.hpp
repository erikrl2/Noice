#pragma once
#include "shader.hpp"
#include "effect.hpp"
#include "object.hpp"
#include "text.hpp"

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

    void UpdateImGui();
    void Update(float dt);

    void RenderToScreen();

    static void OnFramebufferResized(GLFWwindow* window, int w, int h);
    static void OnMouseMoved(GLFWwindow* window, double xpos, double ypos);
    static void OnMouseClicked(GLFWwindow* window, int button, int action, int mods);
    static void OnKeyPressed(GLFWwindow* window, int key, int scancode, int action, int mods);

    Effect& SelectedEffect();
    Mode& SelectedMode();

    void CheckWindowSize();

private:
    GLFWwindow* win = nullptr;
    int width = 1280;
    int height = 720;
    bool minimized = false;

    Effect scrollEffect;
    Shader postShader;
    SimpleMesh quadMesh;

    enum class ModeType { Object, Text, Paint };
    ModeType modeSelect = ModeType::Object;

    ObjectMode objectMode;
    TextMode textMode;
    // TODO: Paint

    bool showSettings = true;
};

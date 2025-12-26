#include "app.hpp"
#include "GLFW/glfw3.h"
#include "framebuffer.hpp"
#include "mesh.hpp"
#include "util.hpp"

#include <glad/glad.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

App::App() {
    InitWindow();

    InitOpenGL();
    InitImGui();

    SetupResources();

    CheckWindowSize();
}

void App::Run() {
    while (!glfwWindowShouldClose(win)) {
        if (minimized) {
            glfwWaitEvents();
            continue;
        }
        float dt = ImGui::GetIO().DeltaTime;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();
        UpdateImGui();
        ImGui::Render();

        Update(dt);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(win);
        glfwPollEvents();
    }
}

App::~App() {
    DestroyResources();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
}

void App::InitWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    win = glfwCreateWindow(width, height, "Noice", nullptr, nullptr);
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    glfwSetWindowUserPointer(win, this);

    glfwSetFramebufferSizeCallback(win, OnFramebufferResized);
    glfwSetCursorPosCallback(win, OnMouseMoved);
    glfwSetMouseButtonCallback(win, OnMouseClicked);
    glfwSetKeyCallback(win, OnKeyPressed);
}

void App::InitOpenGL() {
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

#ifndef NDEBUG
    EnableOpenGLDebugOutput();
#endif

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

void App::InitImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui::StyleColorsDark();
    ImGui::GetIO().IniFilename = nullptr;

    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 430 core");
}

void App::SetupResources() {
    quadMesh = SimpleMesh::CreateFullscreenQuad();
    postShader.Create("assets/shaders/post.vert.glsl", "assets/shaders/post.frag.glsl");
    scrollEffect.Init(width, height);
    objectMode.Init(width, height);
    textMode.Init(width, height);
}

void App::DestroyResources() {
    quadMesh.Destroy();
    postShader.Destroy();
    scrollEffect.Destroy();
    objectMode.Destroy();
    textMode.Destroy();
}

void App::Update(float dt) {
    Effect& e = SelectedEffect();
    Mode& m = SelectedMode();

    m.Update(dt);
    if (m.HasMvp()) e.Apply(m.GetResultFB(), dt, m.GetMvpState());
    else e.Apply(m.GetResultFB(), dt);

    RenderToScreen();
}

void App::UpdateImGui() {
    if (!showSettings) return;
    bool open = ImGui::Begin("Settings", &showSettings, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav);
    if (open) {
        SelectedEffect().UpdateImGui();

        ImGui::Separator();

        ImGui::Text("Mode:");
        bool changed = false;
        changed |= ImGui::RadioButton("Object##Mode", (int*)&modeSelect, (int)ModeType::Object); ImGui::SameLine();
        changed |= ImGui::RadioButton("Text##Mode", (int*)&modeSelect, (int)ModeType::Text);
        if (changed) OnModeChange();

        ImGui::Separator();

        SelectedMode().UpdateImGui();
    }
    ImGui::End();
}

void App::RenderToScreen() {
    Effect& e = SelectedEffect();
    Mode& m = SelectedMode();

    postShader.Use();
    postShader.SetTexture2D("screenTex", !e.IsDisabled() ? e.GetResultTex() : m.GetResultFB().tex);
    postShader.SetVec2("resolution", glm::vec2(width, height));
    postShader.SetInt("showVectors", e.IsDisabled());

    Framebuffer::BindDefault(width, height);

    quadMesh.Draw();
}

void App::OnFramebufferResized(GLFWwindow* window, int w, int h) {
    App& app = *(App*)glfwGetWindowUserPointer(window);

    if (w == 0 || h == 0) { app.minimized = true; return; }

    app.minimized = false;
    app.width = w;
    app.height = h;

    app.SelectedEffect().OnResize(w, h);
    app.SelectedMode().OnResize(w, h);
}

void App::OnMouseMoved(GLFWwindow* window, double xpos, double ypos) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    App& app = *(App*)glfwGetWindowUserPointer(window);

    app.SelectedMode().OnMouseMoved(xpos, ypos);
}

void App::OnMouseClicked(GLFWwindow* window, int button, int action, int mods) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    App& app = *(App*)glfwGetWindowUserPointer(window);

    app.SelectedEffect().OnMouseClicked(button, action);
    app.SelectedMode().OnMouseClicked(button, action);
}

void App::OnKeyPressed(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (ImGui::GetIO().WantCaptureKeyboard) return;
    App& app = *(App*)glfwGetWindowUserPointer(window);

    app.SelectedEffect().OnKeyPressed(key, action);
    app.SelectedMode().OnKeyPressed(key, action);

    switch (key) {
    case GLFW_KEY_ESCAPE:
        if (action == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
        break;
    case GLFW_KEY_H:
        if (action == GLFW_PRESS) app.showSettings = !app.showSettings;
        break;
    case GLFW_KEY_O:
        if (action == GLFW_PRESS) { app.modeSelect = ModeType::Object; app.OnModeChange(); }
        break;
    case GLFW_KEY_T:
        if (action == GLFW_PRESS) { app.modeSelect = ModeType::Text; app.OnModeChange(); }
        break;
    }
}

void App::OnModeChange() {
    SelectedMode().OnResize(width, height);
    SelectedEffect().ClearBuffers();
}

void App::CheckWindowSize() {
    int w, h;
    glfwGetFramebufferSize(win, &w, &h);
    if (w != width || h != height)
        App::OnFramebufferResized(win, w, h);
}

Effect& App::SelectedEffect() {
    return scrollEffect;
}

Mode& App::SelectedMode() {
    switch (modeSelect) {
    case ModeType::Object: return objectMode;
    case ModeType::Text: return textMode;
    case ModeType::Paint: break;
    }
    return textMode;
}

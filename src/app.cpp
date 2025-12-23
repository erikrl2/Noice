#include "app.hpp"
#include "GLFW/glfw3.h"
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
    glEnable(GL_CULL_FACE);
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
    Effect& effect = SelectedEffect();

    switch (modeSelect) {
    case Mode::Object: {
        objectMode.Update(dt);
        effect.ApplyDynamic(objectMode.GetObjFB(), objectMode.GetMvpState(), dt);
        break;
    }
    case Mode::Text: {
        textMode.Update(dt);
        effect.ApplyStatic(textMode.GetObjFB(), dt);
      break;
    }
    case Mode::Paint: {}
    }

    RenderTexToScreen(effect.GetNoiseTex());
}

void App::UpdateImGui() {
    bool open = ImGui::Begin("Settings", 0, ImGuiWindowFlags_AlwaysAutoResize);
    if (open) {
        SelectedEffect().UpdateImGui();

        ImGui::Separator();

        ImGui::Text("Mode:");
        ImGui::RadioButton("Objet##Mode", (int*)&modeSelect, (int)Mode::Object); ImGui::SameLine();
        ImGui::RadioButton("Text##Mode", (int*)&modeSelect, (int)Mode::Text);

        switch (modeSelect) {
        case Mode::Object: objectMode.UpdateImGui(); break;
        case Mode::Text: textMode.UpdateImGui(); break;
        case Mode::Paint: break;
        }
    }
    ImGui::End();
}

void App::RenderTexToScreen(Texture& resultTex) {
    postShader.Use();
    postShader.SetTexture2D("screenTex", !scrollEffect.Disabled() ? resultTex : (true ? textMode.GetObjFB().tex : scrollEffect.GetAccTex())); // TODO
    postShader.SetVec2("resolution", glm::vec2(width, height));
    postShader.SetInt("showFlow", scrollEffect.Disabled());
    Framebuffer::BindDefault(width, height);
    quadMesh.Draw();
}

void App::OnFramebufferResized(GLFWwindow* window, int w, int h) {
    App& app = *(App*)glfwGetWindowUserPointer(window);

    if (w == 0 || h == 0) { app.minimized = true; return; }

    app.minimized = false;
    app.width = w;
    app.height = h;

    app.scrollEffect.OnResize(w, h);
    app.objectMode.OnResize(w, h);
    app.textMode.OnResize(w, h);
}

void App::OnMouseMoved(GLFWwindow* window, double xpos, double ypos) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    App& app = *(App*)glfwGetWindowUserPointer(window);

    if (app.modeSelect == Mode::Object)
        app.objectMode.OnMouseMoved(xpos, ypos);
}

void App::OnMouseClicked(GLFWwindow* window, int button, int action, int mods) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    App& app = *(App*)glfwGetWindowUserPointer(window);

    app.scrollEffect.OnMouseClicked(button, action);

    if (app.modeSelect == Mode::Object)
        app.objectMode.OnMouseClicked(button, action);
}

void App::OnKeyPressed(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (ImGui::GetIO().WantCaptureKeyboard) return;

    if (key == GLFW_KEY_ESCAPE) {
      if (action == GLFW_PRESS) {
          glfwSetWindowShouldClose(window, true);
      }
    }
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

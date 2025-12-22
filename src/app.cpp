#include "app.hpp"
#include "util.hpp"

#include <glad/glad.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <glm/gtc/matrix_transform.hpp>

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

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        UpdateImGui();
        ImGui::Render();

        float dt = ImGui::GetIO().DeltaTime;
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
}

void App::InitOpenGL() {
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

#ifndef NDEBUG
    EnableOpenGLDebugOutput();
#endif

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
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
    carMesh = SimpleMesh::LoadFromOBJ("models/jeep.obj");
    spiderMesh = SimpleMesh::LoadFromOBJ("models/spider.obj");
    //dragonMesh = SimpleMesh::LoadFromOBJ("models/dragon.obj");

    objectShader.Create("shaders/basic.vert.glsl", "shaders/basic.frag.glsl");
    postShader.Create("shaders/post.vert.glsl", "shaders/post.frag.glsl");

    objectFB.Create(width, height, GL_RG16F, GL_LINEAR, true);

    effect3D.Init(width, height);
}

void App::DestroyResources() {
    quadMesh.Destroy();
    carMesh.Destroy();
    spiderMesh.Destroy();
    dragonMesh.Destroy();

    objectShader.Destroy();
    postShader.Destroy();

    objectFB.Destroy();

    effect3D.Destroy();
}

void App::Update(float dt) {
    camera.UpdateCamera(win, dt);

    UpdateTransformMatrices(dt);

    RenderObject();

    effect3D.ApplyDynamic(objectFB, mvpState, dt);

    RenderTexToScreen(effect3D.GetNoiseTex());
}

void App::UpdateImGui() {
    if (!ImGui::Begin("Settings", 0, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }
    Effect& e = SelectedEffect();

    ImGui::Text("Effect:");
    ImGui::SliderFloat("Speed (px/s)", &e.scrollSpeed, 0.0f, 500.0f, "%.0f");
    ImGui::SliderInt("Reset Interval", &e.accResetInterval, 1, 100);
    if (ImGui::SliderInt("Downscale", &e.downscaleFactor, 1, 8)) effect3D.OnResize(width, height);
    ImGui::Checkbox("Disable", &e.disabled); ImGui::SameLine();
    ImGui::Checkbox("Pause", &e.paused);

    ImGui::Separator();
    ImGui::Text("Mesh:");
    bool changed = false;
    changed = ImGui::RadioButton("Car", (int*)&meshSelect, (int)MeshType::Car); ImGui::SameLine();
    changed = ImGui::RadioButton("Spider", (int*)&meshSelect, (int)MeshType::Spider); ImGui::SameLine();
    if (dragonMesh.vao) changed = ImGui::RadioButton("Dragon", (int*)&meshSelect, (int)MeshType::Dragon);
    if (changed) hasValidPrevMvp = false;

    ImGui::Separator();
    ImGui::Text("Window: %dx%d", width, height);
    ImGui::Text("Noise: %dx%d", width / e.downscaleFactor, height / e.downscaleFactor);
    // TODO: text displying active effect

    ImGui::End();

    // TODO: implement 2d textureImage canvas window creation + list management
    // TODO: but first try drawing single window with colored text/background

}

void App::UpdateTransformMatrices(float dt) {
    float aspect = (height > 0) ? ((float)width / (float)height) : 1.0f;
    glm::mat4 proj = camera.GetProjection(aspect);
    glm::mat4 view = camera.GetView();
    glm::mat4 m = glm::mat4(1.0f);

    glm::vec3 translation = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    float scale = 1.0f;

    switch (meshSelect) {
    case MeshType::Car:
        rotation.y = 45.0f;
        break;
    case MeshType::Spider:
        rotation.y = 180.0f;
        scale = 0.025f;
        break;
    case MeshType::Dragon:
        rotation.x = -90.0f;
        rotation.y = 20.0f;
        scale = 0.07f;
        break;
    }

    m = glm::translate(m, translation);
    //m = glm::rotate(m, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    m = glm::rotate(m, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::rotate(m, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    m = glm::scale(m, glm::vec3(scale));

    if (hasValidPrevMvp) {
        mvpState.prevProj = mvpState.currProj;
        mvpState.prevView = mvpState.currView;
        mvpState.prevModel = mvpState.currModel;
    }
    else {
        mvpState.prevProj = proj;
        mvpState.prevView = view;
        mvpState.prevModel = m;
        hasValidPrevMvp = true;
    }
    mvpState.currProj = proj;
    mvpState.currView = view;
    mvpState.currModel = m;
}

void App::RenderObject() {
    objectShader.Use();
    objectShader.SetMat4("viewproj", mvpState.currProj * mvpState.currView);
    objectShader.SetMat4("model", mvpState.currModel);
    objectShader.SetMat4("normalMatrix", glm::transpose(glm::inverse(mvpState.currModel)));

    objectFB.Clear();

    SelectedMesh().Draw(RenderFlag::DepthTest);
}

void App::RenderTexToScreen(Texture& resultTex) {
    postShader.Use();
    postShader.SetTexture2D("screenTex", !effect3D.disabled ? resultTex : (true ? objectFB.tex : effect3D.GetAccTex()));
    postShader.SetVec2("resolution", glm::vec2(width, height));
    postShader.SetInt("showFlow", effect3D.disabled);
    Framebuffer::BindDefault(width, height);
    quadMesh.Draw();
}

void App::OnFramebufferResized(GLFWwindow* window, int w, int h) {
    App& app = *(App*)glfwGetWindowUserPointer(window);

    if (w == 0 || h == 0) { app.minimized = true; return; }

    app.minimized = false;
    app.width = w;
    app.height = h;

    app.hasValidPrevMvp = false;

    app.objectFB.Resize(w, h);
    app.effect3D.OnResize(w, h);
}

void App::OnMouseClicked(GLFWwindow* window, int button, int action, int mods) {
    if (ImGui::GetIO().WantCaptureMouse) return;

    App& app = *(App*)glfwGetWindowUserPointer(window);
    app.camera.OnMouseClicked(window, button, action);

    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            app.effect3D.disabled = !app.effect3D.disabled;
        }
    }
}

void App::OnMouseMoved(GLFWwindow* window, double xpos, double ypos) {
    if (ImGui::GetIO().WantCaptureMouse) return;

    App& app = *(App*)glfwGetWindowUserPointer(window);
    app.camera.OnMouseMoved(xpos, ypos);
}

void App::CheckWindowSize() {
    int w, h;
    glfwGetFramebufferSize(win, &w, &h);
    if (w != width || h != height)
        App::OnFramebufferResized(win, w, h);
}

SimpleMesh& App::SelectedMesh() {
    switch (meshSelect) {
    case MeshType::Car: return carMesh;
    case MeshType::Spider: return spiderMesh;
    case MeshType::Dragon: return dragonMesh;
    default: return carMesh;
    }
}

Effect& App::SelectedEffect() {
    return effect3D;
}

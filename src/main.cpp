#include "shader.hpp"
#include "framebuffer.hpp"
#include "mesh.hpp"
#include "camera.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

static Camera camera;
static bool mouseDown = false;

static float updatesPerSecond = 10.0f;
static int downscaleFactor = 1;
static float lineWidth = 5.0f;
static bool wireframeOn = true;
static bool pauseFlicker = false;
static bool disableEffect = false;
static glm::vec3 color{ 1, 1, 1 };
static int meshSelect = 0;

static void OnMouseMoved(GLFWwindow* window, double xpos, double ypos) {
    if (ImGui::GetIO().WantCaptureMouse) return;

    static bool firstMouse = true;
    static double lastX, lastY;

    if (!mouseDown) {
        firstMouse = true;
        return;
    }
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    double dx = xpos - lastX;
    double dy = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float mouseSensitivity = 0.12f;
    camera.ProcessMouseDelta((float)dx, (float)dy, mouseSensitivity);
}

static void OnMouseClicked(GLFWwindow* window, int button, int action, int mods) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    if (ImGui::GetIO().WantCaptureMouse) return;

    if (action == GLFW_PRESS) {
        mouseDown = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    else if (action == GLFW_RELEASE) {
        mouseDown = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

static void HandleCameraMovement(GLFWwindow* win, float delta) {
    if (ImGui::GetIO().WantCaptureKeyboard) return;

    float camSpeed = 5.0f * delta;
    glm::vec3 front = camera.GetFront();
    glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(front, worldUp));

    glm::vec3 moveDir(0.0f);
    if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) moveDir += front;
    if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) moveDir -= front;
    if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) moveDir -= right;
    if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) moveDir += right;
    if (glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS) moveDir += worldUp;
    if (glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) moveDir -= worldUp;

    if (glm::length(moveDir) > 0.001f) {
        moveDir = glm::normalize(moveDir);
        camera.ProcessKeyboard(moveDir, camSpeed);
    }
}

struct WindowState {
    int width = 0;
    int height = 0;
    bool resized = false;
};

static void OnFramebufferResized(GLFWwindow* window, int w, int h) {
    WindowState* s = (WindowState*)glfwGetWindowUserPointer(window);
    if (!s) return;
    s->width = w;
    s->height = h;
    s->resized = true;
}

static GLFWwindow* InitWindow(WindowState& ws, const char* title) {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* win = glfwCreateWindow(ws.width, ws.height, title, nullptr, nullptr);
    if (!win) {
        std::cerr << "Failed to create GLFW window\n";
        return nullptr;
    }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    glfwSetWindowUserPointer(win, &ws);
    glfwSetFramebufferSizeCallback(win, OnFramebufferResized);
    glfwSetCursorPosCallback(win, OnMouseMoved);
    glfwSetMouseButtonCallback(win, OnMouseClicked);
    return win;
}

static void InitOpenGL() {
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glClearColor(0, 0, 0, 1);
    glLineWidth(lineWidth);
    glDisable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

static void InitImGui(GLFWwindow* win) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
}

struct Resources {
    Shader objectShader{};
    Shader postShader{};
    Shader xorShader{};
    Shader noiseInitShader{};
    SimpleMesh quadMesh;
    SimpleMesh triMesh;
    SimpleMesh carMesh;
    SimpleMesh icosaMesh;
    Framebuffer objectFB;
    Framebuffer noiseFB1;
    Framebuffer noiseFB2;
};

static Resources SetupResources(WindowState& windowState) {
    Resources r;
    r.objectShader = Shader("shaders/basic.vert.glsl", "shaders/basic.frag.glsl");
    r.postShader = Shader("shaders/post.vert.glsl", "shaders/post.frag.glsl");
    r.xorShader = Shader("shaders/post.vert.glsl", "shaders/xor.frag.glsl");
    r.noiseInitShader = Shader("shaders/post.vert.glsl", "shaders/noise_init.frag.glsl");

    r.quadMesh = SimpleMesh::CreateFullscreenQuad();
    r.triMesh = SimpleMesh::CreateTriangle();
    r.carMesh = SimpleMesh::LoadFromOBJ("models/car.obj");
    r.icosaMesh = SimpleMesh::LoadFromOBJ("models/icosahedron.obj");

    r.objectFB.Create(windowState.width, windowState.height, true);
    return r;
}

static void ResetFramebuffers(WindowState& state, Resources& r) {
    int scaledWidth = state.width / downscaleFactor;
    int scaledHeight = state.height / downscaleFactor;

    r.objectFB.Resize(state.width, state.height);
    r.noiseFB1.Resize(scaledWidth, scaledHeight);
    r.noiseFB2.Resize(scaledWidth, scaledHeight);

    r.noiseFB1.Bind();
    r.noiseInitShader.Use();
    r.quadMesh.Draw();
    Framebuffer::Unbind();
}

static void RenderSettingsWindow(WindowState& state, Resources& r) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (!ImGui::Begin("Settings", 0, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    if (ImGui::SliderInt("Downscale", &downscaleFactor, 1, 8)) state.resized = true;
    ImGui::SliderFloat("Speed", &updatesPerSecond, 1.0f, 60.0f, "%.2f");
    ImGui::Checkbox("Pause Flicker", &pauseFlicker);
    ImGui::Checkbox("Disable Effect", &disableEffect);

    ImGui::Separator();
    ImGui::Text("Mesh:");
    ImGui::RadioButton("Triangle", &meshSelect, 0); ImGui::SameLine();
    ImGui::RadioButton("Icosahedron", &meshSelect, 1); ImGui::SameLine();
    ImGui::RadioButton("Car", &meshSelect, 2);
    ImGui::Checkbox("Wireframe", &wireframeOn);
    if (ImGui::SliderFloat("Line Width", &lineWidth, 1.0f, 10.0f, "%.1f")) glLineWidth(lineWidth);
    ImGui::ColorEdit3("Color", &color[0]);

    ImGui::Separator();
    ImGui::Text("Window: %dx%d", state.width, state.height);
    ImGui::Text("Noise: %dx%d", state.width / downscaleFactor, state.height / downscaleFactor);

    ImGui::End();
}

static void UpdateAndRenderObjects(WindowState& ws, Resources& r, float delta) {
    float aspect = (ws.height > 0) ? ((float)ws.width / (float)ws.height) : 1.0f;
    glm::mat4 vp = camera.GetProjection(aspect) * camera.GetView();

    static float angle = 90.0f;
    angle += 20.0f * delta;

    glm::mat4 m = glm::mat4(1.0f);
    if (meshSelect == 2) m = glm::translate(m, glm::vec3(0.0f, -1.0f, -3.0f));
    m = glm::rotate(m, glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));

    r.objectFB.Bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // TODO: move to better place?
    r.objectShader.Use();
    r.objectShader.SetVec3("color", color);
    r.objectShader.SetMat4("mvp", vp * m);
    if (meshSelect == 0) r.triMesh.Draw(wireframeOn, true);
    else if (meshSelect == 1) r.icosaMesh.Draw(wireframeOn, true);
    else if (meshSelect == 2) r.carMesh.Draw(wireframeOn, true);
    Framebuffer::Unbind();
}

static bool ShouldApplyEffect(float delta) {
    if (pauseFlicker) return false;
    static float updateAccumulator = 0.0;
    updateAccumulator += delta;
    float updateInterval = 1.0f / updatesPerSecond;
    if (updateAccumulator >= updateInterval) {
        updateAccumulator = 0.0f;
        return true;
    }
    return false;
}

static void RenderNoiseEffect(Resources& r, Framebuffer& prev, Framebuffer& next) {
    next.Bind();
    r.xorShader.Use();
    r.xorShader.SetTexture2D("prevTex", prev.Texture(), 0);
    r.xorShader.SetTexture2D("objectTex", r.objectFB.Texture(), 1);
    r.quadMesh.Draw();
    Framebuffer::Unbind();
}

static void PresentScene(WindowState& state, Resources& r, Framebuffer& src) {
    Framebuffer::BindDefault(state.width, state.height);
    r.postShader.Use();
    r.postShader.SetTexture2D("screenTex", src.Texture());
    r.quadMesh.Draw();
}

int main() {
    WindowState windowState{ 800, 600 };

    GLFWwindow* win = InitWindow(windowState, "Noice");
    if (!win) return -1;

    InitOpenGL();

    InitImGui(win);

    Resources res = SetupResources(windowState);

    Framebuffer* prevFB, * nextFB; // ping-pong framebuffers
    windowState.resized = true; // triggers fb reset

    while (!glfwWindowShouldClose(win)) {
        float delta = ImGui::GetIO().DeltaTime;

        HandleCameraMovement(win, delta);

        if (windowState.resized) {
            ResetFramebuffers(windowState, res);
            prevFB = &res.noiseFB1;
            nextFB = &res.noiseFB2;
            windowState.resized = false;
        }

        RenderSettingsWindow(windowState, res);

        UpdateAndRenderObjects(windowState, res, delta);

        if (ShouldApplyEffect(delta)) {
            RenderNoiseEffect(res, *prevFB, *nextFB);
            std::swap(prevFB, nextFB);
        }

        PresentScene(windowState, res, !disableEffect ? *prevFB : res.objectFB);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}

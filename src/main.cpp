#include "shader.hpp"
#include "framebuffer.hpp"
#include "mesh.hpp"
#include "camera.hpp"
#include "util.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

enum class MeshType { Triangle = 0, Icosahedron, Car, Count };
enum class EffectType { Flicker = 0, Scroll, Count };

static float updatesPerSecond = 10.0f;
static int downscaleFactor = 1;
static float lineWidth = 5.0f;
static bool wireframeOn = false;
static bool pauseFlicker = false;
static bool disableEffect = false;
static glm::vec3 color{ 1, 1, 1 };
static bool rotateOn = false;
static float scrollSpeed = 1.0f;
static MeshType meshSelect = MeshType::Triangle;
static EffectType effectSelect = EffectType::Scroll;

static Camera camera;
static bool mouseDown = false;

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
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glPolygonOffset(-1.0, -1.0);
    glLineWidth(lineWidth);
}

static void InitImGui(GLFWwindow* win) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui::StyleColorsDark();
    ImGui::GetIO().IniFilename = nullptr;

    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
}

struct Resources {
    Shader objectShader{};
    Shader noiseInitShader{};
    Shader xorShader{};
    Shader scrollShader{};
    Shader postShader{};
    SimpleMesh quadMesh;
    SimpleMesh triMesh;
    SimpleMesh icosaMesh;
    SimpleMesh carMesh;
    Framebuffer objectFB;
    Framebuffer noiseFB1;
    Framebuffer noiseFB2;
};

static glm::vec2 scrollDir{ 0, -1 }; // TEMP FOR TESTING

static Resources SetupResources(WindowState& windowState) {
    Resources r;
    r.objectShader = Shader("shaders/basic.vert.glsl", "shaders/basic.frag.glsl");
    r.noiseInitShader = Shader("shaders/post.vert.glsl", "shaders/noise_init.frag.glsl");
    r.xorShader = Shader("shaders/post.vert.glsl", "shaders/xor.frag.glsl");
    r.scrollShader = Shader("shaders/post.vert.glsl", "shaders/mix.frag.glsl");
    r.postShader = Shader("shaders/post.vert.glsl", "shaders/post.frag.glsl");

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

    ImGui::Text("Effect:");
    ImGui::RadioButton("Flicker", (int*)&effectSelect, 0); ImGui::SameLine();
    ImGui::RadioButton("Scroll", (int*)&effectSelect, 1);
    ImGui::SliderFloat("Flicker Speed", &updatesPerSecond, 1.0f, 60.0f, "%.2f");
    ImGui::SliderFloat("Scroll Speed", &scrollSpeed, 0.0f, 10.0f, "%.0f");
    if (ImGui::SliderInt("Downscale", &downscaleFactor, 1, 8)) state.resized = true;
    ImGui::Checkbox("Pause", &pauseFlicker); ImGui::SameLine();
    ImGui::Checkbox("Disable", &disableEffect);

    ImGui::Separator();
    ImGui::Text("Mesh:");
    ImGui::RadioButton("Triangle", (int*)&meshSelect, 0); ImGui::SameLine();
    ImGui::RadioButton("Icosahedron", (int*)&meshSelect, 1); ImGui::SameLine();
    ImGui::RadioButton("Car", (int*)&meshSelect, 2);
    ImGui::Checkbox("Wireframe", &wireframeOn);
    if (ImGui::SliderFloat("Line Width", &lineWidth, 1.0f, 10.0f, "%.1f")) glLineWidth(lineWidth);
    ImGui::ColorEdit3("Color", &color[0]);
    ImGui::Checkbox("Rotate", &rotateOn);

    ImGui::Separator();
    ImGui::Text("Window: %dx%d", state.width, state.height);
    ImGui::Text("Noise: %dx%d", state.width / downscaleFactor, state.height / downscaleFactor);

    ImGui::Separator();
    ImGuiDirection2D("Scroll Direction", scrollDir, 30.0f);

    ImGui::End();
}

static SimpleMesh& GetMesh(MeshType meshSelect, Resources& r) {
    switch (meshSelect) {
    case MeshType::Triangle: return r.triMesh;
    case MeshType::Icosahedron: return r.icosaMesh;
    case MeshType::Car: return r.carMesh;
    default: assert(false); return r.triMesh;
    }
}

static void UpdateAndRenderObjects(WindowState& ws, Resources& r, float delta) {
    float aspect = (ws.height > 0) ? ((float)ws.width / (float)ws.height) : 1.0f;
    glm::mat4 vp = camera.GetProjection(aspect) * camera.GetView();

    static float angle = 90.0f;
    angle += 20.0f * delta;

    // TEMP FOR TESTING
    static float x = 0.0f;
    static bool movingRight = true;
    // if (movingRight) {
    //     x += 1.0f * delta;
    //     scrollDir = { 1, 0 };
    //     if (x >= 2.0f) movingRight = false;
    // }
    // else {
    //     x -= 1.0f * delta;
    //     scrollDir = { -1, 0 };
    //     if (x <= -2.0f) movingRight = true;
    // }

    glm::mat4 m = glm::mat4(1.0f);
    m = glm::translate(m, glm::vec3(x, 0.0f, 0.0f));
    if (meshSelect == MeshType::Car) m = glm::translate(m, glm::vec3(0.0f, -1.0f, 0.0f));
    if (rotateOn) m = glm::rotate(m, glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));

    // --------------

    r.objectFB.Bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    r.objectShader.Use();
    r.objectShader.SetMat4("mvp", vp * m);

    SimpleMesh& mesh = GetMesh(meshSelect, r);

    switch (effectSelect) {
    case EffectType::Flicker: {
        r.objectShader.SetVec3("color", color);
        if (wireframeOn) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            mesh.Draw();
        }
        else {
            mesh.Draw();
        }
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        break;
    }
    case EffectType::Scroll: {
        if (wireframeOn) {
            r.objectShader.SetVec3("color", glm::vec3(1));
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glEnable(GL_POLYGON_OFFSET_LINE);
            mesh.Draw();
        }
        r.objectShader.SetVec3("color", glm::vec3(1, 0, 0));
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_POLYGON_OFFSET_LINE);
        mesh.Draw();
        break;
    }
    default: assert(false);
    }

    glDisable(GL_DEPTH_TEST);
    Framebuffer::Unbind();
}

static bool ShouldApplyEffect(float delta) {
    if (pauseFlicker) return false;
    static float updateAccumulator = 0.0;
    updateAccumulator += delta;
    if (updateAccumulator >= 1.0f / updatesPerSecond) {
        updateAccumulator = 0.0f;
        return true;
    }
    return false;
}

static Shader& GetEffectShader(EffectType effectSelect, Resources& r) {
    switch (effectSelect) {
    case EffectType::Flicker: return r.xorShader;
    case EffectType::Scroll: return r.scrollShader;
    default: assert(false); return r.xorShader;
    }
}

static void RenderNoiseEffect(Resources& r, Framebuffer& prev, Framebuffer& next, float delta) {
    next.Bind();
    Shader& shader = GetEffectShader(effectSelect, r);
    shader.Use();
    shader.SetTexture2D("prevTex", prev.Texture(), 0);
    shader.SetTexture2D("objectTex", r.objectFB.Texture(), 1);
    shader.SetFloat("doXor", ShouldApplyEffect(delta) ? 1.0f : 0.0f);
    if (effectSelect == EffectType::Scroll)
    {
        shader.SetVec2("scrollDir", !pauseFlicker ? scrollDir * scrollSpeed : glm::vec2(0));
        shader.SetFloat("rand", (float)rand());
    }
    r.quadMesh.Draw();
    Framebuffer::Unbind();
}

static void PresentScene(WindowState& state, Resources& r, Framebuffer& src) {
    Framebuffer::BindDefault(state.width, state.height);
    r.postShader.Use();
    r.postShader.SetTexture2D("screenTex", src.Texture());
    r.postShader.SetVec2("resolution", glm::vec2(state.width, state.height));
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

        RenderNoiseEffect(res, *prevFB, *nextFB, delta);
        std::swap(prevFB, nextFB);

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

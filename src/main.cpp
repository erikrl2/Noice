#include "shader.hpp"
#include "framebuffer.hpp"
#include "mesh.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

float updatesPerSecond = 10.0f;
double updateAccumulator = 0.0;

struct WindowState {
    int width = 0;
    int height = 0;
    bool resized = false;
};

static void framebuffer_size_callback(GLFWwindow* window, int w, int h) {
    WindowState* s = reinterpret_cast<WindowState*>(glfwGetWindowUserPointer(window));
    if (!s) return;
    s->width = w;
    s->height = h;
    s->resized = true;
}

static GLFWwindow* InitWindow(int width, int height, const char* title) {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* win = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!win) {
        std::cerr << "Failed to create GLFW window\n";
        return nullptr;
    }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    return win;
}

static bool InitGlad() {
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "glad load failed\n";
        return false;
    }
    return true;
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
    SimpleMesh triMesh;
    SimpleMesh quadMesh;
    Framebuffer objectFB;
    Framebuffer noiseFB1;
    Framebuffer noiseFB2;
    int downscaleFactor = 1;
    int scaledWidth = 1;
    int scaledHeight = 1;
    float lineWidth = 10.0f;
    bool wireframeOn = true;
    Framebuffer* prevFB = nullptr;
    Framebuffer* nextFB = nullptr;
};

static Resources SetupResources(int windowWidth, int windowHeight) {
    Resources r;
    r.objectShader = Shader("shaders/basic.vert.glsl", "shaders/basic.frag.glsl");
    r.postShader = Shader("shaders/post.vert.glsl", "shaders/post.frag.glsl");
    r.xorShader = Shader("shaders/post.vert.glsl", "shaders/xor.frag.glsl");
    r.noiseInitShader = Shader("shaders/post.vert.glsl", "shaders/noise_init.frag.glsl");

    r.triMesh = SimpleMesh::CreateTriangle();
    r.quadMesh = SimpleMesh::CreateFullscreenQuad();

    r.objectFB.Create(windowWidth, windowHeight);

    r.downscaleFactor = 1;
    r.scaledWidth = windowWidth / r.downscaleFactor;
    r.scaledHeight = windowHeight / r.downscaleFactor;

    r.noiseFB1.Create(r.scaledWidth, r.scaledHeight);
    r.noiseFB2.Create(r.scaledWidth, r.scaledHeight);

    r.noiseFB1.Bind();
    r.noiseInitShader.Use();
    r.quadMesh.Draw();
    Framebuffer::Unbind();

    glLineWidth(r.lineWidth);
    glClearColor(0, 0, 0, 1);

    return r;
}

static void HandleResizeIfNeeded(WindowState& state, Resources& r) {
    if (!state.resized) return;

    r.scaledWidth = state.width / r.downscaleFactor;
    r.scaledHeight = state.height / r.downscaleFactor;

    r.objectFB.Resize(state.width, state.height);
    r.noiseFB1.Resize(r.scaledWidth, r.scaledHeight);
    r.noiseFB2.Resize(r.scaledWidth, r.scaledHeight);

    r.noiseFB1.Bind();
    r.noiseInitShader.Use();
    r.quadMesh.Draw();
    Framebuffer::Unbind();

    r.prevFB = &r.noiseFB1;
    r.nextFB = &r.noiseFB2;

    state.resized = false;
}

static bool ShouldPerformXor(double delta) {
    updateAccumulator += delta;
    double updateInterval = 1.0 / updatesPerSecond;
    if (updateAccumulator >= updateInterval) {
        updateAccumulator = 0.0;
        return true;
    }
    return false;
}

static void RenderGui(WindowState& state, Resources& r) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Settings");
    ImGui::SliderFloat("Speed", &updatesPerSecond, 1.0f, 60.0f);

    if (ImGui::SliderInt("Downscale", &r.downscaleFactor, 1, 8)) {
        state.resized = true;
    }

    ImGui::Checkbox("Wireframe", &r.wireframeOn);
    if (r.wireframeOn && ImGui::SliderFloat("Line Width", &r.lineWidth, 1.0f, 10.0f)) {
        glLineWidth(r.lineWidth);
    }

    ImGui::Text("Window: %dx%d", state.width, state.height);
    ImGui::Text("Noise: %dx%d", r.scaledWidth, r.scaledHeight);
    ImGui::End();
}

static void DoXorPass(Resources& r) {
    r.nextFB->Bind();
    r.xorShader.Use();
    r.xorShader.SetTexture2D("prevTex", r.prevFB->Texture(), 0);
    r.xorShader.SetTexture2D("objectTex", r.objectFB.Texture(), 1);
    r.quadMesh.Draw();
    Framebuffer::Unbind();

    std::swap(r.prevFB, r.nextFB);
}

int main() {
    WindowState windowState{ 800, 600 };

    GLFWwindow* win = InitWindow(windowState.width, windowState.height, "Noice");
    if (!win) return -1;

    if (!InitGlad()) return -1;

    InitImGui(win);

    Resources res = SetupResources(windowState.width, windowState.height);
    res.prevFB = &res.noiseFB1;
    res.nextFB = &res.noiseFB2;

    glfwSetWindowUserPointer(win, &windowState);
    glfwSetFramebufferSizeCallback(win, framebuffer_size_callback);

    while (!glfwWindowShouldClose(win)) {
        static double lastTime = glfwGetTime();
        double now = glfwGetTime();
        double delta = now - lastTime;
        lastTime = now;

        HandleResizeIfNeeded(windowState, res);

        RenderGui(windowState, res);

        res.objectFB.Bind();
        glClear(GL_COLOR_BUFFER_BIT);
        res.objectShader.Use();
        res.triMesh.Draw(res.wireframeOn);
        Framebuffer::Unbind();

        if (ShouldPerformXor(delta)) DoXorPass(res);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, windowState.width, windowState.height);
        res.postShader.Use();
        res.postShader.SetTexture2D("screenTex", res.prevFB->Texture());
        res.quadMesh.Draw();

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

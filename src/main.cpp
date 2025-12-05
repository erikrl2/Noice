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

#include <algorithm>
#include <iostream>

float updatesPerSecond = 10.0f;
double updateAccumulator = 0.0;

struct AppState {
    int width = 0;
    int height = 0;
    bool resized = false;
};

static void framebuffer_size_callback(GLFWwindow* window, int w, int h) {
    AppState* s = reinterpret_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!s) return;
    s->width = w;
    s->height = h;
    s->resized = true;
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    int windowWidth = 800;
    int windowHeight = 600;
    GLFWwindow* win = glfwCreateWindow(windowWidth, windowHeight, "Noice", nullptr, nullptr);
    glfwMakeContextCurrent(win);

    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "glad load failed\n";
        return -1;
    }

    float lineWidth = 10.0f;
    glLineWidth(lineWidth);
    glClearColor(0, 0, 0, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    Shader basic("shaders/basic.vert.glsl", "shaders/basic.frag.glsl");
    Shader post("shaders/post.vert.glsl", "shaders/post.frag.glsl");
    Shader xorShader("shaders/post.vert.glsl", "shaders/xor.frag.glsl");
    Shader noiseInit("shaders/post.vert.glsl", "shaders/noise_init.frag.glsl");

    SimpleMesh triMesh = SimpleMesh::CreateTriangle();
    SimpleMesh quadMesh = SimpleMesh::CreateFullscreenQuad();

    Framebuffer objectFB;
    objectFB.Create(windowWidth, windowHeight);

    int downscaleFactor = 1;
    int accumW = std::max(1, windowWidth / downscaleFactor);
    int accumH = std::max(1, windowHeight / downscaleFactor);

    Framebuffer accumA, accumB;
    accumA.Create(accumW, accumH);
    accumB.Create(accumW, accumH);

    AppState state{windowWidth, windowHeight, false};
    glfwSetWindowUserPointer(win, &state);
    glfwSetFramebufferSizeCallback(win, framebuffer_size_callback);

    accumA.Bind();
    noiseInit.use();
    quadMesh.Draw();
    Framebuffer::Unbind();

    Framebuffer* prev = &accumA;
    Framebuffer* next = &accumB;

    while (!glfwWindowShouldClose(win)) {
        static double lastTime = glfwGetTime();
        double now = glfwGetTime();
        double delta = now - lastTime;
        lastTime = now;

        if (state.resized) {
            windowWidth = std::max(1, state.width);
            windowHeight = std::max(1, state.height);
            accumW = std::max(1, windowWidth / downscaleFactor);
            accumH = std::max(1, windowHeight / downscaleFactor);

            objectFB.Resize(windowWidth, windowHeight);
            accumA.Resize(accumW, accumH);
            accumB.Resize(accumW, accumH);

            accumA.Bind();
            noiseInit.use();
            quadMesh.Draw();
            Framebuffer::Unbind();

            prev = &accumA;
            next = &accumB;

            state.resized = false;
        }

        updateAccumulator += delta;
        double updateInterval = 1.0 / updatesPerSecond;
        bool doXor = false;
        if (updateAccumulator >= updateInterval) {
            updateAccumulator = 0.0;
            doXor = true;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Settings");
        ImGui::SliderFloat("Speed", &updatesPerSecond, 1.0f, 60.0f);

        if (ImGui::SliderInt("Downscale", &downscaleFactor, 1, 8)) state.resized = true;

        static bool wireframeOn = true;
        ImGui::Checkbox("Wireframe", &wireframeOn);
        if (wireframeOn && ImGui::SliderFloat("Line Width", &lineWidth, 1.0f, 10.0f)) glLineWidth(lineWidth);

        ImGui::Text("Window: %dx%d", windowWidth, windowHeight);
        ImGui::Text("Noise: %dx%d", accumW, accumH);
        ImGui::End();

        objectFB.Bind();
        glClear(GL_COLOR_BUFFER_BIT);
        basic.use();
        triMesh.Draw(wireframeOn);
        Framebuffer::Unbind();

        if (doXor) {
            next->Bind();
            xorShader.use();
            xorShader.setTexture2D("prevTex", prev->Texture(), 0);
            xorShader.setTexture2D("objectTex", objectFB.Texture(), 1);
            quadMesh.Draw();
            Framebuffer::Unbind();

            std::swap(prev, next);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, windowWidth, windowHeight);
        post.use();
        post.setTexture2D("screenTex", prev->Texture());
        quadMesh.Draw();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
}

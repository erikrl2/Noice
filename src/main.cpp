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
enum class EffectType { Flicker = 0, Scroll, AdaptScroll, Count };

static EffectType effectSelect = EffectType::AdaptScroll;
static MeshType meshSelect = MeshType::Car;
static int downscaleFactor = 1;
static bool disableEffect = false;
static bool pauseEffect = false;
static float flickerSpeed = 30.0f;
static float scrollSpeed = 200.0f;
static glm::vec2 scrollDir{ 0, -1 };
static bool wireframeOn = false;
static float lineWidth = 5.0f;
static glm::vec3 color{ 1, 1, 1 };
static bool rotateOn = false;

static bool havePrevViewProj = false;
static glm::mat4 prevProj = glm::mat4(1.0f);
static glm::mat4 currProj = glm::mat4(1.0f);
static glm::mat4 prevView = glm::mat4(1.0f);
static glm::mat4 currView = glm::mat4(1.0f);
static glm::mat4 prevModel = glm::mat4(1.0f);
static glm::mat4 currModel = glm::mat4(1.0f);

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
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
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

    glClearColor(0, 0, 0, 0);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glPolygonOffset(-1.0, -1.0);
    glEnable(GL_POLYGON_OFFSET_LINE);
    glLineWidth(lineWidth);
}

static void InitImGui(GLFWwindow* win) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui::StyleColorsDark();
    ImGui::GetIO().IniFilename = nullptr;

    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 430 core");
}

struct Resources {
    Shader objectShader{};
    Shader noiseInitShader{};
    Shader flickerShader{};
    Shader scrollShader{};
    Shader adaptScrollShader{};
    Shader fillGapsShader{};
    Shader postShader{};
    SimpleMesh quadMesh;
    SimpleMesh triMesh;
    SimpleMesh icosaMesh;
    SimpleMesh carMesh;
    Framebuffer objectFB;
    Framebuffer prevObjDepthFB;
    Framebuffer objNormalFB; // TODO: rename to tangentFB
    Framebuffer noisePingFB;
    Framebuffer noisePongFB;
    Framebuffer scrollDirAccPingFB;
    Framebuffer scrollDirAccPongFB;
    Framebuffer lockFB; // NEU
};

static Resources SetupResources(WindowState& windowState) {
    Resources r;
    r.objectShader = Shader("shaders/basic.vert.glsl", "shaders/basic.frag.glsl");
    r.noiseInitShader = Shader("shaders/post.vert.glsl", "shaders/noise_init.frag.glsl");
    r.flickerShader = Shader("shaders/post.vert.glsl", "shaders/flicker.frag.glsl");
    r.scrollShader = Shader("shaders/post.vert.glsl", "shaders/simple_scroll.frag.glsl");
    r.adaptScrollShader = Shader::CreateCompute("shaders/adapt_scroll.comp.glsl");
    r.fillGapsShader = Shader::CreateCompute("shaders/fill_gaps.comp.glsl");
    r.postShader = Shader("shaders/post.vert.glsl", "shaders/post.frag.glsl");

    r.quadMesh = SimpleMesh::CreateFullscreenQuad();
    r.triMesh = SimpleMesh::CreateTriangle(); // currently unused
    r.icosaMesh = SimpleMesh::LoadFromOBJ("models/icosahedron.obj");
    r.carMesh = SimpleMesh::LoadFromOBJ("models/car.obj");

    // triggers correct setup in resize handler
    r.objectFB.hasDepth = r.prevObjDepthFB.hasDepth = r.objNormalFB.hasDepth = true;
    r.scrollDirAccPingFB.internalFormat = GL_RG16F;
    r.scrollDirAccPongFB.internalFormat = GL_RG16F;
    r.lockFB.internalFormat = GL_R32UI; // NEU
    return r;
}

static void ResetFramebuffers(WindowState& state, Resources& r) {
    int scaledWidth = state.width / downscaleFactor;
    int scaledHeight = state.height / downscaleFactor;

    r.objectFB.Resize(state.width, state.height);
    r.prevObjDepthFB.Resize(state.width, state.height);
    r.objNormalFB.Resize(state.width, state.height);
    r.noisePingFB.Resize(scaledWidth, scaledHeight);
    r.noisePongFB.Resize(scaledWidth, scaledHeight);
    r.scrollDirAccPingFB.Resize(scaledWidth, scaledHeight);
    r.scrollDirAccPongFB.Resize(scaledWidth, scaledHeight);
    r.lockFB.Resize(scaledWidth, scaledHeight); // NEU

    r.noiseInitShader.Use();
    r.noisePingFB.Bind();
    r.quadMesh.Draw();
    Framebuffer::Unbind();

    // Clear Accumulators
    r.scrollDirAccPingFB.Bind(true);
    r.scrollDirAccPongFB.Bind(true);
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
    ImGui::RadioButton("Scroll", (int*)&effectSelect, 1); ImGui::SameLine();
    ImGui::RadioButton("Adapt Scroll", (int*)&effectSelect, 2);
    static int refreshRate = glfwGetVideoMode(glfwGetPrimaryMonitor())->refreshRate;
    if (effectSelect != EffectType::AdaptScroll)
        ImGui::SliderFloat("Flicker Speed", &flickerSpeed, 1.0f, (float)refreshRate, "%.2f");
    if (effectSelect != EffectType::Flicker)
        ImGui::SliderFloat("Scroll Speed", &scrollSpeed, 0.0f, 400.0f, "%.0f");
    if (ImGui::SliderInt("Downscale", &downscaleFactor, 1, 8)) state.resized = true;
    ImGui::Checkbox("Pause", &pauseEffect); ImGui::SameLine();
    ImGui::Checkbox("Disable", &disableEffect);

    ImGui::Separator();
    ImGui::Text("Mesh:");
    //ImGui::RadioButton("Triangle", (int*)&meshSelect, 0); ImGui::SameLine();
    ImGui::RadioButton("Icosahedron", (int*)&meshSelect, 1); ImGui::SameLine();
    ImGui::RadioButton("Car", (int*)&meshSelect, 2);
    if (effectSelect != EffectType::AdaptScroll) {
        ImGui::Checkbox("Wireframe", &wireframeOn);
        if (ImGui::SliderFloat("Line Width", &lineWidth, 1.0f, 10.0f, "%.1f")) glLineWidth(lineWidth);
        ImGui::ColorEdit3("Color", &color[0]);
    }
    ImGui::Checkbox("Rotate", &rotateOn);

    ImGui::Separator();
    ImGui::Text("Window: %dx%d", state.width, state.height);
    ImGui::Text("Noise: %dx%d", state.width / downscaleFactor, state.height / downscaleFactor);

    if (effectSelect == EffectType::Scroll) {
        ImGui::Separator();
        ImGuiDirection2D("Scroll Direction", scrollDir, 30.0f);
    }

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

static void UpdateTransformMatrices(WindowState& ws, Resources& r, float delta) {
    float aspect = (ws.height > 0) ? ((float)ws.width / (float)ws.height) : 1.0f;
    glm::mat4 proj = camera.GetProjection(aspect);
    glm::mat4 view = camera.GetView();

    static float x = 0.0f;
    static float angle = 90.0f;
    angle += 20.0f * delta;

    glm::mat4 m = glm::mat4(1.0f);
    m = glm::translate(m, glm::vec3(x, 0.0f, 0.0f));
    if (meshSelect == MeshType::Car) {
        m = glm::translate(m, glm::vec3(0.0f, -1.0f, 0.0f));
        m = glm::rotate(m, glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    }
    if (rotateOn) m = glm::rotate(m, glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));

    if (havePrevViewProj) {
        prevProj = currProj;
        prevView = currView;
        prevModel = currModel;
    }
    else {
        prevProj = proj;
        prevView = view;
        prevModel = m;
        havePrevViewProj = true;
    }
    currProj = proj;
    currView = view;
    currModel = m;
}

static void UpdateAndRenderObjects(WindowState& ws, Resources& r, float delta) {
    UpdateTransformMatrices(ws, r, delta);

    BlitFramebufferDepth(r.objectFB, r.prevObjDepthFB);

    SimpleMesh& mesh = GetMesh(meshSelect, r);

    r.objectShader.Use();
    r.objectShader.SetMat4("viewproj", currProj * currView);
    r.objectShader.SetMat4("model", currModel);
    r.objectShader.SetInt("outputNormal", true);
    r.objectShader.SetMat4("normalMatrix", glm::transpose(glm::inverse(currModel)));

    SetDepthTest(true);
    r.objNormalFB.Bind(true);
    mesh.Draw();

    r.objectShader.SetInt("outputNormal", false);
    r.objectShader.SetVec3("color", color);

    r.objectFB.Bind(true);

    switch (effectSelect) {
    case EffectType::Flicker: {
        SetWireframeMode(wireframeOn);
        SetFaceCulling(!wireframeOn);
        mesh.Draw();
        SetFaceCulling(true);
        SetWireframeMode(false);
        break;
    }
    case EffectType::Scroll: {
        if (wireframeOn) {
            SetWireframeMode(true);
            mesh.Draw();
            SetWireframeMode(false);
        }
        [[fallthrough]];
    }
    case EffectType::AdaptScroll: {
        r.objectShader.SetVec3("color", glm::vec3(1, 0, 0));
        mesh.Draw();
        break;
    }
    default: assert(false);
    }

    Framebuffer::Unbind();
    SetDepthTest(false);
}

static bool ShouldApplyFlickerEffect(float delta) {
    if (pauseEffect) return false;
    static float updateAccumulator = 0.0;
    updateAccumulator += delta;
    if (updateAccumulator >= 1.0f / flickerSpeed) {
        updateAccumulator = 0.0f;
        return true;
    }
    return false;
}

static void RenderNoiseEffect(Resources& r, Framebuffer& prevNoise, Framebuffer& nextNoise, Framebuffer& prevScroll, Framebuffer& nextScroll, float delta) {
    if (effectSelect != EffectType::AdaptScroll) {
        Shader& shader = effectSelect == EffectType::Flicker ? r.flickerShader : r.scrollShader;
        shader.Use();
        shader.SetTexture2D("prevTex", prevNoise.Texture(), 0);
        shader.SetTexture2D("objectTex", r.objectFB.Texture(), 1);
        shader.SetInt("doXor", ShouldApplyFlickerEffect(delta));

        if (effectSelect == EffectType::Scroll) {
            static glm::vec2 scrollAccumulator{ 0.0f, 0.0f };
            if (!pauseEffect) scrollAccumulator += scrollDir * scrollSpeed * delta;
            glm::ivec2 intStep = glm::ivec2(glm::floor(scrollAccumulator));
            scrollAccumulator -= glm::vec2(intStep);

            shader.SetVec2("scrollOffset", glm::vec2(intStep.x, intStep.y));
            shader.SetFloat("rand", (float)rand());
        }

        nextNoise.Bind();
        r.quadMesh.Draw();
        Framebuffer::Unbind();
        return;
    }

    nextNoise.Bind(true); // @Copilot: "true" argument means to clear the framebuffer 
    nextScroll.Bind(true);
    Framebuffer::Unbind();

    // Clear Lock Texture
    r.lockFB.Bind();
    unsigned int val = 0xFFFFFFFF;
    glClearBufferuiv(GL_COLOR, 0, &val);
    Framebuffer::Unbind();

    r.adaptScrollShader.Use();

    r.adaptScrollShader.SetImage2D("prevNoiseTex", prevNoise.Texture(), 0, GL_READ_ONLY, prevNoise.internalFormat);
    r.adaptScrollShader.SetImage2D("currNoiseTex", nextNoise.Texture(), 1, GL_READ_WRITE, nextNoise.internalFormat);
    r.adaptScrollShader.SetImage2D("prevScrollAccTex", prevScroll.Texture(), 2, GL_READ_ONLY, prevScroll.internalFormat);
    r.adaptScrollShader.SetImage2D("currScrollAccTex", nextScroll.Texture(), 3, GL_READ_WRITE, nextScroll.internalFormat);
    r.adaptScrollShader.SetImage2D("lockTex", r.lockFB.Texture(), 4, GL_READ_WRITE, r.lockFB.internalFormat);

    r.adaptScrollShader.SetTexture2D("prevDepthTex", r.prevObjDepthFB.DepthTexture(), 8);
    r.adaptScrollShader.SetTexture2D("currDepthTex", r.objectFB.DepthTexture(), 5);
    r.adaptScrollShader.SetTexture2D("objectTex", r.objectFB.Texture(), 6);
    r.adaptScrollShader.SetTexture2D("tangentTex", r.objNormalFB.Texture(), 7);

    r.adaptScrollShader.SetMat4("prevModel", prevModel);
    r.adaptScrollShader.SetMat4("currModel", currModel);
    r.adaptScrollShader.SetMat4("invPrevModel", glm::inverse(prevModel));
    r.adaptScrollShader.SetMat4("invCurrModel", glm::inverse(currModel));
    r.adaptScrollShader.SetMat4("invPrevView", glm::inverse(prevView));
    r.adaptScrollShader.SetMat4("invPrevProj", glm::inverse(prevProj));
    r.adaptScrollShader.SetMat4("prevViewProj", prevProj * prevView);
    r.adaptScrollShader.SetMat4("currViewProj", currProj * currView);

    r.adaptScrollShader.SetVec2("fullResolution", glm::vec2(r.objectFB.width, r.objectFB.height));
    r.adaptScrollShader.SetInt("downscaleFactor", downscaleFactor);
    r.adaptScrollShader.SetFloat("normalScrollSpeed", scrollSpeed);
    r.adaptScrollShader.SetFloat("deltaTime", pauseEffect ? 0.0f : delta);

    int noiseWidth = prevNoise.width;
    int noiseHeight = prevNoise.height;
    unsigned int numGroupsX = (noiseWidth + 15) / 16;
    unsigned int numGroupsY = (noiseHeight + 15) / 16;

    r.adaptScrollShader.Dispatch(numGroupsX, numGroupsY, 1);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    r.fillGapsShader.Use();
    r.fillGapsShader.SetImage2D("noiseTex", nextNoise.Texture(), 0, GL_READ_WRITE, nextNoise.internalFormat);
    r.fillGapsShader.SetImage2D("currScrollAccTex", nextScroll.Texture(), 1, GL_READ_WRITE, nextScroll.internalFormat);
    r.fillGapsShader.SetFloat("rand", (float)rand());

    r.fillGapsShader.Dispatch(numGroupsX, numGroupsY, 1);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

static void PresentScene(WindowState& state, Resources& r, Framebuffer& src) {
    r.postShader.Use();
    r.postShader.SetTexture2D("screenTex", src.Texture());
    r.postShader.SetVec2("resolution", glm::vec2(state.width, state.height));
    Framebuffer::BindDefault(state.width, state.height);
    r.quadMesh.Draw();
}

int main() {
    WindowState windowState{ 1280, 720 };

    GLFWwindow* win = InitWindow(windowState, "Noice");
    if (!win) return -1;

    InitOpenGL();
    InitImGui(win);

    Resources res = SetupResources(windowState);

    Framebuffer* prevFB, * nextFB;
    Framebuffer* prevScrollFB, * nextScrollFB;
    windowState.resized = true;

    while (!glfwWindowShouldClose(win)) {
        float delta = ImGui::GetIO().DeltaTime;

        HandleCameraMovement(win, delta);

        if (windowState.resized) {
            ResetFramebuffers(windowState, res);
            prevFB = &res.noisePingFB;
            nextFB = &res.noisePongFB;
            prevScrollFB = &res.scrollDirAccPingFB;
            nextScrollFB = &res.scrollDirAccPongFB;
            windowState.resized = false;
            havePrevViewProj = false;
        }

        RenderSettingsWindow(windowState, res);
        UpdateAndRenderObjects(windowState, res, delta);
        RenderNoiseEffect(res, *prevFB, *nextFB, *prevScrollFB, *nextScrollFB, delta);
        std::swap(prevFB, nextFB);
        std::swap(prevScrollFB, nextScrollFB);

        //PresentScene(windowState, res, !disableEffect ? *prevFB : res.objectFB);
        PresentScene(windowState, res, !disableEffect ? *prevFB : *prevScrollFB);

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
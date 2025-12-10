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

// TODO: sort 
static float flickerSpeed = 30.0f;
static int downscaleFactor = 1;
static float lineWidth = 5.0f;
static bool wireframeOn = true;
static bool pauseFlicker = false;
static bool disableEffect = false;
static glm::vec3 color{ 1, 1, 1 };
static bool rotateOn = false;
static float scrollSpeed = 100.0f;
static MeshType meshSelect = MeshType::Triangle;
static EffectType effectSelect = EffectType::AdaptScroll;
static glm::vec2 scrollDir{ 0, -1 };
static glm::vec2 scrollAccumulator{ 0.0f, 0.0f };

static glm::mat4 prevViewProj = glm::mat4(1.0f);
static glm::mat4 currViewProj = glm::mat4(1.0f);
static glm::mat4 currProj = glm::mat4(1.0f);
static glm::mat4 currView = glm::mat4(1.0f);
static glm::mat4 prevProj = glm::mat4(1.0f);
static glm::mat4 prevView = glm::mat4(1.0f);
static glm::mat4 currModel = glm::mat4(1.0f);  // NEU: Current Model Matrix
static glm::mat4 prevModel = glm::mat4(1.0f);  // NEU: Previous Model Matrix
static bool havePrevViewProj = false;

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

static void CheckGLError(const char* label) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "OpenGL Error at " << label << ": 0x" << std::hex << err << std::dec << "\n";
    }
}

static void InitOpenGL() {
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    glClearColor(0, 0, 0, 1);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE); // TODO: make false and fix
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
    ImGui_ImplOpenGL3_Init("#version 430 core");
}

struct Resources {
    Shader objectShader{};
    Shader noiseInitShader{};
    Shader xorShader{};
    Shader scrollShader{};
    Shader adaptScrollShader{};
    Shader forwardScatterShader{};  // NEU: Compute Shader
    Shader fillGapsShader{};        // NEU: Compute Shader
    Shader postShader{};
    SimpleMesh quadMesh;
    SimpleMesh triMesh;
    SimpleMesh icosaMesh;
    SimpleMesh carMesh;
    Framebuffer objectFB;
    Framebuffer noiseFB1;
    Framebuffer noiseFB2;
    Framebuffer prevDepthFB;  // NEU: Speichere vorherigen Depth Buffer
};

static Resources SetupResources(WindowState& windowState) {
    Resources r;
    r.objectShader = Shader("shaders/basic.vert.glsl", "shaders/basic.frag.glsl");
    r.noiseInitShader = Shader("shaders/post.vert.glsl", "shaders/noise_init.frag.glsl");
    r.xorShader = Shader("shaders/post.vert.glsl", "shaders/xor.frag.glsl");
    r.scrollShader = Shader("shaders/post.vert.glsl", "shaders/mix.frag.glsl");
    r.adaptScrollShader = Shader("shaders/post.vert.glsl", "shaders/fix.frag.glsl");

    std::cout << "Loading forward scatter shader...\n";
    r.forwardScatterShader = Shader::CreateCompute("shaders/forward_scatter.comp.glsl");
    std::cout << "Forward scatter shader ID: " << r.forwardScatterShader.id << "\n";

    std::cout << "Loading fill gaps shader...\n";
    r.fillGapsShader = Shader::CreateCompute("shaders/fill_gaps.comp.glsl");
    std::cout << "Fill gaps shader ID: " << r.fillGapsShader.id << "\n";

    r.postShader = Shader("shaders/post.vert.glsl", "shaders/post.frag.glsl");

    r.quadMesh = SimpleMesh::CreateFullscreenQuad();
    r.triMesh = SimpleMesh::CreateTriangle();
    r.icosaMesh = SimpleMesh::LoadFromOBJ("models/icosahedron.obj");
    r.carMesh = SimpleMesh::LoadFromOBJ("models/car.obj");

    r.objectFB.Create(windowState.width, windowState.height, true);
    r.prevDepthFB.Create(windowState.width, windowState.height, true);

    // FIX: Setze Draw Buffer = NONE für prevDepthFB (nur Depth, kein Color)
    glBindFramebuffer(GL_FRAMEBUFFER, r.prevDepthFB.fbo);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return r;
}

static void ResetFramebuffers(WindowState& state, Resources& r) {
    int scaledWidth = state.width / downscaleFactor;
    int scaledHeight = state.height / downscaleFactor;

    r.objectFB.Resize(state.width, state.height);
    r.prevDepthFB.Resize(state.width, state.height);
    r.noiseFB1.Resize(scaledWidth, scaledHeight);
    r.noiseFB2.Resize(scaledWidth, scaledHeight);

    // Initialisiere BEIDE Noise-Buffer mit zufälligem Noise
    r.noiseFB1.Bind();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);  // Alpha = 1.0!
    glClear(GL_COLOR_BUFFER_BIT);
    r.noiseInitShader.Use();
    r.quadMesh.Draw();
    Framebuffer::Unbind();

    r.noiseFB2.Bind();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);  // Alpha = 1.0!
    glClear(GL_COLOR_BUFFER_BIT);
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
    ImGui::RadioButton("Scroll", (int*)&effectSelect, 1); ImGui::SameLine();
    ImGui::RadioButton("Adapt Scroll", (int*)&effectSelect, 2);
    ImGui::SliderFloat("Flicker Speed", &flickerSpeed, 1.0f, 60.0f, "%.2f");
    ImGui::SliderFloat("Scroll Speed", &scrollSpeed, 0.0f, 200.0f, "%.0f");
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

static void UpdateAndRenderObjects(WindowState& ws, Resources& r, float delta) {
    float aspect = (ws.height > 0) ? ((float)ws.width / (float)ws.height) : 1.0f;
    glm::mat4 proj = camera.GetProjection(aspect);
    glm::mat4 view = camera.GetView();
    glm::mat4 vp = proj * view;

    currProj = proj;
    currView = view;
    currViewProj = vp;

    static float angle = 90.0f;
    angle += 20.0f * delta;

    static float x = 0.0f; // for testing
    // move left and right
    static bool movingRight = true;
    if (movingRight) {
        x += 1.0f * delta;
        if (x >= 2.0f) movingRight = false;
    } else {
        x -= 1.0f * delta;
        if (x <= -2.0f) movingRight = true;
    }

    glm::mat4 m = glm::mat4(1.0f);
    m = glm::translate(m, glm::vec3(x, 0.0f, 0.0f));
    if (meshSelect == MeshType::Car) m = glm::translate(m, glm::vec3(0.0f, -1.0f, 0.0f));
    if (rotateOn) m = glm::rotate(m, glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));

    // NEU: Speichere Model-Matrix
    currModel = m;

    // --------------

    r.objectFB.Bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDepthMask(GL_TRUE);
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
    case EffectType::Scroll:
    case EffectType::AdaptScroll: {
        if (wireframeOn) {
            r.objectShader.SetVec3("color", color);
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
    //glDepthMask(GL_FALSE); //TODO: Fix
    Framebuffer::Unbind();
}

static bool ShouldApplyEffect(float delta) {
    if (pauseFlicker) return false;
    static float updateAccumulator = 0.0;
    updateAccumulator += delta;
    if (updateAccumulator >= 1.0f / flickerSpeed) {
        updateAccumulator = 0.0f;
        return true;
    }
    return false;
}

static Shader& GetEffectShader(EffectType effectSelect, Resources& r) {
    switch (effectSelect) {
    case EffectType::Flicker: return r.xorShader;
    case EffectType::Scroll: return r.scrollShader;
    case EffectType::AdaptScroll: return r.adaptScrollShader;
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

    switch (effectSelect) {
    case EffectType::Scroll: {
        glm::vec2 dir(0.0f);
        if (glm::length(scrollDir) > 0.0001f) dir = glm::normalize(scrollDir);

        glm::vec2 deltaPixels = !pauseFlicker ? dir * scrollSpeed * delta : glm::vec2(0.0f);

        scrollAccumulator += deltaPixels;
        glm::ivec2 intStep = glm::ivec2(glm::floor(scrollAccumulator));
        scrollAccumulator -= glm::vec2(intStep);

        shader.SetVec2("scrollOffset", glm::vec2(intStep.x, intStep.y));
        shader.SetFloat("rand", (float)rand());
        break;
    }
    case EffectType::AdaptScroll: {
        shader.SetTexture2D("objectDepthTex", r.objectFB.DepthTexture(), 2);
        shader.SetTexture2D("prevDepthTex", r.prevDepthFB.DepthTexture(), 3); // NEU!

        if (havePrevViewProj) {
            shader.SetMat4("prevViewProj", prevViewProj);
        }
        else {
            shader.SetMat4("prevViewProj", currViewProj);
        }

        shader.SetMat4("invCurrProj", glm::inverse(currProj));
        shader.SetMat4("invCurrView", glm::inverse(currView));
        shader.SetVec2("fullResolution", glm::vec2(r.objectFB.width, r.objectFB.height));
        shader.SetInt("downscaleFactor", downscaleFactor);
        shader.SetFloat("rand", (float)rand());
        break;
    }
    }

    r.quadMesh.Draw();
    Framebuffer::Unbind();
}

static void RenderNoiseEffectForward(Resources& r, Framebuffer& prev, Framebuffer& next, float delta) {
    if (effectSelect != EffectType::AdaptScroll) {
        RenderNoiseEffect(r, prev, next, delta);
        return;
    }

    if (!havePrevViewProj) {
        // Beim ersten Frame: Einfach vorheriges kopieren
        next.Bind();
        r.postShader.Use();
        r.postShader.SetTexture2D("screenTex", prev.Texture());
        r.postShader.SetVec2("resolution", glm::vec2(prev.width, prev.height));
        r.quadMesh.Draw();
        Framebuffer::Unbind();
        return;
    }

    // --- PASS 0: Clear next buffer (Alpha = 0 für Lücken) ---
    next.Bind();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    Framebuffer::Unbind();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // --- PASS 1: Forward Scatter ---
    r.forwardScatterShader.Use();

    r.forwardScatterShader.SetImage2D("prevNoiseTex", prev.Texture(), 0, GL_READ_ONLY);
    r.forwardScatterShader.SetImage2D("currNoiseTex", next.Texture(), 1, GL_READ_WRITE);

    r.forwardScatterShader.SetTexture2D("prevDepthTex", r.prevDepthFB.DepthTexture(), 2);
    r.forwardScatterShader.SetTexture2D("currDepthTex", r.objectFB.DepthTexture(), 3);
    r.forwardScatterShader.SetTexture2D("objectTex", r.objectFB.Texture(), 4);

    // NEU: Sende Model-Matrizen
    r.forwardScatterShader.SetMat4("prevModel", prevModel);
    r.forwardScatterShader.SetMat4("currModel", currModel);
    r.forwardScatterShader.SetMat4("invPrevModel", glm::inverse(prevModel));
    r.forwardScatterShader.SetMat4("invCurrModel", glm::inverse(currModel));

    r.forwardScatterShader.SetMat4("prevViewProj", prevViewProj);
    r.forwardScatterShader.SetMat4("invPrevProj", glm::inverse(prevProj));
    r.forwardScatterShader.SetMat4("invPrevView", glm::inverse(prevView));
    r.forwardScatterShader.SetMat4("currViewProj", currViewProj);
    r.forwardScatterShader.SetVec2("fullResolution", glm::vec2(r.objectFB.width, r.objectFB.height));
    r.forwardScatterShader.SetInt("downscaleFactor", downscaleFactor);

    int noiseWidth = prev.width;
    int noiseHeight = prev.height;
    unsigned int numGroupsX = (noiseWidth + 15) / 16;
    unsigned int numGroupsY = (noiseHeight + 15) / 16;

    r.forwardScatterShader.Dispatch(numGroupsX, numGroupsY, 1);
    CheckGLError("After forward scatter");

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    // --- PASS 2: Fill Gaps ---
    r.fillGapsShader.Use();
    r.fillGapsShader.SetImage2D("noiseTex", next.Texture(), 0, GL_READ_WRITE);
    r.fillGapsShader.SetTexture2D("objectTex", r.objectFB.Texture(), 1);
    r.fillGapsShader.SetFloat("rand", (float)rand());

    r.fillGapsShader.Dispatch(numGroupsX, numGroupsY, 1);
    CheckGLError("After fill gaps");

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
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

    Framebuffer* prevFB, * nextFB;
    windowState.resized = true;

    while (!glfwWindowShouldClose(win)) {
        float delta = ImGui::GetIO().DeltaTime;

        HandleCameraMovement(win, delta);

        if (windowState.resized) {
            ResetFramebuffers(windowState, res);
            prevFB = &res.noiseFB1;
            nextFB = &res.noiseFB2;
            windowState.resized = false;
            havePrevViewProj = false;

            // WICHTIG: Initialisiere prev* mit aktuellen Werten
            float aspect = (windowState.height > 0) ? ((float)windowState.width / (float)windowState.height) : 1.0f;
            prevProj = currProj = camera.GetProjection(aspect);
            prevView = currView = camera.GetView();
            prevViewProj = currViewProj = prevProj * prevView;
            prevModel = currModel = glm::mat4(1.0f);
        }

        RenderSettingsWindow(windowState, res);

        // ======== FIX: Update Previous Matrizen VOR dem Rendering! ========
        // So gehören prev* Matrizen und prevDepthFB zum GLEICHEN Frame

        // 1. Speichere Current -> Previous (BEVOR wir neue Werte berechnen)
        if (havePrevViewProj) {
            prevViewProj = currViewProj;
            prevView = currView;
            prevProj = currProj;
            prevModel = currModel;
        }

        // 2. Update View/Proj/Model für AKTUELLEN Frame
        UpdateAndRenderObjects(windowState, res, delta);

        // 3. Kopiere Depth NACH dem Rendering, BEVOR wir das Noise rendern
        // Jetzt ist prevDepthFB synchron mit prevModel/prevViewProj!
        if (havePrevViewProj) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, res.objectFB.fbo);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, res.prevDepthFB.fbo);
            glBlitFramebuffer(
                0, 0, res.objectFB.width, res.objectFB.height,
                0, 0, res.prevDepthFB.width, res.prevDepthFB.height,
                GL_DEPTH_BUFFER_BIT, GL_NEAREST
            );
            Framebuffer::Unbind();
        }
        else {
            // Beim ersten Frame: Initialisiere prevDepthFB
            glBindFramebuffer(GL_READ_FRAMEBUFFER, res.objectFB.fbo);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, res.prevDepthFB.fbo);
            glBlitFramebuffer(
                0, 0, res.objectFB.width, res.objectFB.height,
                0, 0, res.prevDepthFB.width, res.prevDepthFB.height,
                GL_DEPTH_BUFFER_BIT, GL_NEAREST
            );
            Framebuffer::Unbind();
            havePrevViewProj = true;
        }

        // 4. Render Noise Effect (verwendet prev* vom LETZTEN Frame)
        if (effectSelect == EffectType::AdaptScroll) {
            RenderNoiseEffectForward(res, *prevFB, *nextFB, delta);
        }
        else {
            RenderNoiseEffect(res, *prevFB, *nextFB, delta);
        }
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

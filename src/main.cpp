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

enum class MeshType { Car, Spider, Dragon, Alien, Count };

// TODO: clean up global state -> put in local struct
static MeshType meshSelect = MeshType::Car;
static int downscaleFactor = 1;
static bool disableEffect = false;
static float scrollSpeed = 150.0f;
static int accResetInterval = 10;

// TODO: clean up global state -> put in local struct
static bool havePrevViewProj = false;
static glm::mat4 prevProj = glm::mat4(1.0f);
static glm::mat4 currProj = glm::mat4(1.0f);
static glm::mat4 prevView = glm::mat4(1.0f);
static glm::mat4 currView = glm::mat4(1.0f);
static glm::mat4 prevModel = glm::mat4(1.0f);
static glm::mat4 currModel = glm::mat4(1.0f);

// TODO: clean up global state -> put in local struct
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
    if (ImGui::GetIO().WantCaptureMouse) return;

    switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT: {
        if (action == GLFW_PRESS) {
            mouseDown = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else if (action == GLFW_RELEASE) {
            mouseDown = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        break;
    }
    case GLFW_MOUSE_BUTTON_RIGHT: {
        if (action == GLFW_PRESS) {
            disableEffect = !disableEffect;
        }
        break;
    }
    }
}

static void ProcessCameraMovement(GLFWwindow* win, float dt) {
    if (ImGui::GetIO().WantCaptureKeyboard) return;

    float camSpeed = 5.0f * dt;
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
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE); // TODO: make debug config only

    GLFWwindow* win = glfwCreateWindow(ws.width, ws.height, title, nullptr, nullptr);
    if (!win) { std::cerr << "Failed to create GLFW window\n"; return nullptr; }
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

    EnableOpenGLDebugOutput(); // TODO: make debug config only

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
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
    Shader objectShader;
    Shader scrollShader;
    Shader fillShader;
    Shader postShader;
    SimpleMesh quadMesh;
    SimpleMesh carMesh;
    Framebuffer objectFB;
    Texture prevDepthTex;
    Texture currNoiseTex;
    Texture prevNoiseTex;
    Texture currAccTex;
    Texture prevAccTex;
};

static void SetupResources(Resources& r, const WindowState& ws) {
    r.objectShader.Create("shaders/basic.vert.glsl", "shaders/basic.frag.glsl");
    r.scrollShader.CreateCompute("shaders/adapt_scroll.comp.glsl");
    r.fillShader.CreateCompute("shaders/fill_gaps.comp.glsl");
    r.postShader.Create("shaders/post.vert.glsl", "shaders/post.frag.glsl");

    r.quadMesh = SimpleMesh::CreateFullscreenQuad();
    r.carMesh = SimpleMesh::LoadFromOBJ("models/jeep_t.obj");
    //r.carMesh = SimpleMesh::CreateTriangle();

    r.objectFB.Create(ws.width, ws.height, GL_RG16F, GL_LINEAR, true);
    r.prevDepthTex.Create(ws.width, ws.height, GL_DEPTH_COMPONENT24, GL_LINEAR);

    int scaledWidth = ws.width / downscaleFactor;
    int scaledHeight = ws.height / downscaleFactor;
    r.currNoiseTex.Create(scaledWidth, scaledHeight, GL_RG8, GL_NEAREST);
    r.prevNoiseTex.Create(scaledWidth, scaledHeight, GL_RG8, GL_NEAREST);
    r.currAccTex.Create(scaledWidth, scaledHeight, GL_RG16F, GL_NEAREST);
    r.prevAccTex.Create(scaledWidth, scaledHeight, GL_RG16F, GL_NEAREST);

    r.currNoiseTex.Clear(); r.prevNoiseTex.Clear();
    r.currAccTex.Clear(); r.prevAccTex.Clear();
}

static void ResetFramebuffers(WindowState& state, Resources& r) {
    int scaledWidth = state.width / downscaleFactor;
    int scaledHeight = state.height / downscaleFactor;
    r.objectFB.Resize(state.width, state.height);
    r.prevDepthTex.Resize(state.width, state.height);
    r.currNoiseTex.Resize(scaledWidth, scaledHeight);
    r.prevNoiseTex.Resize(scaledWidth, scaledHeight);
    r.currAccTex.Resize(scaledWidth, scaledHeight);
    r.prevAccTex.Resize(scaledWidth, scaledHeight);

    r.currNoiseTex.Clear(); r.prevNoiseTex.Clear();
    r.currAccTex.Clear(); r.prevAccTex.Clear();
}

static void RenderSettingsWindow(WindowState& state) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (!ImGui::Begin("Settings", 0, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    ImGui::SliderFloat("Speed (px/s)", &scrollSpeed, 0.0f, 500.0f, "%.0f");
    ImGui::SliderInt("Reset Interval", &accResetInterval, 1, 100);
    if (ImGui::SliderInt("Downscale", &downscaleFactor, 1, 8)) state.resized = true;
    ImGui::Checkbox("Disable", &disableEffect);

    ImGui::Separator();
    ImGui::Text("Mesh:");
    ImGui::RadioButton("Car", (int*)&meshSelect, (int)MeshType::Car);

    ImGui::Separator();
    ImGui::Text("Window: %dx%d", state.width, state.height);
    ImGui::Text("Noise: %dx%d", state.width / downscaleFactor, state.height / downscaleFactor);

    ImGui::End();
}

static SimpleMesh& GetMesh(MeshType meshSelect, Resources& r) {
    switch (meshSelect) {
    case MeshType::Car: return r.carMesh;
    default: return r.carMesh;
    }
}

static void UpdateTransformMatrices(WindowState& ws, float dt) {
    float aspect = (ws.height > 0) ? ((float)ws.width / (float)ws.height) : 1.0f;
    glm::mat4 proj = camera.GetProjection(aspect);
    glm::mat4 view = camera.GetView();
    glm::mat4 m = glm::mat4(1.0f);

    m = glm::translate(m, glm::vec3(0.0f, -1.0f, 0.0f));
    m = glm::rotate(m, glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));

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

static void RenderObject(Resources& r, float dt) {
    r.objectShader.Use();
    r.objectShader.SetMat4("viewproj", currProj * currView);
    r.objectShader.SetMat4("model", currModel);
    r.objectShader.SetMat4("normalMatrix", glm::transpose(glm::inverse(currModel)));

    r.objectFB.Clear();

    SimpleMesh& mesh = GetMesh(meshSelect, r);
    mesh.Draw(RenderFlag::DepthTest);
}

static void RenderEffect(Resources& r, float dt) {
    static unsigned frameCount = 0;
    if (++frameCount % accResetInterval == 0) r.prevAccTex.Clear();

    r.scrollShader.Use();
    r.scrollShader.SetImage2D("currNoiseTex", r.currNoiseTex, 0, GL_WRITE_ONLY);
    r.scrollShader.SetImage2D("prevNoiseTex", r.prevNoiseTex, 1, GL_READ_ONLY);
    r.scrollShader.SetImage2D("currAccTex", r.currAccTex, 2, GL_WRITE_ONLY);
    r.scrollShader.SetImage2D("prevAccTex", r.prevAccTex, 3, GL_READ_WRITE);
    r.scrollShader.SetTexture2D("flowTex", r.objectFB.tex, 4);
    r.scrollShader.SetTexture2D("depthTex", r.objectFB.depthTex, 5);
    r.scrollShader.SetFloat("scrollSpeed", scrollSpeed * dt);
    r.scrollShader.DispatchCompute(r.currNoiseTex.width, r.currNoiseTex.height, 16);

    r.fillShader.Use();
    r.fillShader.SetImage2D("currNoiseTex", r.currNoiseTex, 0, GL_READ_WRITE);
    r.fillShader.SetImage2D("prevNoiseTex", r.prevNoiseTex, 1, GL_WRITE_ONLY);
    r.fillShader.SetImage2D("currAccTex", r.currAccTex, 2, GL_WRITE_ONLY);
    r.fillShader.SetImage2D("prevAccTex", r.prevAccTex, 3, GL_READ_WRITE);
    r.fillShader.SetFloat("seed", (float)glfwGetTime());
    r.fillShader.DispatchCompute(r.currNoiseTex.width, r.currNoiseTex.height, 16);


    // ---------


    //r.scrollShader.Use();

    //r.scrollShader.SetImage2D("prevNoiseTex", prev.Texture(), 0, GL_READ_ONLY);
    //r.scrollShader.SetImage2D("currNoiseTex", next.Texture(), 1, GL_READ_WRITE);

    //r.scrollShader.SetTexture2D("prevDepthTex", r.prevDepthFB.DepthTexture(), 2);
    //r.scrollShader.SetTexture2D("currDepthTex", r.objectFB.DepthTexture(), 3);
    //r.scrollShader.SetTexture2D("objectTex", r.objectFB.Texture(), 4);
    //r.scrollShader.SetTexture2D("tangentTex", r.objNormalFB.Texture(), 5);

    //r.scrollShader.SetMat4("prevModel", prevModel);
    //r.scrollShader.SetMat4("currModel", currModel);
    //r.scrollShader.SetMat4("invPrevModel", glm::inverse(prevModel));
    //r.scrollShader.SetMat4("invCurrModel", glm::inverse(currModel));
    //r.scrollShader.SetMat4("invPrevView", glm::inverse(prevView));
    //r.scrollShader.SetMat4("invPrevProj", glm::inverse(prevProj));
    //r.scrollShader.SetMat4("prevViewProj", prevProj * prevView);
    //r.scrollShader.SetMat4("currViewProj", currProj * currView);

    //r.scrollShader.SetVec2("fullResolution", glm::vec2(r.objectFB.width, r.objectFB.height));
    //r.scrollShader.SetInt("downscaleFactor", downscaleFactor);
    //r.scrollShader.SetFloat("normalScrollSpeed", scrollSpeed + 100.0f);
    //r.scrollShader.SetFloat("deltaTime", pauseEffect ? 0.0f : dt);

    //int noiseWidth = prev.width;
    //int noiseHeight = prev.height;

    //r.scrollShader.DispatchCompute(noiseWidth, noiseHeight, 16);

    //r.fillShader.Use();
    //r.fillShader.SetImage2D("noiseTex", next.Texture(), 0, GL_READ_WRITE);
    //r.fillShader.SetFloat("rand", (float)rand());

    //r.fillShader.DispatchCompute(noiseWidth, noiseHeight, 16);
}

static void PresentScene(WindowState& state, Resources& r) {
    r.postShader.Use();
    r.postShader.SetTexture2D("screenTex", !disableEffect ? r.currNoiseTex : r.objectFB.tex);
    r.postShader.SetVec2("resolution", glm::vec2(state.width, state.height));
    r.postShader.SetInt("showFlow", disableEffect); // rename uniform
    Framebuffer::BindDefault(state.width, state.height);
    r.quadMesh.Draw();
}

int main() {
    WindowState ws{ 1280, 720 };

    GLFWwindow* win = InitWindow(ws, "Noice");
    if (!win) return -1;

    InitOpenGL();
    InitImGui(win);

    Resources res;
    SetupResources(res, ws);

    while (!glfwWindowShouldClose(win)) {
        float dt = ImGui::GetIO().DeltaTime;

        if (ws.resized) {
            ResetFramebuffers(ws, res);
            ws.resized = false;
            havePrevViewProj = false;
        }

        ProcessCameraMovement(win, dt);
        UpdateTransformMatrices(ws, dt);

        RenderSettingsWindow(ws);
        RenderObject(res, dt);
        RenderEffect(res, dt);

        PresentScene(ws, res);

        res.objectFB.SwapDepthTex(res.prevDepthTex);
        res.currNoiseTex.Swap(res.prevNoiseTex);
        res.currAccTex.Swap(res.prevAccTex);

        // TODO: put in function
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    // TODO: put in function
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}

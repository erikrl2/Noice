#include "shader.hpp"
#include "framebuffer.hpp"
#include "mesh.hpp"
#include "camera.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

static int width = 800;
static int height = 800;

static Camera camera;
static bool mouseDown = false;

static float scrollSpeed = 150.0f;
static bool showFlow = false;

static void OnMouseMoved(GLFWwindow* window, double xpos, double ypos) {
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
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            mouseDown = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else if (action == GLFW_RELEASE) {
            mouseDown = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
        if (action == GLFW_PRESS) showFlow = !showFlow;
    }
}

static void ProcessKeyboardInput(GLFWwindow* win, float delta) {
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

    if (glfwGetKey(win, GLFW_KEY_UP) == GLFW_PRESS) scrollSpeed += 0.5f;
    if (glfwGetKey(win, GLFW_KEY_DOWN) == GLFW_PRESS) scrollSpeed = glm::max(0.0f, scrollSpeed - 0.5f);
}

struct Resources {
    Shader flowShader;
    Shader adaptScroll;
    Shader fillGaps;
    Shader post;

    SimpleMesh object;
    SimpleMesh quad;

    Framebuffer flowFB;
    Framebuffer noisePing;
    Framebuffer noisePong;
    Framebuffer accPing;
    Framebuffer accPong;
};

static void InitResources(Resources& r) {
    r.flowShader = Shader("shaders/basic.vert.glsl", "shaders/basic.frag.glsl");
    r.adaptScroll = Shader::CreateCompute("shaders/adapt_scroll.comp.glsl");
    r.fillGaps = Shader::CreateCompute("shaders/fill_gaps.comp.glsl");
    r.post = Shader("shaders/post.vert.glsl", "shaders/post.frag.glsl");

    //r.object = SimpleMesh::CreateTriangle();
    r.object = SimpleMesh::LoadFromOBJ("models/jeep.obj");
    r.quad = SimpleMesh::CreateFullscreenQuad();

    r.flowFB.Create(width, height, true, GL_RG16F);
    r.noisePing.Create(width, height, false, GL_RG8);
    r.noisePong.Create(width, height, false, GL_RG8);
    r.accPing.Create(width, height, false, GL_RG16F);
    r.accPong.Create(width, height, false, GL_RG16F);

    r.noisePing.Clear(); r.noisePong.Clear();
    r.accPing.Clear(); r.accPong.Clear();
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* win = glfwCreateWindow(width, height, "Minimal Debug", nullptr, nullptr);
    glfwMakeContextCurrent(win);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSwapInterval(1);
    glfwSetCursorPosCallback(win, OnMouseMoved);
    glfwSetMouseButtonCallback(win, OnMouseClicked);

    Resources res;
    InitResources(res);

    Framebuffer* prevNoise = &res.noisePing;
    Framebuffer* nextNoise = &res.noisePong;
    Framebuffer* prevAcc = &res.accPing;
    Framebuffer* nextAcc = &res.accPong;

    while (!glfwWindowShouldClose(win)) {
        static double lastTime = glfwGetTime();
        double dt = glfwGetTime() - lastTime;
        lastTime = glfwGetTime();

        ProcessKeyboardInput(win, dt);

        float aspect = (height > 0) ? ((float)width / (float)height) : 1.0f;
        glm::mat4 proj = camera.GetProjection(aspect);
        glm::mat4 view = camera.GetView();
        glm::mat4 m = glm::mat4(1.0f);
        m = glm::translate(m, glm::vec3(0.0f, -1.0f, 0.0f));
        m = glm::rotate(m, glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        // 1. Render: Flow Field
        res.flowShader.Use();
        res.flowShader.SetMat4("viewproj", proj * view);
        res.flowShader.SetMat4("model", m);
        res.flowShader.SetMat4("normalMatrix", glm::transpose(glm::inverse(m)));

        res.flowFB.Clear();
        res.object.Draw(DepthTest | FaceCulling);

        // 2. Compute: Adapt Scroll 
        nextNoise->Clear();
        nextAcc->Clear();

        static unsigned frameCount = 0;
        if (++frameCount % 10 == 0) prevAcc->Clear();

        res.adaptScroll.Use();
        res.adaptScroll.SetImage2D("currNoiseTex", nextNoise->Texture(), 0, nextNoise->internalFormat, GL_WRITE_ONLY);
        res.adaptScroll.SetImage2D("currAccTex", nextAcc->Texture(), 1, nextAcc->internalFormat, GL_WRITE_ONLY);
        res.adaptScroll.SetImage2D("prevAccTex", prevAcc->Texture(), 2, prevAcc->internalFormat);
        res.adaptScroll.SetTexture2D("prevNoiseTex", prevNoise->Texture(), 3);
        res.adaptScroll.SetTexture2D("flowTex", res.flowFB.Texture(), 4);
        res.adaptScroll.SetTexture2D("depthTex", res.flowFB.DepthTexture(), 5);
        res.adaptScroll.SetFloat("scrollSpeed", scrollSpeed * dt);

        res.adaptScroll.Dispatch((width + 15) / 16, (height + 15) / 16);

        // 3. Compute: Fill Gaps
        res.fillGaps.Use();
        res.fillGaps.SetImage2D("currNoiseTex", nextNoise->Texture(), 0, nextNoise->internalFormat);
        res.fillGaps.SetImage2D("currAccTex", nextAcc->Texture(), 1, nextAcc->internalFormat, GL_WRITE_ONLY);
        res.fillGaps.SetTexture2D("prevAccTex", prevAcc->Texture(), 2);
        res.fillGaps.SetFloat("seed", (float)glfwGetTime());

        res.fillGaps.Dispatch((width + 15) / 16, (height + 15) / 16);

        // 4. Present
        res.post.Use();
        res.post.SetTexture2D("screenTex", (!showFlow ? nextNoise : &res.flowFB)->Texture());
        res.post.SetVec2("resolution", glm::vec2(width, height));
        res.post.SetInt("showFlow", showFlow);

        Framebuffer::BindDefault(width, height);
        res.quad.Draw();

        std::swap(prevNoise, nextNoise);
        std::swap(prevAcc, nextAcc);

        glfwSwapBuffers(win);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}
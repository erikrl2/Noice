#include "shader.hpp"
#include "framebuffer.hpp"
#include "mesh.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

static int width = 800;
static int height = 800;

struct Resources {
    Shader flowShader;
    Shader adaptScroll;
    Shader fillGaps;
    Shader post;

    SimpleMesh triangle;
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

    r.triangle = SimpleMesh::CreateTriangle();
    r.quad = SimpleMesh::CreateFullscreenQuad();

    r.flowFB.Create(width, height, true, GL_RG16F);
    r.noisePing.Create(width, height, false, GL_RG8);
    r.noisePong.Create(width, height, false, GL_RG8);
    r.accPing.Create(width, height, false, GL_RG16F);
    r.accPong.Create(width, height, false, GL_RG16F);

    r.noisePing.Bind(true); r.noisePong.Bind(true);
    r.accPing.Bind(true); r.accPong.Bind(true);
    Framebuffer::Unbind();
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

    Resources res;
    InitResources(res);

    Framebuffer* prevNoise = &res.noisePing;
    Framebuffer* nextNoise = &res.noisePong;
    Framebuffer* prevAcc = &res.accPing;
    Framebuffer* nextAcc = &res.accPong;

    while (!glfwWindowShouldClose(win)) {
        // 1. Render: Flow Field
        res.flowShader.Use();
        res.flowShader.SetVec2("flowDir", glm::vec2(0.8f, -1.0f));
        res.flowFB.Bind(true);
        glEnable(GL_DEPTH_TEST);
        res.triangle.Draw();
        glDisable(GL_DEPTH_TEST);
        Framebuffer::Unbind();

        // 2. Compute: Adapt Scroll
        nextNoise->Bind(true);
        nextAcc->Bind(true);
        Framebuffer::Unbind();

        res.adaptScroll.Use();
        res.adaptScroll.SetImage2D("prevNoiseTex", prevNoise->Texture(), 0, GL_READ_ONLY, GL_RG8);
        res.adaptScroll.SetImage2D("currNoiseTex", nextNoise->Texture(), 1, GL_READ_WRITE, GL_RG8);
        res.adaptScroll.SetImage2D("prevAccTex", prevAcc->Texture(), 2, GL_READ_WRITE, GL_RG16F);
        res.adaptScroll.SetImage2D("currAccTex", nextAcc->Texture(), 3, GL_READ_WRITE, GL_RG16F);
        res.adaptScroll.SetTexture2D("flowTex", res.flowFB.Texture(), 4);
        res.adaptScroll.SetTexture2D("depthTex", res.flowFB.DepthTexture(), 5);
        res.adaptScroll.SetFloat("scrollSpeed", 1.35f);

        res.adaptScroll.Dispatch((width + 15) / 16, (height + 15) / 16, 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);

        // 3. Compute: Fill Gaps
        res.fillGaps.Use();
        res.fillGaps.SetImage2D("currNoiseTex", nextNoise->Texture(), 0, GL_READ_WRITE, GL_RG8);
        res.fillGaps.SetImage2D("currAccTex", nextAcc->Texture(), 1, GL_READ_WRITE, GL_RG16F);
        res.fillGaps.SetImage2D("prevAccTex", prevAcc->Texture(), 2, GL_READ_ONLY, GL_RG16F);
        res.fillGaps.SetFloat("rand", (float)glfwGetTime());

        res.fillGaps.Dispatch((width + 15) / 16, (height + 15) / 16, 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);

        // 4. Present
        res.post.Use();
        res.post.SetTexture2D("screenTex", (true ? nextNoise : nextAcc)->Texture());
        res.post.SetVec2("resolution", glm::vec2(width, height));

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
#include "shader.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

float quadVertices[] = {
    -1.0f,-1.0f, 0.0f,0.0f,
     1.0f,-1.0f, 1.0f,0.0f,
     1.0f, 1.0f, 1.0f,1.0f,
    -1.0f, 1.0f, 0.0f,1.0f
};
unsigned int quadIdx[] = { 0,1,2,2,3,0 };

float triVertices[] = {
    -0.5f,-0.5f,0.0f,
     0.5f,-0.5f,0.0f,
     0.0f, 0.5f,0.0f
};

float updatesPerSecond = 10.0f;
double updateAccumulator = 0.0;

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(800, 600, "Noice", nullptr, nullptr);
    glfwMakeContextCurrent(win);

    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "glad load failed\n";
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    Shader basic("shaders/basic.vert.glsl", "shaders/basic.frag.glsl");
    Shader post("shaders/post.vert.glsl", "shaders/post.frag.glsl");
    Shader xorShader("shaders/post.vert.glsl", "shaders/xor.frag.glsl");
    Shader noiseInit("shaders/post.vert.glsl", "shaders/noise_init.frag.glsl");

    GLuint triVAO, vbo;
    glGenVertexArrays(1, &triVAO);
    glBindVertexArray(triVAO);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(triVertices), triVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    GLuint quadVAO, qvbo, qebo;
    glGenVertexArrays(1, &quadVAO);
    glBindVertexArray(quadVAO);
    glGenBuffers(1, &qvbo);
    glBindBuffer(GL_ARRAY_BUFFER, qvbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glGenBuffers(1, &qebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, qebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIdx), quadIdx, GL_STATIC_DRAW);

    // create object FBO
    GLuint objectFBO, objectTex;
    glGenFramebuffers(1, &objectFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, objectFBO);

    glGenTextures(1, &objectTex);
    glBindTexture(GL_TEXTURE_2D, objectTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 800, 600, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, objectTex, 0);

    // create accumulation ping-pong FBOs
    GLuint accumFBO_A, accumFBO_B;
    GLuint accumTex_A, accumTex_B;

    auto createAccumTex = [&](GLuint& tex) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 800, 600, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        };

    createAccumTex(accumTex_A);
    createAccumTex(accumTex_B);

    glGenFramebuffers(1, &accumFBO_A);
    glBindFramebuffer(GL_FRAMEBUFFER, accumFBO_A);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, accumTex_A, 0);

    glGenFramebuffers(1, &accumFBO_B);
    glBindFramebuffer(GL_FRAMEBUFFER, accumFBO_B);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, accumTex_B, 0);

    // initialize noise into accumTex_A
    glBindFramebuffer(GL_FRAMEBUFFER, accumFBO_A);
    glViewport(0, 0, 800, 600);
    noiseInit.use();
    glBindVertexArray(quadVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // make accumTex_A the active buffer
    GLuint prevTex = accumTex_A;
    GLuint nextTex = accumTex_B;

    while (!glfwWindowShouldClose(win)) {
        static double lastTime = glfwGetTime();
        double now = glfwGetTime();
        double delta = now - lastTime;
        lastTime = now;

        updateAccumulator += delta;
        double updateInterval = 1.0 / updatesPerSecond;
        bool doXor = false;
        if (updateAccumulator >= updateInterval) {
            updateAccumulator -= updateInterval;
            doXor = true;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Settings");
        ImGui::SliderFloat("Speed", &updatesPerSecond, 1.0f, 60.0f);
        ImGui::End();

        // render object into objectTex
        glBindFramebuffer(GL_FRAMEBUFFER, objectFBO);
        glViewport(0, 0, 800, 600);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        basic.use();
        glBindVertexArray(triVAO);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(10.0f);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        if (doXor) {
            // XOR combine: prevTex + objectTex -> nextTex
            glBindFramebuffer(GL_FRAMEBUFFER, (nextTex == accumTex_A) ? accumFBO_A : accumFBO_B);
            glViewport(0, 0, 800, 600);

            xorShader.use();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, prevTex);
            xorShader.setInt("prevTex", 0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, objectTex);
            xorShader.setInt("objectTex", 1);

            glBindVertexArray(quadVAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            std::swap(prevTex, nextTex);
        }

        // render final accum result to screen
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, 800, 600);
        glClear(GL_COLOR_BUFFER_BIT);

        post.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, prevTex);
        post.setInt("screenTex", 0);

        glBindVertexArray(quadVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

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

#pragma once
#include <glad/glad.h>

struct Framebuffer {
    GLuint fbo = 0;
    GLuint tex = 0;
    GLuint rbo = 0;
    int width = 0;
    int height = 0;
    bool hasDepth = false;

    Framebuffer() = default;
    ~Framebuffer() { Destroy(); }

    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    bool Create(int w, int h, bool attachDepth = false);
    void Destroy();
    void Resize(int w, int h);
    void Bind(bool setViewport = true) const;

    static void Unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }
    static void BindDefault(int w, int h) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, w, h);
    }

    GLuint Texture() const { return tex; }
};
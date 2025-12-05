#pragma once
#include <glad/glad.h>

struct Framebuffer {
    GLuint fbo = 0;
    GLuint tex = 0;
    int width = 0;
    int height = 0;

    Framebuffer() = default;
    ~Framebuffer() { Destroy(); }

    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    bool Create(int w, int h, GLenum internalFormat = GL_RGBA8);
    void Destroy();
    void Resize(int w, int h);
    void Bind(bool setViewport = true) const;

    static void Unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

    GLuint Texture() const { return tex; }
};
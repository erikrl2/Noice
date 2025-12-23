#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>

struct Texture {
    GLuint id = 0;
    int width = 0;
    int height = 0;
    GLint internalFormat = GL_RGBA8;
    GLint filter = GL_NEAREST;
    GLint wrap = GL_CLAMP_TO_BORDER;

    Texture() = default;
    ~Texture() { Destroy(); }
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    
    void Create(int w, int h, GLint internalFormat = GL_RGBA8, GLint filter = GL_NEAREST, GLint wrap = GL_CLAMP_TO_BORDER);
    void Destroy();
    void Resize(int w, int h);
    void Clear() const;
    void Bind() const;

    void Swap(Texture& other);
    void UploadFromCPU(unsigned char* data);
};

struct Framebuffer {
    GLuint fbo = 0;
    Texture tex;
    Texture depthTex;
    bool hasDepth = false;

    Framebuffer() = default;
    ~Framebuffer() { Destroy(); }
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    bool Create(int w, int h, GLint format = GL_RGBA8, GLint filter = GL_NEAREST, GLint wrap = GL_CLAMP_TO_BORDER, bool attachDepth = false);
    void Destroy();
    void Resize(int w, int h);
    void Clear(const glm::vec4& color = glm::vec4(0)) const;

    void SwapColorTex(Texture& other);
    void SwapDepthTex(Texture& other);

    static void Unbind();
    static void BindDefault(int w, int h);
};

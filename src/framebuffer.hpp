#pragma once
#include <glad/glad.h>

struct Texture {
    GLuint id = 0;
    int width = 0;
    int height = 0;
    GLenum internalFormat = GL_RGBA8;
    GLenum filter = GL_NEAREST;

    Texture() = default;
    ~Texture() { Destroy(); }
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    
    void Create(int w, int h, GLenum internalFormat = GL_RGBA8, GLenum filter = GL_NEAREST);
    void Destroy();
    void Resize(int w, int h);
    void Clear() const;

    void Swap(Texture& other);
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

    bool Create(int w, int h, GLenum format = GL_RGBA8, GLenum filter = GL_NEAREST, bool attachDepth = false);
    void Destroy();
    void Resize(int w, int h);
    void Clear() const;

    void SwapColorTex(Texture& other);
    void SwapDepthTex(Texture& other);

    static void Unbind();
    static void BindDefault(int w, int h);
};

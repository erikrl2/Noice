#include "framebuffer.hpp"
#include <glad/glad.h>
#include <iostream>
#include <cassert>

bool Framebuffer::Create(int w, int h, GLint format, GLint filter, GLint wrap, bool attachDepth) {
    hasDepth = attachDepth;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    tex.Create(w, h, format, filter);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex.id, 0);

    if (attachDepth) {
        depthTex.Create(w, h, GL_DEPTH_COMPONENT24, filter);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTex.id, 0);
    }

    bool ok = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    if (!ok) { std::cerr << "Framebuffer incomplete\n"; }

    // glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // glBindTexture(GL_TEXTURE_2D, 0);

    return ok;
}

void Framebuffer::Destroy() {
    if (fbo) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
    if (tex.id) tex.Destroy();
    if (depthTex.id) depthTex.Destroy();
}

void Framebuffer::Resize(int w, int h) {
    if (w == tex.width && h == tex.height) return;
    Destroy();
    Create(w, h, tex.internalFormat, tex.filter, tex.wrap, hasDepth);
}

void Framebuffer::Clear(const glm::vec4& color) const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, tex.width, tex.height);
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Framebuffer::SwapColorTex(Texture& other) {
    assert(tex.width == other.width && tex.height == other.height);
    assert(tex.internalFormat == other.internalFormat);
    std::swap(tex.id, other.id);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex.id, 0);
    // glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::SwapDepthTex(Texture& other) {
    assert(depthTex.width == other.width && depthTex.height == other.height);
    assert(depthTex.internalFormat == other.internalFormat);
    std::swap(depthTex.id, other.id);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTex.id, 0);
    // glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::Unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::BindDefault(int w, int h) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w, h);
}

namespace {
    struct FormatInfo { GLenum format; GLenum type; };

    FormatInfo GetFormatInfo(GLenum internalFormat) {
        switch (internalFormat) {
        case GL_RG16F: return { GL_RG, GL_HALF_FLOAT }; 
        case GL_RG8: return { GL_RG, GL_UNSIGNED_BYTE };
        case GL_R8: return { GL_RED, GL_UNSIGNED_BYTE };
        case GL_DEPTH_COMPONENT24: return { GL_DEPTH_COMPONENT, GL_UNSIGNED_INT };
        default: return { GL_RGBA, GL_UNSIGNED_BYTE };
        }
    }
}

void Texture::Create(int w, int h, GLint internalFormat, GLint filter, GLint wrap) {
    width = w;
    height = h;
    this->internalFormat = internalFormat;
    this->filter = filter;
    this->wrap = wrap;

    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    FormatInfo formatInfo = GetFormatInfo(internalFormat);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, formatInfo.format, formatInfo.type, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    if (wrap == GL_CLAMP_TO_BORDER) {
        float bc = (formatInfo.format == GL_DEPTH_COMPONENT) ? 1.0f : 0.0f;
        GLfloat borderColorV[] = { bc, bc, bc, bc };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColorV);
    }
    // glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::Destroy() {
    if (id) { glDeleteTextures(1, &id); id = 0; }
    width = 0;
    height = 0;
}

void Texture::Resize(int w, int h) {
    if (w == width && h == height) return;
    Destroy();
    Create(w, h, internalFormat, filter, wrap);
}

void Texture::Clear() const {
    GLenum format = GetFormatInfo(internalFormat).format;
    float clearValue[4] = { 0, 0, 0, 0 };
    if (format == GL_DEPTH_COMPONENT) clearValue[0] = 1.0f;
    glClearTexImage(id, 0, format, GL_FLOAT, clearValue);
}

void Texture::Bind() const {
    glBindTexture(GL_TEXTURE_2D, id);
}

void Texture::Swap(Texture& other) {
    assert(width == other.width && height == other.height);
    assert(internalFormat == other.internalFormat);
    std::swap(id, other.id);
}

void Texture::UploadFromCPU(unsigned char* data) {
    FormatInfo fi = GetFormatInfo(internalFormat);
    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, (fi.format == GL_RGBA) ? 4 : 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, fi.format, fi.type, data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

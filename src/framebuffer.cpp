#include "framebuffer.hpp"
#include <glad/glad.h>
#include <iostream>

namespace {
    struct FormatInfo {
        GLenum format;
        GLenum type;
    };

    FormatInfo GetFormatInfo(GLenum internalFormat) {
        if (internalFormat == GL_RG16F) {
            return { GL_RG, GL_HALF_FLOAT };
        }
        else if (internalFormat == GL_R32UI) {
            return { GL_RED_INTEGER, GL_UNSIGNED_INT };
        }
        return { GL_RGBA, GL_UNSIGNED_BYTE };
    }
}

Framebuffer::Framebuffer(Framebuffer&& o) noexcept
    : fbo(o.fbo), tex(o.tex), depthTex(o.depthTex), width(o.width), height(o.height), hasDepth(o.hasDepth), internalFormat(o.internalFormat) {
    o.fbo = o.tex = o.depthTex = o.width = o.height = 0;
    o.hasDepth = false;
    o.internalFormat = GL_RGBA8;
}

Framebuffer& Framebuffer::operator=(Framebuffer&& o) noexcept {
    if (this != &o) {
        Destroy();
        fbo = o.fbo;
        tex = o.tex;
        depthTex = o.depthTex;
        width = o.width;
        height = o.height;
        hasDepth = o.hasDepth;
        internalFormat = o.internalFormat;
        o.fbo = o.tex = o.depthTex = o.width = o.height = 0;
        o.hasDepth = false;
        o.internalFormat = GL_RGBA8;
    }
    return *this;
}

bool Framebuffer::Create(int w, int h, bool attachDepth, GLenum format) {
    width = w; height = h;
    hasDepth = attachDepth;
    internalFormat = format;

    FormatInfo formatInfo = GetFormatInfo(internalFormat);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, formatInfo.format, formatInfo.type, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    GLfloat borderColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    if (attachDepth) {
        glGenTextures(1, &depthTex);
        glBindTexture(GL_TEXTURE_2D, depthTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        GLfloat depthBorder[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, depthBorder);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);
    }
    else {
        depthTex = 0;
    }

    bool ok = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    if (!ok) {
        std::cerr << "Framebuffer incomplete\n";
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    return ok;
}

void Framebuffer::Destroy() {
    if (fbo) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
    if (tex) { glDeleteTextures(1, &tex); tex = 0; }
    if (depthTex) { glDeleteTextures(1, &depthTex); depthTex = 0; }
    width = height = 0;
    hasDepth = false;
}

void Framebuffer::Resize(int w, int h) {
    if (w == width && h == height) return;
    bool wantDepth = hasDepth;
    GLenum savedFormat = internalFormat;
    Destroy();
    Create(w, h, wantDepth, savedFormat);
}

void Framebuffer::Bind(bool clear) const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    if (width > 0 && height > 0) glViewport(0, 0, width, height);
    if (clear) {
        if (internalFormat == GL_R32UI) {
            unsigned int clearVal = 0;
            glClearBufferuiv(GL_COLOR, 0, &clearVal);
        } else {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
    }
}

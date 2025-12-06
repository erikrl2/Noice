#include "framebuffer.hpp"
#include <glad/glad.h>
#include <iostream>

Framebuffer::Framebuffer(Framebuffer&& o) noexcept
    : fbo(o.fbo), tex(o.tex), rbo(o.rbo), width(o.width), height(o.height), hasDepth(o.hasDepth) {
    o.fbo = o.tex = o.rbo = o.width = o.height = 0;
    o.hasDepth = false;
}

Framebuffer& Framebuffer::operator=(Framebuffer&& o) noexcept {
    if (this != &o) {
        Destroy();
        fbo = o.fbo;
        tex = o.tex;
        rbo = o.rbo;
        width = o.width;
        height = o.height;
        hasDepth = o.hasDepth;
        o.fbo = o.tex = o.rbo = o.width = o.height = 0;
        o.hasDepth = false;
    }
    return *this;
}

bool Framebuffer::Create(int w, int h, bool attachDepth) {
    width = w; height = h;
    hasDepth = attachDepth;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    if (attachDepth) {
        glGenRenderbuffers(1, &rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
    } else {
        rbo = 0;
    }

    bool ok = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    if (!ok) {
        std::cerr << "Framebuffer incomplete\n";
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (attachDepth) glBindRenderbuffer(GL_RENDERBUFFER, 0);

    return ok;
}

void Framebuffer::Destroy() {
    if (fbo) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
    if (tex) { glDeleteTextures(1, &tex); tex = 0; }
    if (rbo) { glDeleteRenderbuffers(1, &rbo); rbo = 0; }
    width = height = 0;
    hasDepth = false;
}

void Framebuffer::Resize(int w, int h) {
    if (w == width && h == height) return;
    bool wantDepth = hasDepth;
    Destroy();
    Create(w, h, wantDepth);
}

void Framebuffer::Bind(bool setViewport) const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    if (setViewport && width > 0 && height > 0) {
        glViewport(0, 0, width, height);
    }
}
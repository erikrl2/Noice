#pragma once
#include "framebuffer.hpp"
#include <glad/glad.h>

bool Framebuffer::Create(int w, int h, GLenum internalFormat) {
    width = w; height = h;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    bool ok = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return ok;
}

void Framebuffer::Destroy() {
    if (fbo) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
    if (tex) { glDeleteTextures(1, &tex); tex = 0; }
}

void Framebuffer::Resize(int w, int h) {
    if (w == width && h == height) return;
    Destroy();
    Create(w, h);
}

void Framebuffer::Bind(bool setViewport) const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    if (setViewport && width > 0 && height > 0) {
        glViewport(0, 0, width, height);
    }
}
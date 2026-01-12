#include "framebuffer.hpp"

#include "util.hpp"

#include <cassert>
#include <iostream>

void Texture::CreateOrResize(int width, int height, GLint internalFormat, GLint filter) {
  Destroy();
  FormatInfo formatInfo = util::GetFormatInfo(internalFormat);
  glGenTextures(1, &id);
  glBindTexture(GL_TEXTURE_2D, id);
  glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, formatInfo.format, formatInfo.type, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
  // glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::Destroy() {
  if (id) {
    glDeleteTextures(1, &id);
    id = 0;
  }
}

void Texture::Bind(unsigned unit) const {
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, id);
}

void Framebuffer::CreateOrResize(int width, int height) {
  if (this->width == width && this->height == height) return;
  bool attachDepth = (depthTex.id != 0);
  int oldAttachmentCount = attachmentCount;
  Destroy();

  this->width = width;
  this->height = height;

  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

  attachmentCount = 0;
  for (int i = 0; i < oldAttachmentCount; i++) {
    AttachColorTexture(colorInternalFormats[i], colorFilters[i]);
  }

  if (attachDepth) {
    AttachDepthTexture(depthFilter);
  }

  if (attachmentCount > 0 || attachDepth) {
    Finalize();
  }

  // glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::Destroy() {
  if (fbo) {
    glDeleteFramebuffers(1, &fbo);
    fbo = 0;
  }
  for (Texture& ct : colorTex)
    if (ct.id) ct.Destroy();
  if (depthTex.id) depthTex.Destroy();
}

void Framebuffer::AttachColorTexture(GLint internalFormat, GLint filter) {
  if (attachmentCount >= maxAttachments) {
    std::cerr << "Max color attachments reached\n";
    return;
  }
  // glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  Texture& tex = colorTex[attachmentCount];
  tex.CreateOrResize(width, height, internalFormat, filter);

  colorInternalFormats[attachmentCount] = internalFormat;
  colorFilters[attachmentCount] = filter;

  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + attachmentCount, tex.id, 0);
  attachmentCount++;
}

void Framebuffer::AttachDepthTexture(GLint filter) {
  if (depthTex.id) {
    std::cerr << "Depth attachment already exists\n";
    return;
  }
  // glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  depthTex.CreateOrResize(width, height, GL_DEPTH_COMPONENT24, filter);
  depthFilter = filter;

  glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTex.id, 0);
}

void Framebuffer::Finalize() {
  GLenum drawBuffers[maxAttachments];
  for (int i = 0; i < attachmentCount; i++) drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
  glDrawBuffers(attachmentCount, drawBuffers);

  bool ok = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  if (!ok) std::cerr << "Framebuffer incomplete\n";
  assert(ok);
}

void Framebuffer::SetClearColor(const glm::vec4& color, int attachment) {
  if (attachment == -1) {
    for (int i = 0; i < attachmentCount; i++) clearColors[i] = color;
    return;
  }
  assert(attachment >= 0 && attachment < attachmentCount);
  clearColors[attachment] = color;
}

void Framebuffer::Bind() const {
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glViewport(0, 0, width, height);
}

void Framebuffer::Clear() const {
  Bind();

  for (int i = 0; i < attachmentCount; i++) {
    auto info = util::GetFormatInfo(colorInternalFormats[i]);
    const bool isInteger = info.format == GL_RED_INTEGER || info.format == GL_RG_INTEGER
        || info.format == GL_RGB_INTEGER || info.format == GL_RGBA_INTEGER;

    if (!isInteger) {
      glm::vec4 c = clearColors[i];
      glClearBufferfv(GL_COLOR, i, &c[0]);
    } else {
      switch (info.type) {
      case GL_UNSIGNED_BYTE:
      case GL_UNSIGNED_SHORT:
      case GL_UNSIGNED_INT: {
        glm::uvec4 c = clearColors[i];
        glClearBufferuiv(GL_COLOR, i, &c[0]);
        break;
      }
      default: {
        glm::ivec4 c = clearColors[i];
        glClearBufferiv(GL_COLOR, i, &c[0]);
        break;
      }
      }
    }
  }
  if (depthTex.id) {
    GLfloat depth = 1.0f;
    glClearBufferfv(GL_DEPTH, 0, &depth);
  }
}

void Framebuffer::BindDefault(int width, int height) {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, width, height);
}

void Image::Create(int width, int height, GLint internalFormat, GLint filter) {
  assert(!tex);
  this->width = width;
  this->height = height;
  this->internalFormat = internalFormat;
  this->filter = filter;
  tex.CreateOrResize(width, height, internalFormat, filter);
}

void Image::Resize(int width, int height) {
  if (this->width == width && this->height == height) return;
  assert(tex);
  Destroy();
  Create(width, height, internalFormat, filter);
}

void Image::Destroy() {
  tex.Destroy();
}

void Image::Bind(unsigned unit, GLenum access) const {
  glBindImageTexture(unit, tex.id, 0, GL_FALSE, 0, access, internalFormat);
}

void Image::Clear(const glm::vec4& color) const {
  FormatInfo info = util::GetFormatInfo(internalFormat);

  const bool isInteger = info.format == GL_RED_INTEGER || info.format == GL_RG_INTEGER || info.format == GL_RGB_INTEGER
      || info.format == GL_RGBA_INTEGER;

  if (!isInteger) {
    // Float oder normalized fixed-point: immer float clearen
    glm::vec4 v = color;
    glClearTexImage(tex.id, 0, info.format, GL_FLOAT, &v[0]);
  } else {
    // Integer: signed vs unsigned
    switch (info.type) {
    case GL_UNSIGNED_BYTE:
    case GL_UNSIGNED_SHORT:
    case GL_UNSIGNED_INT: {
      glm::uvec4 v = glm::uvec4(color); // Achtung: Cast-Regeln!
      glClearTexImage(tex.id, 0, info.format, GL_UNSIGNED_INT, &v[0]);
      break;
    }
    case GL_BYTE:
    case GL_SHORT:
    case GL_INT: {
      glm::ivec4 v = glm::ivec4(color);
      glClearTexImage(tex.id, 0, info.format, GL_INT, &v[0]);
      break;
    }
    default: assert(false);
    }
  }
}

void Image::Upload(unsigned char* data) const {
  glBindTexture(GL_TEXTURE_2D, tex.id);
  FormatInfo info = util::GetFormatInfo(internalFormat);
  glPixelStorei(GL_UNPACK_ALIGNMENT, (info.format == GL_RGBA) ? 4 : 1);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, info.format, info.type, data);
}

std::vector<unsigned char> Image::Download() const {
  std::vector<unsigned char> data(width * height * 4);
  glBindTexture(GL_TEXTURE_2D, tex.id);
  FormatInfo info = util::GetFormatInfo(internalFormat);
  glGetTexImage(GL_TEXTURE_2D, 0, info.format, info.type, data.data());
  return data;
}

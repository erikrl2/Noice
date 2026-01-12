#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>

#include <vector>

class Framebuffer;
class Image;

struct Texture {
  GLuint id = 0;

  void Bind(unsigned unit) const;

  operator bool() const { return id != 0; }

private:
  void CreateOrResize(int width, int height, GLint internalFormat, GLint filter);
  void Destroy();

  friend class Framebuffer;
  friend class Image;
};

class Framebuffer {
public:
  void CreateOrResize(int width, int height);
  void Destroy();

  void AttachColorTexture(GLint internalFormat, GLint filter);
  void AttachDepthTexture(GLint filter = GL_NEAREST);
  void Finalize();

  void SetClearColor(const glm::vec4& color, int attachment = -1);

  Texture GetColorTexture(int index = 0) const { return colorTex[index]; }
  Texture GetDepthTexture() const { return depthTex; }
  int GetWidth() const { return width; }
  int GetHeight() const { return height; }

  void Bind() const;
  void Clear() const;

  static void BindDefault(int w, int h);

  operator bool() const { return fbo != 0; }

  Framebuffer() = default;
  Framebuffer(const Framebuffer&) = delete;
  ~Framebuffer() { assert(!*this); }

private:
  GLuint fbo = 0;
  int width = 0;
  int height = 0;

  static const int maxAttachments = 3;
  int attachmentCount = 0;

  Texture colorTex[maxAttachments];

  GLint colorInternalFormats[maxAttachments]{};
  GLint colorFilters[maxAttachments]{};
  glm::vec4 clearColors[maxAttachments]{};

  Texture depthTex;

  GLint depthFilter = GL_NEAREST;
};

class Image {
public:
  void Create(int width, int height, GLint internalFormat, GLint filter);
  void Resize(int width, int height);
  void Destroy();

  Texture GetTexture() const { return tex; }
  int GetWidth() const { return width; }
  int GetHeight() const { return height; }

  void Bind(unsigned unit, GLenum access) const;
  void Clear(const glm::vec4& color = {}) const;
  void Upload(unsigned char* data) const;
  std::vector<unsigned char> Download() const;

  operator Texture() const { return tex; }
  operator bool() const { return tex.id != 0; }

  ~Image() { assert(!*this); }

private:
  Texture tex;
  int width = 0;
  int height = 0;
  GLint internalFormat = GL_RGBA8;
  GLint filter = GL_LINEAR;
};

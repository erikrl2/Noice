#pragma once
#include "util.hpp"

#include <glad/glad.h>

#include <string>
#include <vector>

struct Mesh {
  GLuint vao = 0, vbo = 0, ebo = 0;
  size_t indexCount = 0;
  size_t vertexCount = 0;

  Mesh() = default;
  ~Mesh() { Destroy(); }
  Mesh(Mesh&& other) noexcept;
  Mesh& operator=(Mesh&& other) noexcept;

  void UploadIndexed(const void* vertexData, size_t vertexBytes, const unsigned int* indices, size_t indexCount);
  void UploadArrays(const void* vertexData, size_t vertexBytes, size_t vertexCount);

  void SetAttrib(GLuint location, GLint components, GLenum type, GLboolean normalized, GLsizei stride, size_t offset);

  void Draw(int renderFlags = 0) const;
  void Destroy();

  static Mesh CreateFullscreenQuad();
  static Mesh CreateTriangle();

  struct MeshData {
    std::vector<float> verts;
    std::vector<unsigned int> indices;
  };
  inline static ThreadQueue<MeshData> UploadQueue;

  static MeshData LoadFromOBJ(const std::string& path);
  void UploadDataFromOBJ(const MeshData& data);
};

enum RenderFlag { DepthTest = 1 << 0, CullFace = 1 << 1 };

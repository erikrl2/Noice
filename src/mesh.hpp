#pragma once
#include "util.hpp"

#include <glad/glad.h>

#include <string>
#include <vector>

struct FlowfieldSettings;

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

  void SetAttrib(GLuint location, GLint components, GLenum type, GLboolean normalized, GLsizei stride, size_t offset)
      const;

  void Draw(int renderFlags = 0) const;
  void Destroy();

  static Mesh CreateFullscreenQuad();
  static Mesh CreateTriangle();

  struct MeshData {
    std::vector<float> verts;
    std::vector<unsigned int> indices;
    int slot = -1;
  };
  static MeshData LoadFromOBJ(int slot, const std::string& path, const FlowfieldSettings& settings);

  void UploadDataFromOBJ(const MeshData& data);
};

enum RenderFlag { DepthTest = 1 << 0, CullFace = 1 << 1 };

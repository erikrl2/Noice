#pragma once
#include <glad/glad.h>

#include <cassert>
#include <string>
#include <vector>

struct FlowfieldSettings;

struct MeshFlowfieldData {
  std::vector<float> verts;
  std::vector<unsigned int> indices;
  int id = -1;

  MeshFlowfieldData() = default;
  MeshFlowfieldData(MeshFlowfieldData&&) = default;
};

class Mesh {
public:
  void CreateFullscreenQuad();
  void CreateFromFlowfieldData(const MeshFlowfieldData& data);
  void Destroy();

  void Draw(int renderFlags = 0) const;

  static MeshFlowfieldData CreateFlowfieldDataFromOBJ(
      int id, const std::string& path, const FlowfieldSettings& settings
  );

  void UploadIndexed(const void* vertexData, size_t vertexBytes, const unsigned int* indices, size_t indexCount);
  void SetAttrib(GLuint location, GLint components, GLenum type, GLboolean normalized, GLsizei stride, size_t offset);

  operator bool() const { return vao != 0; }

  Mesh() = default;
  Mesh(const Mesh&) = delete;
  Mesh(Mesh&& o) noexcept: vao(o.vao), vbo(o.vbo), ebo(o.ebo), indexCount(o.indexCount), vertexCount(o.vertexCount) {
    o.vao = o.vbo = o.ebo = 0;
    o.indexCount = o.vertexCount = 0;
  }
  ~Mesh() { assert(!*this); }

private:
  GLuint vao = 0, vbo = 0, ebo = 0;
  size_t indexCount = 0;
  size_t vertexCount = 0;
};

enum RenderFlag { DepthTest = 1 << 0, CullFace = 1 << 1 };

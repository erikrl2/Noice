#pragma once
#include <glad/glad.h>

#include <string>
#include <vector>

struct FlowfieldSettings;

struct MeshFlowfieldData {
  std::vector<float> verts;
  std::vector<unsigned int> indices;
  int slot = -1;

  MeshFlowfieldData() = default;
  MeshFlowfieldData(const MeshFlowfieldData&) = delete;
  MeshFlowfieldData(MeshFlowfieldData&&) = default;
};

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

  static MeshFlowfieldData CreateFlowfieldDataFromOBJ(
      int slot, const std::string& path, const FlowfieldSettings& settings
  );

  void UploadFlowfieldMesh(const MeshFlowfieldData& data);
};

enum RenderFlag { DepthTest = 1 << 0, CullFace = 1 << 1 };

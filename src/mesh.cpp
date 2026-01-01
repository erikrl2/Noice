#include "mesh.hpp"

#include "flowload/flowload.hpp"

#include <glad/glad.h>

#include <iostream>

void Mesh::UploadIndexed(const void* vertexData, size_t vertexBytes, const unsigned int* indices, size_t indexCount) {
  this->indexCount = indexCount;
  vertexCount = 0;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glGenBuffers(1, &ebo);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, vertexBytes, vertexData, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indexCount, indices, GL_STATIC_DRAW);
  glBindVertexArray(0);
}

// unused
void Mesh::UploadArrays(const void* vertexData, size_t vertexBytes, size_t vertexCount) {
  this->vertexCount = vertexCount;
  indexCount = 0;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, vertexBytes, vertexData, GL_STATIC_DRAW);
  glBindVertexArray(0);
}

void Mesh::SetAttrib(
    GLuint location, GLint components, GLenum type, GLboolean normalized, GLsizei stride, size_t offset
) {
  glBindVertexArray(vao);
  // glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glVertexAttribPointer(location, components, type, normalized, stride, (void*)offset);
  glEnableVertexAttribArray(location);
  glBindVertexArray(0);
}

void Mesh::Draw(int renderFlags) const {
  if (!vao) return;
  glBindVertexArray(vao);

  if (renderFlags & RenderFlag::DepthTest) glEnable(GL_DEPTH_TEST);
  if (renderFlags & RenderFlag::CullFace) glEnable(GL_CULL_FACE);

  if (indexCount > 0) {
    glDrawElements(GL_TRIANGLES, (GLsizei)indexCount, GL_UNSIGNED_INT, 0);
  } else {
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertexCount);
  }

  if (renderFlags & RenderFlag::CullFace) glDisable(GL_CULL_FACE);
  if (renderFlags & RenderFlag::DepthTest) glDisable(GL_DEPTH_TEST);
}

void Mesh::Destroy() {
  if (ebo) {
    glDeleteBuffers(1, &ebo);
    ebo = 0;
  }
  if (vbo) {
    glDeleteBuffers(1, &vbo);
    vbo = 0;
  }
  if (vao) {
    glDeleteVertexArrays(1, &vao);
    vao = 0;
  }
}

Mesh::Mesh(Mesh&& other) noexcept:
  vao(other.vao), vbo(other.vbo), ebo(other.ebo), indexCount(other.indexCount), vertexCount(other.vertexCount) {
  other.vao = 0;
  other.vbo = 0;
  other.ebo = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
  if (this != &other) {
    Destroy();
    vao = other.vao;
    vbo = other.vbo;
    ebo = other.ebo;
    indexCount = other.indexCount;
    vertexCount = other.vertexCount;
    other.vao = 0;
    other.vbo = 0;
    other.ebo = 0;
  }
  return *this;
}

Mesh Mesh::CreateFullscreenQuad() {
  static const float quadVerts[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};
  static const unsigned int quadIdx[] = {0, 1, 2, 2, 3, 0};

  Mesh m;
  m.UploadIndexed(quadVerts, sizeof(quadVerts), quadIdx, 6);
  m.SetAttrib(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);

  return m;
}

Mesh Mesh::CreateTriangle() {
  static const float triVerts[] = {-0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.0f};

  Mesh m;
  m.UploadArrays(triVerts, sizeof(triVerts), 3);
  m.SetAttrib(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);

  return m;
}

MeshFlowfieldData Mesh::CreateFlowfieldDataFromOBJ(
    int slot, const std::string& path, const FlowfieldSettings& settings
) {
  MeshFlowfieldData data;

  bool ok = ComputeUvFlowfieldFromOBJ(path, data.verts, data.indices, settings);

  if (ok) std::cout << "Loaded " << path << " (" << (data.verts.size() / 6) << " vertices)" << std::endl;

  data.slot = slot;
  return data;
}

void Mesh::UploadFlowfieldMesh(const MeshFlowfieldData& d) {
  if (d.indices.empty()) return;
  UploadIndexed(d.verts.data(), d.verts.size() * sizeof(float), d.indices.data(), d.indices.size());
  SetAttrib(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 0);
  SetAttrib(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 3 * sizeof(float));
}

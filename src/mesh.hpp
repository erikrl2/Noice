#pragma once
#include <glad/glad.h>

#include <string>
#include <vector>

struct MeshData {
    std::vector<float> verts;
    std::vector<unsigned int> indices;
};

struct SimpleMesh {
    GLuint vao = 0, vbo = 0, ebo = 0;
    size_t indexCount = 0;
    size_t vertexCount = 0;

    SimpleMesh() = default;
    ~SimpleMesh() { Destroy(); }
    SimpleMesh(SimpleMesh&& other) noexcept;
    SimpleMesh& operator=(SimpleMesh&& other) noexcept;

    void UploadIndexed(const void* vertexData, size_t vertexBytes, const unsigned int* indices, size_t indexCount);
    void UploadArrays(const void* vertexData, size_t vertexBytes, size_t vertexCount);

    void SetAttrib(GLuint location, GLint components, GLenum type, GLboolean normalized, GLsizei stride, size_t offset);

    void Draw(int renderFlags = 0) const;
    void Destroy();

    static SimpleMesh CreateFullscreenQuad();
    static SimpleMesh CreateTriangle();

    static MeshData LoadFromOBJ(const std::string& path);
    void UploadDataFromOBJ(const MeshData& data); // TODO: rename
};

enum RenderFlag { DepthTest = 1 << 0, CullFace = 1 << 1 };

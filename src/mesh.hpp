#pragma once
#include <glad/glad.h>
#include <vector>

struct SimpleMesh {
    GLuint vao = 0, vbo = 0, ebo = 0;
    size_t indexCount = 0;
    size_t vertexCount = 0;

    SimpleMesh() = default;
    ~SimpleMesh() { Destroy(); }

    SimpleMesh(const SimpleMesh&) = delete;
    SimpleMesh& operator=(const SimpleMesh&) = delete;
    SimpleMesh& operator=(SimpleMesh&&) = delete;

    SimpleMesh(SimpleMesh&& o) noexcept;

    void UploadIndexed(const void* vertexData, size_t vertexBytes, const unsigned int* indices, size_t indexCount_);
    void UploadArrays(const void* vertexData, size_t vertexBytes, size_t vertexCount_);
    void Draw(bool wireframe = false) const;
    void Destroy();

    static SimpleMesh CreateFullscreenQuad();
    static SimpleMesh CreateTriangle();
};
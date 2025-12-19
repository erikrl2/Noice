#pragma once
#include <glad/glad.h>

#include <string>

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

    void Draw(int renderFlags = 0) const;
    void Destroy();

    static SimpleMesh CreateFullscreenQuad();
    static SimpleMesh CreateTriangle();
    static SimpleMesh LoadFromOBJ(const std::string& path);
};

enum RenderFlag { DepthTest = 1 << 0, };

#pragma once
#include "util.hpp"

#include <glad/glad.h>

#include <string>

struct SimpleMesh {
    GLuint vao = 0, vbo = 0, ebo = 0;
    size_t indexCount = 0;
    size_t vertexCount = 0;

    SimpleMesh() = default;
    ~SimpleMesh() { Destroy(); }

    SimpleMesh(const SimpleMesh&) = delete;
    SimpleMesh& operator=(const SimpleMesh&) = delete;

    SimpleMesh(SimpleMesh&& o) noexcept;
    SimpleMesh& operator=(SimpleMesh&& o) noexcept;

    void UploadIndexed(const void* vertexData, size_t vertexBytes, const unsigned int* indices, size_t indexCount_);
    void UploadArrays(const void* vertexData, size_t vertexBytes, size_t vertexCount_);
    void Draw(int renderFlags = 0) const;
    void Destroy();

    static SimpleMesh CreateFullscreenQuad();
    static SimpleMesh CreateTriangle();

    // Neue Factory: lädt eine OBJ-Datei (interleaved pos(3), normal(3), uv(2))
    static SimpleMesh LoadFromOBJ(const std::string& path);
};
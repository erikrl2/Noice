#pragma once
#include "mesh.hpp"
#include <glad/glad.h>
#include <vector>

SimpleMesh::SimpleMesh(SimpleMesh&& o) noexcept
    : vao(o.vao), vbo(o.vbo), ebo(o.ebo), indexCount(o.indexCount), vertexCount(o.vertexCount) {
    o.vao = o.vbo = o.ebo = 0;
    o.indexCount = o.vertexCount = 0;
}

SimpleMesh& SimpleMesh::operator=(SimpleMesh&& o) noexcept {
    if (this != &o) {
        Destroy();
        vao = o.vao;
        vbo = o.vbo;
        ebo = o.ebo;
        indexCount = o.indexCount;
        vertexCount = o.vertexCount;
        o.vao = o.vbo = o.ebo = 0;
        o.indexCount = o.vertexCount = 0;
    }
    return *this;
}

void SimpleMesh::UploadIndexed(const void* vertexData, size_t vertexBytes, const unsigned int* indices, size_t indexCount_) {
    indexCount = indexCount_;
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

void SimpleMesh::UploadArrays(const void* vertexData, size_t vertexBytes, size_t vertexCount_) {
    indexCount = 0;
    vertexCount = vertexCount_;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertexBytes, vertexData, GL_STATIC_DRAW);
    glBindVertexArray(0);
}

void SimpleMesh::Draw(bool wireframe) const {
    if (!vao) return;
    glBindVertexArray(vao);
    if (wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    if (indexCount > 0) {
        glDrawElements(GL_TRIANGLES, (GLsizei)indexCount, GL_UNSIGNED_INT, 0);
    }
    else {
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertexCount);
    }
    if (wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void SimpleMesh::Destroy() {
    if (ebo) { glDeleteBuffers(1, &ebo); ebo = 0; }
    if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
    if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
}

// Convenience factory: fullscreen quad (pos.xy, uv.xy)
SimpleMesh SimpleMesh::CreateFullscreenQuad() {
    static const float quadVerts[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f
    };
    static const unsigned int quadIdx[] = { 0,1,2, 2,3,0 };

    SimpleMesh m;
    m.UploadIndexed(quadVerts, sizeof(quadVerts), quadIdx, 6);

    // setup attribute pointers: layout(location=0) vec2 pos, layout(location=1) vec2 uv
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    return m;
}

// Convenience factory: simple triangle (pos.xyz)
SimpleMesh SimpleMesh::CreateTriangle() {
    static const float triVerts[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };
    SimpleMesh m;
    m.UploadArrays(triVerts, sizeof(triVerts), 3);

    // setup attribute pointer: layout(location=0) vec3 pos
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    return m;
}
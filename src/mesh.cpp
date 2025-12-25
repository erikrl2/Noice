#include "mesh.hpp"

#include <glad/glad.h>
#include <tiny_obj_loader.h>

#include <unordered_map>
#include <iostream>

void SimpleMesh::UploadIndexed(const void* vertexData, size_t vertexBytes, const unsigned int* indices, size_t indexCount) {
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
void SimpleMesh::UploadArrays(const void* vertexData, size_t vertexBytes, size_t vertexCount) {
    this->vertexCount = vertexCount;
    indexCount = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertexBytes, vertexData, GL_STATIC_DRAW);
    glBindVertexArray(0);
}

void SimpleMesh::SetAttrib(GLuint location, GLint components, GLenum type, GLboolean normalized, GLsizei stride, size_t offset) {
    glBindVertexArray(vao);
    // glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(location, components, type, normalized, stride, (void*)offset);
    glEnableVertexAttribArray(location);
    glBindVertexArray(0);
}

void SimpleMesh::Draw(int renderFlags) const {
    if (!vao) return;
    glBindVertexArray(vao);

    if (renderFlags & RenderFlag::DepthTest) glEnable(GL_DEPTH_TEST);
    if (renderFlags & RenderFlag::CullFace) glEnable(GL_CULL_FACE);

    if (indexCount > 0) {
        glDrawElements(GL_TRIANGLES, (GLsizei)indexCount, GL_UNSIGNED_INT, 0);
    }
    else {
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertexCount);
    }

    if (renderFlags & RenderFlag::CullFace) glDisable(GL_CULL_FACE);
    if (renderFlags & RenderFlag::DepthTest) glDisable(GL_DEPTH_TEST);
}

void SimpleMesh::Destroy() {
    if (ebo) { glDeleteBuffers(1, &ebo); ebo = 0; }
    if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
    if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
}

SimpleMesh::SimpleMesh(SimpleMesh&& other) noexcept
    : vao(other.vao), vbo(other.vbo), ebo(other.ebo), indexCount(other.indexCount), vertexCount(other.vertexCount) {
    other.vao = 0; other.vbo = 0; other.ebo = 0;
}

SimpleMesh& SimpleMesh::operator=(SimpleMesh&& other) noexcept {
    if (this != &other) {
        Destroy();
        vao = other.vao; vbo = other.vbo; ebo = other.ebo;
        indexCount = other.indexCount; vertexCount = other.vertexCount;
        other.vao = 0; other.vbo = 0; other.ebo = 0;
    }
    return *this;
}

SimpleMesh SimpleMesh::CreateFullscreenQuad() {
    static const float quadVerts[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f
    };
    static const unsigned int quadIdx[] = { 0,1,2, 2,3,0 };

    SimpleMesh m;
    m.UploadIndexed(quadVerts, sizeof(quadVerts), quadIdx, 6);
    m.SetAttrib(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);

    return m;
}

SimpleMesh SimpleMesh::CreateTriangle() {
    static const float triVerts[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };

    SimpleMesh m;
    m.UploadArrays(triVerts, sizeof(triVerts), 3);
    m.SetAttrib(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);

    return m;
}

MeshData SimpleMesh::LoadFromOBJ(const std::string& path) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str(), nullptr, true);
    if (!warn.empty()) std::cerr << "tinyobj warning: " << warn << "\n";
    if (!err.empty()) std::cerr << "tinyobj error: " << err << "\n";
    if (!ok) return MeshData{};

    // Only position + normal (no texcoords)
    struct Idx { int v, n; };
    struct IdxHash {
        size_t operator()(Idx const& a) const noexcept {
            return ((size_t)a.v * 73856093u) ^ ((size_t)a.n * 83492791u);
        }
    };
    struct IdxEq {
        bool operator()(Idx const& a, Idx const& b) const noexcept {
            return a.v == b.v && a.n == b.n;
        }
    };

    MeshData d;
    std::unordered_map<Idx, unsigned int, IdxHash, IdxEq> cache;

    d.verts.reserve(attrib.vertices.size() / 3 * 6);
    d.indices.reserve(shapes.size() * 3 * 100); // rough estimate

    for (const auto& shape : shapes) {
        const auto& mesh = shape.mesh;
        for (size_t i = 0; i < mesh.indices.size(); ++i) {
            tinyobj::index_t idx = mesh.indices[i];
            Idx key{ idx.vertex_index, idx.normal_index };

            auto it = cache.find(key);
            if (it != cache.end()) {
                d.indices.push_back(it->second);
                continue;
            }

            unsigned int newIndex = (unsigned int)(d.verts.size() / 6);

            if (key.v >= 0 && (size_t)(3 * key.v + 2) < attrib.vertices.size()) {
                d.verts.push_back(attrib.vertices[3 * key.v + 0]);
                d.verts.push_back(attrib.vertices[3 * key.v + 1]);
                d.verts.push_back(attrib.vertices[3 * key.v + 2]);
            } else {
                d.verts.push_back(0.0f); d.verts.push_back(0.0f); d.verts.push_back(0.0f);
            }

            if (key.n >= 0 && (size_t)(3 * key.n + 2) < attrib.normals.size()) {
                d.verts.push_back(attrib.normals[3 * key.n + 0]);
                d.verts.push_back(attrib.normals[3 * key.n + 1]);
                d.verts.push_back(attrib.normals[3 * key.n + 2]);
            } else {
                d.verts.push_back(0.0f); d.verts.push_back(0.0f); d.verts.push_back(0.0f);
            }

            cache.emplace(key, newIndex);
            d.indices.push_back(newIndex);
        }
    }

    if (d.verts.empty() || d.indices.empty()) {
        std::cerr << "OBJ produced no geometry: " << path << "\n";
        return MeshData{};
    }

    return d;
}

void SimpleMesh::UploadDataFromOBJ(const MeshData& d) {
    if (d.indices.empty()) return;
    UploadIndexed(d.verts.data(), d.verts.size() * sizeof(float), d.indices.data(), d.indices.size());
    SetAttrib(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 0);
    SetAttrib(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 3 * sizeof(float));
}

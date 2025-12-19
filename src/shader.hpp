#pragma once
#include "framebuffer.hpp"
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <string>

struct Shader {
    unsigned int id = 0;

    Shader() = default;
    ~Shader() { Destroy(); }
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void Create(const char* v, const char* f);
    void CreateCompute(const char* c);
    void Destroy();

    void Use() const;

    void SetInt(const std::string& name, int value) const;
    void SetFloat(const std::string& name, float value) const;
    void SetVec2(const std::string& name, const glm::vec2& v) const;
    void SetVec3(const std::string& name, const glm::vec3& v) const;
    void SetMat4(const std::string& name, const glm::mat4& m) const;

    void SetTexture2D(const std::string& name, const Texture& texture, unsigned unit = 0) const;

    void DispatchCompute(int width, int height, int groupSize, bool barrier = true) const;
    void SetImage2D(const std::string& name, const Texture& texture, unsigned unit, GLenum access = GL_READ_WRITE) const;
};
#pragma once
#include <glm/glm.hpp>
#include <string>
#include <glad/glad.h>

struct Shader {
    unsigned int id;

    Shader() = default;
    Shader(const char* v, const char* f);

    // TODO: clean
    static Shader CreateCompute(const char* computePath);

    void Use() const;
    void Dispatch(unsigned int numGroupsX, unsigned int numGroupsY, unsigned int numGroupsZ = 1) const;
    void SetInt(const std::string& name, int value) const;
    void SetFloat(const std::string& name, float value) const;
    void SetVec2(const std::string& name, const glm::vec2& v) const;
    void SetVec3(const std::string& name, const glm::vec3& v) const;
    void SetMat4(const std::string& name, const glm::mat4& m) const;
    void SetTexture2D(const std::string& name, GLuint texture, unsigned unit = 0) const;
    void SetImage2D(const std::string& name, GLuint texture, unsigned unit, GLenum access = GL_WRITE_ONLY) const;
};
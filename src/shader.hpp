#pragma once
#include <glm/glm.hpp>
#include <string>
#include <glad/glad.h>

struct Shader {
    unsigned int id;

    Shader(const char* v, const char* f);

    void use() const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec2(const std::string& name, const glm::vec2& v) const;
    void setTexture2D(const std::string& name, GLuint texture, unsigned unit = 0) const;
};

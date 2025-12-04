#pragma once

#include <glm/glm.hpp>
#include <string>

class Shader {
public:
    unsigned int id;

    Shader(const char* v, const char* f);

    void use() const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec2(const std::string& name, const glm::vec2& v) const;
};

#include "shader.hpp"
#include <glad/glad.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

static std::string load(const char* p) {
    std::ifstream f(p);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void printShaderLog(GLuint shader, const char* name) {
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_FALSE) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> buf(len ? len : 1);
        glGetShaderInfoLog(shader, (GLsizei)buf.size(), nullptr, buf.data());
        std::cerr << "Shader compile error (" << (name ? name : "") << "):\n" << buf.data() << "\n";
    }
}

static void printProgramLog(GLuint prog, const char* name) {
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (ok == GL_FALSE) {
        GLint len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> buf(len ? len : 1);
        glGetProgramInfoLog(prog, (GLsizei)buf.size(), nullptr, buf.data());
        std::cerr << "Program link error (" << (name ? name : "") << "):\n" << buf.data() << "\n";
    }
}

Shader::Shader(const char* v, const char* f) {
    std::string vs = load(v);
    std::string fs = load(f);
    const char* vc = vs.c_str();
    const char* fc = fs.c_str();

    unsigned int vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &vc, nullptr);
    glCompileShader(vert);
    printShaderLog(vert, v);

    unsigned int frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &fc, nullptr);
    glCompileShader(frag);
    printShaderLog(frag, f);

    id = glCreateProgram();
    glAttachShader(id, vert);
    glAttachShader(id, frag);
    glLinkProgram(id);
    printProgramLog(id, (std::string(v) + " + " + std::string(f)).c_str());

    glDeleteShader(vert);
    glDeleteShader(frag);
}

void Shader::Use() const { glUseProgram(id); }

void Shader::SetInt(const std::string& name, int value) const {
    glUniform1i(glGetUniformLocation(id, name.c_str()), value);
}

void Shader::SetFloat(const std::string& name, float value) const {
    glUniform1f(glGetUniformLocation(id, name.c_str()), value);
}

void Shader::SetVec2(const std::string& name, const glm::vec2& v) const {
    glUniform2f(glGetUniformLocation(id, name.c_str()), v.x, v.y);
}

void Shader::SetVec3(const std::string& name, const glm::vec3& v) const
{
    glUniform3f(glGetUniformLocation(id, name.c_str()), v.x, v.y, v.z);
}

void Shader::SetMat4(const std::string& name, const glm::mat4& m) const
{
    glUniformMatrix4fv(glGetUniformLocation(id, name.c_str()), 1, GL_FALSE, &m[0][0]);
}

void Shader::SetTexture2D(const std::string& name, GLuint texture, unsigned unit) const {
    glActiveTexture(GL_TEXTURE0 + (GLenum)unit);
    glBindTexture(GL_TEXTURE_2D, texture);
    SetInt(name, (int)unit);
}

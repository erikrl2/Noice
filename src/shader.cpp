#include "shader.hpp"

#include "util.hpp"

#include <glad/glad.h>

#include <iostream>

static void printShaderLog(GLuint shader, const char* name);
static void printProgramLog(GLuint prog, const char* name);

void Shader::Create(const char* v, const char* f) {
  std::string vs = util::ReadFileString(v);
  std::string fs = util::ReadFileString(f);
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

void Shader::CreateCompute(const char* c) {
  std::string cs = util::ReadFileString(c);
  const char* cc = cs.c_str();

  unsigned int comp = glCreateShader(GL_COMPUTE_SHADER);
  glShaderSource(comp, 1, &cc, nullptr);
  glCompileShader(comp);
  printShaderLog(comp, c);

  id = glCreateProgram();
  glAttachShader(id, comp);
  glLinkProgram(id);
  printProgramLog(id, c);

  glDeleteShader(comp);
}

void Shader::Destroy() {
  if (id) {
    glDeleteProgram(id);
    id = 0;
  }
}

void Shader::Use() const {
  glUseProgram(id);
}

void Shader::SetInt(const std::string& name, int value) const {
  glUniform1i(glGetUniformLocation(id, name.c_str()), value);
}

void Shader::SetUint(const std::string& name, unsigned int value) const {
  glUniform1ui(glGetUniformLocation(id, name.c_str()), value);
}

void Shader::SetFloat(const std::string& name, float value) const {
  glUniform1f(glGetUniformLocation(id, name.c_str()), value);
}

void Shader::SetVec2(const std::string& name, const glm::vec2& v) const {
  glUniform2f(glGetUniformLocation(id, name.c_str()), v.x, v.y);
}

void Shader::SetVec3(const std::string& name, const glm::vec3& v) const {
  glUniform3f(glGetUniformLocation(id, name.c_str()), v.x, v.y, v.z);
}

void Shader::SetMat4(const std::string& name, const glm::mat4& m) const {
  glUniformMatrix4fv(glGetUniformLocation(id, name.c_str()), 1, GL_FALSE, &m[0][0]);
}

void Shader::SetTexture(const std::string& name, const Texture& texture, unsigned unit) const {
  glActiveTexture(GL_TEXTURE0 + (GLenum)unit);
  glBindTexture(GL_TEXTURE_2D, texture.id);
  SetInt(name, (int)unit);
}

void Shader::SetImage(const std::string& name, const Texture& texture, unsigned unit, GLenum access) const {
  glBindImageTexture(unit, texture.id, 0, GL_FALSE, 0, access, texture.internalFormat);
  SetInt(name, unit);
}

void Shader::DispatchCompute(int width, int height, int groupSize, bool barrier) const {
  int numGroupsX = (width + groupSize - 1) / groupSize;
  int numGroupsY = (height + groupSize - 1) / groupSize;
  glDispatchCompute(numGroupsX, numGroupsY, 1);
  if (barrier) glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

// ---

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

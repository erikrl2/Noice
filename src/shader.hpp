#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>

#include <span>
#include <string>
#include <unordered_map>

class StorageBuffer;

class Shader {
public:
  void CreateVertFrag(const std::string& vertFilepath, const std::string& fragFilepath);
  void CreateCompute(const std::string& compFilepath);
  void Destroy();

  void Use() const;

  void SetInt(const std::string& name, int value) const;
  void SetUint(const std::string& name, unsigned int value) const;
  void SetFloat(const std::string& name, float value) const;
  void SetVec2(const std::string& name, const glm::vec2& v) const;
  void SetVec3(const std::string& name, const glm::vec3& v) const;
  void SetMat4(const std::string& name, const glm::mat4& m) const;
  void SetMat4v(const std::string& name, GLsizei count, const glm::mat4* m) const;

  void DispatchCompute(int width, int height, int groupSize) const;
  void DispatchComputeIndirect(const StorageBuffer& indirectArgs) const;
  void SetMemoryBarrier(GLbitfield barriers) const;

  operator bool() const { return program != 0; }

  Shader() = default;
  Shader(const Shader&) = delete;
  ~Shader() { assert(!*this); }

private:
  GLint GetUniformLocation(const std::string& name) const;

  GLuint program = 0;
  mutable std::unordered_map<std::string, GLint> uniformLocationCache;
};

class StorageBuffer {
public:
  void Create(GLsizeiptr sizeBytes, const void* initialData, GLenum usage);
  void Destroy();

  void Bind(GLuint binding) const;
  void BindAsDispatchIndirect() const;

  template<typename T>
  void Upload(std::span<T> data) {
    Upload((GLsizeiptr)(sizeof(T) * data.size()), data.data());
  }

  operator bool() const { return id != 0; }

  StorageBuffer() = default;
  StorageBuffer(const StorageBuffer&) = delete;
  ~StorageBuffer() { assert(!*this); }

private:
  void Upload(GLsizeiptr sizeBytes, const void* data);

private:
  GLuint id = 0;
  GLsizeiptr sizeBytes = 0;
  GLenum usage = GL_STREAM_DRAW;
};

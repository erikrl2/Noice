#pragma once
#include <glm/glm.hpp>

#include <vector>
#include <string>

bool ReadFileBytes(const char* path, std::vector<unsigned char>& out);

std::string ReadFileString(const char* path);

bool ImGuiDirection2D(const char* label, glm::vec2& dir, float radius = 32.0f);

void EnableOpenGLDebugOutput();

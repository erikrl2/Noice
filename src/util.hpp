#pragma once
#include "framebuffer.hpp"

#include <glm/glm.hpp>

// ImGui Helpers
bool ImGuiDirection2D(const char* label, glm::vec2& dir, float radius = 64.0f);

// OpenGL Helpers
void SetWireframeMode(bool enable);
//void SetWireframeZOffset(bool enable);
void SetFaceCulling(bool enable);
void SetDepthTest(bool enable);
void BlitFramebufferColor(const Framebuffer& src, const Framebuffer& dst);
void BlitFramebufferDepth(const Framebuffer& src, const Framebuffer& dst);

enum RenderFlags {
    DepthTest = 1 << 0,
    FaceCulling = 1 << 1,
};

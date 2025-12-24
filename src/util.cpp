#include "util.hpp"

#include <glad/glad.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <iostream>
#include <fstream>
#include <sstream>

bool ReadFileBytes(const char* path, std::vector<unsigned char>& out) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;

    auto size = file.tellg();
    out.resize(size);

    file.seekg(0);
    file.read((char*)out.data(), size);

    return file.good();
}

std::string ReadFileString(const char* path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool ImGuiDirection2D(const char* label, glm::vec2& dir, float radius) {
    ImGui::BeginGroup();
    ImGui::Text("%s", label);

    ImVec2 pad = ImGui::GetCursorScreenPos();
    float diameter = radius * 2.0f;
    ImVec2 center = ImVec2(pad.x + radius, pad.y + radius);

    ImGui::InvisibleButton((std::string(label) + "##dirpad").c_str(), ImVec2(diameter, diameter));
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddCircleFilled(center, radius, IM_COL32(48, 48, 48, 255));
    dl->AddCircle(center, radius, IM_COL32(200, 200, 200, 120), 32, 1.0f);

    dl->AddLine(ImVec2(center.x - radius, center.y), ImVec2(center.x + radius, center.y), IM_COL32(120, 120, 120, 90));
    dl->AddLine(ImVec2(center.x, center.y - radius), ImVec2(center.x, center.y + radius), IM_COL32(120, 120, 120, 90));

    const float center_btn_r = 9.0f;
    bool changed = false;
    ImGuiIO& io = ImGui::GetIO();

    const bool center_hovered = (ImLengthSqr(ImVec2(io.MousePos.x - center.x, io.MousePos.y - center.y)) <= center_btn_r * center_btn_r);
    ImU32 center_col = center_hovered ? IM_COL32(80, 80, 80, 255) : IM_COL32(70, 70, 70, 255);
    dl->AddCircleFilled(center, center_btn_r, center_col);
    dl->AddCircle(center, center_btn_r, IM_COL32(220, 220, 220, 120), 24, 1.0f);

    if ((ImGui::IsItemHovered() || ImGui::IsItemActive() || center_hovered) && center_hovered && ImGui::IsMouseClicked(0)) {
        if (dir.x != 0.0f || dir.y != 0.0f) {
            dir = glm::vec2(0.0f, 0.0f);
            changed = true;
        }
    }

    const float eps = 0.0001f;
    const float thumb_track = radius - 8.0f;

    ImVec2 thumb = center;
    if (glm::length(dir) > eps) {
        glm::vec2 n = glm::normalize(dir);
        thumb = ImVec2(center.x + n.x * thumb_track, center.y - n.y * thumb_track);

        dl->AddLine(center, thumb, IM_COL32(240, 120, 80, 200), 2.0f);
        dl->AddCircleFilled(thumb, 6.0f, IM_COL32(240, 120, 80, 255));
    } else {
        dl->AddCircleFilled(thumb, 5.0f, IM_COL32(240, 120, 80, 200));
    }

    if ((ImGui::IsItemActive() || ImGui::IsItemHovered()) && io.MouseDown[0]) {
        const bool pressing_center = (ImLengthSqr(ImVec2(io.MousePos.x - center.x, io.MousePos.y - center.y)) <= center_btn_r * center_btn_r);
        if (!pressing_center) {
            ImVec2 m = io.MousePos;
            glm::vec2 delta = glm::vec2(m.x - center.x, center.y - m.y);
            if (glm::length(delta) > eps) {
                glm::vec2 newDir = glm::normalize(delta);
                if (newDir != dir) {
                    dir = newDir;
                    changed = true;
                }
            }
        }
    }

    ImGui::EndGroup();
    return changed;
}

static const char* DebugSourceToString(GLenum source) {
    switch (source) {
    case GL_DEBUG_SOURCE_API: return "API";
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM: return "WindowSystem";
    case GL_DEBUG_SOURCE_SHADER_COMPILER: return "ShaderCompiler";
    case GL_DEBUG_SOURCE_THIRD_PARTY: return "ThirdParty";
    case GL_DEBUG_SOURCE_APPLICATION: return "Application";
    case GL_DEBUG_SOURCE_OTHER: return "Other";
    default: return "Unknown";
    }
}

static const char* DebugTypeToString(GLenum type) {
    switch (type) {
    case GL_DEBUG_TYPE_ERROR: return "Error";
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "Deprecated";
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "Undefined";
    case GL_DEBUG_TYPE_PORTABILITY: return "Portability";
    case GL_DEBUG_TYPE_PERFORMANCE: return "Performance";
    case GL_DEBUG_TYPE_MARKER: return "Marker";
    case GL_DEBUG_TYPE_PUSH_GROUP: return "PushGroup";
    case GL_DEBUG_TYPE_POP_GROUP: return "PopGroup";
    case GL_DEBUG_TYPE_OTHER: return "Other";
    default: return "Unknown";
    }
}

static const char* DebugSeverityToString(GLenum severity) {
    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH: return "HIGH";
    case GL_DEBUG_SEVERITY_MEDIUM: return "MEDIUM";
    case GL_DEBUG_SEVERITY_LOW: return "LOW";
    case GL_DEBUG_SEVERITY_NOTIFICATION: return "NOTIFY";
    default: return "Unknown";
    }
}

static void GLAPIENTRY OnOpenGLDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
    (void)length;
    (void)userParam;

    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
    if (id == 131218) return;

    std::cerr
        << "[GL] "
        << DebugSeverityToString(severity)
        << " | " << DebugSourceToString(source)
        << " | " << DebugTypeToString(type)
        << " | id=" << id
        << " | " << (message ? message : "")
        << "\n";

#ifdef _MSC_VER
    if (type == GL_DEBUG_TYPE_ERROR) { __debugbreak(); }
#endif
}

void EnableOpenGLDebugOutput() {
    if (!GLAD_GL_KHR_debug && !GLAD_GL_VERSION_4_3) {
        std::cerr << "OpenGL debug output not available (need GL 4.3 or KHR_debug).\n";
        return;
    }

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(OnOpenGLDebugMessage, nullptr);

    glDebugMessageControl(
        GL_DONT_CARE,
        GL_DONT_CARE,
        GL_DEBUG_SEVERITY_NOTIFICATION,
        0,
        nullptr,
        GL_FALSE
    );
}

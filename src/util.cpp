#include "util.hpp"
#include <glad/glad.h>
#include <iostream>

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

    if (type == GL_DEBUG_TYPE_ERROR) { __debugbreak(); }
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

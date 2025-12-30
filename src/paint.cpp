#include "paint.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>

void PaintMode::Init(int w, int h) {
    quad = Mesh::CreateFullscreenQuad();

    canvasFB.Create(w, h, GL_RGBA16F, GL_LINEAR, GL_CLAMP_TO_EDGE);
    resultFB.Create(w, h, GL_RG16F, GL_LINEAR, GL_CLAMP_TO_EDGE);

    stampShader.Create("assets/shaders/post.vert.glsl", "assets/shaders/paint_stamp.frag.glsl");
    resolveShader.Create("assets/shaders/post.vert.glsl", "assets/shaders/paint_resolve.frag.glsl");

    canvasFB.Clear();
}

void PaintMode::Destroy() {
    stampShader.Destroy();
    resolveShader.Destroy();
    canvasFB.Destroy();
    resultFB.Destroy();
    quad.Destroy();
}

void PaintMode::UpdateImGui() {
    ImGui::SliderFloat("Brush Radius", &brushRadiusPx, 2.0f, 400.0f, "%.0f");
}

void PaintMode::Update(float dt) {
    if (leftDown) {
        if (!havePrev) {
            prevX = mouseX;
            prevY = mouseY;
            havePrev = true;
        }
        else {
            PaintSegment(prevX, prevY, mouseX, mouseY, dt);
            prevX = mouseX;
            prevY = mouseY;
        }
    }

    ResolveToRG();
}

static inline void SafeNormalize(float& x, float& y) {
    float l = std::sqrt(x * x + y * y);
    if (l < 1e-6f) { x = y = 0.0f; return; }
    x /= l; y /= l;
}

void PaintMode::PaintSegment(double x0, double y0, double x1, double y1, float dt) {
    float dx = float(x1 - x0);
    float dy = float(y1 - y0);
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 1e-4f) return;

    float dirX = dx, dirY = dy;
    SafeNormalize(dirX, dirY);

    float spacing = std::max(0.5f, brushRadiusPx * 0.25f);
    int steps = std::max(1, (int)std::ceil(dist / spacing));

    for (int i = 0; i <= steps; i++) {
        float t = float(i) / float(steps);
        float cx = float(x0) + dx * t;
        float cy = float(y0) + dy * t;
        StampAt(cx, cy, dirX, dirY);
    }
}

void PaintMode::StampAt(float cx, float cy, float dx, float dy) {
    canvasFB.Bind();
    stampShader.Use();

    stampShader.SetVec2("uCenterPx", { cx, cy });
    stampShader.SetFloat("uRadiusPx", brushRadiusPx);
    stampShader.SetVec2("uDir", { dx, dy });
    stampShader.SetFloat("uSoftness", 0.60f);
    stampShader.SetFloat("uStrength", 0.55f);
    stampShader.SetTexture("uCanvas", canvasFB.tex);

    quad.Draw();
}

void PaintMode::ResolveToRG() {
    resultFB.Bind();
    resolveShader.Use();
    resolveShader.SetTexture("uCanvas", canvasFB.tex);
    quad.Draw();
}

void PaintMode::OnResize(int w, int h) {
    canvasFB.Resize(w, h);
    resultFB.Resize(w, h);
}

void PaintMode::OnMouseMoved(double x, double y) {
    mouseX = x;
    mouseY = y;
}

void PaintMode::OnMouseScrolled(float offset) {
    if (offset > 0.0f) {
        brushRadiusPx += 5.0f;
    }
    else if (offset < 0.0f) {
        brushRadiusPx -= 5.0f;
        brushRadiusPx = std::max(2.0f, brushRadiusPx);
    }
}

void PaintMode::OnMouseClicked(int button, int action) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            leftDown = true;
            havePrev = false;
        }
        else {
            leftDown = false;
            havePrev = false;
        }
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        canvasFB.Clear();
    }
}

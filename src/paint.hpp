#pragma once
#include "mode.hpp"
#include "shader.hpp"
#include "framebuffer.hpp"
#include "mesh.hpp"

class PaintMode : public Mode {
public:
    void Init(int width, int height);
    void Destroy();

    void Update(float dt) override;
    void UpdateImGui() override;

    void OnResize(int width, int height) override;
    void OnMouseClicked(int button, int action) override;
    void OnMouseMoved(double xpos, double ypos) override;
    void OnMouseScrolled(float offset) override;

    Framebuffer& GetResultFB() override { return resultFB; }

private:
    void PaintSegment(double x0, double y0, double x1, double y1, float dt);
    void StampAt(float cx, float cy, float dirX, float dirY);

    void ResolveToRG();

private:
    float brushRadiusPx = 30.0f;

    bool leftDown = false;
    bool rightDown = false;
    bool havePrev = false;
    double mouseX = 0, mouseY = 0;
    double prevX = 0, prevY = 0;

    Shader stampShader;
    Shader resolveShader;

    Framebuffer canvasFB;
    Framebuffer resultFB;

    Mesh quad;
};

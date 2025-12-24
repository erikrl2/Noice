#pragma once
#include "framebuffer.hpp"
#include "effect.hpp"

class Mode {
public:
    virtual void UpdateImGui() = 0;
    virtual void Update(float dt) = 0;

    virtual void OnResize(int width, int height) {}
    virtual void OnMouseClicked(int button, int action) {}
    virtual void OnMouseMoved(double xpos, double ypos) {}

    virtual Framebuffer& GetResultFB() = 0;

    virtual const MvpState* GetMvpState() { return nullptr; }
    bool HasMvp() { return (GetMvpState() != nullptr); }
};

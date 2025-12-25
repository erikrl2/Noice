#include "effect.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <imgui.h>

void Effect::Init(int width, int height) {
    scrollShader.CreateCompute("assets/shaders/scroll_move.comp.glsl");
    fillShader.CreateCompute("assets/shaders/scroll_fill.comp.glsl");

    int scaledWidth = width / downscaleFactor;
    int scaledHeight = height / downscaleFactor;
    currNoiseTex.Create(scaledWidth, scaledHeight, GL_RG8, GL_NEAREST);
    prevNoiseTex.Create(scaledWidth, scaledHeight, GL_RG8, GL_NEAREST);
    currAccTex.Create(scaledWidth, scaledHeight, GL_RG16F, GL_NEAREST);
    prevAccTex.Create(scaledWidth, scaledHeight, GL_RG16F, GL_NEAREST);

    prevDepthTex.Create(width, height, GL_DEPTH_COMPONENT24, GL_LINEAR);

    currNoiseTex.Clear(); prevNoiseTex.Clear();
    currAccTex.Clear(); prevAccTex.Clear();
}

void Effect::Destroy() {
    scrollShader.Destroy();
    fillShader.Destroy();

    currNoiseTex.Destroy();
    prevNoiseTex.Destroy();
    currAccTex.Destroy();
    prevAccTex.Destroy();

    prevDepthTex.Destroy();
}

void Effect::UpdateImGui() {
    ImGui::Text("Effect:");
    ImGui::SliderFloat("Speed (px/s)", &scrollSpeed, 0.0f, 500.0f, "%.0f");
    ImGui::SliderInt("Reset Interval", &accResetInterval, 1, 100);
    if (ImGui::SliderInt("Downscale", &downscaleFactor, 1, 8)) OnResize(prevDepthTex.width, prevDepthTex.height);
    ImGui::Checkbox("Disable", &disabled); ImGui::SameLine();
    ImGui::Checkbox("Pause", &paused);
}

void Effect::Apply(Framebuffer& in, float dt, const MvpState* mats) {
    assert((mats == nullptr) | in.hasDepth); // mats => depth
    ScatterPass(in, dt, mats);
    FillPass();
    SwapBuffers(in);
}

void Effect::ScatterPass(Framebuffer& in, float dt, const MvpState* mats) {
    assert(in.tex.internalFormat == GL_RG16F);

    static unsigned frameCount = 0;
    if (++frameCount % accResetInterval == 0) prevAccTex.Clear();

    scrollShader.Use();

    scrollShader.SetImage2D("currNoiseTex", currNoiseTex, 0, GL_WRITE_ONLY);
    scrollShader.SetImage2D("prevNoiseTex", prevNoiseTex, 1, GL_READ_ONLY);
    scrollShader.SetImage2D("currAccTex", currAccTex, 2, GL_WRITE_ONLY);
    scrollShader.SetImage2D("prevAccTex", prevAccTex, 3, GL_READ_WRITE);
    scrollShader.SetTexture2D("flowTex", in.tex, 0);
    scrollShader.SetFloat("scrollSpeed", scrollSpeed * dt / downscaleFactor * (int)!paused);

    if (mats != nullptr) {
        scrollShader.SetTexture2D("currDepthTex", in.depthTex, 1);
        scrollShader.SetTexture2D("prevDepthTex", prevDepthTex, 2);

        scrollShader.SetMat4("invPrevProj", glm::inverse(mats->prevProj));
        scrollShader.SetMat4("invPrevView", glm::inverse(mats->prevView));
        scrollShader.SetMat4("invPrevModel", glm::inverse(mats->prevModel));
        scrollShader.SetMat4("currModel", mats->currModel);
        scrollShader.SetMat4("currViewProj", mats->currProj * mats->currView);
    }
    scrollShader.SetInt("reproject", (mats != nullptr));

    scrollShader.DispatchCompute(currNoiseTex.width, currNoiseTex.height, 16);
}

void Effect::FillPass() {
    fillShader.Use();

    fillShader.SetImage2D("currNoiseTex", currNoiseTex, 0, GL_READ_WRITE);
    fillShader.SetImage2D("prevNoiseTex", prevNoiseTex, 1, GL_WRITE_ONLY);
    fillShader.SetImage2D("currAccTex", currAccTex, 2, GL_WRITE_ONLY);
    fillShader.SetImage2D("prevAccTex", prevAccTex, 3, GL_READ_WRITE);
    fillShader.SetUint("seed", (uint32_t)rand());

    fillShader.DispatchCompute(currNoiseTex.width, currNoiseTex.height, 16);
}

void Effect::SwapBuffers(Framebuffer& in) {
    currNoiseTex.Swap(prevNoiseTex);
    currAccTex.Swap(prevAccTex);
    if (in.hasDepth) in.SwapDepthTex(prevDepthTex);
}

void Effect::ClearBuffers() {
    currNoiseTex.Clear(); prevNoiseTex.Clear();
    currAccTex.Clear(); prevAccTex.Clear();
}

void Effect::OnResize(int width, int height) {
    int scaledWidth = width / downscaleFactor;
    int scaledHeight = height / downscaleFactor;
    currNoiseTex.Resize(scaledWidth, scaledHeight);
    prevNoiseTex.Resize(scaledWidth, scaledHeight);
    currAccTex.Resize(scaledWidth, scaledHeight);
    prevAccTex.Resize(scaledWidth, scaledHeight);

    prevDepthTex.Resize(width, height);

    ClearBuffers();
}

void Effect::OnMouseClicked(int button, int action) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            disabled = !disabled;
        }
    }
}

void Effect::OnKeyPressed(int key, int action) {
    switch (key) {
    case GLFW_KEY_F:
        if (action == GLFW_PRESS) paused = !paused;
        break;
    case GLFW_KEY_TAB:
        if (action == GLFW_PRESS) disabled = !disabled;
        break;
    }
}

#include "effect.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

void Effect::Init(int width, int height) {
    scrollShader.CreateCompute("shaders/adapt_scroll.comp.glsl");
    fillShader.CreateCompute("shaders/fill_gaps.comp.glsl");

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

void Effect::ApplyDynamic(Framebuffer& in, MvpState& mats, float dt) {
    ScatterPass(in, dt, &mats);
    FillPass();
    SwapBuffers(&in);
}

void Effect::ApplyStatic(Framebuffer& in, float dt) {
    ScatterPass(in, dt);
    FillPass();
    SwapBuffers();
}

void Effect::ScatterPass(Framebuffer& in, float dt, MvpState* mats) {
    static unsigned frameCount = 0;
    if (++frameCount % accResetInterval == 0) prevAccTex.Clear();

    scrollShader.Use();

    scrollShader.SetImage2D("currNoiseTex", currNoiseTex, 0, GL_WRITE_ONLY);
    scrollShader.SetImage2D("prevNoiseTex", prevNoiseTex, 1, GL_READ_ONLY);
    scrollShader.SetImage2D("currAccTex", currAccTex, 2, GL_WRITE_ONLY);
    scrollShader.SetImage2D("prevAccTex", prevAccTex, 3, GL_READ_WRITE);
    scrollShader.SetTexture2D("flowTex", in.tex, 0);
    scrollShader.SetFloat("scrollSpeed", scrollSpeed * dt / downscaleFactor * (int)!paused);

    bool isDynamic = (mats != nullptr);
    if (isDynamic) {
        scrollShader.SetTexture2D("currDepthTex", in.depthTex, 1);
        scrollShader.SetTexture2D("prevDepthTex", prevDepthTex, 2);

        scrollShader.SetMat4("invPrevProj", glm::inverse(mats->prevProj));
        scrollShader.SetMat4("invPrevView", glm::inverse(mats->prevView));
        scrollShader.SetMat4("invPrevModel", glm::inverse(mats->prevModel));
        scrollShader.SetMat4("currModel", mats->currModel);
        scrollShader.SetMat4("currViewProj", mats->currProj * mats->currView);
    }
    scrollShader.SetInt("isDynamic", isDynamic);

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

void Effect::SwapBuffers(Framebuffer* in) {
    currNoiseTex.Swap(prevNoiseTex);
    currAccTex.Swap(prevAccTex);
    if (in) in->SwapDepthTex(prevDepthTex);
}

void Effect::OnResize(int width, int height) {
    int scaledWidth = width / downscaleFactor;
    int scaledHeight = height / downscaleFactor;
    currNoiseTex.Resize(scaledWidth, scaledHeight);
    prevNoiseTex.Resize(scaledWidth, scaledHeight);
    currAccTex.Resize(scaledWidth, scaledHeight);
    prevAccTex.Resize(scaledWidth, scaledHeight);

    prevDepthTex.Resize(width, height);

    currNoiseTex.Clear(); prevNoiseTex.Clear();
    currAccTex.Clear(); prevAccTex.Clear();
}

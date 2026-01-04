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

  ClearBuffers();
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
  ImGui::DragFloat("Speed", &scrollSpeed, 0.1f, 0.0f, 0.0f, "%.1f");
  ImGui::DragInt("Sync rate", &accResetInterval, 0.1f, 0, 1000, "%d", ImGuiSliderFlags_ClampOnInput);
  if (ImGui::SliderInt("Downscale", &downscaleFactor, 1, 8, "%d", ImGuiSliderFlags_NoInput))
    OnResize(prevDepthTex.width, prevDepthTex.height);
  ImGui::Checkbox("Disable", &disabled);
  ImGui::SameLine();
  ImGui::Checkbox("Pause", &paused);
}

void Effect::ApplyAttached(Framebuffer& in, float dt, const MvpState* mats) {
  assert(mats && in.hasDepth);
  ScatterPass(in, dt, mats);
  FillPass();
  SwapBuffers(in);
}

void Effect::Apply(Framebuffer& in, float dt) {
  ScatterPass(in, dt);
  FillPass();
  SwapBuffers(in);
}

void Effect::ScatterPass(Framebuffer& in, float dt, const MvpState* mats) {
  assert(in.tex.internalFormat == GL_RG16F);

  if (accResetInterval > 0) {
    static unsigned frameCount = 0;
    if (++frameCount % accResetInterval == 0) prevAccTex.Clear();
  }

  bool attachedEffect = (mats != nullptr);
  float speed = scrollSpeed * dt / downscaleFactor * (int)!paused;

  scrollShader.Use();

  scrollShader.SetImage("uCurrNoiseTex", currNoiseTex, 0, GL_WRITE_ONLY);
  scrollShader.SetImage("uPrevNoiseTex", prevNoiseTex, 1, GL_READ_ONLY);
  scrollShader.SetImage("uCurrAccTex", currAccTex, 2, GL_WRITE_ONLY);
  scrollShader.SetImage("uPrevAccTex", prevAccTex, 3, GL_READ_WRITE);
  scrollShader.SetTexture("uFlowTex", in.tex, 0);

  scrollShader.SetInt("uReproject", attachedEffect);
  if (attachedEffect) {
    scrollShader.SetTexture("uCurrDepthTex", in.depthTex, 1);
    scrollShader.SetTexture("uPrevDepthTex", prevDepthTex, 2);

    scrollShader.SetMat4("uInvPrevProj", glm::inverse(mats->prevProj));
    scrollShader.SetMat4("uInvPrevView", glm::inverse(mats->prevView));
    scrollShader.SetMat4("uInvPrevModel", glm::inverse(mats->prevModel));
    scrollShader.SetMat4("uCurrModel", mats->currModel);
    scrollShader.SetMat4("uCurrViewProj", mats->currProj * mats->currView);

    scrollShader.SetFloat("uScrollSpeed", speed);
  } else {
    // speed adjustment: scrollspeed unit here is [pixels per second] and not [pixels per worldspace-unit per second]
    scrollShader.SetFloat("uScrollSpeed", speed * 20.0f);
  }

  scrollShader.DispatchCompute(currNoiseTex.width, currNoiseTex.height, 16);
}

void Effect::FillPass() {
  fillShader.Use();

  fillShader.SetImage("uCurrNoiseTex", currNoiseTex, 0, GL_READ_WRITE);
  fillShader.SetImage("uPrevNoiseTex", prevNoiseTex, 1, GL_WRITE_ONLY);
  fillShader.SetImage("uCurrAccTex", currAccTex, 2, GL_WRITE_ONLY);
  fillShader.SetImage("uPrevAccTex", prevAccTex, 3, GL_READ_WRITE);
  fillShader.SetUint("uSeed", (uint32_t)rand());

  fillShader.DispatchCompute(currNoiseTex.width, currNoiseTex.height, 16);
}

void Effect::SwapBuffers(Framebuffer& in) {
  currNoiseTex.Swap(prevNoiseTex);
  currAccTex.Swap(prevAccTex);
  if (in.hasDepth) in.SwapDepthTex(prevDepthTex);
}

void Effect::ClearBuffers() {
  currNoiseTex.Clear();
  prevNoiseTex.Clear();
  currAccTex.Clear();
  prevAccTex.Clear();
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
  if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS) {
    disabled = !disabled;
  }
}

void Effect::OnMouseScrolled(float offset) {
  scrollSpeed += offset * 0.5f;
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

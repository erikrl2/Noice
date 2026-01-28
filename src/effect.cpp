#include "effect.hpp"

#include "util.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <imgui.h>

void Effect::Init(int width, int height) {
  scrollShader.CreateCompute("assets/shaders/effect/scroll_move.comp.glsl");
  fillShader.CreateCompute("assets/shaders/effect/scroll_fill.comp.glsl");

  scaledWidth = width / downscaleFactor;
  scaledHeight = height / downscaleFactor;

  for (auto& img : effectImgs) {
    img.noise.Create(scaledWidth, scaledHeight, GL_RG8, GL_NEAREST);
    img.acc.Create(scaledWidth, scaledHeight, GL_RG32F, GL_NEAREST);
  }

  claimImg.Create(scaledWidth, scaledHeight, GL_R32UI, GL_NEAREST);

  ClearBuffers();
}

void Effect::Destroy() {
  modelSSB.Destroy();
  scrollShader.Destroy();
  fillShader.Destroy();

  for (auto& img : effectImgs) {
    img.noise.Destroy();
    img.acc.Destroy();
  }
  claimImg.Destroy();
}

void Effect::UpdateImGui() {
  int fullWidth = scaledWidth * downscaleFactor;
  int fullHeight = scaledHeight * downscaleFactor;

  ImGui::Checkbox("Disable", &disabled);
  ImGui::SameLine();
  ImGui::Checkbox("Pause", &paused);
  if (disabled) {
    ImGui::SameLine();
    ImGui::Checkbox("Show Acc", &showAcc);
  }

  ImGui::DragFloat("Speed", &scrollSpeed, 0.1f, 0.0f, 0.0f, "%.1f", ImGuiSliderFlags_NoRoundToFormat);
  ImGui::DragInt("Acc Reset Rate", &accResetInterval, 0.1f, 0, 1000, "%d", ImGuiSliderFlags_ClampOnInput);
  if (ImGui::Button("Clear Acc")) ClearAcc();

  // if (ImGui::SliderInt("Downscale", &downscaleFactor, 1, 8, "%d", ImGuiSliderFlags_NoInput))
  // OnResize(fullWidth, fullHeight);
}

Texture Effect::Apply(const EffectInputData& in, float dt) {
  std::swap(curr, prev);

  ScatterPass(in, dt);
  FillPass(in);

  if (disabled) return showAcc ? effectImgs[curr].acc : in.prevFlowTex;
  return effectImgs[curr].noise;
}

void Effect::ScatterPass(const EffectInputData& in, float dt) {
  if (accResetInterval > 0) {
    static unsigned frameCount = 0;
    if (++frameCount % accResetInterval == 0) effectImgs[prev].acc.Clear();
  }

  // dt = 1.0f / 144.0f; // DEBUG
  float speed = scrollSpeed * dt / downscaleFactor * (int)!paused;

  claimImg.Clear({-1, 0, 0, 0});

  scrollShader.Use();

  effectImgs[curr].noise.Bind(0, GL_WRITE_ONLY);
  effectImgs[prev].noise.Bind(1, GL_READ_ONLY);
  effectImgs[curr].acc.Bind(2, GL_WRITE_ONLY);
  effectImgs[prev].acc.Bind(3, GL_READ_WRITE);
  claimImg.Bind(4, GL_READ_WRITE);

  in.currIdTex.Bind(0);

  (in.reproject ? in.prevIdTex : in.currIdTex).Bind(1);

  if (in.reproject) {
    in.prevDepthTex.Bind(2);

    modelSSB.Upload(in.modelMats);
    modelSSB.Bind(0);

    scrollShader.SetMat4v("uViewMat", 2, in.prevCurrView);
    scrollShader.SetMat4v("uProjMat", 2, in.prevCurrProj);
    scrollShader.SetInt("uCurrInd", in.currInd);
  }

  if (in.flow) {
    in.prevFlowTex.Bind(3);
    scrollShader.SetFloat("uScrollSpeed", speed * (in.reproject ? 1 : 20));
  }

  scrollShader.SetInt("uReproject", in.reproject);
  scrollShader.SetInt("uFlow", in.flow);

  scrollShader.DispatchCompute(scaledWidth, scaledHeight, 16);
}

void Effect::FillPass(const EffectInputData& in) {
  fillShader.Use();

  effectImgs[curr].noise.Bind(0, GL_READ_WRITE);
  effectImgs[prev].noise.Bind(1, GL_WRITE_ONLY);
  effectImgs[curr].acc.Bind(2, GL_READ_WRITE);
  effectImgs[prev].acc.Bind(3, GL_READ_WRITE);

  in.currIdTex.Bind(0);
  in.prevIdTex.Bind(1);

  fillShader.SetUint("uSeed", util::RandomInt());

  fillShader.DispatchCompute(scaledWidth, scaledHeight, 16);
}

void Effect::ClearBuffers() {
  for (auto& img : effectImgs) {
    img.noise.Clear();
    img.acc.Clear();
  }
}

void Effect::ClearAcc() {
  effectImgs[curr].acc.Clear();
}

void Effect::OnResize(int width, int height) {
  scaledWidth = width / downscaleFactor;
  scaledHeight = height / downscaleFactor;

  for (auto& img : effectImgs) {
    img.noise.Resize(scaledWidth, scaledHeight);
    img.acc.Resize(scaledWidth, scaledHeight);
  }
  claimImg.Resize(scaledWidth, scaledHeight);

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
  case GLFW_KEY_R:
    if (action == GLFW_PRESS) ClearAcc();
    break;
  }
}

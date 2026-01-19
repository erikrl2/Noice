#include "effect.hpp"

#include "util.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <imgui.h>

void Effect::Init(int width, int height) {
  accGather.CreateCompute("assets/shaders/effect/acc_gather.comp.glsl");
  noiseScatter.CreateCompute("assets/shaders/effect/noise_scatter.comp.glsl");
  noiseFill.CreateCompute("assets/shaders/effect/noise_fill.comp.glsl");

  modelSSB.Create(sizeof(glm::mat4[2]) * 6, nullptr, GL_DYNAMIC_DRAW); // or GL_STREAM_DRAW ?

  scaledWidth = width / downscaleFactor;
  scaledHeight = height / downscaleFactor;

  for (auto& img : effectImgs) {
    img.noise.Create(scaledWidth, scaledHeight, GL_RG8, GL_NEAREST);
    img.acc.Create(scaledWidth, scaledHeight, GL_RG32F, GL_NEAREST);
  }

  moveStepImg.Create(scaledWidth, scaledHeight, GL_RG16I, GL_NEAREST);

  ClearBuffers();
}

void Effect::Destroy() {
  modelSSB.Destroy();
  accGather.Destroy();
  noiseScatter.Destroy();
  noiseFill.Destroy();

  for (auto& img : effectImgs) {
    img.noise.Destroy();
    img.acc.Destroy();
  }
  moveStepImg.Destroy();
}

void Effect::UpdateImGui() {
  int fullWidth = scaledWidth * downscaleFactor;
  int fullHeight = scaledHeight * downscaleFactor;

  ImGui::Checkbox("Disable", &disabled);
  ImGui::SameLine();
  ImGui::Checkbox("Pause", &paused);

  ImGui::DragFloat("Speed", &scrollSpeed, 0.1f, 0.0f, 0.0f, "%.1f", ImGuiSliderFlags_NoRoundToFormat);
  ImGui::DragInt("Sync rate", &accResetInterval, 0.1f, 0, 1000, "%d", ImGuiSliderFlags_ClampOnInput);
  if (ImGui::Button("Clear Accumulation")) effectImgs[curr].acc.Clear();

  // if (ImGui::SliderInt("Downscale", &downscaleFactor, 1, 8, "%d", ImGuiSliderFlags_NoInput))
  // OnResize(fullWidth, fullHeight);
}

Texture Effect::Apply(const EffectInputData& in, float dt) {
  std::swap(curr, prev);

  // modelSSB.Upload(in.modelMats);

  AccGather(in, dt);
  NoiseScatter(in, dt);
  NoiseFill(in);

#ifdef NDEBUG // FLOW
  return !disabled ? effectImgs[curr].noise : in.currFlowTex;
#else // ACC
  return !disabled ? effectImgs[curr].noise : effectImgs[curr].acc;
#endif
}

void Effect::AccGather(const EffectInputData& in, float dt) {
  effectImgs[curr].acc.Bind(0, GL_WRITE_ONLY);
  effectImgs[prev].acc.Bind(1, GL_READ_ONLY);

  in.currIdTex.Bind(0);
  in.currMotionTex.Bind(1);
  in.currFlowTex.Bind(2);

  accGather.Use();
  accGather.SetFloat("uScrollSpeed", scrollSpeed);
  accGather.SetFloat("uDt", dt);
  accGather.SetFloat("uDownscaleFactor", (float)downscaleFactor);
  accGather.SetInt("uUseFlow", !paused);
  accGather.DispatchCompute(scaledWidth, scaledHeight, 16);
}

void Effect::NoiseScatter(const EffectInputData& in, float dt) {
  effectImgs[curr].noise.Bind(0, GL_WRITE_ONLY);
  effectImgs[prev].noise.Bind(1, GL_READ_ONLY);

  in.prevIdTex.Bind(0);
  in.currIdTex.Bind(1);
  in.prevMotionTex.Bind(2);

  //modelSSB.Bind(0);

  noiseScatter.Use();
  // noiseScatter.SetFloat("uScrollSpeed", scrollSpeed * dt / downscaleFactor * (int)!paused);
  noiseScatter.DispatchCompute(scaledWidth, scaledHeight, 16);
}

void Effect::NoiseFill(const EffectInputData& in) {
  effectImgs[curr].noise.Bind(0, GL_READ_WRITE);
  effectImgs[prev].noise.Bind(1, GL_WRITE_ONLY);

  noiseFill.Use();
  noiseFill.SetUint("uSeed", util::RandomInt());
  noiseFill.DispatchCompute(scaledWidth, scaledHeight, 16);
}

void Effect::ClearBuffers() {
  for (auto& img : effectImgs) {
    img.noise.Clear();
    img.acc.Clear();
  }
  moveStepImg.Clear();
}

void Effect::OnResize(int width, int height) {
  scaledWidth = width / downscaleFactor;
  scaledHeight = height / downscaleFactor;

  for (auto& img : effectImgs) {
    img.noise.Resize(scaledWidth, scaledHeight);
    img.acc.Resize(scaledWidth, scaledHeight);
  }
  moveStepImg.Resize(scaledWidth, scaledHeight);

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
    if (action == GLFW_PRESS) effectImgs[curr].acc.Clear();
    break;
  }
}

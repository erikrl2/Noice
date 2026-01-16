#include "effect.hpp"

#include "util.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <imgui.h>

void Effect::Init(int width, int height) {
  scrollShader.CreateCompute("assets/shaders/scroll_move.comp.glsl");
  fillShader.CreateCompute("assets/shaders/scroll_fill.comp.glsl");

  modelSSB.Create(sizeof(glm::mat4[2]) * 6, nullptr, GL_DYNAMIC_DRAW); // or GL_STREAM_DRAW ?

  scaledWidth = width / downscaleFactor;
  scaledHeight = height / downscaleFactor;

  for (auto& img : effectImgs) {
    img.noise.Create(scaledWidth, scaledHeight, GL_RG8, GL_NEAREST);
    img.acc.Create(scaledWidth, scaledHeight, GL_RG32F, GL_NEAREST);
  }

  ClearBuffers();

  // std::srand((unsigned)std::time(nullptr));
}

void Effect::Destroy() {
  modelSSB.Destroy();
  scrollShader.Destroy();
  fillShader.Destroy();

  for (auto& img : effectImgs) {
    img.noise.Destroy();
    img.acc.Destroy();
  }
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

  ScatterPass(in, dt);
  FillPass(in);

#ifdef NDEBUG // FLOW
  return !disabled ? effectImgs[curr].noise : in.prevFlowTex;
#else // ACC
  return !disabled ? effectImgs[curr].noise : effectImgs[curr].acc;
#endif
}

void Effect::ScatterPass(const EffectInputData& in, float dt) {
  if (accResetInterval > 0) {
    static unsigned frameCount = 0;
    if (++frameCount % accResetInterval == 0) effectImgs[prev].acc.Clear();
  }

  // dt = 1.0f / 144.0f; // DEBUG
  float speed = scrollSpeed * dt / downscaleFactor * (int)!paused;

  scrollShader.Use();

  effectImgs[curr].noise.Bind(0, GL_WRITE_ONLY);
  effectImgs[prev].noise.Bind(1, GL_READ_ONLY);
  effectImgs[curr].acc.Bind(2, GL_WRITE_ONLY);
  effectImgs[prev].acc.Bind(3, GL_READ_WRITE);

  in.prevFlowTex.Bind(0);
  in.currIdTex.Bind(1);

  if (in.reproject) {
    in.prevIdTex.Bind(2);
    in.prevLocalPosTex.Bind(3);

    modelSSB.Upload(in.modelMats);
    modelSSB.Bind(0);

    scrollShader.SetMat4v("uViewProj", 2, in.prevCurrViewProj);

    scrollShader.SetInt("uCurrInd", in.currInd);
    scrollShader.SetFloat("uScrollSpeed", speed);
  } else {
    in.currIdTex.Bind(2);

    // speed adjustment: scrollspeed unit here is [pixels per second] and not [pixels per worldspace-unit per second]
    scrollShader.SetFloat("uScrollSpeed", speed * 20.0f);
  }
  scrollShader.SetInt("uReproject", in.reproject);

  scrollShader.DispatchCompute(scaledWidth, scaledHeight, 16);
}

void Effect::FillPass(const EffectInputData& in) {
  fillShader.Use();

  effectImgs[curr].noise.Bind(0, GL_READ_WRITE);
  effectImgs[prev].noise.Bind(1, GL_WRITE_ONLY);
  effectImgs[curr].acc.Bind(2, GL_READ_WRITE);
  effectImgs[prev].acc.Bind(3, GL_READ_WRITE);

  in.currIdTex.Bind(0);

  fillShader.SetUint("uSeed", util::RandomInt());

  fillShader.DispatchCompute(scaledWidth, scaledHeight, 16);
}

void Effect::ClearBuffers() {
  for (auto& img : effectImgs) {
    img.noise.Clear();
    img.acc.Clear();
  }
}

void Effect::OnResize(int width, int height) {
  scaledWidth = width / downscaleFactor;
  scaledHeight = height / downscaleFactor;

  for (auto& img : effectImgs) {
    img.noise.Resize(scaledWidth, scaledHeight);
    img.acc.Resize(scaledWidth, scaledHeight);
  }

  // curr = 0, prev = 1;
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

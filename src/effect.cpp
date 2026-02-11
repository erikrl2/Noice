#include "effect.hpp"

#include "util.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <imgui.h>

void Effect::Init(int width, int height) {
  scatterShader.CreateCompute("assets/shaders/effect/effect_scatter.comp.glsl");
  fillShader.CreateCompute("assets/shaders/effect/effect_fill.comp.glsl");
  fillJfaShader.CreateCompute("assets/shaders/effect/effect_fill_jfa.comp.glsl");

  jfaInitShader.CreateCompute("assets/shaders/effect/effect_jfa_init.comp.glsl");
  jfaStepShader.CreateCompute("assets/shaders/effect/effect_jfa_step.comp.glsl");

  this->width = width;
  this->height = height;

  for (auto& img : effectImgs) {
    img.noise.Create(width, height, GL_RG8, GL_NEAREST);
    img.acc.Create(width, height, GL_RG32F, GL_NEAREST);
  }

  claimImg.Create(width, height, GL_R32UI, GL_NEAREST);

  for (auto& s : seed) s.Create(width, height, GL_RGBA16I, GL_NEAREST);

  needsJfaFlag.Create(1, 1, GL_R32UI, GL_NEAREST);
  needsJfaMask.Create(width, height, GL_R8UI, GL_NEAREST);

  ClearBuffers();
}

void Effect::Destroy() {
  modelSSB.Destroy();

  scatterShader.Destroy();
  fillShader.Destroy();
  fillJfaShader.Destroy();

  for (auto& img : effectImgs) {
    img.noise.Destroy();
    img.acc.Destroy();
  }
  claimImg.Destroy();

  jfaInitShader.Destroy();
  jfaStepShader.Destroy();
  for (auto& s : seed) s.Destroy();

  needsJfaFlag.Destroy();
  needsJfaMask.Destroy();
}

void Effect::UpdateImGui() {
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

  float speed = scrollSpeed * dt * (int)!paused;

  claimImg.Clear({-1, 0, 0, 0});

  scatterShader.Use();

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

    scatterShader.SetMat4v("uViewMat", 2, in.prevCurrView);
    scatterShader.SetMat4v("uProjMat", 2, in.prevCurrProj);
    scatterShader.SetInt("uCurrInd", in.currInd);
  }

  if (in.flow) {
    in.prevFlowTex.Bind(3);
    scatterShader.SetFloat("uScrollSpeed", speed * (in.reproject ? 1 : 20));
  }

  scatterShader.SetInt("uReproject", in.reproject);
  scatterShader.SetInt("uFlow", in.flow);

  scatterShader.DispatchCompute(width, height, 16);
}

void Effect::FillPass(const EffectInputData& in) {
  needsJfaFlag.Clear();
  needsJfaMask.Clear();

  fillShader.Use();

  effectImgs[curr].noise.Bind(0, GL_READ_WRITE);
  effectImgs[prev].noise.Bind(1, GL_WRITE_ONLY);
  effectImgs[curr].acc.Bind(2, GL_READ_WRITE);
  effectImgs[prev].acc.Bind(3, GL_READ_WRITE);

  needsJfaFlag.Bind(5, GL_READ_WRITE);
  needsJfaMask.Bind(6, GL_WRITE_ONLY);

  in.currIdTex.Bind(0);
  in.prevIdTex.Bind(1);

  fillShader.SetUint("uSeed", util::RandomInt());

  fillShader.DispatchCompute(width, height, 16);

  if (accResetInterval > 0) return;

  BuildAccSeedMap(in);

  fillJfaShader.Use();

  effectImgs[curr].acc.Bind(2, GL_READ_WRITE);
  seed[lastSeed].Bind(4, GL_READ_ONLY);
  needsJfaFlag.Bind(5, GL_READ_ONLY);
  needsJfaMask.Bind(6, GL_READ_ONLY);

  in.currIdTex.Bind(0);

  fillJfaShader.DispatchCompute(width, height, 16);
}

void Effect::BuildAccSeedMap(const EffectInputData& in) {
  jfaInitShader.Use();
  seed[0].Bind(0, GL_WRITE_ONLY);
  effectImgs[curr].noise.Bind(1, GL_READ_ONLY);
  needsJfaFlag.Bind(5, GL_READ_ONLY);
  in.currIdTex.Bind(0);
  jfaInitShader.DispatchCompute(width, height, 16);

  int maxDim = std::max(width, height);
  int step = 1;
  while (step < maxDim) step <<= 1;

  int src = 0;
  for (step >>= 1; step >= 1; step >>= 1) {
    jfaStepShader.Use();
    seed[src].Bind(0, GL_READ_ONLY);
    seed[1 - src].Bind(1, GL_WRITE_ONLY);
    needsJfaFlag.Bind(5, GL_READ_ONLY);
    in.currIdTex.Bind(0);
    jfaStepShader.SetInt("uStep", step);
    jfaStepShader.DispatchCompute(width, height, 16);
    src = 1 - src;
  }

  lastSeed = src;
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
  this->width = width;
  this->height = height;

  for (auto& img : effectImgs) {
    img.noise.Resize(width, height);
    img.acc.Resize(width, height);
  }
  claimImg.Resize(width, height);
  for (auto& s : seed) s.Resize(width, height);

  needsJfaMask.Resize(width, height);

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

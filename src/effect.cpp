#include "effect.hpp"

#include "util.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <imgui.h>

void Effect::Init(int width, int height) {
  const std::string resourceDir = "assets/shaders/effect/";

  scatterShader.CreateCompute(resourceDir + "effect_scatter.comp.glsl");
  fillShader.CreateCompute(resourceDir + "effect_fill.comp.glsl");
  fillJfaShader.CreateCompute(resourceDir + "effect_fill_jfa.comp.glsl");

  jfaInitShader.CreateCompute(resourceDir + "effect_jfa_init.comp.glsl");
  jfaStepShader.CreateCompute(resourceDir + "effect_jfa_step.comp.glsl");
  jfaIndirectArgsShader.CreateCompute(resourceDir + "effect_jfa_indirect_args.comp.glsl");

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

  struct Args {
    unsigned gx, gy, gz;
  } initArgs{0, 0, 0};
  jfaIndirectArgsSSB.Create(sizeof(Args), &initArgs, GL_DYNAMIC_DRAW);

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

  jfaIndirectArgsShader.Destroy();
  jfaIndirectArgsSSB.Destroy();
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
  if (disabled && !showAcc) return in.prevFlowTex;

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
  scatterShader.SetMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
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
  fillShader.SetMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

  if (accResetInterval > 0) return;

  // ---
  // Use Jump Flooding to fill large disocclusion holes in acc if needed.
  // Only really useful for 2D scenes that require precise/consistent acc values.
  // To avoid generating unnessary jfa seed maps, dispatch group sizes are conditionally set to zero if not needed.

  jfaIndirectArgsShader.Use();
  needsJfaFlag.Bind(5, GL_READ_ONLY);
  jfaIndirectArgsSSB.Bind(1);
  jfaIndirectArgsShader.SetUint("uGroupsX", (width + 16 - 1) / 16);
  jfaIndirectArgsShader.SetUint("uGroupsY", (height + 16 - 1) / 16);
  jfaIndirectArgsShader.DispatchCompute(1, 1, 1);
  jfaIndirectArgsShader.SetMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

  BuildAccSeedMap(in);

  fillJfaShader.Use();

  effectImgs[curr].acc.Bind(2, GL_READ_WRITE);
  seed[lastSeed].Bind(4, GL_READ_ONLY);
  needsJfaMask.Bind(6, GL_READ_ONLY);

  in.currIdTex.Bind(0);

  fillJfaShader.DispatchComputeIndirect(jfaIndirectArgsSSB);
  fillJfaShader.SetMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void Effect::BuildAccSeedMap(const EffectInputData& in) {
  jfaInitShader.Use();
  seed[0].Bind(0, GL_WRITE_ONLY);
  effectImgs[curr].noise.Bind(1, GL_READ_ONLY);
  in.currIdTex.Bind(0);
  jfaInitShader.DispatchComputeIndirect(jfaIndirectArgsSSB);
  jfaInitShader.SetMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

  int maxDim = std::max(width, height);
  int step = 1;
  while (step < maxDim) step <<= 1;

  int src = 0;
  for (step >>= 1; step >= 1; step >>= 1) {
    jfaStepShader.Use();
    seed[src].Bind(0, GL_READ_ONLY);
    seed[1 - src].Bind(1, GL_WRITE_ONLY);
    in.currIdTex.Bind(0);
    jfaStepShader.SetInt("uStep", step);

    jfaStepShader.DispatchComputeIndirect(jfaIndirectArgsSSB);
    jfaStepShader.SetMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
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

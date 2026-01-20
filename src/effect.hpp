#pragma once
#include "framebuffer.hpp"
#include "shader.hpp"

#include <span>

struct EffectInputData {
  Texture currFlowTex, prevFlowTex;
  Texture currMotionTex, prevMotionTex;
  Texture currIdTex, prevIdTex;
  Texture currLocalPosTex, prevLocalPosTex;

  glm::mat4* prevCurrViewProj = nullptr;
  std::span<glm::mat4[2]> modelMats;

  int currInd = 0;
  bool reproject = false;
};

class Effect {
public:
  float scrollSpeed = 7.0f;
  int accResetInterval = 0;
  int downscaleFactor = 1;
  bool paused = true;
  bool disabled = false;

public:
  void Init(int width, int height);
  void Destroy();

  void UpdateImGui();

  Texture Apply(const EffectInputData& in, float dt);

  void ClearBuffers();

  void OnResize(int width, int height);
  void OnMouseClicked(int button, int action);
  void OnMouseScrolled(float offset);
  void OnKeyPressed(int key, int action);

  int GetWidth() const { return scaledWidth; }
  int GetHeight() const { return scaledHeight; }

private:
  void AccGather(const EffectInputData& in, float dt);
  void NoiseScatter(const EffectInputData& in, float dt);
  void NoiseFill(const EffectInputData& in);

private:
  // scaled size
  int scaledWidth = 0, scaledHeight = 0;

  struct EffectImage {
    Image noise;
    Image acc;
  };

  EffectImage effectImgs[2];
  int curr = 0, prev = 1;

  Image moveStepImg;
  Image claimImg;

  Shader accGather;
  Shader noiseScatter;
  Shader noiseFill;

  StorageBuffer modelSSB;
};

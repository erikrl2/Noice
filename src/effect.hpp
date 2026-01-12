#pragma once
#include "framebuffer.hpp"
#include "shader.hpp"

#include <span>

struct EffectInputData {
  Texture currFlowTex, currIdTex;
  bool reproject = false;
  Texture prevLocalPosTex, prevIdTex;
  glm::mat4* prevCurrViewProj;
  std::span<glm::mat4[2]> modelMats;
  int currInd = 0;
};

class Effect {
public:
  float scrollSpeed = 7.0f;
  int accResetInterval = 10;
  int downscaleFactor = 1;
  bool paused = false;
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

private:
  void ScatterPass(const EffectInputData& in, float dt);
  void FillPass();

private:
  // scaled size
  int scaledWidth = 0, scaledHeight = 0;

  struct EffectImage {
    Image noise;
    Image acc;
  };

  EffectImage effectImgs[2];
  int curr = 0, prev = 1;

  Shader scrollShader;
  Shader fillShader;

  StorageBuffer modelSSB;
};

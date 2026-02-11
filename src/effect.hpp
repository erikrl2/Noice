#pragma once
#include "framebuffer.hpp"
#include "shader.hpp"

#include <span>

struct EffectInputData {
  Texture currIdTex;

  bool reproject = false;
  Texture prevIdTex;
  Texture prevDepthTex;
  glm::mat4* prevCurrProj = nullptr;
  glm::mat4* prevCurrView = nullptr;
  std::span<glm::mat4[2]> modelMats;
  int currInd = 0;

  bool flow = true;
  Texture prevFlowTex;
};

class Effect {
public:
  float scrollSpeed = 3.0f;
  int accResetInterval = 0;
  bool paused = false;
  bool disabled = false;
  bool showAcc = false;

public:
  Effect() { self = this; }

  void Init(int width, int height);
  void Destroy();

  void UpdateImGui();

  Texture Apply(const EffectInputData& in, float dt);

  void ClearBuffers();
  void ClearAcc();

  void OnResize(int width, int height);
  void OnMouseClicked(int button, int action);
  void OnMouseScrolled(float offset);
  void OnKeyPressed(int key, int action);

  static Effect* Get() { return self; }

private:
  void ScatterPass(const EffectInputData& in, float dt);
  void FillPass(const EffectInputData& in);

  void BuildAccSeedMap(const EffectInputData& in);

private:
  int width = 0, height = 0;

  struct EffectImage {
    Image noise;
    Image acc;
  };

  EffectImage effectImgs[2];
  int curr = 0, prev = 1;

  Image claimImg;

  Shader scatterShader;
  Shader fillShader;

  Shader fillJfaShader;
  Image needsJfaFlag;
  Image needsJfaMask;

  Shader jfaInitShader;
  Shader jfaStepShader;
  Image seed[2];
  int lastSeed = 0;

  StorageBuffer modelSSB;

private:
  inline static Effect* self = nullptr;
};

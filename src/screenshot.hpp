#pragma once
#include "framebuffer.hpp"
#include "shader.hpp"

#include <string>

class Screenshot {
public:
  enum class Method { Average = 0, AbsDiffSum = 1, Count };

  struct Options {
    Method method = Method::Average;
    int targetFrames = 30;
    float gain = 1.0f;
    float gamma = 1.0f;
    std::string baseName = "capture";
  };

public:
  Screenshot() = default;
  ~Screenshot() { Destroy(); }

  void Init(int width, int height);
  void Destroy();

  void UpdateImGui();
  void Update(const Texture& source);

  void OnMouseClicked(int button, int action);
  void OnKeyPressed(int key, int action);

  bool IsCapturing() const { return capturing; }
  bool IsActive() const { return hasResult || capturing; }
  const Texture& GetResultTex() const { return outTex; }

private:
  void Begin();
  void Reset();

  void Accumulate(const Texture& source);
  void Finalize();
  void ClearBuffers();
  void ResizeBuffers(int w, int h);

  void SavePNG();

private:
  int width = 0, height = 0;

  bool capturing = false;
  bool hasResult = false;

  Options options;
  int collectedFrames = 0;

  Shader accumShader;
  Shader finalizeShader;

  Texture accumTex;
  Texture prevTex;
  Texture outTex;
};

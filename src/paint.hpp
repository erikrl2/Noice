#pragma once
#include "framebuffer.hpp"
#include "mesh.hpp"
#include "mode.hpp"
#include "shader.hpp"

class PaintMode : public Mode {
public:
  void Init(int width, int height);
  void Destroy();

  void Update(float dt) override;
  void UpdateImGui() override;

  void OnResize(int width, int height) override;
  void OnMouseClicked(int button, int action) override;
  void OnMouseMoved(double xpos, double ypos) override;
  void OnMouseScrolled(float offset) override;

  Framebuffer& GetResultFB() override { return resultFB; }

private:
  void PaintSegment(const glm::vec2& from, const glm::vec2& to);
  void StampAt(const glm::vec2& center, const glm::vec2& dir);

  void Resolve();

private:
  float brushRadius = 30.0f;

  bool leftDown = false;
  bool havePrev = false;
  glm::vec2 mousePos{0.0f, 0.0f};
  glm::vec2 prevPos{0.0f, 0.0f};

  Shader stampShader;
  Shader resolveShader;

  Framebuffer canvasFB;
  Framebuffer resultFB;

  Mesh quad;
};

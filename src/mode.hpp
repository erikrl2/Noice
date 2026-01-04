#pragma once
#include "framebuffer.hpp"

class Mode {
public:
  virtual void UpdateImGui() = 0;
  virtual void Update(float dt) = 0;

  virtual void OnResize(int width, int height) {}
  virtual void OnMouseClicked(int button, int action) {}
  virtual void OnMouseMoved(double xpos, double ypos) {}
  virtual void OnMouseScrolled(float offset) {}
  virtual void OnKeyPressed(int key, int action) {}
  virtual void OnFileDrop(const std::string& path) {}

  virtual Framebuffer& GetResultFB() = 0;
};

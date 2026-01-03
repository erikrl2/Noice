#pragma once
#include "effect.hpp"
#include "object.hpp"
#include "paint.hpp"
#include "screenshot.hpp"
#include "shader.hpp"
#include "text.hpp"

#include <GLFW/glfw3.h>

class App {
public:
  App();
  ~App();

  void Run();

private:
  void InitWindow();
  void InitOpenGL();
  void InitImGui();
  void SetupResources();
  void DestroyResources();

  void UpdateImGui();
  void Update(float dt);

  void RenderToScreen();

  static void OnFramebufferResized(GLFWwindow* window, int w, int h);
  static void OnMouseMoved(GLFWwindow* window, double xpos, double ypos);
  static void OnMouseScroll(GLFWwindow* window, double xoffset, double yoffset);
  static void OnMouseClicked(GLFWwindow* window, int button, int action, int mods);
  static void OnKeyPressed(GLFWwindow* window, int key, int scancode, int action, int mods);
  static void OnFileDrop(GLFWwindow* window, int count, const char** paths);

  void OnModeChange();
  void SetModePointer();

  void CheckWindowSize();

private:
  GLFWwindow* win = nullptr;
  int width = 1280;
  int height = 720;
  bool minimized = false;

  Mesh quadMesh;
  Shader postShader;

  Effect effect;

  ObjectMode objectMode;
  TextMode textMode;
  PaintMode paintMode;

  enum class ModeType { Object, Text, Paint, Count };
  ModeType modeSelect = ModeType::Object;
  Mode* modePtr = nullptr;

  Screenshot screenshot;

  bool showSettings = true;
};

#include "screenshot.hpp"

#include "util.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <stb_image_write.h>

#include <iostream>

void Screenshot::Init(int width, int height) {
  accumShader.CreateCompute("assets/shaders/screenshot/screenshot_accum.comp.glsl");
  finalizeShader.CreateCompute("assets/shaders/screenshot/screenshot_finalize.comp.glsl");

  accumImg.Create(width, height, GL_R16F, GL_NEAREST);
  prevImg.Create(width, height, GL_R8, GL_NEAREST);
  outImg.Create(width, height, GL_R8, GL_NEAREST);

  this->width = width;
  this->height = height;
}

void Screenshot::Destroy() {
  accumShader.Destroy();
  finalizeShader.Destroy();

  accumImg.Destroy();
  prevImg.Destroy();
  outImg.Destroy();
}

void Screenshot::UpdateImGui() {
  const char* methods[] = {"AVG", "SAD"};
  int m = (int)options.method;

  ImGui::BeginDisabled(capturing);
  if (ImGui::Combo("Method", &m, methods, (int)Method::Count)) {
    options.method = (Method)m;
  }
  ImGui::DragInt("Frames", &options.targetFrames, 0.25f, 1, 1000, "%d", ImGuiSliderFlags_ClampOnInput);
  ImGui::DragFloat("Gain", &options.gain, 0.01f, 0.0f, 5.0f, "%.2f", ImGuiSliderFlags_ClampOnInput);
  ImGui::DragFloat("Gamma", &options.gamma, 0.01f, 0.1f, 5.0f, "%.2f", ImGuiSliderFlags_ClampOnInput);
  ImGui::EndDisabled();

  static char baseNameBuf[128] = "capture";
  if (hasResult) {
    if (ImGui::InputText("Filename", baseNameBuf, sizeof(baseNameBuf))) options.baseName = baseNameBuf;
  }

  if (!capturing) {
    if (!hasResult) {
      if (ImGui::Button("Start capture")) Begin();
      ImGui::SetItemTooltip("shortcut: C");
    }
  } else {
    if (ImGui::Button("Cancel capture")) Reset();
    ImGui::Text("Capturing: %d / %d", collectedFrames, options.targetFrames);
  }

  if (hasResult) {
    if (ImGui::Button("Save PNG")) SavePNG();
    ImGui::SameLine();
    if (ImGui::Button("Recapture")) Begin();
    ImGui::SetItemTooltip("shortcut: C");
    ImGui::SameLine();
    if (ImGui::Button("Close preview")) Reset();
    ImGui::SetItemTooltip("shortcut: ESC or right click");
  }
}

void Screenshot::Update(int width, int height, Texture source) {
  if (this->width != width || this->height != height) ResizeBuffers(width, height);
  if (!capturing) return;

  Accumulate(source);
  collectedFrames++;

  Finalize();

  if (collectedFrames >= options.targetFrames) {
    capturing = false;
    hasResult = true;
  }
}

void Screenshot::Accumulate(Texture source) {
  accumShader.Use();

  accumImg.Bind(0, GL_READ_WRITE);
  prevImg.Bind(1, GL_READ_WRITE);

  source.Bind(0);

  accumShader.SetInt("uMethod", (int)options.method);
  accumShader.SetInt("uFrameIndex", collectedFrames);

  accumShader.DispatchCompute(width, height, 16);
}

void Screenshot::Finalize() {
  finalizeShader.Use();

  accumImg.Bind(0, GL_READ_ONLY);
  outImg.Bind(1, GL_WRITE_ONLY);

  finalizeShader.SetInt("uMethod", (int)options.method);
  finalizeShader.SetInt("uFrames", collectedFrames);
  finalizeShader.SetFloat("uGain", options.gain);
  finalizeShader.SetFloat("uGamma", options.gamma);

  finalizeShader.DispatchCompute(width, height, 16);
}

void Screenshot::Begin() {
  capturing = true;
  hasResult = false;
  collectedFrames = 0;

  ClearBuffers();
}

void Screenshot::Reset() {
  capturing = false;
  hasResult = false;
  collectedFrames = 0;
}

void Screenshot::ClearBuffers() {
  accumImg.Clear();
  prevImg.Clear();
  outImg.Clear();
}

void Screenshot::ResizeBuffers(int width, int height) {
  this->width = width, this->height = height;

  accumImg.Resize(width, height);
  prevImg.Resize(width, height);
  outImg.Resize(width, height);

  Reset();
}

void Screenshot::OnMouseClicked(int button, int action) {
  if (action == GLFW_PRESS) {
    if (IsActive()) Reset();
  }
}

void Screenshot::OnKeyPressed(int key, int action) {
  if (key == GLFW_KEY_C && action == GLFW_PRESS) {
    if (!util::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) Begin();
  }
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    if (IsActive()) Reset();
  }
}

void Screenshot::SavePNG() {
  if (!hasResult) return;

  std::vector<unsigned char> pixels = outImg.Download();

  std::string filename = options.baseName + ".png";

  stbi_flip_vertically_on_write(1);
  int ok = stbi_write_png(filename.c_str(), outImg.GetWidth(), outImg.GetHeight(), 1, pixels.data(), outImg.GetWidth());

  if (!ok) {
    std::cerr << "Screenshot: failed to write png: " << filename << "\n";
  } else {
    std::cout << "Screenshot: saved " << filename << " (" << outImg.GetWidth() << "x" << outImg.GetHeight() << ")\n";
  }
}

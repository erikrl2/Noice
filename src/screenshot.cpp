#include "screenshot.hpp"

#include "util.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <stb_image_write.h>

#include <iostream>

void Screenshot::Init(int width, int height) {
  accumShader.CreateCompute("assets/shaders/screenshot_accum.comp.glsl");
  finalizeShader.CreateCompute("assets/shaders/screenshot_finalize.comp.glsl");

  accumTex.Create(width, height, GL_R16F, GL_NEAREST);
  prevTex.Create(width, height, GL_R8, GL_NEAREST);
  outTex.Create(width, height, GL_R8, GL_NEAREST);

  this->width = width;
  this->height = height;
}

void Screenshot::Destroy() {
  accumShader.Destroy();
  finalizeShader.Destroy();

  accumTex.Destroy();
  prevTex.Destroy();
  outTex.Destroy();
}

void Screenshot::UpdateImGui() {
  const char* methods[] = {"Average", "Average of changes"};
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
    if (!hasResult)
      if (ImGui::Button("Start capture")) Begin();
  } else {
    if (ImGui::Button("Cancel capture")) Reset();
    ImGui::Text("Capturing: %d / %d", collectedFrames, options.targetFrames);
  }

  if (hasResult) {
    if (ImGui::Button("Save PNG")) SavePNG();
    ImGui::SameLine();
    if (ImGui::Button("Recapture")) Begin();
    ImGui::SameLine();
    if (ImGui::Button("Close preview")) Reset();
  }
}

void Screenshot::Update(const Texture& source) {
  if (source.width != width || source.height != height) ResizeBuffers(source.width, source.height);
  if (!capturing) return;

  Accumulate(source);
  collectedFrames++;

  Finalize();

  if (collectedFrames >= options.targetFrames) {
    capturing = false;
    hasResult = true;
  }
}

void Screenshot::Accumulate(const Texture& source) {
  accumShader.Use();

  accumShader.SetTexture("uSourceTex", source, 0);

  accumShader.SetImage("uAccumTex", accumTex, 0, GL_READ_WRITE);
  accumShader.SetImage("uPrevTex", prevTex, 1, GL_READ_WRITE);

  accumShader.SetInt("uMethod", (int)options.method);
  accumShader.SetInt("uFrameIndex", collectedFrames);

  accumShader.DispatchCompute(outTex.width, outTex.height, 16);
}

void Screenshot::Finalize() {
  finalizeShader.Use();

  finalizeShader.SetImage("uAccumTex", accumTex, 0, GL_READ_ONLY);
  finalizeShader.SetImage("uOutTex", outTex, 1, GL_WRITE_ONLY);

  finalizeShader.SetInt("uMethod", (int)options.method);
  finalizeShader.SetInt("uFrames", collectedFrames);
  finalizeShader.SetFloat("uGain", options.gain);
  finalizeShader.SetFloat("uGamma", options.gamma);

  finalizeShader.DispatchCompute(outTex.width, outTex.height, 16);
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
  accumTex.Clear();
  prevTex.Clear();
  outTex.Clear();
}

void Screenshot::ResizeBuffers(int w, int h) {
  width = w, height = h;

  accumTex.Resize(width, height);
  prevTex.Resize(width, height);
  outTex.Resize(width, height);

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

  std::vector<unsigned char> pixels = outTex.Download();

  std::string filename = options.baseName + ".png";

  stbi_flip_vertically_on_write(1);
  int ok = stbi_write_png(filename.c_str(), outTex.width, outTex.height, 1, pixels.data(), outTex.width);

  if (!ok) {
    std::cerr << "Screenshot: failed to write png: " << filename << "\n";
  } else {
    std::cout << "Screenshot: saved " << filename << " (" << outTex.width << "x" << outTex.height << ")\n";
  }
}

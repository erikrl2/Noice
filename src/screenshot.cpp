#include "screenshot.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <stb_image_write.h>

#include <iostream>

void Screenshot::Init(int width, int height) {
  accumShader.CreateCompute("assets/shaders/screenshot_accum.comp.glsl");
  finalizeShader.CreateCompute("assets/shaders/screenshot_finalize.comp.glsl");

  sumTex.Create(width, height, GL_R16F, GL_NEAREST);
  diffSumTex.Create(width, height, GL_R16F, GL_NEAREST);
  prevTex.Create(width, height, GL_R8, GL_NEAREST);
  outTex.Create(width, height, GL_R8, GL_NEAREST);

  this->width = width;
  this->height = height;
}

void Screenshot::Destroy() {
  accumShader.Destroy();
  finalizeShader.Destroy();

  sumTex.Destroy();
  diffSumTex.Destroy();
  prevTex.Destroy();
  outTex.Destroy();
}

void Screenshot::UpdateImGui() {
  const char* methods[] = {"Average", "AbsDiffSum"};
  int m = (int)options.method;

  ImGui::BeginDisabled(capturing);
  if (ImGui::Combo("Method", &m, methods, (int)Method::Count)) {
    options.method = (Method)m;
  }
  ImGui::DragInt("Frames", &options.targetFrames, 1.0f, 1, 5000, "%d", ImGuiSliderFlags_NoInput);
  ImGui::DragFloat("Gain", &options.gain, 0.01f, 0.0f, 100.0f, "%.2f", ImGuiSliderFlags_NoInput);
  ImGui::DragFloat("Gamma", &options.gamma, 0.01f, 0.1f, 5.0f, "%.2f", ImGuiSliderFlags_NoInput);
  ImGui::EndDisabled();

  static char baseNameBuf[128] = "capture";
  if (hasResult) {
    if (ImGui::InputText("Filename", baseNameBuf, sizeof(baseNameBuf))) options.baseName = baseNameBuf;
  }

  if (!capturing) {
    if (!hasResult)
      if (ImGui::Button("Start capture")) Begin();
  } else {
    if (ImGui::Button("Cancel capture")) Cancel();
    ImGui::Text("Capturing: %d / %d", collectedFrames, options.targetFrames);
  }

  if (hasResult) {
    if (ImGui::Button("Save PNG")) SavePNG();
    ImGui::SameLine();
    if (ImGui::Button("Close preview")) hasResult = false;
  }
}

void Screenshot::Update(const Texture& source) {
  if (source.width != width || source.height != height) ResizeBuffers(source.width, source.height);
  if (!capturing) return;

  Accumulate(source);
  collectedFrames++;

  if (collectedFrames >= options.targetFrames) {
    FinalizeToRGBA8();
    capturing = false;
    hasResult = true;
  }
}

void Screenshot::Accumulate(const Texture& source) {
  accumShader.Use();

  accumShader.SetTexture("uSourceTex", source, 0);

  accumShader.SetImage("uSumTex", sumTex, 0, GL_READ_WRITE);
  accumShader.SetImage("uDiffSumTex", diffSumTex, 1, GL_READ_WRITE);
  accumShader.SetImage("uPrevTex", prevTex, 2, GL_READ_WRITE);

  accumShader.SetInt("uMethod", (int)options.method);
  accumShader.SetInt("uFrameIndex", collectedFrames);

  accumShader.DispatchCompute(outTex.width, outTex.height, 16);
}

void Screenshot::FinalizeToRGBA8() {
  finalizeShader.Use();

  finalizeShader.SetImage("uSumTex", sumTex, 0, GL_READ_ONLY);
  finalizeShader.SetImage("uDiffSumTex", diffSumTex, 1, GL_READ_ONLY);

  finalizeShader.SetImage("uOutTex", outTex, 2, GL_WRITE_ONLY);

  finalizeShader.SetInt("uMethod", (int)options.method);
  finalizeShader.SetInt("uFrames", std::max(1, collectedFrames));
  finalizeShader.SetFloat("uGain", options.gain);
  finalizeShader.SetFloat("uGamma", options.gamma);

  finalizeShader.DispatchCompute(outTex.width, outTex.height, 16);
}

void Screenshot::Begin() {
  capturing = true;
  collectedFrames = 0;

  ResetBuffers();
}

void Screenshot::Cancel() {
  capturing = false;
  hasResult = false;
  collectedFrames = 0;
}

void Screenshot::ResetBuffers() {
  sumTex.Clear();
  diffSumTex.Clear();
  prevTex.Clear();
  outTex.Clear();
}

void Screenshot::ResizeBuffers(int w, int h) {
  width = w, height = h;

  sumTex.Resize(width, height);
  diffSumTex.Resize(width, height);
  prevTex.Resize(width, height);
  outTex.Resize(width, height);

  if (capturing) Cancel();

  hasResult = false;
}

void Screenshot::OnMouseClicked(int button, int action) {
  hasResult = false;
}

void Screenshot::OnKeyPressed(int key, int action) {
  if (key == GLFW_KEY_C && action == GLFW_PRESS) {
    hasResult = false;
    Begin();
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

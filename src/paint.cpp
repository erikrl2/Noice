#include "paint.hpp"

#include "effect.hpp"
#include "util.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>

void PaintMode::Init(int width, int height) {
  quad.CreateFullscreenQuad();

  canvasFB.CreateOrResize(width, height);
  canvasFB.AttachColorTexture(GL_RG16F, GL_NEAREST); // 0: flow
  canvasFB.Finalize();

  resultFB.CreateOrResize(width, height);
  resultFB.AttachColorTexture(GL_RG16F, GL_NEAREST); // 0: flow
  resultFB.AttachColorTexture(GL_R8I, GL_NEAREST); // 1: id
  resultFB.SetClearColor({-1, 0, 0, 0}, 1);
  resultFB.Finalize();

  stampShader.CreateVertFrag(
      util::AssetPath("shaders/post.vert.glsl"), util::AssetPath("shaders/paint/paint_stamp.frag.glsl")
  );
  resolveShader.CreateVertFrag(
      util::AssetPath("shaders/post.vert.glsl"), util::AssetPath("shaders/paint/paint_resolve.frag.glsl")
  );

  canvasFB.Clear();
}

void PaintMode::Destroy() {
  stampShader.Destroy();
  resolveShader.Destroy();
  canvasFB.Destroy();
  resultFB.Destroy();
  quad.Destroy();
}

void PaintMode::UpdateImGui() {
  ImGui::DragFloat("Brush Radius", &brushRadius, 0.1f, 1.0f, 100.0f, "%.0f", ImGuiSliderFlags_ClampOnInput);
  if (ImGui::Button("Clear Canvas")) canvasFB.Clear();
  ImGui::SetItemTooltip("shortcut: right click");
}

void PaintMode::Update(float dt) {
  if (leftDown) {
    if (!havePrev) {
      prevPos = mousePos;
      havePrev = true;
    } else {
      PaintSegment(prevPos, mousePos);
      prevPos = mousePos;
    }
  }

  Resolve();
}

void PaintMode::PaintSegment(const glm::vec2& from, const glm::vec2& to) {
  glm::vec2 delta = to - from;
  float dist = glm::length(delta);
  if (dist < 1e-4f) return;

  glm::vec2 dir = glm::normalize(delta);

  float spacing = std::max(0.5f, brushRadius * 0.25f);
  int steps = std::max(1, (int)std::ceil(dist / spacing));

  for (int i = 0; i <= steps; i++) {
    float t = float(i) / float(steps);
    glm::vec2 center = from + delta * t;
    StampAt(center, dir);
  }
}

void PaintMode::StampAt(const glm::vec2& center, const glm::vec2& dir) {
  canvasFB.Bind();
  stampShader.Use();

  canvasFB.GetColorTexture().Bind(0);

  stampShader.SetVec2("uCenter", center);
  stampShader.SetFloat("uRadius", brushRadius);
  stampShader.SetVec2("uDir", dir);

  quad.Draw();
}

void PaintMode::Resolve() {
  resultFB.Clear();
  resolveShader.Use();

  canvasFB.GetColorTexture().Bind(0);

  quad.Draw();
}

void PaintMode::OnResize(int width, int height) {
  canvasFB.CreateOrResize(width, height);
  resultFB.CreateOrResize(width, height);
}

void PaintMode::OnMouseMoved(double xpos, double ypos) {
  mousePos = glm::vec2(float(xpos), float(ypos)) * util::GetDpiScaleFactor();
}

void PaintMode::OnMouseScrolled(float offset) {
  // if (offset > 0.0f) {
  //   brushRadius += 2.0f;
  // } else if (offset < 0.0f) {
  //   brushRadius -= 2.0f;
  //   brushRadius = std::max(2.0f, brushRadius);
  // }
}

void PaintMode::OnMouseClicked(int button, int action) {
  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    if (action == GLFW_PRESS) {
      leftDown = true;
      havePrev = false;
    } else {
      leftDown = false;
      havePrev = false;
    }
  }
  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
    canvasFB.Clear();
  }
}

EffectInputData PaintMode::GetEffectInputData() {
  EffectInputData data;
  data.prevFlowTex = resultFB.GetColorTexture(0);
  data.currIdTex = resultFB.GetColorTexture(1);
  return data;
}

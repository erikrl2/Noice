#include "text.hpp"

#include "util.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <stb_truetype.h>

static const char* fontPath = "assets/fonts/courier-mon.ttf";

void TextMode::Init(int width, int height) {
  textFB.Create(width, height, GL_RG16F, GL_LINEAR);
  textShader.Create("assets/shaders/text.vert.glsl", "assets/shaders/text.frag.glsl");

  LoadFontAtlas();
}

void TextMode::Destroy() {
  DestroyFontAtlas();
  textMesh.Destroy();
  textShader.Destroy();
  textFB.Destroy();
}

void TextMode::UpdateImGui() {
  if (ImGui::InputTextMultiline("Text", &text, ImVec2(0, 32), ImGuiInputTextFlags_WordWrap)) dirtyMesh = true;

  if (ImGui::SliderFloat("Font Size", &bakeFontPx, 20.0f, 290.0f, "%.0f", ImGuiSliderFlags_NoInput)) LoadFontAtlas();

  int scaleSliderFlag = ImGuiSliderFlags_ClampOnInput | ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoRoundToFormat;
  dirtyMesh |= ImGui::DragFloat("Scale", &scale, 0.1f, 0.1f, 100.0f, "%.2f", scaleSliderFlag);
  dirtyMesh |= ImGui::SliderFloat("Wrap width", &wrapWidthFrac, 0.1f, 1.0f, "%.2f", ImGuiSliderFlags_NoInput);

  util::ImGuiDirection2D("Text", direction);
  ImGui::SameLine();
  util::ImGuiDirection2D("Background", bgDir);

  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 39.0f);
  dirtyMesh |= ImGui::Checkbox("Center", &center);
}

void TextMode::Update(float dt) {
  if (!ImGui::GetIO().WantCaptureKeyboard) {
    auto win = glfwGetCurrentContext();
    if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) {
      scale *= 1.0f + dt;
      dirtyMesh = true;
    }
    if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) {
      scale *= 1.0f - dt;
      dirtyMesh = true;
    }
  }

  if (dirtyMesh) RebuildTextMesh();

  textFB.Clear(glm::vec4(bgDir.x, bgDir.y, 0.0f, 0.0f));

  textShader.Use();
  textShader.SetVec2("uScreenSize", {textFB.tex.width, textFB.tex.height});
  textShader.SetVec2("uDir", direction);
  textShader.SetTexture("uFontAtlas", fontAtlasTex);

  textMesh.Draw();
}

void TextMode::LoadFontAtlas() {
  std::vector<unsigned char> newTtf;
  if (!util::ReadFileBytes(fontPath, newTtf)) return;

  ttfBuffer.swap(newTtf);

  atlasPixels.assign((size_t)atlasW * (size_t)atlasH, 0);

  int res = stbtt_BakeFontBitmap(
      ttfBuffer.data(), 0, bakeFontPx, atlasPixels.data(), atlasW, atlasH, firstChar, charCount, baked
  );

  if (res <= 0) return;

  if (fontAtlasTex.id == 0) {
    fontAtlasTex.Create(atlasW, atlasH, GL_R8, GL_LINEAR, GL_CLAMP_TO_EDGE);
  }

  fontAtlasTex.Upload(atlasPixels.data());

  dirtyMesh = true;
}

void TextMode::DestroyFontAtlas() {
  fontAtlasTex.Destroy();
  atlasPixels.clear();
  ttfBuffer.clear();
}

struct TextVertex {
  float pos[2];
  float uv[2];
};

static inline void AddQuad(
    std::vector<TextVertex>& v,
    std::vector<unsigned int>& idx,
    float x0,
    float y0,
    float x1,
    float y1,
    float u0,
    float v0,
    float u1,
    float v1
);

void TextMode::RebuildTextMesh() {
  dirtyMesh = false;

  textMesh.Destroy();

  if (text.empty()) return;

  std::vector<TextVertex> verts;
  std::vector<unsigned int> indices;
  verts.reserve(text.size() * 4);
  indices.reserve(text.size() * 6);

  float screenW = (float)textFB.tex.width;
  float screenH = (float)textFB.tex.height;
  float wrapWidth = (screenW * wrapWidthFrac) / scale;

  // Pass 1: layout text with word-based wrapping and compute bounds
  std::vector<stbtt_aligned_quad> quads;
  quads.reserve(text.size());

  float x = 0.0f;
  float y = 0.0f;
  float minX = 1e9f, minY = 1e9f;
  float maxX = -1e9f, maxY = -1e9f;

  size_t i = 0;
  while (i < text.size()) {
    char c = text[i];

    if (c == '\n') {
      x = 0.0f;
      y += bakeFontPx;
      ++i;
      continue;
    }

    if (c == ' ' || c == '\t') {
      int code = (unsigned char)c;
      if (code >= firstChar && code < firstChar + charCount) {
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(baked, atlasW, atlasH, code - firstChar, &x, &y, &q, 1);
        quads.push_back(q);

        minX = (q.x0 < minX) ? q.x0 : minX;
        minY = (q.y0 < minY) ? q.y0 : minY;
        maxX = (q.x1 > maxX) ? q.x1 : maxX;
        maxY = (q.y1 > maxY) ? q.y1 : maxY;
      }
      ++i;
      continue;
    }

    size_t wordStart = i;
    size_t wordEnd = i;
    while (wordEnd < text.size() && text[wordEnd] != ' ' && text[wordEnd] != '\t' && text[wordEnd] != '\n') ++wordEnd;

    float wordStartX = x;
    float tempX = x;
    float tempY = y;

    std::vector<stbtt_aligned_quad> wordQuads;
    for (size_t j = wordStart; j < wordEnd; ++j) {
      int code = (unsigned char)text[j];
      if (code < firstChar || code >= firstChar + charCount) continue;

      stbtt_aligned_quad q;
      stbtt_GetBakedQuad(baked, atlasW, atlasH, code - firstChar, &tempX, &tempY, &q, 1);
      wordQuads.push_back(q);
    }

    float wordWidth = tempX - wordStartX;

    if (x + wordWidth > wrapWidth && x > 0.0f) {
      x = 0.0f;
      y += bakeFontPx;
    }

    for (auto& q : wordQuads) {
      if (x == 0.0f && wordStartX > 0.0f) {
        float dx = -wordStartX;
        q.x0 += dx;
        q.x1 += dx;
        q.y0 += (y - tempY);
        q.y1 += (y - tempY);
      }

      quads.push_back(q);

      minX = (q.x0 < minX) ? q.x0 : minX;
      minY = (q.y0 < minY) ? q.y0 : minY;
      maxX = (q.x1 > maxX) ? q.x1 : maxX;
      maxY = (q.y1 > maxY) ? q.y1 : maxY;
    }

    x += wordWidth;
    i = wordEnd;
  }

  if (minX > maxX || quads.empty()) return;

  float textW = (maxX - minX) * scale;
  float textH = (maxY - minY) * scale;

  float originX = 0.0f;
  float originY = 0.0f;
  if (center) {
    originX = 0.5f * (screenW - textW);
    originY = 0.5f * (screenH - textH);
  }

  // Pass 2: emit quads
  for (const auto& q : quads) {
    float x0 = originX + (q.x0 - minX) * scale;
    float y0 = originY + (q.y0 - minY) * scale;
    float x1 = originX + (q.x1 - minX) * scale;
    float y1 = originY + (q.y1 - minY) * scale;

    AddQuad(verts, indices, x0, y0, x1, y1, q.s0, q.t0, q.s1, q.t1);
  }

  textMesh.UploadIndexed(verts.data(), verts.size() * sizeof(TextVertex), indices.data(), indices.size());
  textMesh.SetAttrib(0, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex), offsetof(TextVertex, pos));
  textMesh.SetAttrib(1, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex), offsetof(TextVertex, uv));
}

static inline void AddQuad(
    std::vector<TextVertex>& v,
    std::vector<unsigned int>& idx,
    float x0,
    float y0,
    float x1,
    float y1,
    float u0,
    float v0,
    float u1,
    float v1
) {
  unsigned int base = (unsigned int)v.size();
  v.push_back({{x0, y0}, {u0, v0}});
  v.push_back({{x1, y0}, {u1, v0}});
  v.push_back({{x1, y1}, {u1, v1}});
  v.push_back({{x0, y1}, {u0, v1}});

  idx.push_back(base + 0);
  idx.push_back(base + 1);
  idx.push_back(base + 2);
  idx.push_back(base + 2);
  idx.push_back(base + 3);
  idx.push_back(base + 0);
}

void TextMode::OnResize(int width, int height) {
  textFB.Resize(width, height);
  dirtyMesh = true;
}

void TextMode::OnKeyPressed(int key, int action) {
  if (key == GLFW_KEY_R && action == GLFW_PRESS) {
    center = !center;
    dirtyMesh = true;
  }
}

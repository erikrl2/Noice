#pragma once
#include "shader.hpp"
#include "framebuffer.hpp"
#include "mesh.hpp"
#include "mode.hpp"

#include <stb_truetype.h>
#include <glm/glm.hpp>

#include <string>
#include <vector>

class TextMode : public Mode {
public:
    void Init(int width, int height);
    void Destroy();

    void UpdateImGui() override;
    void Update(float dt) override;

    void OnResize(int width, int height) override;
    void OnKeyPressed(int key, int action) override;

    Framebuffer& GetResultFB() override { return textFB; }

private:
    void LoadFontAtlas();
    void DestroyFontAtlas();

    void RebuildTextMesh();

private:
    Shader textShader;
    Framebuffer textFB;

    Texture fontAtlasTex;
    const int atlasW = 2048;
    const int atlasH = 2048;

    static const int firstChar = 32;
    static const int charCount = 95;
    stbtt_bakedchar baked[charCount]{};

    std::vector<unsigned char> ttfBuffer;
    std::vector<unsigned char> atlasPixels;

    Mesh textMesh;
    bool dirtyMesh = true;

    std::string text = "NOICE";
    float bakeFontPx = 290.0f;
    float scale = 1.0f;
    bool center = true;

    glm::vec2 direction = glm::vec2(1.0f, 0.0f);
    glm::vec2 bgDir = glm::vec2(0.0f, 0.0f);

    float wrapWidthFrac = 0.9f;
};

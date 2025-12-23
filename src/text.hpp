#pragma once
#include "shader.hpp"
#include "framebuffer.hpp"
#include "mesh.hpp"

#include <stb_truetype.h>
#include <glm/glm.hpp>

#include <string>
#include <vector>

class TextMode {
public:
    void Init(int width, int height);
    void Destroy();

    void UpdateImGui();
    void Update(float dt);

    Framebuffer& GetObjFB() { return textFB; } // TODO: rename

private:
    void LoadFontAtlas(const char* ttfPath);
    void DestroyFontAtlas();

    void RebuildTextMesh(); // rebuild VBO/IBO for current text
    void DestroyTextMesh();

private:
    Shader textShader;
    Framebuffer textFB;

    // --- stb baked font atlas ---
    GLuint fontAtlasTex = 0;
    int atlasW = 1024;
    int atlasH = 1024;

    static constexpr int kFirstChar = 32;
    static constexpr int kCharCount = 95; // 32..126 inclusive
    stbtt_bakedchar baked[kCharCount]{};

    std::vector<unsigned char> ttfBuffer;
    std::vector<unsigned char> atlasPixels;

    // --- text mesh (quads) ---
    SimpleMesh textMesh;

    // --- parameters ---
    std::string text = "NOICE";
    float bakeFontPx = 256.0f; // requires re-bake atlas when changed
    float scale = 1.0f;        // additional scale after baking
    bool center = true;

    glm::vec2 direction = glm::vec2(1.0f, 0.0f); // written into RG16F
    float threshold = 0.02f;
    float softness = 0.05f;
    bool premultiply = true;

    bool additiveBlend = false;

    // state
    bool dirtyMesh = true;
};

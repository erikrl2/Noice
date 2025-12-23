#include "text.hpp"

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

// TODO: move to own cpp file if compile times are long
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

static bool ReadFileBytes(const char* path, std::vector<unsigned char>& out)
{
    FILE* f = nullptr;
#ifdef _MSC_VER
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) { fclose(f); return false; }
    out.resize((size_t)size);

    size_t read = fread(out.data(), 1, (size_t)size, f);
    fclose(f);
    return read == (size_t)size;
}

struct TextVertex {
    float pos[2]; // pixel coords
    float uv[2];  // 0..1
};

static inline void AddQuad(std::vector<TextVertex>& v, std::vector<unsigned int>& idx,
                           float x0, float y0, float x1, float y1,
                           float u0, float v0, float u1, float v1)
{
    unsigned int base = (unsigned int)v.size();
    v.push_back({ {x0, y0}, {u0, v0} });
    v.push_back({ {x1, y0}, {u1, v0} });
    v.push_back({ {x1, y1}, {u1, v1} });
    v.push_back({ {x0, y1}, {u0, v1} });

    idx.push_back(base + 0);
    idx.push_back(base + 1);
    idx.push_back(base + 2);
    idx.push_back(base + 2);
    idx.push_back(base + 3);
    idx.push_back(base + 0);
}

void TextMode::Init(int width, int height)
{
    textFB.Create(width, height, GL_RG16F, GL_LINEAR);
    textShader.Create("assets/shaders/text.vert.glsl", "assets/shaders/text.frag.glsl");

    LoadFontAtlas("assets/fonts/courier-mon.ttf");
    dirtyMesh = true;
}

void TextMode::Destroy()
{
    DestroyTextMesh();
    DestroyFontAtlas();

    textShader.Destroy();
    textFB.Destroy();
}

void TextMode::OnResize(int width, int height)
{
    textFB.Resize(width, height);
    dirtyMesh = true;
}

void TextMode::UpdateImGui()
{
    ImGui::Text("TextMode");

    // Basic InputText without imgui_stdlib (fixed buffer)
    char buf[256];
#ifdef _MSC_VER
    strcpy_s(buf, text.c_str());
#else
    std::snprintf(buf, sizeof(buf), "%s", text.c_str());
#endif
    if (ImGui::InputText("Text", buf, sizeof(buf)))
    {
        text = buf;
        dirtyMesh = true;
    }

    float prevBake = bakeFontPx;
    if (ImGui::SliderFloat("Bake font px", &bakeFontPx, 16.0f, 140.0f, "%.0f"))
    {
        // re-bake atlas on change
        if (bakeFontPx != prevBake)
        {
            LoadFontAtlas("assets/fonts/courier-mon.ttf");
            dirtyMesh = true;
        }
    }

    if (ImGui::SliderFloat("Scale", &scale, 0.1f, 4.0f, "%.2f"))
        dirtyMesh = true;

    if (ImGui::SliderFloat("Wrap width", &wrapWidthFrac, 0.1f, 1.0f, "%.2f"))
        dirtyMesh = true;

    ImGui::Checkbox("Center", &center);

    ImGui::Separator();
    ImGui::SliderFloat2("Direction (RG)", glm::value_ptr(direction), -1.0f, 1.0f);
    ImGui::SliderFloat2("BG Direction", glm::value_ptr(bgDir), -1.0f, 1.0f);
    ImGui::SliderFloat("Threshold", &threshold, 0.0f, 0.5f, "%.4f");
    ImGui::SliderFloat("Softness", &softness, 0.0f, 0.25f, "%.4f");
    ImGui::Checkbox("Premultiply", &premultiply);
}

void TextMode::Update(float /*dt*/)
{
    if (dirtyMesh)
        RebuildTextMesh();

    // Bind FBO + viewport via Clear()
    textFB.Clear(glm::vec4(bgDir.x, bgDir.y, 0.0f, 0.0f));

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    textShader.Use();
    textShader.SetVec2("uScreenSize", glm::vec2((float)textFB.tex.width, (float)textFB.tex.height));
    textShader.SetVec2("uDir", direction);
    textShader.SetFloat("uThreshold", threshold);
    textShader.SetFloat("uSoftness", softness);
    textShader.SetInt("uPremultiply", premultiply ? 1 : 0);

    textShader.SetTexture2D("uFontAtlas", fontAtlasTex, 0);

    textMesh.Draw(0);
}

void TextMode::LoadFontAtlas(const char* ttfPath)
{
    std::vector<unsigned char> newTtf;
    if (!ReadFileBytes(ttfPath, newTtf))
        return;

    ttfBuffer.swap(newTtf);

    atlasPixels.assign((size_t)atlasW * (size_t)atlasH, 0);

    int res = stbtt_BakeFontBitmap(
        ttfBuffer.data(), 0,
        bakeFontPx,
        atlasPixels.data(), atlasW, atlasH,
        kFirstChar, kCharCount,
        baked);

    if (res <= 0)
        return;

    // Create texture if needed, or ensure it has correct dimensions
    if (fontAtlasTex.id == 0)
    {
        fontAtlasTex.Create(atlasW, atlasH, GL_R8, GL_LINEAR, GL_CLAMP_TO_EDGE);
    }
    else if (fontAtlasTex.width != atlasW || fontAtlasTex.height != atlasH)
    {
        fontAtlasTex.Resize(atlasW, atlasH);
    }

    // Upload pixels using glTexSubImage2D
    glBindTexture(GL_TEXTURE_2D, fontAtlasTex.id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, atlasW, atlasH, GL_RED, GL_UNSIGNED_BYTE, atlasPixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void TextMode::DestroyFontAtlas()
{
    fontAtlasTex.Destroy();
    atlasPixels.clear();
    ttfBuffer.clear();
}

void TextMode::DestroyTextMesh()
{
    textMesh.Destroy();
}

void TextMode::RebuildTextMesh()
{
    dirtyMesh = false;

    DestroyTextMesh();

    if (text.empty() || fontAtlasTex.id == 0)
        return;

    std::vector<TextVertex> verts;
    std::vector<unsigned int> indices;
    verts.reserve(text.size() * 4);
    indices.reserve(text.size() * 6);

    float screenW = (float)textFB.tex.width;
    float screenH = (float)textFB.tex.height;
    float wrapWidth = screenW * wrapWidthFrac;

    // Pass 1: layout text with wrapping and compute bounds
    std::vector<stbtt_aligned_quad> quads;
    quads.reserve(text.size());

    float x = 0.0f;
    float y = 0.0f;
    float minX =  1e9f, minY =  1e9f;
    float maxX = -1e9f, maxY = -1e9f;

    for (size_t i = 0; i < text.size(); ++i)
    {
        char c = text[i];
        
        if (c == '\n')
        {
            x = 0.0f;
            y += bakeFontPx;
            continue;
        }

        int code = (unsigned char)c;
        if (code < kFirstChar || code >= kFirstChar + kCharCount)
            continue;

        stbtt_aligned_quad q;
        float prevX = x;
        stbtt_GetBakedQuad(baked, atlasW, atlasH, code - kFirstChar, &x, &y, &q, 1);

        // Check if we need to wrap
        if (x > wrapWidth && prevX > 0.0f)
        {
            x = 0.0f;
            y += bakeFontPx;
            stbtt_GetBakedQuad(baked, atlasW, atlasH, code - kFirstChar, &x, &y, &q, 1);
        }

        quads.push_back(q);

        minX = (q.x0 < minX) ? q.x0 : minX;
        minY = (q.y0 < minY) ? q.y0 : minY;
        maxX = (q.x1 > maxX) ? q.x1 : maxX;
        maxY = (q.y1 > maxY) ? q.y1 : maxY;
    }

    if (minX > maxX || quads.empty())
        return;

    float textW = (maxX - minX) * scale;
    float textH = (maxY - minY) * scale;

    float originX = 0.0f;
    float originY = 0.0f;
    if (center)
    {
        originX = 0.5f * (screenW - textW);
        originY = 0.5f * (screenH - textH);
    }

    // Pass 2: emit quads
    for (const auto& q : quads)
    {
        float x0 = originX + (q.x0 - minX) * scale;
        float y0 = originY + (q.y0 - minY) * scale;
        float x1 = originX + (q.x1 - minX) * scale;
        float y1 = originY + (q.y1 - minY) * scale;

        AddQuad(verts, indices, x0, y0, x1, y1, q.s0, q.t0, q.s1, q.t1);
    }

    // upload
    textMesh.UploadIndexed(verts.data(), verts.size() * sizeof(TextVertex), indices.data(), indices.size());

    // setup attribute pointers using helper
    textMesh.SetAttrib(0, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex), offsetof(TextVertex, pos));
    textMesh.SetAttrib(1, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex), offsetof(TextVertex, uv));
}

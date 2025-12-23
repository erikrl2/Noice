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
    if (ImGui::SliderFloat("Bake font px", &bakeFontPx, 16.0f, 512.0f, "%.0f"))
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

    ImGui::Checkbox("Center", &center);

    ImGui::Separator();
    ImGui::SliderFloat2("Direction (RG)", glm::value_ptr(direction), -1.0f, 1.0f);
    ImGui::SliderFloat("Threshold", &threshold, 0.0f, 0.5f, "%.4f");
    ImGui::SliderFloat("Softness", &softness, 0.0f, 0.25f, "%.4f");
    ImGui::Checkbox("Premultiply", &premultiply);

    ImGui::Separator();
    ImGui::Checkbox("Additive blend", &additiveBlend);
}

void TextMode::Update(float /*dt*/)
{
    if (dirtyMesh)
        RebuildTextMesh();

    // Bind FBO + viewport via Clear()
    textFB.Clear();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    if (additiveBlend)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
    }
    else
    {
        // "overwrite": write directly
        glDisable(GL_BLEND);
    }

    textShader.Use();
    textShader.SetVec2("uScreenSize", glm::vec2((float)textFB.tex.width, (float)textFB.tex.height));
    textShader.SetVec2("uDir", direction);
    textShader.SetFloat("uThreshold", threshold);
    textShader.SetFloat("uSoftness", softness);
    textShader.SetInt("uPremultiply", premultiply ? 1 : 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fontAtlasTex);
    textShader.SetInt("uFontAtlas", 0);

    textMesh.Draw(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
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

    if (fontAtlasTex == 0)
        glGenTextures(1, &fontAtlasTex);

    // TODO: use Texture class
    glBindTexture(GL_TEXTURE_2D, fontAtlasTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, atlasW, atlasH, 0, GL_RED, GL_UNSIGNED_BYTE, atlasPixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void TextMode::DestroyFontAtlas()
{
    if (fontAtlasTex)
    {
        glDeleteTextures(1, &fontAtlasTex);
        fontAtlasTex = 0;
    }
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

    if (text.empty() || fontAtlasTex == 0)
        return;

    std::vector<TextVertex> verts;
    std::vector<unsigned int> indices;
    verts.reserve(text.size() * 4);
    indices.reserve(text.size() * 6);

    // Pass 1: compute bounds
    float x = 0.0f;
    float y = 0.0f;

    float minX =  1e9f, minY =  1e9f;
    float maxX = -1e9f, maxY = -1e9f;

    for (char c : text)
    {
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
        stbtt_GetBakedQuad(baked, atlasW, atlasH, code - kFirstChar, &x, &y, &q, 1);

        minX = (q.x0 < minX) ? q.x0 : minX;
        minY = (q.y0 < minY) ? q.y0 : minY;
        maxX = (q.x1 > maxX) ? q.x1 : maxX;
        maxY = (q.y1 > maxY) ? q.y1 : maxY;
    }

    if (minX > maxX)
        return;

    float textW = (maxX - minX) * scale;
    float textH = (maxY - minY) * scale;

    float screenW = (float)textFB.tex.width;
    float screenH = (float)textFB.tex.height;

    float originX = 0.0f;
    float originY = 0.0f;
    if (center)
    {
        originX = 0.5f * (screenW - textW);
        originY = 0.5f * (screenH - textH);
    }

    // Pass 2: emit quads
    x = 0.0f;
    y = 0.0f;
    for (char c : text)
    {
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
        stbtt_GetBakedQuad(baked, atlasW, atlasH, code - kFirstChar, &x, &y, &q, 1);

        float x0 = originX + (q.x0 - minX) * scale;
        float y0 = originY + (q.y0 - minY) * scale;
        float x1 = originX + (q.x1 - minX) * scale;
        float y1 = originY + (q.y1 - minY) * scale;

        AddQuad(verts, indices, x0, y0, x1, y1, q.s0, q.t0, q.s1, q.t1);
    }

    // upload
    textMesh.UploadIndexed(verts.data(), verts.size() * sizeof(TextVertex), indices.data(), indices.size());

    // setup attribute pointers: layout(location=0) vec2 pos, layout(location=1) vec2 uv 
    glBindVertexArray(textMesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, textMesh.vbo);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex), (void*)offsetof(TextVertex, pos));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex), (void*)offsetof(TextVertex, uv));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

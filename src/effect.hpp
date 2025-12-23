#pragma once
#include "shader.hpp"
#include "framebuffer.hpp"
#include "mesh.hpp"

struct MvpState {
    glm::mat4 prevProj = glm::mat4(1.0f);
    glm::mat4 currProj = glm::mat4(1.0f);
    glm::mat4 prevView = glm::mat4(1.0f);
    glm::mat4 currView = glm::mat4(1.0f);
    glm::mat4 prevModel = glm::mat4(1.0f);
    glm::mat4 currModel = glm::mat4(1.0f);
};

class Effect {
public:
    void Init(int width, int height);
    void Destroy();

    void UpdateImGui();

    void ApplyDynamic(Framebuffer& in, MvpState& mats, float dt);
    void ApplyStatic(Framebuffer& in, float dt);

    Texture& GetNoiseTex() { return prevNoiseTex; }
    Texture& GetAccTex() { return prevAccTex; }
    bool Disabled() { return disabled; }

    void OnResize(int width, int height);
    void OnMouseClicked(int button, int action);

private:
    void ScatterPass(Framebuffer& in, float dt, MvpState* mats = nullptr);
    void FillPass();
    void SwapBuffers(Framebuffer* in = nullptr);

private:
    float scrollSpeed = 150.0f;
    int accResetInterval = 10;
    int downscaleFactor = 1;
    bool disabled = false;
    bool paused = false;

    Texture prevDepthTex;
    Texture currNoiseTex;
    Texture prevNoiseTex;
    Texture currAccTex;
    Texture prevAccTex;

    Shader scrollShader;
    Shader fillShader;

    SimpleMesh quadMesh;
};

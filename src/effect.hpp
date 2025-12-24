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

    void Apply(Framebuffer& in, float dt, const MvpState* mats = nullptr);

    void ClearBuffers();

    Texture& GetResultTex() { return prevNoiseTex; }
    bool IsDisabled() { return disabled; }

    void OnResize(int width, int height);
    void OnMouseClicked(int button, int action);
    void OnKeyPressed(int key, int action);

private:
    void ScatterPass(Framebuffer& in, float dt, const MvpState* mats);
    void FillPass();
    void SwapBuffers(Framebuffer& in);

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

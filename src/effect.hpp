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

    void Apply(Framebuffer& in, const MvpState& mats, float dt);

    Texture& GetNoiseTex() { return prevNoiseTex; }
    Texture& GetAccTex() { return prevAccTex; }

    void OnResize(int width, int height);

public:
    float scrollSpeed = 150.0f;
    int accResetInterval = 10;
    int downscaleFactor = 1;
    bool disabled = false;
    bool paused = false;

private:
    Texture prevDepthTex;
    Texture currNoiseTex;
    Texture prevNoiseTex;
    Texture currAccTex;
    Texture prevAccTex;

    static inline SimpleMesh quadMesh;
    static inline Shader scrollShader;
    static inline Shader fillShader;
};

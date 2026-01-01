#pragma once
#include "camera.hpp"
#include "framebuffer.hpp"
#include "mesh.hpp"
#include "mode.hpp"
#include "shader.hpp"

class ObjectMode : public Mode {
public:
  void Init(int width, int height);
  void Destroy();

  void UpdateImGui() override;
  void Update(float dt) override;

  void OnResize(int width, int height) override;
  void OnMouseClicked(int button, int action) override;
  void OnMouseMoved(double xpos, double ypos) override;
  void OnKeyPressed(int key, int action) override;

  Framebuffer& GetResultFB() override { return objectFB; }
  const MvpState* GetMvpState() override { return &mvpState; }

public:
  enum class MeshType { Debug, Car, Spider, Dragon, Alien, Head, Count };

private:
  void UpdateTransformMatrices(float dt);
  void RenderObject();

  Mesh& SelectedMesh();

private:
  MeshType currentMeshType = MeshType::Car;
  bool uniformFlow = false;

  Mesh meshes[(size_t)MeshType::Count];

  Shader objectShader;
  Framebuffer objectFB;

  Camera camera;
  MvpState mvpState;
  bool hasValidPrevMvp = false;
};

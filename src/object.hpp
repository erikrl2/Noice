#pragma once
#include "camera.hpp"
#include "flowfield/flowfield.hpp"
#include "framebuffer.hpp"
#include "mesh.hpp"
#include "mode.hpp"
#include "shader.hpp"
#include "util.hpp"

class ObjectMode: public Mode {
public:
  enum class Model { Custom, Car, Interior, Dragon, Alien, Head, Count };

  struct Transform {
    glm::vec3 translation = {0.0f, 0.0f, 0.0f};
    glm::vec3 rotation = {0.0f, 0.0f, 0.0f};
    float scale = 1.0f;
  };

  struct ModelLoadJob {
    Model type;
    std::string path;
    FlowfieldSettings settings;
  };

public:
  void Init(int width, int height);
  void Destroy();

  void UpdateImGui() override;
  void Update(float dt) override;

  void OnResize(int width, int height) override;
  void OnMouseClicked(int button, int action) override;
  void OnMouseMoved(double xpos, double ypos) override;
  void OnKeyPressed(int key, int action) override;
  void OnFileDrop(const std::string& path) override;

  Framebuffer& GetResultFB() override { return objectFB; }
  const MvpState* GetMvpState() override { return &mvpState; }

private:
  void UpdateTransformMatrices(float dt);
  void RenderObject();

  void SetInitialObjectTransforms();
  void SetInitialFlowfieldSettings();

private:
  Model objectSelect = Model::Car;

  Transform transforms[(size_t)Model::Count];
  FlowfieldSettings flowSettings[(size_t)Model::Count];

  Mesh meshes[(size_t)Model::Count];

  Shader objectShader;
  Framebuffer objectFB;

  Camera camera;
  MvpState mvpState;
  bool hasValidPrevMvp = false;

private:
  std::thread meshLoaderThread;

  Queue<ModelLoadJob> meshJobQueue;
  Queue<MeshFlowfieldData> uploadQueue;

  void LoadMeshAsync(Model type);

  static void MeshLoaderThreadFunc(Queue<ModelLoadJob>& meshJobQueue, Queue<MeshFlowfieldData>& uploadQueue);
};

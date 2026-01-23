#pragma once
#include "camera.hpp"
#include "flowfield/flowfield.hpp"
#include "framebuffer.hpp"
#include "mesh.hpp"
#include "mode.hpp"
#include "shader.hpp"
#include "util.hpp"

#include <thread>

class ObjectMode: public Mode {
public:
#ifdef NDEBUG
  enum class Model { Custom, Car, Interior, Dragon, Alien, Head, Count };
#else
  enum class Model { Custom, Count };
#endif

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

  EffectInputData GetEffectInputData() override;

private:
  void UpdateViewProjMatrix();
  void UpdateModelMatrices();
  void RenderObjects();

  void SetInitialObjectTransforms();
  void SetInitialFlowfieldSettings();

private:
  int width = 0, height = 0;
  int curr = 0, prev = 1;

  Model objectSelect = Model::Custom;

  Transform transforms[(int)Model::Count];
  FlowfieldSettings flowSettings[(int)Model::Count];

  Mesh meshes[(int)Model::Count];

  Shader objectShader;

  Framebuffer objectFBs[2];

  Camera camera;
  bool isOrtho = false;
  glm::mat4 projMat[2]{};
  glm::mat4 viewMat[2]{};
  glm::mat4 modelMats[(int)Model::Count][2]{};

  std::thread meshLoaderThread;

  Queue<ModelLoadJob> meshJobQueue;
  Queue<MeshFlowfieldData> uploadQueue;

  void LoadMeshAsync(Model type);

  static void MeshLoaderThreadFunc(Queue<ModelLoadJob>& meshJobQueue, Queue<MeshFlowfieldData>& uploadQueue);
};

#pragma once
#include "camera.hpp"
#include "flowfield/flowfield.hpp"
#include "framebuffer.hpp"
#include "mesh.hpp"
#include "mode.hpp"
#include "shader.hpp"
#include "util.hpp"

enum class ObjectType { Custom, Car, Interior, Dragon, Alien, Head, Count };

struct ObjectTransform {
  glm::vec3 translation = {0.0f, 0.0f, 0.0f};
  glm::vec3 rotation = {0.0f, 0.0f, 0.0f};
  float scale = 1.0f;
};

struct ObjectLoadJob {
  ObjectType type;
  std::string path;
  FlowfieldSettings settings;
};

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
  void OnFileDrop(const std::string& path) override;

  Framebuffer& GetResultFB() override { return objectFB; }
  const MvpState* GetMvpState() override { return &mvpState; }

private:
  void UpdateTransformMatrices(float dt);
  void RenderObject();

  void SetInitialObjectTransforms();
  void SetInitialFlowfieldSettings();

private:
  ObjectType currentObject = ObjectType::Car;

  ObjectTransform objectTransforms[(size_t)ObjectType::Count];
  FlowfieldSettings flowSettings[(size_t)ObjectType::Count];

  bool uniformFlow = false;

  Mesh meshes[(size_t)ObjectType::Count];

  Shader objectShader;
  Framebuffer objectFB;

  Camera camera;
  MvpState mvpState;
  bool hasValidPrevMvp = false;

private:
  std::thread meshLoaderThread;

  Queue<ObjectLoadJob> meshJobQueue;
  Queue<MeshFlowfieldData> uploadQueue;

  void LoadMeshAsync(ObjectType type);

  static void MeshLoaderThreadFunc(Queue<ObjectLoadJob>& meshJobQueue, Queue<MeshFlowfieldData>& uploadQueue);
};

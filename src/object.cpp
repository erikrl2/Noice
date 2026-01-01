#include "object.hpp"

#include "flowload/flowload.hpp"
#include "mesh.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <iostream>
#include <thread>

// TODO: make static stuff non-static
static std::thread meshLoaderThread;

struct MeshLoadJob {
  ObjectMode::MeshType type;
  std::string path;
  FlowfieldSettings settings;
};
static ThreadQueue<Mesh::MeshData> uploadQueue;
static ThreadQueue<MeshLoadJob> meshJobQueue;

static std::string GetMeshPath(ObjectMode::MeshType type) {
  switch (type) {
  case ObjectMode::MeshType::Debug: return "assets/models/debug.obj";
  case ObjectMode::MeshType::Car: return "assets/models/car.obj";
  case ObjectMode::MeshType::Spider: return "assets/models/spider.obj";
  case ObjectMode::MeshType::Dragon: return "assets/models/dragon.obj";
  case ObjectMode::MeshType::Alien: return "assets/models/alien.obj";
  case ObjectMode::MeshType::Head: return "assets/models/head.obj";
  default: return "";
  }
}

static FlowfieldSettings flowSettings[(size_t)ObjectMode::MeshType::Count] = {
    {'U', -1}, // Debug
    {'V', 10}, // Car
    {'V', -1}, // Spider
    {'V', -1}, // Dragon
    {'U', -1}, // Alien
    {'V', -1} // Head
};

static void MeshLoaderThreadFunc() {
  while (meshJobQueue) {
    if (auto job = meshJobQueue.TryPop()) {
      uploadQueue.Push(Mesh::LoadFromOBJ((int)job->type, job->path, job->settings));
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
}

static void EnqueueMeshLoad(ObjectMode::MeshType type) {
  std::cout << "Enqueueing mesh load: " << GetMeshPath(type) << std::endl;
  meshJobQueue.Push(MeshLoadJob{type, GetMeshPath(type), flowSettings[(int)type]});
}

void ObjectMode::Init(int width, int height) {
  meshLoaderThread = std::thread(MeshLoaderThreadFunc);
  EnqueueMeshLoad(MeshType::Debug);
  EnqueueMeshLoad(MeshType::Car);
  EnqueueMeshLoad(MeshType::Spider);
  EnqueueMeshLoad(MeshType::Dragon);
  EnqueueMeshLoad(MeshType::Alien);
  EnqueueMeshLoad(MeshType::Head);

  objectShader.Create("assets/shaders/object.vert.glsl", "assets/shaders/object.frag.glsl");

  objectFB.Create(width, height, GL_RG16F, GL_LINEAR, GL_CLAMP_TO_BORDER, true);
}

void ObjectMode::Destroy() {
  objectShader.Destroy();
  objectFB.Destroy();

  uploadQueue.Close();
  meshJobQueue.Close();
  meshLoaderThread.join();
  for (auto& m : meshes) m.Destroy();
}

void ObjectMode::UpdateImGui() {
  bool changed = false;
  changed |= ImGui::RadioButton("Car", (int*)&currentMeshType, (int)MeshType::Car);
  ImGui::SameLine();
  changed |= ImGui::RadioButton("Spider", (int*)&currentMeshType, (int)MeshType::Spider);
  ImGui::SameLine();
  changed |= ImGui::RadioButton("Dragon", (int*)&currentMeshType, (int)MeshType::Dragon);
  changed |= ImGui::RadioButton("Alien", (int*)&currentMeshType, (int)MeshType::Alien);
  ImGui::SameLine();
  changed |= ImGui::RadioButton("Head", (int*)&currentMeshType, (int)MeshType::Head);
  ImGui::SameLine();
  changed |= ImGui::RadioButton("Debug", (int*)&currentMeshType, (int)MeshType::Debug);
  if (changed) hasValidPrevMvp = false;

  ImGui::NewLine();
  ImGui::Checkbox("Uniform Flow", &uniformFlow);

  ImGui::NewLine();
  FlowfieldSettings& stored = flowSettings[(int)currentMeshType];
  ImGui::Text("Flow Settings (Axis: %c, CreaseAngle: %.0f)", stored.axis, stored.creaseThresholdAngle);
  static FlowfieldSettings edit = stored;
  if (changed) edit = stored;
  if (ImGui::RadioButton("U", edit.axis == 'U')) edit.axis = 'U';
  ImGui::SameLine();
  if (ImGui::RadioButton("V", edit.axis == 'V')) edit.axis = 'V';
  ImGui::SliderFloat("Crease Angle", &edit.creaseThresholdAngle, -1.0f, 90.0f, "%.0f");
  if (ImGui::Button("Reload Mesh")) {
    stored = edit;
    EnqueueMeshLoad(currentMeshType);
  }
}

void ObjectMode::Update(float dt) {
  while (auto d = uploadQueue.TryPop()) meshes[d->slot].UploadDataFromOBJ(*d);

  camera.Update(dt);
  UpdateTransformMatrices(dt);
  RenderObject();
}

void ObjectMode::RenderObject() {
  objectFB.Clear();

  objectShader.Use();
  objectShader.SetMat4("uModel", mvpState.currModel);
  objectShader.SetMat4("uView", mvpState.currView);
  objectShader.SetMat4("uViewproj", mvpState.currProj * mvpState.currView);
  objectShader.SetVec2("uViewportSize", {objectFB.tex.width, objectFB.tex.height});
  objectShader.SetInt("uUniformFlow", uniformFlow);

  SelectedMesh().Draw(RenderFlag::DepthTest);
}

void ObjectMode::UpdateTransformMatrices(float dt) {
  float width = (float)objectFB.tex.width, height = (float)objectFB.tex.height;
  float aspect = (height > 0) ? width / height : 1.0f;
  glm::mat4 proj = camera.GetProjection(aspect);
  glm::mat4 view = camera.GetView();
  glm::mat4 m = glm::mat4(1.0f);

  glm::vec3 translation = glm::vec3(0.0f);
  glm::vec3 rotation = glm::vec3(0.0f);
  glm::vec3 scale = glm::vec3(1.0f);

  switch (currentMeshType) {
  case MeshType::Car: rotation = glm::vec3(0, 45, 0); break;
  case MeshType::Spider:
    rotation = glm::vec3(0, 180, 0);
    scale = glm::vec3(0.025f);
    break;
  case MeshType::Dragon:
    translation = glm::vec3(0, -0.5f, 0);
    rotation = glm::vec3(0, 20, 0);
    scale = glm::vec3(0.07f);
    break;
  case MeshType::Alien:
    translation = glm::vec3(0, -0.5f, 0);
    rotation = glm::vec3(0, 60, 0);
    scale = glm::vec3(0.12f);
    break;
  case MeshType::Head:
    translation = glm::vec3(0, -1.0f, 0);
    rotation = glm::vec3(-90, -135, 0);
    scale = glm::vec3(0.20f);
    break;
  case MeshType::Debug: translation = glm::vec3(0, 1.0f, 0); break;
  default: break;
  }

  m = glm::translate(m, translation);
  // m = glm::rotate(m, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
  m = glm::rotate(m, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
  m = glm::rotate(m, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
  m = glm::scale(m, glm::vec3(scale));

  if (hasValidPrevMvp) {
    mvpState.prevProj = mvpState.currProj;
    mvpState.prevView = mvpState.currView;
    mvpState.prevModel = mvpState.currModel;
  } else {
    mvpState.prevProj = proj;
    mvpState.prevView = view;
    mvpState.prevModel = m;
    hasValidPrevMvp = true;
  }
  mvpState.currProj = proj;
  mvpState.currView = view;
  mvpState.currModel = m;
}

void ObjectMode::OnResize(int width, int height) {
  hasValidPrevMvp = false;
  objectFB.Resize(width, height);
}

void ObjectMode::OnMouseClicked(int button, int action) {
  camera.OnMouseClicked(button, action);
}

void ObjectMode::OnMouseMoved(double xpos, double ypos) {
  camera.OnMouseMoved(xpos, ypos);
}

void ObjectMode::OnKeyPressed(int key, int action) {
  if (key == GLFW_KEY_Q && action == GLFW_PRESS) {
    uniformFlow = !uniformFlow;
  }
}

Mesh& ObjectMode::SelectedMesh() {
  return meshes[(int)currentMeshType];
}

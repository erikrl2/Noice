#include "object.hpp"

#include "flowfield/flowfield.hpp"
#include "mesh.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <thread>

static std::string meshFilePaths[(size_t)ObjectType::Count] = {
    "assets/models/debug.obj",
    "assets/models/car.obj",
    "assets/models/interior.obj", // spider.obj
    "assets/models/dragon.obj",
    "assets/models/alien.obj",
    "assets/models/head.obj"};

void ObjectMode::Init(int width, int height) {
  SetInitialFlowfieldSettings();
  SetInitialObjectTransforms();

  meshLoaderThread = std::thread(MeshLoaderThreadFunc, std::ref(meshJobQueue), std::ref(uploadQueue));
  for (int type = 0; type < (int)ObjectType::Count; type++) LoadMeshAsync((ObjectType)type);

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

static bool meshChanged = false;

void ObjectMode::UpdateImGui() {
  meshChanged |= ImGui::RadioButton("Car", (int*)&currentObject, (int)ObjectType::Car);
  ImGui::SameLine();
  meshChanged |= ImGui::RadioButton("Interior", (int*)&currentObject, (int)ObjectType::Interior);
  ImGui::SameLine();
  meshChanged |= ImGui::RadioButton("Dragon", (int*)&currentObject, (int)ObjectType::Dragon);
  meshChanged |= ImGui::RadioButton("Alien", (int*)&currentObject, (int)ObjectType::Alien);
  ImGui::SameLine();
  meshChanged |= ImGui::RadioButton("Head", (int*)&currentObject, (int)ObjectType::Head);
  ImGui::SameLine();
  meshChanged |= ImGui::RadioButton("Custom", (int*)&currentObject, (int)ObjectType::Custom);
  if (meshChanged) hasValidPrevMvp = false;

  ImGui::NewLine();
  ImGui::DragFloat3("Translation", (float*)&objectTransforms[(int)currentObject].translation.x, 0.1f);
  ImGui::DragFloat3("Rotation", (float*)&objectTransforms[(int)currentObject].rotation.x, 0.5f);
  ImGui::DragFloat("Scale", &objectTransforms[(int)currentObject].scale, 0.02f);

  ImGui::NewLine();
  ImGui::Checkbox("Simple Flow", &uniformFlow);

  FlowfieldSettings& stored = flowSettings[(int)currentObject];
  static FlowfieldSettings edit = stored;
  if (meshChanged) edit = stored;

  ImGui::NewLine();
  if (ImGui::RadioButton("U", edit.axis == 'U')) edit.axis = 'U';
  ImGui::SameLine();
  if (ImGui::RadioButton("V", edit.axis == 'V')) edit.axis = 'V';
  ImGui::SameLine();
  if (ImGui::RadioButton("A", edit.axis == 'A')) edit.axis = 'A';

  ImGui::DragFloat("CreaseDeg", &edit.creaseThresholdAngle, 0.1f, -1.0f, 90.0f, "%.0f", ImGuiSliderFlags_NoInput);

  bool differs = (edit.axis != stored.axis) || (edit.creaseThresholdAngle != stored.creaseThresholdAngle);
  if (!differs) ImGui::BeginDisabled();
  if (ImGui::Button("Reload Mesh")) {
    stored = edit;
    LoadMeshAsync(currentObject);
  }
  if (!differs) ImGui::EndDisabled();

  meshChanged = false;
}

void ObjectMode::Update(float dt) {
  while (auto d = uploadQueue.TryPop()) meshes[d->slot].UploadFlowfieldMesh(*d);

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

  meshes[(int)currentObject].Draw(RenderFlag::DepthTest);
}

void ObjectMode::UpdateTransformMatrices(float dt) {
  float width = (float)objectFB.tex.width, height = (float)objectFB.tex.height;
  float aspect = (height > 0) ? width / height : 1.0f;
  glm::mat4 proj = camera.GetProjection(aspect);
  glm::mat4 view = camera.GetView();

  ObjectTransform& t = objectTransforms[(int)currentObject];

  glm::mat4 m = glm::mat4(1.0f);
  m = glm::translate(m, t.translation);
  m = glm::rotate(m, glm::radians(t.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
  m = glm::rotate(m, glm::radians(t.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
  m = glm::rotate(m, glm::radians(t.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
  m = glm::scale(m, glm::vec3(t.scale));

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

void ObjectMode::OnFileDrop(const std::string& path) {
  if (path.size() > 4 && path.substr(path.size() - 4) == ".obj") {
    if (currentObject != ObjectType::Custom) {
      currentObject = ObjectType::Custom;
      meshChanged = true;
    }
    meshFilePaths[(int)ObjectType::Custom] = path;
    LoadMeshAsync(ObjectType::Custom);
  }
}

void ObjectMode::SetInitialObjectTransforms() {
  objectTransforms[(int)ObjectType::Car].rotation.y = 45.0f;
  objectTransforms[(int)ObjectType::Car].scale = 10.0f;

  objectTransforms[(int)ObjectType::Interior].rotation.y = 0.0f;
  objectTransforms[(int)ObjectType::Interior].scale = 20.0f;

  objectTransforms[(int)ObjectType::Dragon].rotation.y = 20.0f;
  objectTransforms[(int)ObjectType::Dragon].scale = 0.7f;

  objectTransforms[(int)ObjectType::Alien].rotation.y = 60.0f;
  objectTransforms[(int)ObjectType::Alien].scale = 1.2f;

  objectTransforms[(int)ObjectType::Head].translation.y = -5.0f;
  objectTransforms[(int)ObjectType::Head].rotation.x = -90.0f;
  objectTransforms[(int)ObjectType::Head].rotation.y = -135.0f;
  objectTransforms[(int)ObjectType::Head].scale = 2.0f;
}

void ObjectMode::SetInitialFlowfieldSettings() {
  flowSettings[(int)ObjectType::Custom] = {'U', -1};
  flowSettings[(int)ObjectType::Car] = {'A', 12};
  flowSettings[(int)ObjectType::Interior] = {'A', 80};
  flowSettings[(int)ObjectType::Dragon] = {'A', 15};
  flowSettings[(int)ObjectType::Alien] = {'A', 45};
  flowSettings[(int)ObjectType::Head] = {'A', -1};
}

void ObjectMode::LoadMeshAsync(ObjectType type) {
  std::string& path = meshFilePaths[(int)type];
  FlowfieldSettings& settings = flowSettings[(int)type];
  meshJobQueue.Push(ObjectLoadJob{type, path, settings});
}

void ObjectMode::MeshLoaderThreadFunc(Queue<ObjectLoadJob>& meshJobQueue, Queue<MeshFlowfieldData>& uploadQueue) {
  while (meshJobQueue) {
    if (auto job = meshJobQueue.TryPop()) {
      uploadQueue.Push(Mesh::CreateFlowfieldDataFromOBJ((int)job->type, job->path, job->settings));
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
}

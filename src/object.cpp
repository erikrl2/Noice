#include "object.hpp"

#include "flowfield/flowfield.hpp"
#include "mesh.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <thread>

static std::string meshFilePaths[] = {
    "assets/models/debug.obj",
    "assets/models/car.obj",
    "assets/models/interior.obj",
    "assets/models/dragon.obj",
    "assets/models/alien.obj",
    "assets/models/head.obj"
};

void ObjectMode::Init(int width, int height) {
  this->width = width;
  this->height = height;

  SetInitialFlowfieldSettings();
  SetInitialObjectTransforms();

  meshLoaderThread = std::thread(MeshLoaderThreadFunc, std::ref(meshJobQueue), std::ref(uploadQueue));
  for (int type = 0; type < (int)Model::Count; type++) LoadMeshAsync((Model)type);

  objectShader.CreateVertFrag("assets/shaders/object.vert.glsl", "assets/shaders/object.frag.glsl");

  for (auto& fb : objectFBs) {
    fb.CreateOrResize(width, height);
    fb.AttachColorTexture(GL_RGB32F, GL_NEAREST); // 0: flow
    fb.AttachColorTexture(GL_RGB32F, GL_NEAREST); // 1: localPos
    fb.AttachColorTexture(GL_R8I, GL_NEAREST); // 2: id
    fb.SetClearColor({-1, 0, 0, 0}, 2);
    fb.AttachDepthTexture();
    fb.Finalize();
  }

  UpdateViewProjMatrix();
  UpdateModelMatrices();
}

void ObjectMode::Destroy() {
  objectShader.Destroy();

  for (auto& fb : objectFBs) fb.Destroy();

  uploadQueue.Close();
  meshJobQueue.Close();
  meshLoaderThread.join();
  for (auto& m : meshes) m.Destroy();
}

static bool meshChanged = false;

void ObjectMode::UpdateImGui() {
  static const char* objects[] = {"Custom", "Car", "Interior", "Dragon", "Alien", "Head"};
  int o = (int)objectSelect;

  if (ImGui::Combo("Object", &o, objects, (int)Model::Count)) {
    objectSelect = (Model)o;
    meshChanged = true;
  }
  ImGui::SeparatorText("Transform");
  int flags = ImGuiSliderFlags_NoRoundToFormat;
  ImGui::DragFloat3("Translation", (float*)&transforms[(int)objectSelect].translation.x, 0.1f, 0, 0, "%.1f", flags);
  ImGui::DragFloat3("Rotation", (float*)&transforms[(int)objectSelect].rotation.x, 0.5f, 0, 0, "%.1f", flags);
  ImGui::DragFloat("Scale", &transforms[(int)objectSelect].scale, 0.02f, 0, 0, "%.2f", flags);

  FlowfieldSettings& stored = flowSettings[(int)objectSelect];
  static FlowfieldSettings edit = stored;
  if (meshChanged) edit = stored;
  meshChanged = false;

  ImGui::SeparatorText("Flow Settings");
  if (ImGui::RadioButton("U", edit.axis == 'U')) edit.axis = 'U';
  ImGui::SameLine();
  if (ImGui::RadioButton("V", edit.axis == 'V')) edit.axis = 'V';
  ImGui::SameLine();
  if (ImGui::RadioButton("Auto", edit.axis == 'A')) edit.axis = 'A';

  ImGui::DragFloat("Crease Deg", &edit.creaseThresholdAngle, 0.1f, 0.0f, 90.0f, "%.0f", ImGuiSliderFlags_ClampOnInput);

  bool differs = (edit.axis != stored.axis) || (edit.creaseThresholdAngle != stored.creaseThresholdAngle);
  ImGui::BeginDisabled(!differs);
  if (ImGui::Button("Reload Mesh")) {
    stored = edit;
    LoadMeshAsync(objectSelect);
  }
  ImGui::EndDisabled();

  ImGui::SeparatorText("Camera");
  if (ImGui::Checkbox("Orthographic", &isOrtho)) UpdateViewProjMatrix();
}

void ObjectMode::Update(float dt) {
  while (auto d = uploadQueue.TryPop()) meshes[d->id].CreateFromFlowfieldData(*d);

  std::swap(curr, prev);

  camera.Update(dt);

  UpdateViewProjMatrix();
  UpdateModelMatrices();

  RenderObjects();
}

void ObjectMode::UpdateViewProjMatrix() {
  float aspect = (height > 0) ? (float)width / (float)height : 1.0f;
  glm::mat4 proj = camera.GetProjection(aspect, isOrtho);
  glm::mat4 view = camera.GetView();
  viewProj[curr] = proj * view;
}

void ObjectMode::UpdateModelMatrices() {
  for (int id = 0; id < (int)Model::Count; id++) {
    Transform& t = transforms[id];

    glm::mat4 m = glm::mat4(1.0f);
    m = glm::translate(m, t.translation);
    m = glm::rotate(m, glm::radians(t.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    m = glm::rotate(m, glm::radians(t.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::rotate(m, glm::radians(t.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    m = glm::scale(m, glm::vec3(t.scale));

    modelMats[id][curr] = m;
  }
}

void ObjectMode::RenderObjects() {
  objectFBs[curr].Clear();

  objectShader.Use();

  for (int id = 0; id < (int)Model::Count; id++) {
    if (meshes[id]) {
      objectShader.SetMat4("uMvp", viewProj[curr] * modelMats[id][curr]);
      objectShader.SetInt("uObjectId", id);

      meshes[id].Draw(RenderFlag::DepthTest);
    }
  }
}

void ObjectMode::OnResize(int width, int height) {
  this->width = width;
  this->height = height;
  for (auto& fb : objectFBs) fb.CreateOrResize(width, height);

  // curr = 0, prev = 1;
  UpdateViewProjMatrix();
}

void ObjectMode::OnMouseClicked(int button, int action) {
  camera.OnMouseClicked(button, action);
}

void ObjectMode::OnMouseMoved(double xpos, double ypos) {
  camera.OnMouseMoved(xpos, ypos);
}

void ObjectMode::OnKeyPressed(int key, int action) {}

void ObjectMode::OnFileDrop(const std::string& path) {
  if (path.size() > 4 && path.substr(path.size() - 4) == ".obj") {
    if (objectSelect != Model::Custom) {
      objectSelect = Model::Custom;
      meshChanged = true;
    }
    meshFilePaths[(int)Model::Custom] = path;
    LoadMeshAsync(Model::Custom);
  }
}

void ObjectMode::SetInitialObjectTransforms() {
#ifdef NDEBUG
  transforms[(int)Model::Custom].translation.x = -80.0f;
  transforms[(int)Model::Custom].rotation.y = 90.0f;
  transforms[(int)Model::Custom].scale = 0.5f;
  transforms[(int)Model::Car].translation = {-11.3f, 0.0f, -35.9f};
  transforms[(int)Model::Car].rotation.y = 27.5f;
  transforms[(int)Model::Car].scale = 7.92f;
  transforms[(int)Model::Interior].translation = {-30.6f, 5.4f, -8.5f};
  transforms[(int)Model::Interior].rotation.y = 0.0f;
  transforms[(int)Model::Interior].scale = 4.46f;
  transforms[(int)Model::Dragon].translation = {1.9f, 0.2f, 43.2f};
  transforms[(int)Model::Dragon].rotation.y = -195.0f;
  transforms[(int)Model::Dragon].scale = 0.62f;
  transforms[(int)Model::Alien].translation = {23.1f, 0.0f, -26.6f};
  transforms[(int)Model::Alien].rotation.y = -85.5f;
  transforms[(int)Model::Alien].scale = 1.0f;
  transforms[(int)Model::Head].translation = {45.8f, -5.0f, 9.8f};
  transforms[(int)Model::Head].rotation.x = -90.0f;
  transforms[(int)Model::Head].rotation.y = 111.5f;
  transforms[(int)Model::Head].scale = 1.82f;
#else
  transforms[(int)Model::Custom].translation.z = -20.0f;
#endif
}

void ObjectMode::SetInitialFlowfieldSettings() {
  flowSettings[(int)Model::Custom] = {'U', 0};
#ifdef NDEBUG
  flowSettings[(int)Model::Car] = {'A', 12};
  flowSettings[(int)Model::Interior] = {'A', 80};
  flowSettings[(int)Model::Dragon] = {'A', 15};
  flowSettings[(int)Model::Alien] = {'A', 45};
  flowSettings[(int)Model::Head] = {'A', 0};
#endif
}

void ObjectMode::LoadMeshAsync(Model type) {
  std::string& path = meshFilePaths[(int)type];
  FlowfieldSettings& settings = flowSettings[(int)type];
  meshJobQueue.Push(ModelLoadJob{type, path, settings});
}

void ObjectMode::MeshLoaderThreadFunc(Queue<ModelLoadJob>& meshJobQueue, Queue<MeshFlowfieldData>& uploadQueue) {
  while (meshJobQueue) {
    if (auto job = meshJobQueue.TryPop()) {
      uploadQueue.Push(Mesh::CreateFlowfieldDataFromOBJ((int)job->type, job->path, job->settings));
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
}

EffectInputData ObjectMode::GetEffectInputData() {
  EffectInputData data;
  data.reproject = true;

  data.prevFlowTex = objectFBs[prev].GetColorTexture(0);
  data.prevLocalPosTex = objectFBs[prev].GetColorTexture(1);
  data.prevIdTex = objectFBs[prev].GetColorTexture(2);
  data.currIdTex = objectFBs[curr].GetColorTexture(2);

  data.prevCurrViewProj = viewProj;
  data.modelMats = std::span<glm::mat4[2]>(modelMats, (int)Model::Count);
  data.currInd = curr;
  return data;
}

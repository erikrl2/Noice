#include "object.hpp"

#include "effect.hpp"
#include "flowfield/flowfield.hpp"
#include "mesh.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <thread>

static std::string meshNames[] = {
#ifdef NDEBUG
    "Quad", "Car", "Interior", "Dragon", "Alien", "Head"
#else
    "Quad",
    "Quad",
#endif
};

static std::string meshFilePaths[] = {
#ifdef NDEBUG
    "assets/models/debug.obj",
    "assets/models/car.obj",
    "assets/models/interior.obj",
    "assets/models/dragon.obj",
    "assets/models/alien.obj",
    "assets/models/head.obj"
#else
    "assets/models/debug.obj",
    "assets/models/debug.obj",
#endif
};

void ObjectMode::Init(int width, int height) {
  this->width = width;
  this->height = height;

  SetInitialFlowfieldSettings();
  SetInitialObjectTransforms();

  meshLoaderThread = std::thread(MeshLoaderThreadFunc, std::ref(meshJobQueue), std::ref(uploadQueue));
  for (int type = 0; type < (int)Model::Count; type++) LoadMeshAsync((Model)type);

  objectShader.CreateVertFrag("assets/shaders/object/object.vert.glsl", "assets/shaders/object/object.frag.glsl");

  for (auto& fb : objectFBs) {
    fb.CreateOrResize(width, height);
    fb.AttachColorTexture(GL_RG32F, GL_NEAREST); // 0: flow
    fb.AttachColorTexture(GL_R8I, GL_NEAREST); // 1: id
    fb.SetClearColor({-1, 0, 0, 0}, 1);
    fb.AttachDepthTexture(GL_DEPTH_COMPONENT32F, GL_NEAREST);
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
  int o = (int)objectSelect;

  if (ImGui::Combo(
          "Object", &o, [](void* d, int i) { return ((std::string*)d)[i].c_str(); }, meshNames, (int)Model::Count
      )) {
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
  projMat[curr] = camera.GetProjection(aspect, isOrtho);
  viewMat[curr] = camera.GetView();
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
  objectShader.SetVec2("uViewportSize", {width, height});
  objectShader.SetMat4("uViewProj", projMat[curr] * viewMat[curr]);

  for (int id = 0; id < (int)Model::Count; id++) {
    if (meshes[id]) {
      objectShader.SetMat4("uModel", modelMats[id][curr]);
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

  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
    glm::vec2 pos = util::GetMousePosition();

    glm::vec2 dpi = util::GetDpiScaleFactor();
    int px = (int)(pos.x * dpi.x);
    int py = height - 1 - (int)(pos.y * dpi.y);

    if (px >= 0 && px < width && py >= 0 && py < height) {
      GLbyte id = -1;
      objectFBs[curr].ReadPixel(1, px, py, &id);

      if (id >= 0 && id < (int)Model::Count) {
        objectSelect = (Model)id;
        meshChanged = true;
      }
    }
  }
}

void ObjectMode::OnMouseMoved(double xpos, double ypos) {
  camera.OnMouseMoved(xpos, ypos);
}

void ObjectMode::OnKeyPressed(int key, int action) {}

void ObjectMode::OnFileDrop(const std::string& path) {
  if (path.size() <= 4) return;
  std::string ext = path.substr(path.size() - 4);
  if (ext == ".obj" || ext == ".stl") {
    meshFilePaths[(int)objectSelect] = path;
    meshNames[(int)objectSelect] = "Custom " + std::to_string((int)objectSelect);
    transforms[(int)objectSelect].scale = 1.0f;
    LoadMeshAsync(objectSelect);
  }
}

void ObjectMode::SetInitialObjectTransforms() {
#ifdef NDEBUG
  transforms[(int)Model::M0].translation.x = -80.0f;
  transforms[(int)Model::M0].rotation.y = 90.0f;
  transforms[(int)Model::M0].scale = 0.5f;
  transforms[(int)Model::M1].translation = {-11.3f, 0.0f, -35.9f};
  transforms[(int)Model::M1].rotation.y = 27.5f;
  transforms[(int)Model::M1].scale = 7.92f;
  transforms[(int)Model::M2].translation = {-30.6f, 5.4f, -8.5f};
  transforms[(int)Model::M2].rotation.y = 0.0f;
  transforms[(int)Model::M2].scale = 4.46f;
  transforms[(int)Model::M3].translation = {1.9f, 0.2f, 43.2f};
  transforms[(int)Model::M3].rotation.y = -195.0f;
  transforms[(int)Model::M3].scale = 0.62f;
  transforms[(int)Model::M4].translation = {23.1f, 0.0f, -26.6f};
  transforms[(int)Model::M4].rotation.y = -85.5f;
  transforms[(int)Model::M4].scale = 1.0f;
  transforms[(int)Model::M5].translation = {45.8f, -5.0f, 9.8f};
  transforms[(int)Model::M5].rotation.x = -90.0f;
  transforms[(int)Model::M5].rotation.y = 111.5f;
  transforms[(int)Model::M5].scale = 1.82f;
#else
  transforms[(int)Model::M0].translation.z = -20.0f;
  transforms[(int)Model::M0].scale = 0.75f;
  transforms[(int)Model::M1].translation = {0.0f, 4.0f, -19.0f};
  transforms[(int)Model::M1].scale = 0.3f;
#endif
}

void ObjectMode::SetInitialFlowfieldSettings() {
  flowSettings[(int)Model::M0] = {'U', 0};
#ifdef NDEBUG
  flowSettings[(int)Model::M1] = {'A', 12};
  flowSettings[(int)Model::M2] = {'A', 80};
  flowSettings[(int)Model::M3] = {'A', 15};
  flowSettings[(int)Model::M4] = {'A', 45};
  flowSettings[(int)Model::M5] = {'A', 0};
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
  data.flow = true;

  data.prevFlowTex = objectFBs[prev].GetColorTexture(0);
  data.currIdTex = objectFBs[curr].GetColorTexture(1);
  data.prevIdTex = objectFBs[prev].GetColorTexture(1);
  data.prevDepthTex = objectFBs[prev].GetDepthTexture();

  data.prevCurrProj = projMat;
  data.prevCurrView = viewMat;
  data.modelMats = std::span<glm::mat4[2]>(modelMats, (int)Model::Count);
  data.currInd = curr;
  return data;
}

#include "object.hpp"

#include "effect.hpp"
#include "flowfield/flowfield.hpp"
#include "mesh.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <filesystem>
#include <thread>

void ObjectMode::Init(int width, int height) {
  this->width = width;
  this->height = height;

  SetInitialModelData();

  meshLoaderThread = std::thread(MeshLoaderThreadFunc, std::ref(meshJobQueue), std::ref(uploadQueue));
  for (int id = 0; id < models.size(); id++) LoadMeshAsync(id);

  objectShader.CreateVertFrag(
      util::AssetPath("shaders/object/object.vert.glsl"), util::AssetPath("shaders/object/object.frag.glsl")
  );

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
  for (auto& m : models) m.mesh.Destroy();
}

static bool meshChanged = false;

void ObjectMode::UpdateImGui() {
  if (ImGui::Combo(
          "Object",
          &objectSelect,
          [](void* d, int i) { return ((Model*)d)[i].name.c_str(); },
          models.data(),
          (int)models.size()
      )) {
    meshChanged = true;
  }
  ImGui::SetItemTooltip("shortcut: right click on object");
  ImGui::BeginDisabled(models.size() > 127);
  if (ImGui::Button("New")) {
    objectSelect = (int)models.size();
    models.emplace_back();
    models[objectSelect].name = std::to_string(objectSelect) + ": empty";
    meshChanged = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Clear")) {
    models[objectSelect].mesh.Destroy();
    models[objectSelect].filepath.clear();
    models[objectSelect].flowSettings = {};
    models[objectSelect].transform = {};
    models[objectSelect].name = std::to_string(objectSelect) + ": empty";
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::Text("Drag and drop obj/stl files");
  ImGui::SeparatorText("Transform");
  int flags = ImGuiSliderFlags_NoRoundToFormat;
  Transform& t = models[objectSelect].transform;
  ImGui::DragFloat3("Translation", (float*)&t.translation.x, 0.1f, 0, 0, "%.1f", flags);
  ImGui::DragFloat3("Rotation", (float*)&t.rotation.x, 0.5f, 0, 0, "%.1f", flags);
  ImGui::DragFloat("Scale", &t.scale, 0.02f, 0, 0, "%.2f", flags);

  FlowfieldSettings& stored = models[objectSelect].flowSettings;
  static FlowfieldSettings edit = stored;
  if (meshChanged) edit = stored;
  meshChanged = false;

  ImGui::SeparatorText("Flow Settings");
  if (ImGui::RadioButton("U", edit.axis == 'U')) edit.axis = 'U';
  ImGui::SameLine();
  if (ImGui::RadioButton("V", edit.axis == 'V')) edit.axis = 'V';
  ImGui::SameLine();
  if (ImGui::RadioButton("Auto", edit.axis == 'A')) edit.axis = 'A';
  ImGui::SameLine();
  if (ImGui::RadioButton("Mikk", edit.axis == 'M')) edit.axis = 'M';

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
  while (auto d = uploadQueue.TryPop()) models[d->id].mesh.CreateFromFlowfieldData(*d);

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
  if (modelMats.size() != models.size()) modelMats.resize(models.size());

  for (int id = 0; id < models.size(); id++) {
    Transform& t = models[id].transform;

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

  for (int id = 0; id < models.size(); id++) {
    if (models[id].mesh) {
      objectShader.SetMat4("uModel", modelMats[id][curr]);
      objectShader.SetInt("uObjectId", id);

      models[id].mesh.Draw(RenderFlag::DepthTest);
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

      if (id >= 0 && id < models.size()) {
        objectSelect = id;
        meshChanged = true;
      }
    }
  }
}

void ObjectMode::OnMouseMoved(double xpos, double ypos) {
  camera.OnMouseMoved(xpos, ypos);
}

void ObjectMode::OnKeyPressed(int key, int action) {}

void ObjectMode::OnFileDrop(const std::string& pathStr) {
  std::filesystem::path path(pathStr);
  if (!path.has_extension()) return;

  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
  if (ext != ".obj" && ext != ".stl") return;

  Model& model = models[objectSelect];
  model.mesh.Destroy();
  model.filepath = path.string();
  model.name = std::to_string(objectSelect) + ": " + path.stem().string();
  model.transform.scale = 1.0f;
  model.transform.rotation = {-90.0f, 0.0f, 0.0f};
  model.flowSettings = {};

  LoadMeshAsync(objectSelect);
}

void ObjectMode::SetInitialModelData() {
#ifdef NDEBUG
  models.resize(6);

  models[0].name = "0: quad";
  models[1].name = "1: car";
  models[2].name = "2: interior";
  models[3].name = "3: dragon";
  models[4].name = "4: alien";
  models[5].name = "5: head";

  models[0].filepath = util::AssetPath("models/quad.obj");
  models[1].filepath = util::AssetPath("models/car.obj");
  models[2].filepath = util::AssetPath("models/interior.obj");
  models[3].filepath = util::AssetPath("models/dragon.obj");
  models[4].filepath = util::AssetPath("models/alien.obj");
  models[5].filepath = util::AssetPath("models/head.obj");

  models[0].transform = {{-80.0f, 0.0f, 0.0f}, {0.0f, 90.0f, 0.0f}, 0.5f};
  models[1].transform = {{-11.3f, 0.0f, -35.9f}, {0.0f, 27.5f, 0.0f}, 7.92f};
  models[2].transform = {{-30.6f, 5.4f, -8.5f}, {0.0f, 0.0f, 0.0f}, 4.46f};
  models[3].transform = {{1.9f, 0.2f, 43.2f}, {0.0f, -195.0f, 0.0f}, 0.62f};
  models[4].transform = {{23.1f, 0.0f, -26.6f}, {0.0f, -85.5f, 0.0f}, 1.0f};
  models[5].transform = {{45.8f, -5.0f, 9.8f}, {-90.0f, 111.5f, 0.0f}, 1.82f};

  models[0].flowSettings = {'U', 0};
  models[1].flowSettings = {'A', 12};
  models[2].flowSettings = {'A', 80};
  models[3].flowSettings = {'A', 15};
  models[4].flowSettings = {'A', 45};
  models[5].flowSettings = {'A', 0};
#else
  models.resize(2);

  models[0].name = "0: quad";
  models[1].name = "1: quad";

  models[0].filepath = util::AssetPath("models/quad.obj");
  models[1].filepath = util::AssetPath("models/quad.obj");

  models[0].transform = {{0.0f, 0.0f, -20.0f}, {0.0f, 0.0f, 0.0f}, 0.75f};
  models[1].transform = {{0.0f, 4.0f, -19.0f}, {0.0f, 0.0f, 0.0f}, 0.3f};

  models[0].flowSettings = {'U', 0};
  models[1].flowSettings = {'V', 0};
#endif
}

void ObjectMode::LoadMeshAsync(int model) {
  std::string& path = models[model].filepath;
  FlowfieldSettings& settings = models[model].flowSettings;
  meshJobQueue.Push(ModelLoadJob{model, path, settings});
}

void ObjectMode::MeshLoaderThreadFunc(Queue<ModelLoadJob>& meshJobQueue, Queue<MeshFlowfieldData>& uploadQueue) {
  while (meshJobQueue) {
    if (auto job = meshJobQueue.TryPop()) {
      uploadQueue.Push(Mesh::CreateFlowfieldDataFromFile(job->modelID, job->path, job->settings));
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
  data.modelMats = std::span<std::array<glm::mat4, 2>>(modelMats);
  data.currInd = curr;
  return data;
}

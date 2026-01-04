#include "object.hpp"

#include "flowfield/flowfield.hpp"
#include "mesh.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <thread>

static std::string meshFilePaths[(size_t)ObjectMode::Model::Count] = {
    "assets/models/debug.obj",
    "assets/models/car.obj",
    "assets/models/interior.obj",
    "assets/models/dragon.obj",
    "assets/models/alien.obj",
    "assets/models/head.obj"
};

void ObjectMode::Init(int width, int height) {
  SetInitialFlowfieldSettings();
  SetInitialObjectTransforms();

  meshLoaderThread = std::thread(MeshLoaderThreadFunc, std::ref(meshJobQueue), std::ref(uploadQueue));
  for (int type = 0; type < (int)Model::Count; type++) LoadMeshAsync((Model)type);

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
  static const char* objects[] = {"Custom", "Car", "Interior", "Dragon", "Alien", "Head"};
  int o = (int)objectSelect;

  if (ImGui::Combo("Object", &o, objects, (int)Model::Count)) {
    objectSelect = (Model)o;
    meshChanged = true;
    hasValidPrevMvp = false;
  }

  ImGui::SeparatorText("Transform");
  ImGui::DragFloat3("Translation", (float*)&transforms[(int)objectSelect].translation.x, 0.1f, 0, 0, "%.1f");
  ImGui::DragFloat3("Rotation", (float*)&transforms[(int)objectSelect].rotation.x, 0.5f, 0, 0, "%.1f");
  ImGui::DragFloat("Scale", &transforms[(int)objectSelect].scale, 0.02f, 0, 0, "%.2f");

  FlowfieldSettings& stored = flowSettings[(int)objectSelect];
  static FlowfieldSettings edit = stored;
  if (meshChanged) edit = stored;

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
  objectShader.SetMat4("uViewproj", mvpState.currProj * mvpState.currView);
  objectShader.SetVec2("uViewportSize", {objectFB.tex.width, objectFB.tex.height});

  meshes[(int)objectSelect].Draw(RenderFlag::DepthTest);
}

void ObjectMode::UpdateTransformMatrices(float dt) {
  float width = (float)objectFB.tex.width, height = (float)objectFB.tex.height;
  float aspect = (height > 0) ? width / height : 1.0f;
  glm::mat4 proj = camera.GetProjection(aspect);
  glm::mat4 view = camera.GetView();

  Transform& t = transforms[(int)objectSelect];

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
  transforms[(int)Model::Car].rotation.y = 45.0f;
  transforms[(int)Model::Car].scale = 10.0f;

  transforms[(int)Model::Interior].rotation.y = 0.0f;
  transforms[(int)Model::Interior].scale = 20.0f;

  transforms[(int)Model::Dragon].rotation.y = 20.0f;
  transforms[(int)Model::Dragon].scale = 0.7f;

  transforms[(int)Model::Alien].rotation.y = 60.0f;
  transforms[(int)Model::Alien].scale = 1.2f;

  transforms[(int)Model::Head].translation.y = -5.0f;
  transforms[(int)Model::Head].rotation.x = -90.0f;
  transforms[(int)Model::Head].rotation.y = -135.0f;
  transforms[(int)Model::Head].scale = 2.0f;
}

void ObjectMode::SetInitialFlowfieldSettings() {
  flowSettings[(int)Model::Custom] = {'U', 0};
  flowSettings[(int)Model::Car] = {'A', 12};
  flowSettings[(int)Model::Interior] = {'A', 80};
  flowSettings[(int)Model::Dragon] = {'A', 15};
  flowSettings[(int)Model::Alien] = {'A', 45};
  flowSettings[(int)Model::Head] = {'A', 0};
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

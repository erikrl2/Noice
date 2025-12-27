#include "object.hpp"
#include "mesh.hpp"
#include "util.hpp"

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <thread>

static std::thread meshLoaderThread;
static ThreadQueue<MeshData> uploadQueue;

void ObjectMode::Init(int width, int height) {
    meshLoaderThread = std::thread([&]() {
        uploadQueue.push(SimpleMesh::LoadFromOBJ("assets/models/debug.obj"));
        uploadQueue.push(SimpleMesh::LoadFromOBJ("assets/models/car.obj"));
        uploadQueue.push(SimpleMesh::LoadFromOBJ("assets/models/spider.obj"));
        uploadQueue.push(SimpleMesh::LoadFromOBJ("assets/models/dragon.obj"));
        uploadQueue.push(SimpleMesh::LoadFromOBJ("assets/models/alien.obj"));
        uploadQueue.push(SimpleMesh::LoadFromOBJ("assets/models/head.obj"));
        });

    objectShader.Create("assets/shaders/object.vert.glsl", "assets/shaders/object.frag.glsl");

    objectFB.Create(width, height, GL_RG16F, GL_LINEAR, GL_CLAMP_TO_BORDER, true);
}

void ObjectMode::Destroy() {
    objectShader.Destroy();
    objectFB.Destroy();

    meshLoaderThread.join();
    for (auto& m : meshes) m.Destroy();
}

#define IMGUI_MESH_RADIO(mesh) changed |= ImGui::RadioButton(#mesh, (int*)&meshSelect, (int)MeshType::mesh)

void ObjectMode::UpdateImGui() {
    ImGui::Text("Object Mode");
    bool changed = false;
    IMGUI_MESH_RADIO(Car); ImGui::SameLine();
    IMGUI_MESH_RADIO(Spider); ImGui::SameLine();
    IMGUI_MESH_RADIO(Dragon);
    IMGUI_MESH_RADIO(Alien); ImGui::SameLine();
    IMGUI_MESH_RADIO(Head); ImGui::SameLine();
    IMGUI_MESH_RADIO(Debug);
    if (changed) hasValidPrevMvp = false;

    ImGui::Checkbox("Uniform Flow", &uniformFlow);
}

void ObjectMode::Update(float dt) {
    static int meshesLoaded = 0;
    if (meshesLoaded < meshes.size())
        while (auto d = uploadQueue.try_pop()) meshes[meshesLoaded++].UploadDataFromOBJ(*d);

    camera.Update(dt);
    UpdateTransformMatrices(dt);
    RenderObject();
}

void ObjectMode::RenderObject() {
    objectShader.Use();
    objectShader.SetMat4("model", mvpState.currModel);
    objectShader.SetMat4("view", mvpState.currView);
    objectShader.SetMat4("viewproj", mvpState.currProj * mvpState.currView);
    objectShader.SetVec2("viewportSize", glm::vec2(objectFB.tex.width, objectFB.tex.height));
    objectShader.SetInt("uniformFlow", uniformFlow);

    objectFB.Clear();

    SelectedMesh().Draw(RenderFlag::DepthTest | RenderFlag::CullFace);
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

    switch (meshSelect) {
    case MeshType::Car:
        rotation = glm::vec3(0, 45, 0);
        break;
    case MeshType::Spider:
        rotation = glm::vec3(0, 180, 0);
        scale = glm::vec3(0.025f);
        break;
    case MeshType::Dragon:
        rotation = glm::vec3(-90, 20, 0);
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
    case MeshType::Debug:
        translation = glm::vec3(0, 1.0f, 0);
        break;
    default: break;
    }

    m = glm::translate(m, translation);
    //m = glm::rotate(m, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    m = glm::rotate(m, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::rotate(m, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    m = glm::scale(m, glm::vec3(scale));

    if (hasValidPrevMvp) {
        mvpState.prevProj = mvpState.currProj;
        mvpState.prevView = mvpState.currView;
        mvpState.prevModel = mvpState.currModel;
    }
    else {
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
    if (key == GLFW_KEY_Q) {
        if (action == GLFW_PRESS) {
            uniformFlow = !uniformFlow;
        }
    }
}

SimpleMesh& ObjectMode::SelectedMesh() {
    return meshes[(int)meshSelect];
}

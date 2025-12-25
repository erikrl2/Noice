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
        uploadQueue.push(SimpleMesh::LoadFromOBJ("assets/models/jeep.obj"));
        uploadQueue.push(SimpleMesh::LoadFromOBJ("assets/models/spider.obj"));
        uploadQueue.push(SimpleMesh::LoadFromOBJ("assets/models/dragon.obj"));
        uploadQueue.push(SimpleMesh::LoadFromOBJ("assets/models/alien.obj"));
        uploadQueue.push(SimpleMesh::LoadFromOBJ("assets/models/head.obj"));
    });

    objectShader.Create("assets/shaders/basic.vert.glsl", "assets/shaders/basic.frag.glsl");

    objectFB.Create(width, height, GL_RG16F, GL_LINEAR, GL_CLAMP_TO_BORDER, true);
}

void ObjectMode::Destroy() {
    objectShader.Destroy();
    objectFB.Destroy();

    meshLoaderThread.join();
    for (auto& m : meshes) m.Destroy();
}

void ObjectMode::UpdateImGui() {
    ImGui::Text("Object Mode");
    bool changed = false;
    changed |= ImGui::RadioButton("Car", (int*)&meshSelect, (int)MeshType::Car); ImGui::SameLine();
    changed |= ImGui::RadioButton("Spider", (int*)&meshSelect, (int)MeshType::Spider); ImGui::SameLine();
    changed |= ImGui::RadioButton("Dragon", (int*)&meshSelect, (int)MeshType::Dragon); ImGui::SameLine();
    changed |= ImGui::RadioButton("Alien", (int*)&meshSelect, (int)MeshType::Alien); ImGui::SameLine();
    changed |= ImGui::RadioButton("Head", (int*)&meshSelect, (int)MeshType::Head);
    if (changed) hasValidPrevMvp = false;
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
    objectShader.SetMat4("viewproj", mvpState.currProj * mvpState.currView);
    objectShader.SetMat4("model", mvpState.currModel);
    objectShader.SetMat4("normalMatrix", glm::transpose(glm::inverse(mvpState.currModel)));

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
    float scale = 1.0f;

    switch (meshSelect) {
    case MeshType::Car:
        rotation.y = 45.0f;
        break;
    case MeshType::Spider:
        rotation.y = 180.0f;
        scale = 0.025f;
        break;
    case MeshType::Dragon:
        rotation.x = -90.0f;
        rotation.y = 20.0f;
        scale = 0.07f;
        break;
    case MeshType::Alien:
        translation.y = -0.5f;
        rotation.y = 60.0f;
        scale = 0.12f;
        break;
    case MeshType::Head:
        translation.y = -1.0f;
        rotation.x = -90.0f;
        rotation.y = -135.0f;
        scale = 0.20f;
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

SimpleMesh& ObjectMode::SelectedMesh() {
    return meshes[(int)meshSelect];
}

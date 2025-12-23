#include "object.hpp"

#include <imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

void ObjectMode::Init(int width, int height) {
    carMesh = SimpleMesh::LoadFromOBJ("assets/models/jeep.obj");
    spiderMesh = SimpleMesh::LoadFromOBJ("assets/models/spider.obj");
    //dragonMesh = SimpleMesh::LoadFromOBJ("assets/models/dragon.obj");

    objectShader.Create("assets/shaders/basic.vert.glsl", "assets/shaders/basic.frag.glsl");

    objectFB.Create(width, height, GL_RG16F, GL_LINEAR, true);
}

void ObjectMode::Destroy() {
    carMesh.Destroy();
    spiderMesh.Destroy();
    dragonMesh.Destroy();
    objectShader.Destroy();
    objectFB.Destroy();
}

void ObjectMode::UpdateImGui() {
    ImGui::Text("Mesh:");
    bool changed = false;
    changed = ImGui::RadioButton("Car", (int*)&meshSelect, (int)MeshType::Car); ImGui::SameLine();
    changed = ImGui::RadioButton("Spider", (int*)&meshSelect, (int)MeshType::Spider); ImGui::SameLine();
    if (dragonMesh.vao) changed = ImGui::RadioButton("Dragon", (int*)&meshSelect, (int)MeshType::Dragon);
    if (changed) hasValidPrevMvp = false;
}

void ObjectMode::Update(float dt) {
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
    case MeshType::Alien: break;
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
    switch (meshSelect) {
    case MeshType::Car: return carMesh;
    case MeshType::Spider: return spiderMesh;
    case MeshType::Dragon: return dragonMesh;
    default: return carMesh;
    }
}

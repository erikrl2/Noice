#include "app.hpp"

#include "framebuffer.hpp"
#include "mesh.hpp"
#include "util.hpp"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>

App::App() {
  InitWindow();

  InitOpenGL();
  InitImGui();

  SetupResources();

  CheckWindowSize();
}

void App::Run() {
  while (!glfwWindowShouldClose(win)) {
    if (minimized) {
      glfwWaitEvents();
      continue;
    }
    float dt = ImGui::GetIO().DeltaTime;

    static float hz = (float)glfwGetVideoMode(glfwGetPrimaryMonitor())->refreshRate;
    dt = std::min(dt, 1.0f / hz + 0.0005f);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    ImGui::NewFrame();
    UpdateImGui();
    ImGui::Render();

    Update(dt);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(win);
    glfwPollEvents();
  }
}

App::~App() {
  DestroyResources();

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwTerminate();
}

void App::InitWindow() {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  win = glfwCreateWindow(width, height, "Noice", nullptr, nullptr);
  glfwMakeContextCurrent(win);
  glfwSwapInterval(1);

  glfwSetWindowUserPointer(win, this);

  glfwSetFramebufferSizeCallback(win, OnFramebufferResized);
  glfwSetCursorPosCallback(win, OnMouseMoved);
  glfwSetMouseButtonCallback(win, OnMouseClicked);
  glfwSetKeyCallback(win, OnKeyPressed);
  glfwSetScrollCallback(win, OnMouseScroll);
  glfwSetDropCallback(win, OnFileDrop);
}

void App::InitOpenGL() {
  gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

#ifndef NDEBUG
  util::EnableOpenGLDebugOutput();
#endif

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
}

void App::InitImGui() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGui::StyleColorsDark();
  ImGui::GetIO().IniFilename = nullptr;

  ImGui_ImplGlfw_InitForOpenGL(win, true);
  ImGui_ImplOpenGL3_Init("#version 430 core");
}

void App::SetupResources() {
  quadMesh.CreateFullscreenQuad();

  postShader.CreateVertFrag("assets/shaders/post.vert.glsl", "assets/shaders/post.frag.glsl");

  effect.Init(width, height);

  objectMode.Init(width, height);
  textMode.Init(width, height);
  paintMode.Init(width, height);

  screenshot.Init(width, height);

  SetModePointer();
}

void App::DestroyResources() {
  quadMesh.Destroy();
  postShader.Destroy();
  effect.Destroy();
  objectMode.Destroy();
  textMode.Destroy();
  paintMode.Destroy();
  screenshot.Destroy();
}

void App::UpdateImGui() {
  if (!showSettings) return;
  bool open = ImGui::Begin("Settings", &showSettings, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav);
  if (open) {
#if 1
    ImGui::Text("FPS: %.1f (%.2f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
    if (ImGui::Checkbox("VSync", &vsync)) {
      glfwSwapInterval(vsync ? 1 : 0);
    }
    ImGui::Separator();
#endif

    if (ImGui::CollapsingHeader("Effect", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::BeginDisabled(screenshot.IsCapturing());
      effect.UpdateImGui();
      ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("Mode", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::BeginDisabled(screenshot.IsCapturing());

      bool changed = false;
      changed |= ImGui::RadioButton("Object##Mode", (int*)&modeSelect, (int)ModeType::Object);
      ImGui::SameLine();
      changed |= ImGui::RadioButton("Text##Mode", (int*)&modeSelect, (int)ModeType::Text);
      ImGui::SameLine();
      changed |= ImGui::RadioButton("Paint##Mode", (int*)&modeSelect, (int)ModeType::Paint);
      if (changed) OnModeChange();

      modePtr->UpdateImGui();

      ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("Screenshot", ImGuiTreeNodeFlags_DefaultOpen)) {
      screenshot.UpdateImGui();
    }
  }
  ImGui::End();
}

void App::Update(float dt) {
  if (!screenshot.IsCapturing()) {
    modePtr->Update(!screenshot.IsActive() ? dt : 0.0f);
  } else {
    effect.disabled = false;
  }

  Texture effectTex = effect.Apply(modePtr->GetEffectInputData(), dt);

  if (screenshot.IsCapturing()) screenshot.Update(width, height, effectTex);

  RenderToScreen(effectTex);
}

void App::RenderToScreen(Texture src) {
  if (screenshot.IsActive()) {
    src = screenshot.GetResult();
  }

  Framebuffer::BindDefault(width, height);
  postShader.Use();

  src.Bind(0);

  postShader.SetVec2("uFullResolution", {width, height});
  postShader.SetInt("uShowVectors", effect.disabled && !screenshot.IsActive());
  postShader.SetInt("uNormalizeVectors", !effect.showAcc);

  quadMesh.Draw();
}

void App::OnFramebufferResized(GLFWwindow* window, int w, int h) {
  App& app = *(App*)glfwGetWindowUserPointer(window);

  if (w == 0 || h == 0) {
    app.minimized = true;
    return;
  }

  app.minimized = false;
  app.width = w;
  app.height = h;

  app.effect.OnResize(w, h);
  app.modePtr->OnResize(w, h);
}

void App::OnMouseMoved(GLFWwindow* window, double xpos, double ypos) {
  if (ImGui::GetIO().WantCaptureMouse) return;
  App& app = *(App*)glfwGetWindowUserPointer(window);

  if (!app.screenshot.IsActive()) {
    app.modePtr->OnMouseMoved(xpos, ypos);
  }
}

void App::OnMouseScroll(GLFWwindow* window, double xoffset, double yoffset) {
  if (ImGui::GetIO().WantCaptureMouse) return;
  App& app = *(App*)glfwGetWindowUserPointer(window);

  if (!app.screenshot.IsActive()) {
    app.effect.OnMouseScrolled((float)yoffset);
    app.modePtr->OnMouseScrolled((float)yoffset);
  }
}

void App::OnMouseClicked(GLFWwindow* window, int button, int action, int mods) {
  if (ImGui::GetIO().WantCaptureMouse) return;
  App& app = *(App*)glfwGetWindowUserPointer(window);

  if (!app.screenshot.IsActive()) {
    app.effect.OnMouseClicked(button, action);
    app.modePtr->OnMouseClicked(button, action);
  }
  app.screenshot.OnMouseClicked(button, action);
}

void App::OnKeyPressed(GLFWwindow* window, int key, int scancode, int action, int mods) {
  if (ImGui::GetIO().WantCaptureKeyboard) return;
  App& app = *(App*)glfwGetWindowUserPointer(window);

  switch (key) {
  case GLFW_KEY_ESCAPE:
    if (!app.screenshot.IsActive() && action == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
    break;
  case GLFW_KEY_H:
    if (action == GLFW_PRESS) app.showSettings = !app.showSettings;
    break;
  case GLFW_KEY_O:
    if (action == GLFW_PRESS) {
      app.modeSelect = ModeType::Object;
      app.OnModeChange();
    }
    break;
  case GLFW_KEY_T:
    if (action == GLFW_PRESS) {
      app.modeSelect = ModeType::Text;
      app.OnModeChange();
    }
    break;
  case GLFW_KEY_P:
    if (action == GLFW_PRESS) {
      app.modeSelect = ModeType::Paint;
      app.OnModeChange();
    }
    break;
  }

  if (!app.screenshot.IsActive()) {
    app.effect.OnKeyPressed(key, action);
    app.modePtr->OnKeyPressed(key, action);
  }
  app.screenshot.OnKeyPressed(key, action);
}

void App::OnFileDrop(GLFWwindow* window, int count, const char** paths) {
  App& app = *(App*)glfwGetWindowUserPointer(window);
  app.modePtr->OnFileDrop(paths[0]);
}

void App::OnModeChange() {
  SetModePointer();
  modePtr->OnResize(width, height);
  effect.ClearBuffers();
}

void App::SetModePointer() {
  switch (modeSelect) {
  case ModeType::Object: modePtr = &objectMode; break;
  case ModeType::Text: modePtr = &textMode; break;
  case ModeType::Paint: modePtr = &paintMode; break;
  }
}

void App::CheckWindowSize() {
  int w, h;
  glfwGetFramebufferSize(win, &w, &h);
  if (w != width || h != height) App::OnFramebufferResized(win, w, h);
}

cmake_minimum_required(VERSION 3.10)

include(FetchContent)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG latest
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

get_property(glfwTargets DIRECTORY "${glfw_SOURCE_DIR}/src" PROPERTY BUILDSYSTEM_TARGETS)
set_property(TARGET ${glfwTargets} PROPERTY FOLDER "Dependencies")

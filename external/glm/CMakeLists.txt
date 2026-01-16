cmake_minimum_required(VERSION 3.10)

include(FetchContent)

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.2
)
FetchContent_MakeAvailable(glm)

get_property(glmTargets DIRECTORY "${glm_SOURCE_DIR}/glm" PROPERTY BUILDSYSTEM_TARGETS)
set_property(TARGET ${glmTargets} PROPERTY FOLDER "Dependencies")

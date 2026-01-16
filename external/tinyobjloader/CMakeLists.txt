cmake_minimum_required(VERSION 3.10)

include(FetchContent)

FetchContent_Declare(
  tinyobjloader
  GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
  GIT_TAG release
)
FetchContent_MakeAvailable(tinyobjloader)

get_property(tinyobjloaderTargets DIRECTORY "${tinyobjloader_SOURCE_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
set_property(TARGET ${tinyobjloaderTargets} PROPERTY FOLDER "Dependencies")

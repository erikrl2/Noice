cmake_minimum_required(VERSION 3.10)

include(FetchContent)

FetchContent_Declare(
  libigl
  GIT_REPOSITORY https://github.com/libigl/libigl.git
  GIT_TAG v2.5.0
)

set(LIBIGL_WITH_OPENGL OFF CACHE BOOL "" FORCE)
set(LIBIGL_WITH_VIEWER OFF CACHE BOOL "" FORCE)
set(LIBIGL_WITH_PNG OFF CACHE BOOL "" FORCE)
set(LIBIGL_WITH_IMGUI OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(libigl)

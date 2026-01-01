cmake_minimum_required(VERSION 3.10)

include(FetchContent)

FetchContent_Declare(
  eigen
  GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
  GIT_TAG 3.4
)
FetchContent_MakeAvailable(eigen)

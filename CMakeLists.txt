cmake_minimum_required(VERSION 3.10)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project(Noice C CXX)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(external/glfw)
add_subdirectory(external/glad)
add_subdirectory(external/imgui)
add_subdirectory(external/glm)
add_subdirectory(external/stb)

add_subdirectory(external/eigen)
add_subdirectory(external/libigl)
add_subdirectory(external/tinyobjloader)

file(GLOB FLOWLOAD_SOURCES "src/flowload/*.cpp" "src/flowload/*.hpp")
add_library(flowload STATIC ${FLOWLOAD_SOURCES})
target_link_libraries(flowload PRIVATE igl::core Eigen3::Eigen tinyobjloader)

file(GLOB NOICE_SOURCES "src/*.cpp" "src/*.hpp")
add_executable(Noice ${NOICE_SOURCES})
target_link_libraries(Noice PRIVATE flowload glfw glad imgui glm::glm stb)


file(GLOB_RECURSE ASSETS CONFIGURE_DEPENDS "assets/*")
add_custom_command(
  OUTPUT "${CMAKE_BINARY_DIR}/assets/.assets_copied"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "${CMAKE_BINARY_DIR}/assets"
  COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_BINARY_DIR}/assets/.assets_copied"
  DEPENDS ${ASSETS}
)
add_custom_target(copy_assets ALL DEPENDS "${CMAKE_BINARY_DIR}/assets/.assets_copied")
add_dependencies(Noice copy_assets)


find_program(CLANG_FORMAT clang-format)
add_custom_target(format COMMAND ${CLANG_FORMAT} -i ${NOICE_SOURCES} ${FLOWLOAD_SOURCES})

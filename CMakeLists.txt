cmake_minimum_required(VERSION 3.10)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project(Noice C CXX)

set(CMAKE_CXX_STANDARD 20)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(external/glfw)
add_subdirectory(external/glad)
add_subdirectory(external/imgui)
add_subdirectory(external/glm)
add_subdirectory(external/stb)

add_subdirectory(external/eigen)
add_subdirectory(external/libigl)
add_subdirectory(external/tinyobjloader)

add_library(compile_options INTERFACE)
target_compile_options(compile_options INTERFACE
  $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU>>:-march=native>
  $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU>>:-mtune=native>
  $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/arch:AVX2>
)

file(GLOB CMAKE_FILES "external/*/CMakeLists.txt")
file(GLOB NOICE_SOURCES "src/*.cpp" "src/*.hpp")
file(GLOB_RECURSE SHADER_FILES "assets/shaders/*.glsl")
file(GLOB FLOWFIELD_SOURCES "src/flowfield/*.cpp" "src/flowfield/*.hpp")

add_library(flowfield STATIC ${FLOWFIELD_SOURCES})
target_link_libraries(flowfield PRIVATE igl::core Eigen3::Eigen tinyobjloader compile_options)

add_library(CMakeFiles INTERFACE)
target_sources(CMakeFiles INTERFACE ${CMAKE_FILES})
add_library(Shaders INTERFACE)
target_sources(Shaders INTERFACE ${SHADER_FILES})

add_executable(Noice ${NOICE_SOURCES})
target_link_libraries(Noice PRIVATE glfw glad imgui glm::glm stb flowfield compile_options Shaders CMakeFiles)

# Utility targets
file(GLOB_RECURSE ASSETS CONFIGURE_DEPENDS "assets/*")
set(ASSET_DST "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/assets")
set(ASSET_STAMP "${ASSET_DST}/.assets_copied")
add_custom_command(OUTPUT "${ASSET_STAMP}" COMMAND ${CMAKE_COMMAND} -E make_directory "${ASSET_DST}"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "${ASSET_DST}"
  COMMAND ${CMAKE_COMMAND} -E touch "${ASSET_STAMP}" DEPENDS ${ASSETS} VERBATIM
)
add_custom_target(copy_assets ALL DEPENDS "${ASSET_STAMP}")
add_dependencies(Noice copy_assets)

find_program(CLANG_FORMAT clang-format)
add_custom_target(format COMMAND ${CLANG_FORMAT} -i ${NOICE_SOURCES} ${FLOWFIELD_SOURCES})

# Visual Studio specific settings
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT Noice)
set_target_properties(Noice PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Noice>")

source_group(TREE "${CMAKE_SOURCE_DIR}/src" PREFIX "Source" FILES ${NOICE_SOURCES})
source_group(TREE "${CMAKE_SOURCE_DIR}/src/flowfield" PREFIX "Source" FILES ${FLOWFIELD_SOURCES})
source_group(TREE "${CMAKE_SOURCE_DIR}/assets/shaders" PREFIX "Shaders" FILES ${SHADER_FILES})
source_group(TREE "${CMAKE_SOURCE_DIR}/external" PREFIX "CMakeFiles" FILES ${CMAKE_FILES})

set_property(TARGET Noice flowfield PROPERTY FOLDER "App")
set_property(TARGET copy_assets format PROPERTY FOLDER "Tools")

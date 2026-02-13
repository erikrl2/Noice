vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO jpcy/xatlas
    REF f700c7790aaa030e794b52ba7791a05c085faf0c
    SHA512 1f7afcc9056ab636abef017033aaf63d219cdec95e871beade2c694f8e8b4a58563cf506c5afb6d0d5536233f791e11adbcf3f6f26548105b31d381289892dea
    HEAD_REF master
)

file(WRITE "${SOURCE_PATH}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.15)

project(xatlas LANGUAGES C CXX)

# vcpkg controls static/shared via VCPKG_LIBRARY_LINKAGE => BUILD_SHARED_LIBS in toolchain.
add_library(xatlas
    source/xatlas/xatlas.cpp
)

add_library(xatlas::xatlas ALIAS xatlas)

target_include_directories(xatlas
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/source/xatlas>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(xatlas PUBLIC cxx_std_11)

# Match upstream intent: shared lib exports C API symbols by default
# (Upstream premake sets XATLAS_C_API=1 and XATLAS_EXPORT_API=1 for the shared target.)
if(BUILD_SHARED_LIBS)
    target_compile_definitions(xatlas PRIVATE XATLAS_C_API=1 XATLAS_EXPORT_API=1)
endif()

# Install
include(GNUInstallDirs)

install(TARGETS xatlas
    EXPORT xatlasTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES
    "${CMAKE_CURRENT_LIST_DIR}/source/xatlas/xatlas.h"
    DESTINATION include
)

# Optional natvis (useful on MSVC)
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/source/xatlas.natvis")
    install(FILES "${CMAKE_CURRENT_LIST_DIR}/source/xatlas.natvis"
        DESTINATION share/xatlas
    )
endif()

# Package config
include(CMakePackageConfigHelpers)

set(XATLAS_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/xatlas")

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/xatlasConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/xatlasConfig.cmake"
    INSTALL_DESTINATION "${XATLAS_INSTALL_CMAKEDIR}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/xatlasConfigVersion.cmake"
    VERSION "0.0.0"
    COMPATIBILITY AnyNewerVersion
)

install(EXPORT xatlasTargets
    NAMESPACE xatlas::
    DESTINATION "${XATLAS_INSTALL_CMAKEDIR}"
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/xatlasConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/xatlasConfigVersion.cmake"
    DESTINATION "${XATLAS_INSTALL_CMAKEDIR}"
)
]=])

file(MAKE_DIRECTORY "${SOURCE_PATH}/cmake")
file(WRITE "${SOURCE_PATH}/cmake/xatlasConfig.cmake.in" [=[
@PACKAGE_INIT@
include("${CMAKE_CURRENT_LIST_DIR}/xatlasTargets.cmake")
]=])

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/xatlas)

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

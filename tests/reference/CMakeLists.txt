cmake_minimum_required(VERSION 3.20)
project(Dust3DQtBakeReference LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
option(DUST3D_REFERENCE_ENABLE_SANITIZERS "Instrument only this optional reference executable" OFF)
if(DUST3D_REFERENCE_ENABLE_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "Reference sanitizers require Clang or GCC")
    endif()
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# This standalone project is never included by the native build.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui)
find_package(Threads REQUIRED)
get_filename_component(DUST3D_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(DUST3D_CORE_BUILD_DIR "" CACHE PATH "Existing native CMake build containing the four core archives")
if(NOT EXISTS "${DUST3D_CORE_BUILD_DIR}/CMakeCache.txt")
    message(FATAL_ERROR "Set DUST3D_CORE_BUILD_DIR to a configured native core build from this checkout")
endif()
file(STRINGS "${DUST3D_CORE_BUILD_DIR}/CMakeCache.txt" cache_source REGEX "^CMAKE_HOME_DIRECTORY:INTERNAL=")
string(REPLACE "CMAKE_HOME_DIRECTORY:INTERNAL=" "" cache_source "${cache_source}")
get_filename_component(cache_source "${cache_source}" REALPATH)
get_filename_component(expected_source "${DUST3D_SOURCE_ROOT}" REALPATH)
if(NOT cache_source STREQUAL expected_source)
    message(FATAL_ERROR "DUST3D_CORE_BUILD_DIR belongs to a different source directory; configure the native core from this checkout")
endif()
file(STRINGS "${DUST3D_CORE_BUILD_DIR}/CMakeCache.txt" native_sanitizers REGEX "^DUST3D_ENABLE_SANITIZERS:BOOL=")
string(REPLACE "DUST3D_ENABLE_SANITIZERS:BOOL=" "" native_sanitizers "${native_sanitizers}")
if((native_sanitizers AND NOT DUST3D_REFERENCE_ENABLE_SANITIZERS) OR
   (DUST3D_REFERENCE_ENABLE_SANITIZERS AND NOT native_sanitizers))
    message(FATAL_ERROR "Native and reference sanitizer options must match")
endif()
# A generated reference-owned stamp forces relinking after the nested build.
# Native archives are not byproducts here: cleaning the probe must not delete them.
set(reference_native_stamp "${CMAKE_CURRENT_BINARY_DIR}/native-refresh.stamp")
add_custom_target(reference_native_build
    COMMAND "${CMAKE_COMMAND}" --build "${DUST3D_CORE_BUILD_DIR}" --target dust3d_engine --parallel 4
    COMMAND "${CMAKE_COMMAND}" -E touch "${reference_native_stamp}"
    BYPRODUCTS "${reference_native_stamp}"
    VERBATIM)
foreach(component engine core remesher triangle)
    set(archive "${DUST3D_CORE_BUILD_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}dust3d_${component}${CMAKE_STATIC_LIBRARY_SUFFIX}")
    if(NOT EXISTS "${archive}")
        message(FATAL_ERROR "Build the native core first and set DUST3D_CORE_BUILD_DIR; missing ${archive}")
    endif()
    add_library(reference_${component} STATIC IMPORTED)
    set_target_properties(reference_${component} PROPERTIES IMPORTED_LOCATION "${archive}")
endforeach()

add_executable(dust3d_qt_bake_reference
    qt_bake_reference.cc
    "${DUST3D_SOURCE_ROOT}/application/sources/uv_map_generator.cc"
    "${DUST3D_SOURCE_ROOT}/application/sources/uv_map_generator.h"
    "${DUST3D_SOURCE_ROOT}/application/sources/model_mesh.cc"
    "${DUST3D_SOURCE_ROOT}/application/sources/image_forever.cc")
add_dependencies(dust3d_qt_bake_reference reference_native_build)
set_property(TARGET dust3d_qt_bake_reference APPEND PROPERTY LINK_DEPENDS "${reference_native_stamp}")
target_include_directories(dust3d_qt_bake_reference PRIVATE
    "${DUST3D_SOURCE_ROOT}" "${DUST3D_SOURCE_ROOT}/native/Engine/include"
    "${DUST3D_SOURCE_ROOT}/application/sources"
    "${DUST3D_SOURCE_ROOT}/third_party" "${DUST3D_SOURCE_ROOT}/third_party/eigen"
    "${DUST3D_SOURCE_ROOT}/third_party/rapidxml-1.13"
    "${DUST3D_SOURCE_ROOT}/third_party/earcut.hpp/include"
    "${DUST3D_SOURCE_ROOT}/third_party/autoremesher/tbbshim"
    "${DUST3D_SOURCE_ROOT}/third_party/autoremesher/include"
    "${DUST3D_SOURCE_ROOT}/third_party/autoremesher/thirdparty/isotropicremesher")
target_compile_definitions(dust3d_qt_bake_reference PRIVATE
    PROJECT_DEFINED_APP_NAME="Dust3DReference"
    PROJECT_DEFINED_APP_HUMAN_VER="reference-test"
    PROJECT_DEFINED_APP_HOMEPAGE_URL="https://dust3d.org/")
target_link_libraries(dust3d_qt_bake_reference PRIVATE
    reference_engine reference_core reference_remesher reference_triangle
    Threads::Threads Qt6::Core Qt6::Gui)

include(CTest)
foreach(scenario solid image image-alpha gradient material procedural transparent corpus input-bounds portable-solid portable-image portable-gradient portable-material portable-procedural portable-transparent)
    add_test(NAME qt_reference_${scenario}
        COMMAND dust3d_qt_bake_reference ${scenario} "${DUST3D_SOURCE_ROOT}/tests/core/fixtures")
    set_tests_properties(qt_reference_${scenario} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 300)
endforeach()

# Qt consumes its own command-line flags unless the probe isolates GUI startup arguments.
add_test(NAME qt_reference_rejects_platform_option
    COMMAND dust3d_qt_bake_reference solid "${DUST3D_SOURCE_ROOT}/tests/core/fixtures" -platform offscreen)
add_test(NAME qt_reference_rejects_unknown_scenario
    COMMAND dust3d_qt_bake_reference unknown "${DUST3D_SOURCE_ROOT}/tests/core/fixtures")
set_tests_properties(qt_reference_rejects_platform_option qt_reference_rejects_unknown_scenario
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" WILL_FAIL TRUE TIMEOUT 30)

add_test(NAME qt_reference_incremental_relink
    COMMAND "${CMAKE_COMMAND}" -DREFERENCE_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
        -DNATIVE_CORE_ARCHIVE=$<TARGET_FILE:reference_core>
        -P "${CMAKE_CURRENT_LIST_DIR}/check_incremental_relink.cmake")
set_tests_properties(qt_reference_incremental_relink PROPERTIES RUN_SERIAL TRUE TIMEOUT 180)

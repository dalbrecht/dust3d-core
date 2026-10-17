cmake_minimum_required(VERSION 3.20)
project(Dust3DCore VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(DUST3D_ENABLE_SANITIZERS "Instrument the engine and tests with ASan/UBSan" OFF)
if(DUST3D_ENABLE_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "Sanitizers require Clang or GCC")
    endif()
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

include(cmake/CoreSources.cmake)
find_package(Threads REQUIRED)
add_library(dust3d_triangle STATIC third_party/GuigueDevillers03/tri_tri_intersect.c)
target_include_directories(dust3d_triangle PUBLIC third_party/GuigueDevillers03)
add_library(dust3d_remesher STATIC ${DUST3D_REMESHER_SOURCES})
target_include_directories(dust3d_remesher BEFORE PUBLIC
    third_party/autoremesher/tbbshim
    third_party/autoremesher/include
    third_party/autoremesher/thirdparty/isotropicremesher
    third_party/eigen)
add_library(dust3d_core STATIC ${DUST3D_CORE_SOURCES})
target_include_directories(dust3d_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    third_party
    third_party/eigen
    third_party/rapidxml-1.13
    third_party/earcut.hpp/include)
target_link_libraries(dust3d_core PUBLIC dust3d_remesher dust3d_triangle Threads::Threads)
add_library(dust3d_engine STATIC
    native/Engine/Dust3DBridge.cc
    native/Engine/IndexedMesh.cc
    native/Engine/RetainedPayload.cc
    native/Engine/SnapshotInvalidation.cc
    native/Engine/DocumentReader.cc
    native/Engine/SnapshotValidator.cc)
target_include_directories(dust3d_engine PUBLIC native/Engine/include)
target_link_libraries(dust3d_engine PRIVATE dust3d_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(dust3d_contract_tests tests/core/contract_tests.cc)
    target_link_libraries(dust3d_contract_tests PRIVATE dust3d_engine)
    foreach(scenario null_handles invalid_archive input_limit capsule sword ownership rounded_crossing boolean_boundaries)
        add_test(NAME contract_${scenario}
            COMMAND dust3d_contract_tests ${scenario} ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
        set_tests_properties(contract_${scenario} PROPERTIES TIMEOUT 60)
    endforeach()
    add_executable(dust3d_boolean_weld_tests tests/core/boolean_weld_tests.cc)
    target_link_libraries(dust3d_boolean_weld_tests PRIVATE dust3d_core)
    add_test(NAME boolean_weld_zero_bucket COMMAND dust3d_boolean_weld_tests)
    set_tests_properties(boolean_weld_zero_bucket PROPERTIES TIMEOUT 60)
    add_executable(dust3d_numeric_validation_tests tests/core/numeric_validation_tests.cc)
    target_link_libraries(dust3d_numeric_validation_tests PRIVATE dust3d_engine)
    add_test(NAME contract_numeric_decimals COMMAND dust3d_numeric_validation_tests)
    set_tests_properties(contract_numeric_decimals PROPERTIES TIMEOUT 60)
    add_executable(dust3d_empty_result_tests tests/core/empty_result_tests.cc)
    target_link_libraries(dust3d_empty_result_tests PRIVATE dust3d_engine)
    add_test(NAME contract_successful_empty COMMAND dust3d_empty_result_tests)
    set_tests_properties(contract_successful_empty PROPERTIES TIMEOUT 60)
    add_executable(dust3d_stitching_tests tests/core/stitching_tests.cc)
    target_link_libraries(dust3d_stitching_tests PRIVATE dust3d_engine)
    foreach(scenario lines loops fill)
        add_test(NAME contract_stitching_${scenario} COMMAND dust3d_stitching_tests ${scenario})
        set_tests_properties(contract_stitching_${scenario} PROPERTIES TIMEOUT 60)
    endforeach()
    add_executable(dust3d_wrapping_normals_tests tests/core/wrapping_normals_tests.cc)
    target_link_libraries(dust3d_wrapping_normals_tests PRIVATE dust3d_engine dust3d_core)
    foreach(scenario angles openings precision)
        add_test(NAME contract_wrapping_${scenario} COMMAND dust3d_wrapping_normals_tests ${scenario})
        set_tests_properties(contract_wrapping_${scenario} PROPERTIES TIMEOUT 60)
    endforeach()
    add_executable(dust3d_indexed_mesh_tests tests/core/indexed_mesh_tests.cc)
    target_include_directories(dust3d_indexed_mesh_tests PRIVATE native/Engine)
    target_link_libraries(dust3d_indexed_mesh_tests PRIVATE dust3d_engine dust3d_core)
    foreach(scenario null_handles reconstruction ownership budget attribute_seams invalid_attributes concurrent)
        add_test(NAME contract_indexed_${scenario} COMMAND dust3d_indexed_mesh_tests ${scenario})
        set_tests_properties(contract_indexed_${scenario} PROPERTIES TIMEOUT 60)
    endforeach()
    add_executable(dust3d_snapshot_invalidation_tests tests/core/snapshot_invalidation_tests.cc)
    target_include_directories(dust3d_snapshot_invalidation_tests PRIVATE native/Engine)
    target_link_libraries(dust3d_snapshot_invalidation_tests PRIVATE dust3d_engine dust3d_core)
    add_test(NAME contract_snapshot_invalidation COMMAND dust3d_snapshot_invalidation_tests)
    set_tests_properties(contract_snapshot_invalidation PROPERTIES TIMEOUT 60)
    add_executable(dust3d_retained_payload_tests tests/core/retained_payload_tests.cc)
    target_include_directories(dust3d_retained_payload_tests PRIVATE native/Engine)
    target_link_libraries(dust3d_retained_payload_tests PRIVATE dust3d_engine dust3d_core)
    add_test(NAME contract_retained_payload COMMAND dust3d_retained_payload_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    set_tests_properties(contract_retained_payload PROPERTIES TIMEOUT 60)
    # A C translation unit must consume the public ABI without C++/Qt headers.
    add_executable(dust3d_c_consumer tests/core/c_consumer.c)
    set_target_properties(dust3d_c_consumer PROPERTIES LINKER_LANGUAGE CXX)
    target_link_libraries(dust3d_c_consumer PRIVATE dust3d_engine)
    add_test(NAME contract_c_consumer COMMAND dust3d_c_consumer)
endif()

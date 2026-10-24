cmake_minimum_required(VERSION 3.20)
project(Dust3DCore VERSION 0.1.0 LANGUAGES C CXX)

include(cmake/CheckGlbCatalogQuarantine.cmake)

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
find_package(ZLIB REQUIRED)
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
    native/Engine/ObjDocument.cc
    native/Engine/ObjGeometryPlan.cc
    native/Engine/ObjExportBridge.cc
    native/Engine/FbxExportBridge.cc
    native/Engine/FbxBakedMaterial.cc
    native/Engine/PngEncoding.cc
    native/Engine/FbxAtlasGeometry.cc
    native/Engine/FbxGeometrySelection.cc
    native/Engine/FbxMaterialImagePlan.cc
    native/Engine/FbxMeshScene.cc
    native/Engine/FbxMeshScenePlan.cc
    native/Engine/FbxDocument.cc
    native/Engine/FbxNode.cc
    native/Engine/FbxPropertyBlock.cc
    native/Engine/FbxAnimationMath.cc
    native/Engine/RigTransformValidation.cc
    native/Engine/SourceBakedMesh.cc
    native/Engine/ImportedSurface.cc
    native/Engine/ImportedAssets.cc
    native/Engine/ImportedModels.cc
    native/Engine/GlbReader.cc
    native/Engine/BakedMesh.cc
    native/Engine/Cancellation.cc
    native/Engine/UvMetadata.cc
    native/Engine/GenerationSession.cc
    native/Engine/Dust3DBridge.cc
    native/Engine/SourceProvenance.cc
    native/Engine/IndexedMesh.cc
    native/Engine/RetainedPayload.cc
    native/Engine/SnapshotInvalidation.cc
    native/Engine/DocumentReader.cc
    native/Engine/SnapshotValidator.cc)
target_include_directories(dust3d_engine PUBLIC native/Engine/include)
target_link_libraries(dust3d_engine PRIVATE dust3d_core ZLIB::ZLIB)

add_library(dust3d_worker_protocol STATIC native/Engine/WorkerProtocol.cc native/Engine/WorkerMesh.cc native/Engine/WorkerFailure.cc native/Engine/WorkerAdmission.cc native/Engine/WorkerQueue.cc native/Engine/WorkerLifetime.cc native/Engine/WorkerResources.cc native/Engine/WorkerResourceLaunch.cc)
target_include_directories(dust3d_worker_protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(dust3d_worker_operation STATIC native/Engine/WorkerOperation.cc)
target_link_libraries(dust3d_worker_operation PUBLIC dust3d_worker_protocol dust3d_engine)

add_library(dust3d_worker_resource_setup STATIC native/Engine/WorkerResourceSetup.cc)
target_link_libraries(dust3d_worker_resource_setup PUBLIC dust3d_worker_protocol)
if(APPLE OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_library(dust3d_worker_unix_resources STATIC native/Engine/WorkerUnixResources.cc)
    target_link_libraries(dust3d_worker_unix_resources PUBLIC dust3d_worker_resource_setup)
endif()

if(UNIX)
    add_executable(dust3d_export native/ExportCli/main.cc)
    set_target_properties(dust3d_export PROPERTIES OUTPUT_NAME dust3d-export)
    target_link_libraries(dust3d_export PRIVATE dust3d_engine)
    add_executable(dust3d_worker native/Engine/WorkerMain.cc)
    target_link_libraries(dust3d_worker PRIVATE dust3d_worker_operation dust3d_worker_resource_setup)
    if(TARGET dust3d_worker_unix_resources)
        target_compile_definitions(dust3d_worker PRIVATE DUST3D_WORKER_HAS_RESOURCE_SETUP=1)
        target_link_libraries(dust3d_worker PRIVATE dust3d_worker_unix_resources)
    endif()
endif()

# Linux needs glibc's close-from spawn action to avoid inheriting unrelated application descriptors.
set(DUST3D_HAS_WORKER_TRANSPORT ${APPLE})
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include(CheckSymbolExists)
    include(CMakePushCheckState)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
    check_symbol_exists(posix_spawn_file_actions_addclosefrom_np "spawn.h" DUST3D_HAS_SPAWN_CLOSEFROM)
    cmake_pop_check_state()
    set(DUST3D_HAS_WORKER_TRANSPORT ${DUST3D_HAS_SPAWN_CLOSEFROM})
endif()
if(DUST3D_HAS_WORKER_TRANSPORT)
    add_library(dust3d_worker_transport STATIC native/Engine/WorkerTransport.cc)
    target_link_libraries(dust3d_worker_transport PUBLIC dust3d_worker_protocol)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(dust3d_worker_resource_observation_tests tests/core/worker_resource_observation_tests.cc)
    target_link_libraries(dust3d_worker_resource_observation_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_resource_observation_contracts COMMAND dust3d_worker_resource_observation_tests)
    set_tests_properties(worker_resource_observation_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_worker_resource_launch_tests tests/core/worker_resource_launch_tests.cc)
    target_link_libraries(dust3d_worker_resource_launch_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_resource_launch_contracts COMMAND dust3d_worker_resource_launch_tests)
    set_tests_properties(worker_resource_launch_contracts PROPERTIES TIMEOUT 30)
    if(TARGET dust3d_worker_unix_resources)
        add_executable(dust3d_worker_resource_setup_tests tests/core/worker_resource_setup_tests.cc)
        target_link_libraries(dust3d_worker_resource_setup_tests PRIVATE dust3d_worker_unix_resources)
        add_test(NAME worker_resource_setup_contracts COMMAND dust3d_worker_resource_setup_tests)
        set_tests_properties(worker_resource_setup_contracts PROPERTIES TIMEOUT 30)
    endif()
    add_executable(dust3d_worker_resource_tests tests/core/worker_resource_tests.cc)
    target_link_libraries(dust3d_worker_resource_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_resource_contracts COMMAND dust3d_worker_resource_tests)
    set_tests_properties(worker_resource_contracts PROPERTIES TIMEOUT 30)

    add_executable(dust3d_snapshot_writer_tests tests/core/snapshot_writer_tests.cc)
    target_link_libraries(dust3d_snapshot_writer_tests PRIVATE dust3d_core)
    add_test(NAME snapshot_writer_contracts COMMAND dust3d_snapshot_writer_tests)
    set_tests_properties(snapshot_writer_contracts PROPERTIES TIMEOUT 30)




    add_executable(dust3d_original_export_preparation_tests tests/core/original_export_preparation_tests.cc)
    target_include_directories(dust3d_original_export_preparation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} native/Engine)
    target_link_libraries(dust3d_original_export_preparation_tests PRIVATE dust3d_engine)
    add_test(NAME original_export_preparation_contracts COMMAND dust3d_original_export_preparation_tests)
    set_tests_properties(original_export_preparation_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_obj_document_tests tests/core/obj_document_tests.cc)
    target_include_directories(dust3d_obj_document_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} native/Engine)
    target_link_libraries(dust3d_obj_document_tests PRIVATE dust3d_engine)
    add_test(NAME obj_document_contracts COMMAND dust3d_obj_document_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    set_tests_properties(obj_document_contracts PROPERTIES TIMEOUT 60)
    add_executable(dust3d_obj_export_api_tests tests/core/obj_export_api_tests.cc)
    target_link_libraries(dust3d_obj_export_api_tests PRIVATE dust3d_engine)
    add_test(NAME obj_export_api_contracts COMMAND dust3d_obj_export_api_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    set_tests_properties(obj_export_api_contracts PROPERTIES TIMEOUT 60)
    add_executable(dust3d_fbx_export_api_tests tests/core/fbx_export_api_tests.cc)
    target_link_libraries(dust3d_fbx_export_api_tests PRIVATE dust3d_engine)
    add_test(NAME fbx_export_api_contracts COMMAND dust3d_fbx_export_api_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    set_tests_properties(fbx_export_api_contracts PROPERTIES TIMEOUT 60)
    add_executable(dust3d_fbx_baked_material_tests tests/core/fbx_baked_material_tests.cc)
    target_include_directories(dust3d_fbx_baked_material_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} native/Engine)
    target_link_libraries(dust3d_fbx_baked_material_tests PRIVATE dust3d_engine)
    add_test(NAME fbx_baked_material_contracts COMMAND dust3d_fbx_baked_material_tests)
    set_tests_properties(fbx_baked_material_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_png_encoding_tests tests/core/png_encoding_tests.cc)
    target_include_directories(dust3d_png_encoding_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} native/Engine)
    target_link_libraries(dust3d_png_encoding_tests PRIVATE dust3d_engine)
    add_test(NAME png_encoding_contracts COMMAND dust3d_png_encoding_tests)
    set_tests_properties(png_encoding_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_fbx_generation_geometry_tests tests/core/fbx_generation_geometry_tests.cc)
    target_include_directories(dust3d_fbx_generation_geometry_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} native/Engine)
    target_link_libraries(dust3d_fbx_generation_geometry_tests PRIVATE dust3d_engine)
    add_test(NAME fbx_generation_geometry_contracts COMMAND dust3d_fbx_generation_geometry_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    set_tests_properties(fbx_generation_geometry_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_fbx_geometry_selection_tests tests/core/fbx_geometry_selection_tests.cc)
    target_include_directories(dust3d_fbx_geometry_selection_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} native/Engine)
    target_link_libraries(dust3d_fbx_geometry_selection_tests PRIVATE dust3d_engine)
    add_test(NAME fbx_geometry_selection_contracts COMMAND dust3d_fbx_geometry_selection_tests)
    set_tests_properties(fbx_geometry_selection_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_fbx_material_scene_tests tests/core/fbx_material_scene_tests.cc)
    target_include_directories(dust3d_fbx_material_scene_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} native/Engine)
    target_link_libraries(dust3d_fbx_material_scene_tests PRIVATE dust3d_engine)
    add_test(NAME fbx_material_scene_contracts COMMAND dust3d_fbx_material_scene_tests)
    set_tests_properties(fbx_material_scene_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_fbx_mesh_scene_tests tests/core/fbx_mesh_scene_tests.cc)
    target_include_directories(dust3d_fbx_mesh_scene_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} native/Engine)
    target_link_libraries(dust3d_fbx_mesh_scene_tests PRIVATE dust3d_engine)
    add_test(NAME fbx_mesh_scene_contracts COMMAND dust3d_fbx_mesh_scene_tests)
    set_tests_properties(fbx_mesh_scene_contracts PROPERTIES TIMEOUT 30)
    # Optional standard-library acceptance tooling does not add a production dependency.
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME obj_scene_verifier_contracts COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover
            -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/core" -p test_obj_scene_import.py)
        add_test(NAME obj_scene_verifier_optimized_contracts COMMAND "${Python3_EXECUTABLE}" -B -O -m unittest discover
            -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/core" -p test_obj_scene_import.py)
        set_tests_properties(obj_scene_verifier_contracts obj_scene_verifier_optimized_contracts PROPERTIES TIMEOUT 30
            ENVIRONMENT "DUST3D_BUILD_ROOT=${CMAKE_CURRENT_BINARY_DIR}")
        if(UNIX)
            add_test(NAME export_cli_contracts COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover
                -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/core" -p test_export_cli.py)
            set_tests_properties(export_cli_contracts PROPERTIES TIMEOUT 180
                ENVIRONMENT "DUST3D_BUILD_ROOT=${CMAKE_CURRENT_BINARY_DIR};DUST3D_CORE_FIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures;DUST3D_EXPORT_EXECUTABLE=$<TARGET_FILE:dust3d_export>")
        endif()
        add_test(NAME fbx_baked_scene_verifier_contracts COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover
            -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/core" -p test_fbx_baked_scene_import.py)
        add_test(NAME fbx_baked_scene_verifier_optimized_contracts COMMAND "${Python3_EXECUTABLE}" -B -O -m unittest discover
            -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/core" -p test_fbx_baked_scene_import.py)
        set_tests_properties(fbx_baked_scene_verifier_contracts fbx_baked_scene_verifier_optimized_contracts PROPERTIES TIMEOUT 30
            ENVIRONMENT "DUST3D_BUILD_ROOT=${CMAKE_CURRENT_BINARY_DIR}")
        add_test(NAME png_encoding_independent_contracts COMMAND "${Python3_EXECUTABLE}" -B -O
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/core/verify_png_encoding.py" $<TARGET_FILE:dust3d_png_encoding_tests>)
        set_tests_properties(png_encoding_independent_contracts PROPERTIES TIMEOUT 90
            ENVIRONMENT "DUST3D_BUILD_ROOT=${CMAKE_CURRENT_BINARY_DIR}")
        add_test(NAME fbx_mesh_scene_verifier_contracts COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover
            -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/core" -p test_fbx_mesh_scene_import.py)
        set_tests_properties(fbx_mesh_scene_verifier_contracts PROPERTIES TIMEOUT 30
            ENVIRONMENT "DUST3D_BUILD_ROOT=${CMAKE_CURRENT_BINARY_DIR}")
        add_test(NAME fbx_mesh_scene_verifier_optimized_contracts COMMAND "${Python3_EXECUTABLE}" -B -O -m unittest discover
            -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/core" -p test_fbx_mesh_scene_import.py)
        set_tests_properties(fbx_mesh_scene_verifier_optimized_contracts PROPERTIES TIMEOUT 30
            ENVIRONMENT "DUST3D_BUILD_ROOT=${CMAKE_CURRENT_BINARY_DIR}")
        add_test(NAME fbx_material_scene_verifier_contracts COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover
            -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/core" -p test_fbx_material_scene_import.py)
        set_tests_properties(fbx_material_scene_verifier_contracts PROPERTIES TIMEOUT 30
            ENVIRONMENT "DUST3D_BUILD_ROOT=${CMAKE_CURRENT_BINARY_DIR}")
        add_test(NAME fbx_material_scene_verifier_optimized_contracts COMMAND "${Python3_EXECUTABLE}" -B -O -m unittest discover
            -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/core" -p test_fbx_material_scene_import.py)
        set_tests_properties(fbx_material_scene_verifier_optimized_contracts PROPERTIES TIMEOUT 30
            ENVIRONMENT "DUST3D_BUILD_ROOT=${CMAKE_CURRENT_BINARY_DIR}")
    else()
        message(STATUS "Skipping optional FBX importer-verifier contracts: Python3 unavailable")
    endif()
    # Existing MIT codec is an ordinary known-positive test oracle, never a product dependency.
    add_library(dust3d_fbx_reference STATIC
        application/third_party/fbx/src/fbxdocument.cpp
        application/third_party/fbx/src/fbxnode.cpp
        application/third_party/fbx/src/fbxproperty.cpp
        application/third_party/fbx/src/fbxutil.cpp
        application/third_party/miniz/miniz.c)
    target_include_directories(dust3d_fbx_reference PUBLIC application/third_party/fbx/src PRIVATE application/third_party/miniz)
    add_executable(dust3d_fbx_reference_tests tests/core/fbx_reference_tests.cc)
    target_include_directories(dust3d_fbx_reference_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} native/Engine)
    target_link_libraries(dust3d_fbx_reference_tests PRIVATE dust3d_engine dust3d_fbx_reference)
    add_test(NAME fbx_reference_contracts COMMAND dust3d_fbx_reference_tests "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(fbx_reference_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_fbx_document_tests tests/core/fbx_document_tests.cc)
    target_include_directories(dust3d_fbx_document_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} native/Engine)
    target_link_libraries(dust3d_fbx_document_tests PRIVATE dust3d_engine)
    add_test(NAME fbx_document_contracts COMMAND dust3d_fbx_document_tests)
    set_tests_properties(fbx_document_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_fbx_node_tests tests/core/fbx_node_tests.cc)
    target_include_directories(dust3d_fbx_node_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} native/Engine)
    target_link_libraries(dust3d_fbx_node_tests PRIVATE dust3d_engine)
    add_test(NAME fbx_node_contracts COMMAND dust3d_fbx_node_tests)
    set_tests_properties(fbx_node_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_fbx_property_block_tests tests/core/fbx_property_block_tests.cc)
    target_include_directories(dust3d_fbx_property_block_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} native/Engine)
    target_link_libraries(dust3d_fbx_property_block_tests PRIVATE dust3d_engine)
    add_test(NAME fbx_property_block_contracts COMMAND dust3d_fbx_property_block_tests)
    set_tests_properties(fbx_property_block_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_fbx_transform_tests tests/core/fbx_transform_tests.cc)
    target_include_directories(dust3d_fbx_transform_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} native/Engine)
    target_link_libraries(dust3d_fbx_transform_tests PRIVATE dust3d_engine)
    add_test(NAME fbx_transform_contracts COMMAND dust3d_fbx_transform_tests)
    set_tests_properties(fbx_transform_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_fbx_animation_math_tests tests/core/fbx_animation_math_tests.cc)
    target_include_directories(dust3d_fbx_animation_math_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} native/Engine)
    target_link_libraries(dust3d_fbx_animation_math_tests PRIVATE dust3d_engine)
    add_test(NAME fbx_animation_math_contracts COMMAND dust3d_fbx_animation_math_tests)
    set_tests_properties(fbx_animation_math_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_worker_lifetime_tests tests/core/worker_lifetime_tests.cc)
    target_link_libraries(dust3d_worker_lifetime_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_lifetime_contracts COMMAND dust3d_worker_lifetime_tests)
    set_tests_properties(worker_lifetime_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_worker_queue_tests tests/core/worker_queue_tests.cc)
    target_link_libraries(dust3d_worker_queue_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_queue_contracts COMMAND dust3d_worker_queue_tests)
    set_tests_properties(worker_queue_contracts PROPERTIES TIMEOUT 30)
    if(TARGET dust3d_worker_transport)
        add_executable(dust3d_worker_transport_tests tests/core/worker_transport_tests.cc)
        target_link_libraries(dust3d_worker_transport_tests PRIVATE dust3d_worker_transport dust3d_engine)
        add_test(NAME worker_transport_contracts COMMAND dust3d_worker_transport_tests $<TARGET_FILE:dust3d_worker> ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
        set_tests_properties(worker_transport_contracts PROPERTIES TIMEOUT 60)
    endif()
    add_executable(dust3d_worker_admission_tests tests/core/worker_admission_tests.cc)
    target_link_libraries(dust3d_worker_admission_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_admission_contracts COMMAND dust3d_worker_admission_tests)
    set_tests_properties(worker_admission_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_worker_operation_tests tests/core/worker_operation_tests.cc)
    target_link_libraries(dust3d_worker_operation_tests PRIVATE dust3d_worker_operation)
    add_test(NAME worker_operation_contracts COMMAND dust3d_worker_operation_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    set_tests_properties(worker_operation_contracts PROPERTIES TIMEOUT 60)
    if(UNIX)
        add_executable(dust3d_worker_process_tests tests/core/worker_process_tests.cc)
        target_link_libraries(dust3d_worker_process_tests PRIVATE dust3d_engine dust3d_worker_protocol)
        add_test(NAME worker_process_contracts COMMAND "${CMAKE_COMMAND}"
            "-DWORKER=$<TARGET_FILE:dust3d_worker>"
            "-DVERIFIER=$<TARGET_FILE:dust3d_worker_process_tests>"
            "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures"
            "-DSCRATCH=${CMAKE_CURRENT_BINARY_DIR}/worker-process-tests"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/worker_process_tests.cmake")
        set_tests_properties(worker_process_contracts PROPERTIES TIMEOUT 180)
        if(TARGET dust3d_worker_unix_resources)
            if(DUST3D_ENABLE_SANITIZERS)
                set(DUST3D_RESOURCE_RUNTIME "asan")
            else()
                set(DUST3D_RESOURCE_RUNTIME "ordinary")
            endif()
            add_test(NAME worker_resource_process_contracts COMMAND "${CMAKE_COMMAND}"
                "-DWORKER=$<TARGET_FILE:dust3d_worker>"
                "-DVERIFIER=$<TARGET_FILE:dust3d_worker_process_tests>"
                "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures"
                "-DSCRATCH=${CMAKE_CURRENT_BINARY_DIR}/worker-resource-process-tests"
                "-DRUNTIME=${DUST3D_RESOURCE_RUNTIME}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/worker_resource_process_tests.cmake")
            set_tests_properties(worker_resource_process_contracts PROPERTIES TIMEOUT 180)
        endif()
    endif()
    add_executable(dust3d_worker_failure_tests tests/core/worker_failure_tests.cc)
    target_link_libraries(dust3d_worker_failure_tests PRIVATE dust3d_worker_protocol dust3d_engine)
    add_test(NAME worker_failure_contracts COMMAND dust3d_worker_failure_tests)
    set_tests_properties(worker_failure_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_worker_mesh_tests tests/core/worker_mesh_tests.cc)
    target_link_libraries(dust3d_worker_mesh_tests PRIVATE dust3d_worker_protocol dust3d_engine)
    add_test(NAME worker_mesh_contracts COMMAND dust3d_worker_mesh_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    set_tests_properties(worker_mesh_contracts PROPERTIES TIMEOUT 60)
    add_executable(dust3d_worker_protocol_tests tests/core/worker_protocol_tests.cc)
    target_link_libraries(dust3d_worker_protocol_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_protocol_contracts COMMAND dust3d_worker_protocol_tests)
    set_tests_properties(worker_protocol_contracts PROPERTIES TIMEOUT 30)
    # Verify the real C importer and owned Sendable copies on native macOS hosts.
    # Sanitized C++ contracts remain separate because swiftc does not link UBSan.
    if(APPLE AND NOT DUST3D_ENABLE_SANITIZERS AND NOT CMAKE_CROSSCOMPILING)
        find_program(DUST3D_XCRUN xcrun REQUIRED)
        set(DUST3D_SWIFT_MODULE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/swift-module")
        file(GENERATE OUTPUT "${DUST3D_SWIFT_MODULE_DIRECTORY}/module.modulemap"
            CONTENT "module Dust3DEngine { header \"${CMAKE_CURRENT_SOURCE_DIR}/native/Engine/include/Dust3DBridge.h\" export * }\n")
        set(DUST3D_SWIFT_MIP_CONSUMER "${CMAKE_CURRENT_BINARY_DIR}/source-baked-mip-swift")
        add_custom_command(OUTPUT "${DUST3D_SWIFT_MIP_CONSUMER}"
            COMMAND "${DUST3D_XCRUN}" swift-format lint --strict "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/SourceBakedMipSmoke.swift"
            COMMAND "${DUST3D_XCRUN}" swiftc -swift-version 6 -strict-concurrency=complete
                -warnings-as-errors -parse-as-library
                -module-cache-path "${CMAKE_CURRENT_BINARY_DIR}/swift-module-cache"
                -I "${DUST3D_SWIFT_MODULE_DIRECTORY}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/SourceBakedMipSmoke.swift"
                "$<TARGET_FILE:dust3d_engine>" "$<TARGET_FILE:dust3d_core>"
                "$<TARGET_FILE:dust3d_remesher>" "$<TARGET_FILE:dust3d_triangle>"
                -lc++ -o "${DUST3D_SWIFT_MIP_CONSUMER}"
            DEPENDS tests/swift/SourceBakedMipSmoke.swift native/Engine/include/Dust3DBridge.h
                dust3d_engine dust3d_core dust3d_remesher dust3d_triangle
            VERBATIM)
        add_custom_target(dust3d_source_baked_mip_swift ALL DEPENDS "${DUST3D_SWIFT_MIP_CONSUMER}")
        add_test(NAME source_baked_mip_swift
            COMMAND "${DUST3D_SWIFT_MIP_CONSUMER}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures/Capsule.ds3")
        set_tests_properties(source_baked_mip_swift PROPERTIES TIMEOUT 60)
        set(DUST3D_SWIFT_FBX_CONSUMER "${CMAKE_CURRENT_BINARY_DIR}/fbx-export-swift")
        add_custom_command(OUTPUT "${DUST3D_SWIFT_FBX_CONSUMER}"
            COMMAND "${DUST3D_XCRUN}" swift-format lint --strict "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/FbxExportSmoke.swift"
            COMMAND "${DUST3D_XCRUN}" swiftc -swift-version 6 -strict-concurrency=complete
                -warnings-as-errors -parse-as-library
                -module-cache-path "${CMAKE_CURRENT_BINARY_DIR}/swift-module-cache"
                -I "${DUST3D_SWIFT_MODULE_DIRECTORY}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/FbxExportSmoke.swift"
                "$<TARGET_FILE:dust3d_engine>" "$<TARGET_FILE:dust3d_core>"
                "$<TARGET_FILE:dust3d_remesher>" "$<TARGET_FILE:dust3d_triangle>"
                -lc++ -lz -o "${DUST3D_SWIFT_FBX_CONSUMER}"
            DEPENDS tests/swift/FbxExportSmoke.swift native/Engine/include/Dust3DBridge.h
                dust3d_engine dust3d_core dust3d_remesher dust3d_triangle
            VERBATIM)
        add_custom_target(dust3d_fbx_export_swift ALL DEPENDS "${DUST3D_SWIFT_FBX_CONSUMER}")
        add_test(NAME fbx_export_swift COMMAND "${DUST3D_SWIFT_FBX_CONSUMER}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures/Capsule.ds3")
        set_tests_properties(fbx_export_swift PROPERTIES TIMEOUT 60)
    endif()
    add_executable(dust3d_source_baked_mip_tests tests/core/source_baked_mip_tests.cc)
    target_link_libraries(dust3d_source_baked_mip_tests PRIVATE dust3d_engine)
    add_test(NAME source_baked_mip_contracts COMMAND dust3d_source_baked_mip_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    set_tests_properties(source_baked_mip_contracts PROPERTIES TIMEOUT 180)
    add_executable(dust3d_source_texture_mip_tests tests/core/source_texture_mip_tests.cc)
    target_link_libraries(dust3d_source_texture_mip_tests PRIVATE dust3d_core)
    add_test(NAME source_texture_mip_contracts COMMAND dust3d_source_texture_mip_tests)
    set_tests_properties(source_texture_mip_contracts PROPERTIES TIMEOUT 60)
    add_executable(dust3d_snapshot_numeric_tests tests/core/snapshot_numeric_tests.cc native/Engine/SnapshotValidator.cc)
    target_include_directories(dust3d_snapshot_numeric_tests PRIVATE . native/Engine native/Engine/include)
    add_test(NAME contract_snapshot_numeric COMMAND dust3d_snapshot_numeric_tests)
    set_tests_properties(contract_snapshot_numeric PROPERTIES TIMEOUT 30)
    add_test(NAME contract_glb_catalog_quarantine COMMAND "${CMAKE_COMMAND}"
        "-DQUARANTINE_CHECK=${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckGlbCatalogQuarantine.cmake"
        "-DTEST_WORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/quarantine-tests"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/glb_catalog_quarantine_tests.cmake")
    set_tests_properties(contract_glb_catalog_quarantine PROPERTIES TIMEOUT 30)
    # Core #84: retain fault-probe evidence in history, never ordinary builds/CI.
    add_executable(dust3d_json_comparison_tests tests/core/json_comparison_tests.cc)
    target_include_directories(dust3d_json_comparison_tests PRIVATE third_party)
    add_test(NAME contract_json_comparison COMMAND dust3d_json_comparison_tests)
    set_tests_properties(contract_json_comparison PROPERTIES TIMEOUT 60)
    add_executable(dust3d_source_bake_composition_tests tests/core/source_bake_composition_tests.cc)
    target_link_libraries(dust3d_source_bake_composition_tests PRIVATE dust3d_engine dust3d_core)
    add_test(NAME source_bake_composition COMMAND dust3d_source_bake_composition_tests)
    set_tests_properties(source_bake_composition PROPERTIES TIMEOUT 60)
    add_executable(dust3d_source_baked_c_consumer tests/core/source_baked_mesh_c_consumer.c)
    set_target_properties(dust3d_source_baked_c_consumer PROPERTIES LINKER_LANGUAGE CXX)
    target_link_libraries(dust3d_source_baked_c_consumer PRIVATE dust3d_engine)
    add_test(NAME source_baked_c_consumer COMMAND dust3d_source_baked_c_consumer)
    add_executable(dust3d_source_baked_mesh_tests tests/core/source_baked_mesh_tests.cc)
    target_link_libraries(dust3d_source_baked_mesh_tests PRIVATE dust3d_engine)
    add_test(NAME source_baked_mesh_contracts COMMAND dust3d_source_baked_mesh_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    set_tests_properties(source_baked_mesh_contracts PROPERTIES TIMEOUT 60)
    add_executable(dust3d_source_texture_baker_tests tests/core/source_texture_baker_tests.cc)
    target_link_libraries(dust3d_source_texture_baker_tests PRIVATE dust3d_engine dust3d_core)
    add_test(NAME source_texture_baker_contracts COMMAND dust3d_source_texture_baker_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    set_tests_properties(source_texture_baker_contracts PROPERTIES TIMEOUT 60)
    add_executable(dust3d_source_uv_atlas_tests tests/core/source_uv_atlas_tests.cc)
    target_link_libraries(dust3d_source_uv_atlas_tests PRIVATE dust3d_engine dust3d_core)
    add_test(NAME source_uv_atlas_contracts COMMAND dust3d_source_uv_atlas_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    set_tests_properties(source_uv_atlas_contracts PROPERTIES TIMEOUT 60)
    add_executable(dust3d_color_texture_sampler_tests tests/core/color_texture_sampler_tests.cc)
    target_link_libraries(dust3d_color_texture_sampler_tests PRIVATE dust3d_core)
    add_test(NAME color_texture_sampler_contracts COMMAND dust3d_color_texture_sampler_tests)
    set_tests_properties(color_texture_sampler_contracts PROPERTIES TIMEOUT 60)
    add_executable(dust3d_imported_surface_tests tests/core/imported_surface_tests.cc)
    target_link_libraries(dust3d_imported_surface_tests PRIVATE dust3d_engine)
    add_test(NAME contract_imported_surfaces COMMAND dust3d_imported_surface_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    set_tests_properties(contract_imported_surfaces PROPERTIES TIMEOUT 60)
    add_executable(dust3d_imported_asset_tests tests/core/imported_asset_tests.cc)
    target_include_directories(dust3d_imported_asset_tests PRIVATE third_party)
    target_link_libraries(dust3d_imported_asset_tests PRIVATE dust3d_engine)
    add_test(NAME contract_imported_assets COMMAND dust3d_imported_asset_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    set_tests_properties(contract_imported_assets PROPERTIES TIMEOUT 60)
    add_executable(dust3d_position_key_tests tests/core/position_key_tests.cc)
    target_link_libraries(dust3d_position_key_tests PRIVATE dust3d_core)
    add_test(NAME contract_position_quantization COMMAND dust3d_position_key_tests)
    set_tests_properties(contract_position_quantization PROPERTIES TIMEOUT 60)
    add_executable(dust3d_spine_deformer_tests tests/core/spine_deformer_tests.cc)
    target_link_libraries(dust3d_spine_deformer_tests PRIVATE dust3d_core)
    add_test(NAME contract_spine_deformation COMMAND dust3d_spine_deformer_tests)
    set_tests_properties(contract_spine_deformation PROPERTIES TIMEOUT 60)
    add_executable(dust3d_imported_model_tests tests/core/imported_model_tests.cc)
    target_link_libraries(dust3d_imported_model_tests PRIVATE dust3d_engine dust3d_core)
    add_test(NAME contract_imported_models COMMAND dust3d_imported_model_tests)
    set_tests_properties(contract_imported_models PROPERTIES TIMEOUT 60)
    add_executable(dust3d_glb_reader_tests tests/core/glb_reader_tests.cc)
    target_include_directories(dust3d_glb_reader_tests PRIVATE native/Engine)
    target_link_libraries(dust3d_glb_reader_tests PRIVATE dust3d_engine dust3d_core)
    add_test(NAME contract_glb_reader COMMAND dust3d_glb_reader_tests)
    set_tests_properties(contract_glb_reader PROPERTIES TIMEOUT 60)
    add_executable(dust3d_baked_mesh_tests tests/core/baked_mesh_tests.cc)
    target_link_libraries(dust3d_baked_mesh_tests PRIVATE dust3d_engine dust3d_core)
    add_test(NAME baked_mesh_contracts COMMAND dust3d_baked_mesh_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    set_tests_properties(baked_mesh_contracts PROPERTIES TIMEOUT 60)
    add_executable(dust3d_texture_baker_tests tests/core/texture_baker_tests.cc)
    target_link_libraries(dust3d_texture_baker_tests PRIVATE dust3d_engine dust3d_core)
    add_test(NAME texture_baker_contracts COMMAND dust3d_texture_baker_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    set_tests_properties(texture_baker_contracts PROPERTIES TIMEOUT 60)
    add_executable(dust3d_uv_metadata_tests tests/core/uv_metadata_tests.cc)
    target_link_libraries(dust3d_uv_metadata_tests PRIVATE dust3d_engine dust3d_core)
    add_test(NAME uv_metadata_contracts COMMAND dust3d_uv_metadata_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    set_tests_properties(uv_metadata_contracts PROPERTIES TIMEOUT 60)
    add_executable(dust3d_uv_packer_numeric_tests tests/core/uv_packer_numeric_tests.cc)
    target_link_libraries(dust3d_uv_packer_numeric_tests PRIVATE dust3d_core)
    add_test(NAME uv_packer_numeric_contracts COMMAND dust3d_uv_packer_numeric_tests)
    set_tests_properties(uv_packer_numeric_contracts PROPERTIES TIMEOUT 60)
    add_executable(dust3d_uv_atlas_tests tests/core/uv_atlas_tests.cc)
    target_link_libraries(dust3d_uv_atlas_tests PRIVATE dust3d_engine dust3d_core)
    add_test(NAME uv_atlas_contracts COMMAND dust3d_uv_atlas_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    set_tests_properties(uv_atlas_contracts PROPERTIES TIMEOUT 60)
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
    foreach(scenario lines loops fill output_containment)
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
    add_executable(dust3d_generation_session_tests tests/core/generation_session_tests.cc)
    target_link_libraries(dust3d_generation_session_tests PRIVATE dust3d_engine dust3d_core)
    add_test(NAME contract_generation_sessions COMMAND dust3d_generation_session_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    # The expanded wrapping/boolean corpus exceeds 60 seconds under Linux ASan
    # on a contended hosted runner; retain the workload with a bounded test timeout.
    set_tests_properties(contract_generation_sessions PROPERTIES TIMEOUT 180)
    add_executable(dust3d_source_provenance_tests tests/core/source_provenance_tests.cc)
    target_include_directories(dust3d_source_provenance_tests PRIVATE native/Engine)
    target_link_libraries(dust3d_source_provenance_tests PRIVATE dust3d_engine dust3d_core)
    add_test(NAME contract_source_provenance COMMAND dust3d_source_provenance_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    set_tests_properties(contract_source_provenance PROPERTIES TIMEOUT 60)
    # A C translation unit must consume the public ABI without C++/Qt headers.
    add_executable(dust3d_c_consumer tests/core/c_consumer.c)
    set_target_properties(dust3d_c_consumer PROPERTIES LINKER_LANGUAGE CXX)
    target_link_libraries(dust3d_c_consumer PRIVATE dust3d_engine)
    add_test(NAME contract_c_consumer COMMAND dust3d_c_consumer)
endif()

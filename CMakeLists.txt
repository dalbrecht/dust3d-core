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
    native/Engine/EngineSupport.cc
    native/Engine/AnimationCatalogBridge.cc
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

add_library(dust3d_worker_protocol STATIC native/Engine/WorkerImportedSurface.cc native/Engine/WorkerImportedMaterials.cc native/Engine/WorkerLegacyBaked.cc native/Engine/WorkerSourceBaked.cc native/Engine/WorkerBakedTextures.cc native/Engine/WorkerIndexedCopyBudget.cc native/Engine/WorkerIndexedSources.cc native/Engine/WorkerIndexedMesh.cc native/Engine/WorkerSourceRequest.cc native/Engine/WorkerSourcedMesh.cc native/Engine/WorkerSourceHints.cc native/Engine/WorkerProtocol.cc native/Engine/WorkerReplyCollector.cc native/Engine/WorkerMesh.cc native/Engine/WorkerFailure.cc native/Engine/WorkerAdmission.cc native/Engine/WorkerQueue.cc native/Engine/WorkerLifetime.cc native/Engine/WorkerCleanup.cc native/Engine/WorkerAttempt.cc native/Engine/WorkerUv.cc native/Engine/WorkerUvMesh.cc native/Engine/WorkerUvRequest.cc native/Engine/WorkerResources.cc native/Engine/WorkerResourceLaunch.cc)
target_include_directories(dust3d_worker_protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(dust3d_worker_operation STATIC native/Engine/WorkerImportedSurfaceCopy.cc native/Engine/WorkerImportedMaterialsCopy.cc native/Engine/WorkerLegacyBakedCopy.cc native/Engine/WorkerMeshCopyInspection.cc native/Engine/WorkerSourceBakedCopy.cc native/Engine/WorkerOperation.cc native/Engine/WorkerUvOperation.cc native/Engine/WorkerSourceOperation.cc native/Engine/WorkerIndexedSourceOperation.cc)
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
    add_library(dust3d_worker_job_io STATIC native/Engine/WorkerJobIO.cc)
    target_link_libraries(dust3d_worker_job_io PUBLIC dust3d_worker_operation)
    add_library(dust3d_worker_runtime STATIC native/WorkerRuntime/WorkerRuntimeBridge.cc native/WorkerRuntime/WorkerReplyBridge.cc native/WorkerRuntime/WorkerDecodedBridge.cc native/WorkerRuntime/WorkerRequestBridge.cc native/WorkerRuntime/WorkerQueueBridge.cc)
    target_include_directories(dust3d_worker_runtime PUBLIC native/WorkerRuntime/include PRIVATE native/Engine)
    target_link_libraries(dust3d_worker_runtime PUBLIC dust3d_worker_job_io)
    add_executable(dust3d_worker native/Engine/WorkerMain.cc)
    target_link_libraries(dust3d_worker PRIVATE dust3d_worker_job_io dust3d_worker_resource_setup)
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
    add_executable(dust3d_spring_step_admission_tests
        tests/core/spring_step_admission_tests.cc
        dust3d/animation/spring_step_admission.cc)
    target_include_directories(dust3d_spring_step_admission_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME spring_step_admission_contracts COMMAND dust3d_spring_step_admission_tests)
    set_tests_properties(spring_step_admission_contracts PROPERTIES TIMEOUT 10)
    add_executable(dust3d_animation_audio_metadata_tests
        tests/core/animation_audio_metadata_tests.cc
        native/Engine/EngineSupport.cc
        native/Engine/AnimationCatalogBridge.cc
        dust3d/animation/surface_material.cc
        dust3d/animation/parameter_catalog.cc
        dust3d/animation/animation_defaults.cc
        dust3d/animation/animation_catalog.cc
        dust3d/animation/biped/clip_metadata.cc)
    target_include_directories(dust3d_animation_audio_metadata_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} native/Engine/include)
    add_test(NAME animation_audio_metadata_contracts COMMAND dust3d_animation_audio_metadata_tests)
    set_tests_properties(animation_audio_metadata_contracts PROPERTIES TIMEOUT 10)
    add_executable(dust3d_animation_timing_api_tests
        tests/core/animation_timing_api_tests.cc
        native/Engine/EngineSupport.cc
        native/Engine/AnimationCatalogBridge.cc
        dust3d/animation/surface_material.cc
        dust3d/animation/parameter_catalog.cc
        dust3d/animation/animation_defaults.cc
        dust3d/animation/animation_catalog.cc
        dust3d/animation/biped/clip_metadata.cc)
    target_include_directories(dust3d_animation_timing_api_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} native/Engine/include)
    add_test(NAME animation_timing_api_contracts COMMAND dust3d_animation_timing_api_tests)
    set_tests_properties(animation_timing_api_contracts PROPERTIES TIMEOUT 10)
    add_executable(dust3d_animation_control_api_tests
        tests/core/animation_control_api_tests.cc
        native/Engine/EngineSupport.cc
        native/Engine/AnimationCatalogBridge.cc
        dust3d/animation/surface_material.cc
        dust3d/animation/parameter_catalog.cc
        dust3d/animation/animation_defaults.cc
        dust3d/animation/animation_catalog.cc
        dust3d/animation/biped/clip_metadata.cc)
    target_include_directories(dust3d_animation_control_api_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} native/Engine/include)
    add_test(NAME animation_control_api_contracts COMMAND dust3d_animation_control_api_tests)
    set_tests_properties(animation_control_api_contracts PROPERTIES TIMEOUT 10)
    add_executable(dust3d_animation_timing_controls_tests
        tests/core/animation_timing_controls_tests.cc
        dust3d/animation/parameter_catalog.cc
        dust3d/animation/animation_defaults.cc
        dust3d/animation/animation_catalog.cc
        dust3d/animation/biped/clip_metadata.cc)
    target_include_directories(dust3d_animation_timing_controls_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME animation_timing_controls_contracts COMMAND dust3d_animation_timing_controls_tests)
    set_tests_properties(animation_timing_controls_contracts PROPERTIES TIMEOUT 10)

    add_executable(dust3d_animation_parameter_catalog_tests
        tests/core/animation_parameter_catalog_tests.cc
        dust3d/animation/parameter_catalog.cc
        dust3d/animation/animation_defaults.cc
        dust3d/animation/animation_catalog.cc
        dust3d/animation/biped/clip_metadata.cc)
    target_include_directories(dust3d_animation_parameter_catalog_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME animation_parameter_catalog_contracts COMMAND dust3d_animation_parameter_catalog_tests)
    set_tests_properties(animation_parameter_catalog_contracts PROPERTIES TIMEOUT 10)
    # Compile the unchanged reference callback interface, without executing or linking it.
    add_library(dust3d_animation_parameter_adapter_declarations OBJECT tests/reference/animation_parameter_adapter_compile.cc)
    target_include_directories(dust3d_animation_parameter_adapter_declarations PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_executable(dust3d_animation_defaults_tests
        tests/core/animation_defaults_tests.cc
        dust3d/animation/animation_defaults.cc
        dust3d/animation/animation_catalog.cc
        dust3d/animation/biped/clip_metadata.cc)
    target_include_directories(dust3d_animation_defaults_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME animation_defaults_contracts COMMAND dust3d_animation_defaults_tests)
    set_tests_properties(animation_defaults_contracts PROPERTIES TIMEOUT 10)
    # The owning C API stays independently linkable without generation code.
    add_executable(dust3d_animation_catalog_api_tests
        tests/core/animation_catalog_api_tests.cc
        native/Engine/EngineSupport.cc
        native/Engine/AnimationCatalogBridge.cc
        dust3d/animation/surface_material.cc
        dust3d/animation/parameter_catalog.cc
        dust3d/animation/animation_defaults.cc
        dust3d/animation/animation_catalog.cc
        dust3d/animation/biped/clip_metadata.cc)
    target_include_directories(dust3d_animation_catalog_api_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR} native/Engine/include)
    add_test(NAME animation_catalog_api_contracts COMMAND dust3d_animation_catalog_api_tests)
    set_tests_properties(animation_catalog_api_contracts PROPERTIES TIMEOUT 10)
    # Compile dispatch declarations for drift detection; never link this object to a native product/test.
    add_library(dust3d_animation_dispatch_declarations OBJECT dust3d/animation/biped/clip_catalog.cc)
    target_include_directories(dust3d_animation_dispatch_declarations PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR} third_party third_party/eigen)
    # Only declaration metadata is linked: no generator, rig, mesh or engine test dependency.
    add_test(NAME animation_reference_source_contracts
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/animation_reference_sources_tests.cmake)
    add_executable(dust3d_animation_catalog_metadata_tests
        tests/core/animation_catalog_metadata_tests.cc
        dust3d/animation/animation_catalog.cc
        dust3d/animation/biped/clip_metadata.cc)
    target_include_directories(dust3d_animation_catalog_metadata_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME animation_catalog_metadata_contracts COMMAND dust3d_animation_catalog_metadata_tests)
    set_tests_properties(animation_catalog_metadata_contracts PROPERTIES TIMEOUT 10)
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
    add_executable(dust3d_worker_attempt_tests tests/core/worker_attempt_tests.cc)
    target_link_libraries(dust3d_worker_attempt_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_attempt_contracts COMMAND dust3d_worker_attempt_tests)
    set_tests_properties(worker_attempt_contracts PROPERTIES TIMEOUT 30)

    add_executable(dust3d_worker_cleanup_tests tests/core/worker_cleanup_tests.cc)
    target_link_libraries(dust3d_worker_cleanup_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_cleanup_contracts COMMAND dust3d_worker_cleanup_tests)
    set_tests_properties(worker_cleanup_contracts PROPERTIES TIMEOUT 30)

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
        add_test(NAME worker_xpc_entitlement_policy COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover
            -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/core" -p test_worker_xpc_entitlements.py)
        add_test(NAME worker_xpc_entitlement_policy_optimized COMMAND "${Python3_EXECUTABLE}" -B -O -m unittest discover
            -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/core" -p test_worker_xpc_entitlements.py)
        set_tests_properties(worker_xpc_entitlement_policy worker_xpc_entitlement_policy_optimized PROPERTIES TIMEOUT 30)
        add_test(NAME generated_obj_verifier_contracts COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover
            -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/core" -p test_generated_obj_import.py)
        add_test(NAME generated_obj_verifier_optimized_contracts COMMAND "${Python3_EXECUTABLE}" -B -O -m unittest discover
            -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/core" -p test_generated_obj_import.py)
        set_tests_properties(generated_obj_verifier_contracts generated_obj_verifier_optimized_contracts PROPERTIES TIMEOUT 30
            ENVIRONMENT "DUST3D_BUILD_ROOT=${CMAKE_CURRENT_BINARY_DIR}")
        add_test(NAME static_export_benchmark_contracts COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover
            -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/core" -p test_static_export_benchmark.py)
        add_test(NAME static_export_benchmark_optimized_contracts COMMAND "${Python3_EXECUTABLE}" -B -O -m unittest discover
            -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/core" -p test_static_export_benchmark.py)
        set_tests_properties(static_export_benchmark_contracts static_export_benchmark_optimized_contracts PROPERTIES TIMEOUT 30
            ENVIRONMENT "DUST3D_BUILD_ROOT=${CMAKE_CURRENT_BINARY_DIR}")
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
        add_executable(dust3d_worker_uv_visual_tests tests/core/worker_uv_visual_tests.cc)
        target_link_libraries(dust3d_worker_uv_visual_tests PRIVATE dust3d_worker_transport dust3d_worker_operation)
        add_test(NAME worker_uv_visual_contracts COMMAND dust3d_worker_uv_visual_tests $<TARGET_FILE:dust3d_worker> ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
        set_tests_properties(worker_uv_visual_contracts PROPERTIES TIMEOUT 60)
        add_executable(dust3d_worker_uv_dispatch_tests tests/core/worker_uv_dispatch_tests.cc)
        target_link_libraries(dust3d_worker_uv_dispatch_tests PRIVATE dust3d_worker_transport dust3d_worker_operation)
        add_test(NAME worker_uv_dispatch_contracts COMMAND dust3d_worker_uv_dispatch_tests $<TARGET_FILE:dust3d_worker> ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures/Capsule.ds3)
        set_tests_properties(worker_uv_dispatch_contracts PROPERTIES TIMEOUT 60)
        add_executable(dust3d_worker_transport_tests tests/core/worker_transport_tests.cc)
        target_link_libraries(dust3d_worker_transport_tests PRIVATE dust3d_worker_transport dust3d_engine)
        add_test(NAME worker_transport_contracts COMMAND dust3d_worker_transport_tests $<TARGET_FILE:dust3d_worker> ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
        set_tests_properties(worker_transport_contracts PROPERTIES TIMEOUT 60)
    endif()
    add_executable(dust3d_worker_uv_admission_tests tests/core/worker_uv_admission_tests.cc)
    target_link_libraries(dust3d_worker_uv_admission_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_uv_admission_contracts COMMAND dust3d_worker_uv_admission_tests)
    set_tests_properties(worker_uv_admission_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_worker_admission_tests tests/core/worker_admission_tests.cc)
    target_link_libraries(dust3d_worker_admission_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_admission_contracts COMMAND dust3d_worker_admission_tests)
    set_tests_properties(worker_admission_contracts PROPERTIES TIMEOUT 30)
    if(TARGET dust3d_worker_runtime)
        add_executable(dust3d_worker_indexed_runtime_tests tests/core/worker_indexed_runtime_tests.cc)
        target_link_libraries(dust3d_worker_indexed_runtime_tests PRIVATE dust3d_worker_runtime)
        add_test(NAME worker_indexed_runtime_contracts COMMAND dust3d_worker_indexed_runtime_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures/Capsule.ds3)
        set_tests_properties(worker_indexed_runtime_contracts PROPERTIES TIMEOUT 60)
        add_executable(dust3d_worker_source_runtime_tests tests/core/worker_source_runtime_tests.cc)
        target_link_libraries(dust3d_worker_source_runtime_tests PRIVATE dust3d_worker_runtime)
        add_test(NAME worker_source_runtime_contracts COMMAND dust3d_worker_source_runtime_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures/Capsule.ds3)
        set_tests_properties(worker_source_runtime_contracts PROPERTIES TIMEOUT 60)
        add_executable(dust3d_worker_request_bridge_tests tests/core/worker_request_bridge_tests.cc)
        target_link_libraries(dust3d_worker_request_bridge_tests PRIVATE dust3d_worker_runtime)
        add_test(NAME worker_request_bridge_contracts COMMAND dust3d_worker_request_bridge_tests)
        add_executable(dust3d_worker_request_c_smoke tests/core/worker_request_c_smoke.c)
        set_target_properties(dust3d_worker_request_c_smoke PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES LINKER_LANGUAGE CXX)
        target_link_libraries(dust3d_worker_request_c_smoke PRIVATE dust3d_worker_runtime)
        add_test(NAME worker_request_c_contracts COMMAND dust3d_worker_request_c_smoke)
        add_executable(dust3d_worker_service_queue_tests tests/core/worker_service_queue_bridge_tests.cc)
        target_link_libraries(dust3d_worker_service_queue_tests PRIVATE dust3d_worker_runtime)
        add_test(NAME worker_service_queue_contracts COMMAND dust3d_worker_service_queue_tests)
        add_executable(dust3d_worker_service_queue_c_smoke tests/core/worker_service_queue_c_smoke.c)
        set_target_properties(dust3d_worker_service_queue_c_smoke PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES LINKER_LANGUAGE CXX)
        target_link_libraries(dust3d_worker_service_queue_c_smoke PRIVATE dust3d_worker_runtime)
        add_test(NAME worker_service_queue_c_contracts COMMAND dust3d_worker_service_queue_c_smoke)
        add_executable(dust3d_worker_decoded_bridge_tests tests/core/worker_decoded_bridge_tests.cc)
        target_link_libraries(dust3d_worker_decoded_bridge_tests PRIVATE dust3d_worker_runtime)
        add_test(NAME worker_decoded_bridge_contracts COMMAND dust3d_worker_decoded_bridge_tests)
        add_executable(dust3d_worker_decoded_c_smoke tests/core/worker_decoded_c_smoke.c)
        set_target_properties(dust3d_worker_decoded_c_smoke PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES LINKER_LANGUAGE CXX)
        target_link_libraries(dust3d_worker_decoded_c_smoke PRIVATE dust3d_worker_runtime)
        add_test(NAME worker_decoded_c_contracts COMMAND dust3d_worker_decoded_c_smoke)
        add_executable(dust3d_worker_reply_bridge_tests tests/core/worker_reply_bridge_tests.cc)
        target_link_libraries(dust3d_worker_reply_bridge_tests PRIVATE dust3d_worker_runtime)
        add_test(NAME worker_reply_bridge_contracts COMMAND dust3d_worker_reply_bridge_tests)
        add_executable(dust3d_worker_reply_c_smoke tests/core/worker_reply_c_smoke.c)
        set_target_properties(dust3d_worker_reply_c_smoke PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES LINKER_LANGUAGE CXX)
        target_link_libraries(dust3d_worker_reply_c_smoke PRIVATE dust3d_worker_runtime)
        add_test(NAME worker_reply_c_contracts COMMAND dust3d_worker_reply_c_smoke)
        add_executable(dust3d_worker_runtime_c_smoke tests/core/worker_runtime_c_smoke.c)
        set_target_properties(dust3d_worker_runtime_c_smoke PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES LINKER_LANGUAGE CXX)
        target_link_libraries(dust3d_worker_runtime_c_smoke PRIVATE dust3d_worker_runtime)
        add_test(NAME worker_runtime_c_contracts COMMAND dust3d_worker_runtime_c_smoke)
        add_executable(dust3d_worker_runtime_bridge_tests tests/core/worker_runtime_bridge_tests.cc)
        target_link_libraries(dust3d_worker_runtime_bridge_tests PRIVATE dust3d_worker_runtime)
        add_test(NAME worker_runtime_bridge_contracts COMMAND dust3d_worker_runtime_bridge_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures/Capsule.ds3 ${CMAKE_CURRENT_BINARY_DIR})
        set_tests_properties(worker_runtime_c_contracts worker_runtime_bridge_contracts PROPERTIES TIMEOUT 60)
    endif()
    if(TARGET dust3d_worker_job_io)
        add_executable(dust3d_worker_job_io_tests tests/core/worker_job_io_tests.cc)
        target_link_libraries(dust3d_worker_job_io_tests PRIVATE dust3d_worker_job_io)
        add_test(NAME worker_job_io_contracts COMMAND dust3d_worker_job_io_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures/Capsule.ds3 ${CMAKE_CURRENT_BINARY_DIR})
        set_tests_properties(worker_job_io_contracts PROPERTIES TIMEOUT 60)
    endif()
    add_executable(dust3d_worker_uv_operation_tests tests/core/worker_uv_operation_tests.cc)
    target_link_libraries(dust3d_worker_uv_operation_tests PRIVATE dust3d_worker_operation)
    add_test(NAME worker_uv_operation_contracts COMMAND dust3d_worker_uv_operation_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures/Capsule.ds3)
    set_tests_properties(worker_uv_operation_contracts PROPERTIES TIMEOUT 60)
    add_executable(dust3d_worker_source_operation_tests tests/core/worker_source_operation_tests.cc)
    target_link_libraries(dust3d_worker_source_operation_tests PRIVATE dust3d_worker_operation)
    add_test(NAME worker_source_operation_contracts COMMAND dust3d_worker_source_operation_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures/Capsule.ds3)
    set_tests_properties(worker_source_operation_contracts PROPERTIES TIMEOUT 60)
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
    add_executable(dust3d_worker_uv_request_tests tests/core/worker_uv_request_tests.cc)
    target_link_libraries(dust3d_worker_uv_request_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_uv_request_contracts COMMAND dust3d_worker_uv_request_tests)
    set_tests_properties(worker_uv_request_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_worker_uv_copy_budget_tests tests/core/worker_uv_copy_budget_tests.cc)
    target_link_libraries(dust3d_worker_uv_copy_budget_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_uv_copy_budget_contracts COMMAND dust3d_worker_uv_copy_budget_tests)
    set_tests_properties(worker_uv_copy_budget_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_worker_source_hints_tests tests/core/worker_source_hints_tests.cc)
    target_link_libraries(dust3d_worker_source_hints_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_source_hints_contracts COMMAND dust3d_worker_source_hints_tests)
    set_tests_properties(worker_source_hints_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_worker_indexed_sources_tests tests/core/worker_indexed_sources_tests.cc)
    target_link_libraries(dust3d_worker_indexed_sources_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_indexed_sources_contracts COMMAND dust3d_worker_indexed_sources_tests)
    set_tests_properties(worker_indexed_sources_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_worker_indexed_source_operation_tests tests/core/worker_indexed_source_operation_tests.cc)
    target_link_libraries(dust3d_worker_indexed_source_operation_tests PRIVATE dust3d_worker_operation)
    add_test(NAME worker_indexed_source_operation_contracts COMMAND dust3d_worker_indexed_source_operation_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures/Capsule.ds3)
    set_tests_properties(worker_indexed_source_operation_contracts PROPERTIES TIMEOUT 60)
    add_executable(dust3d_worker_imported_surface_tests tests/core/worker_imported_surface_tests.cc)
    target_link_libraries(dust3d_worker_imported_surface_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_imported_surface_contracts COMMAND dust3d_worker_imported_surface_tests)
    set_tests_properties(worker_imported_surface_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_worker_imported_surface_copy_tests tests/core/worker_imported_surface_copy_tests.cc)
    target_link_libraries(dust3d_worker_imported_surface_copy_tests PRIVATE dust3d_worker_operation)
    add_test(NAME worker_imported_surface_copy_contracts COMMAND dust3d_worker_imported_surface_copy_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    set_tests_properties(worker_imported_surface_copy_contracts PROPERTIES TIMEOUT 60)
    add_executable(dust3d_worker_imported_materials_tests tests/core/worker_imported_materials_tests.cc)
    target_link_libraries(dust3d_worker_imported_materials_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_imported_materials_contracts COMMAND dust3d_worker_imported_materials_tests)
    set_tests_properties(worker_imported_materials_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_worker_imported_materials_copy_tests tests/core/worker_imported_materials_copy_tests.cc)
    target_link_libraries(dust3d_worker_imported_materials_copy_tests PRIVATE dust3d_worker_operation)
    add_test(NAME worker_imported_materials_copy_contracts COMMAND dust3d_worker_imported_materials_copy_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    set_tests_properties(worker_imported_materials_copy_contracts PROPERTIES TIMEOUT 60)
    add_executable(dust3d_worker_legacy_baked_tests tests/core/worker_legacy_baked_tests.cc)
    target_link_libraries(dust3d_worker_legacy_baked_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_legacy_baked_contracts COMMAND dust3d_worker_legacy_baked_tests)
    set_tests_properties(worker_legacy_baked_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_worker_legacy_baked_copy_tests tests/core/worker_legacy_baked_copy_tests.cc)
    target_link_libraries(dust3d_worker_legacy_baked_copy_tests PRIVATE dust3d_worker_operation)
    add_test(NAME worker_legacy_baked_copy_contracts COMMAND dust3d_worker_legacy_baked_copy_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    set_tests_properties(worker_legacy_baked_copy_contracts PROPERTIES TIMEOUT 60)
    add_executable(dust3d_worker_source_baked_copy_tests tests/core/worker_source_baked_copy_tests.cc)
    target_link_libraries(dust3d_worker_source_baked_copy_tests PRIVATE dust3d_worker_operation)
    add_test(NAME worker_source_baked_copy_contracts COMMAND dust3d_worker_source_baked_copy_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    set_tests_properties(worker_source_baked_copy_contracts PROPERTIES TIMEOUT 60)
    add_executable(dust3d_worker_source_baked_tests tests/core/worker_source_baked_tests.cc)
    target_link_libraries(dust3d_worker_source_baked_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_source_baked_contracts COMMAND dust3d_worker_source_baked_tests)
    set_tests_properties(worker_source_baked_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_worker_baked_textures_tests tests/core/worker_baked_textures_tests.cc)
    target_link_libraries(dust3d_worker_baked_textures_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_baked_textures_contracts COMMAND dust3d_worker_baked_textures_tests)
    set_tests_properties(worker_baked_textures_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_worker_indexed_mesh_tests tests/core/worker_indexed_mesh_tests.cc)
    target_link_libraries(dust3d_worker_indexed_mesh_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_indexed_mesh_contracts COMMAND dust3d_worker_indexed_mesh_tests)
    set_tests_properties(worker_indexed_mesh_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_worker_sourced_mesh_tests tests/core/worker_sourced_mesh_tests.cc)
    target_link_libraries(dust3d_worker_sourced_mesh_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_sourced_mesh_contracts COMMAND dust3d_worker_sourced_mesh_tests)
    set_tests_properties(worker_sourced_mesh_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_worker_uv_mesh_tests tests/core/worker_uv_mesh_tests.cc)
    target_link_libraries(dust3d_worker_uv_mesh_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_uv_mesh_contracts COMMAND dust3d_worker_uv_mesh_tests)
    set_tests_properties(worker_uv_mesh_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_worker_uv_tests tests/core/worker_uv_tests.cc)
    target_link_libraries(dust3d_worker_uv_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_uv_payload_contracts COMMAND dust3d_worker_uv_tests)
    set_tests_properties(worker_uv_payload_contracts PROPERTIES TIMEOUT 30)
    add_executable(dust3d_worker_uv_owner_tests tests/core/worker_uv_owner_tests.cc)
    target_link_libraries(dust3d_worker_uv_owner_tests PRIVATE dust3d_worker_protocol dust3d_engine)
    add_test(NAME worker_uv_owner_contracts COMMAND dust3d_worker_uv_owner_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures/Capsule.ds3)
    set_tests_properties(worker_uv_owner_contracts PROPERTIES TIMEOUT 60)
    add_executable(dust3d_worker_mesh_tests tests/core/worker_mesh_tests.cc)
    target_link_libraries(dust3d_worker_mesh_tests PRIVATE dust3d_worker_protocol dust3d_engine)
    add_test(NAME worker_mesh_contracts COMMAND dust3d_worker_mesh_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures)
    set_tests_properties(worker_mesh_contracts PROPERTIES TIMEOUT 60)
    add_executable(dust3d_worker_reply_collector_tests tests/core/worker_reply_collector_tests.cc)
    target_link_libraries(dust3d_worker_reply_collector_tests PRIVATE dust3d_worker_protocol)
    add_test(NAME worker_reply_collector_contracts COMMAND dust3d_worker_reply_collector_tests)
    set_tests_properties(worker_reply_collector_contracts PROPERTIES TIMEOUT 30)
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
        set(DUST3D_WORKER_SWIFT_MODULE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/worker-swift-module")
        file(GENERATE OUTPUT "${DUST3D_WORKER_SWIFT_MODULE_DIRECTORY}/module.modulemap"
            CONTENT "module Dust3DWorkerRuntime { header \"${CMAKE_CURRENT_SOURCE_DIR}/native/WorkerRuntime/include/Dust3DWorkerRuntime.h\" export * }\n")
        set(DUST3D_SWIFT_WORKER_CONSUMER "${CMAKE_CURRENT_BINARY_DIR}/worker-runtime-swift")
        add_custom_command(OUTPUT "${DUST3D_SWIFT_WORKER_CONSUMER}"
            COMMAND "${DUST3D_XCRUN}" swift-format lint --strict "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerRuntimeSmoke.swift"
            COMMAND "${DUST3D_XCRUN}" swiftc -swift-version 6 -strict-concurrency=complete
                -warnings-as-errors -parse-as-library
                -module-cache-path "${CMAKE_CURRENT_BINARY_DIR}/swift-module-cache"
                -I "${DUST3D_WORKER_SWIFT_MODULE_DIRECTORY}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerRuntimeSmoke.swift"
                "$<TARGET_FILE:dust3d_worker_runtime>" "$<TARGET_FILE:dust3d_worker_job_io>"
                "$<TARGET_FILE:dust3d_worker_operation>" "$<TARGET_FILE:dust3d_worker_protocol>"
                "$<TARGET_FILE:dust3d_engine>" "$<TARGET_FILE:dust3d_core>"
                "$<TARGET_FILE:dust3d_remesher>" "$<TARGET_FILE:dust3d_triangle>"
                -lc++ -lz -o "${DUST3D_SWIFT_WORKER_CONSUMER}"
            DEPENDS tests/swift/WorkerRuntimeSmoke.swift native/WorkerRuntime/include/Dust3DWorkerRuntime.h
                dust3d_worker_runtime dust3d_worker_job_io dust3d_worker_operation dust3d_worker_protocol
                dust3d_engine dust3d_core dust3d_remesher dust3d_triangle
            VERBATIM)
        add_custom_target(dust3d_worker_runtime_swift ALL DEPENDS "${DUST3D_SWIFT_WORKER_CONSUMER}")
        add_test(NAME worker_runtime_swift COMMAND "${DUST3D_SWIFT_WORKER_CONSUMER}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures/Capsule.ds3" "${CMAKE_CURRENT_BINARY_DIR}")
        set_tests_properties(worker_runtime_swift PROPERTIES TIMEOUT 60)
        set(DUST3D_SWIFT_SERVICE_QUEUE_CONSUMER "${CMAKE_CURRENT_BINARY_DIR}/worker-service-queue-swift")
        add_custom_command(OUTPUT "${DUST3D_SWIFT_SERVICE_QUEUE_CONSUMER}"
            COMMAND "${DUST3D_XCRUN}" swift-format lint --strict
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerServiceQueue.swift"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerServiceQueueSmoke.swift"
            COMMAND "${DUST3D_XCRUN}" swiftc -swift-version 6 -strict-concurrency=complete
                -warnings-as-errors -parse-as-library
                -module-cache-path "${CMAKE_CURRENT_BINARY_DIR}/swift-module-cache"
                -I "${DUST3D_WORKER_SWIFT_MODULE_DIRECTORY}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerServiceQueue.swift"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerServiceQueueSmoke.swift"
                "$<TARGET_FILE:dust3d_worker_runtime>" "$<TARGET_FILE:dust3d_worker_job_io>"
                "$<TARGET_FILE:dust3d_worker_operation>" "$<TARGET_FILE:dust3d_worker_protocol>"
                "$<TARGET_FILE:dust3d_engine>" "$<TARGET_FILE:dust3d_core>"
                "$<TARGET_FILE:dust3d_remesher>" "$<TARGET_FILE:dust3d_triangle>"
                -lc++ -lz -o "${DUST3D_SWIFT_SERVICE_QUEUE_CONSUMER}"
            DEPENDS tests/swift/WorkerServiceQueue.swift tests/swift/WorkerServiceQueueSmoke.swift
                native/WorkerRuntime/include/Dust3DWorkerRuntime.h
                dust3d_worker_runtime dust3d_worker_job_io dust3d_worker_operation dust3d_worker_protocol
                dust3d_engine dust3d_core dust3d_remesher dust3d_triangle VERBATIM)
        add_custom_target(dust3d_worker_service_queue_swift ALL DEPENDS "${DUST3D_SWIFT_SERVICE_QUEUE_CONSUMER}")
        add_test(NAME worker_service_queue_swift COMMAND "${DUST3D_SWIFT_SERVICE_QUEUE_CONSUMER}")
        set_tests_properties(worker_service_queue_swift PROPERTIES TIMEOUT 30)
        if(Python3_Interpreter_FOUND)
            set(DUST3D_CHANNEL_METADATA_BINARY "${CMAKE_CURRENT_BINARY_DIR}/worker-channel-metadata")
            add_custom_command(OUTPUT "${DUST3D_CHANNEL_METADATA_BINARY}"
                COMMAND "${DUST3D_XCRUN}" swift-format lint --strict
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerXPCChannels.swift"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerChannelSmoke.swift"
                COMMAND "${DUST3D_XCRUN}" swiftc -swift-version 6 -strict-concurrency=complete
                    -warnings-as-errors -parse-as-library
                    -module-cache-path "${CMAKE_CURRENT_BINARY_DIR}/swift-module-cache"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerXPCCommon.swift"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerXPCChannels.swift"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerChannelSmoke.swift"
                    -o "${DUST3D_CHANNEL_METADATA_BINARY}"
                DEPENDS tests/swift/WorkerXPCCommon.swift tests/swift/WorkerXPCChannels.swift tests/swift/WorkerChannelSmoke.swift
                VERBATIM)
            add_custom_target(dust3d_worker_channel_metadata ALL DEPENDS "${DUST3D_CHANNEL_METADATA_BINARY}")
            add_test(NAME worker_channel_metadata COMMAND "${DUST3D_CHANNEL_METADATA_BINARY}")
            set_tests_properties(worker_channel_metadata PROPERTIES TIMEOUT 30)
            set(DUST3D_ADMISSION_BINARY "${CMAKE_CURRENT_BINARY_DIR}/worker-admission-values")
            add_custom_command(OUTPUT "${DUST3D_ADMISSION_BINARY}"
                COMMAND "${DUST3D_XCRUN}" swift-format lint --strict
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerServiceAdmission.swift"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerServiceOwnership.swift"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerServiceScheduler.swift"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerAdmissionSmoke.swift"
                COMMAND "${DUST3D_XCRUN}" swiftc -swift-version 6 -strict-concurrency=complete
                    -warnings-as-errors -parse-as-library
                    -module-cache-path "${CMAKE_CURRENT_BINARY_DIR}/swift-module-cache"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerServiceAdmission.swift"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerServiceOwnership.swift"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerServiceScheduler.swift"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerAdmissionSmoke.swift"
                    -o "${DUST3D_ADMISSION_BINARY}"
                DEPENDS tests/swift/WorkerServiceAdmission.swift tests/swift/WorkerServiceOwnership.swift tests/swift/WorkerServiceScheduler.swift tests/swift/WorkerAdmissionSmoke.swift
                VERBATIM)
            add_custom_target(dust3d_worker_admission_values ALL DEPENDS "${DUST3D_ADMISSION_BINARY}")
            add_test(NAME worker_admission_values COMMAND "${DUST3D_ADMISSION_BINARY}")
            set_tests_properties(worker_admission_values PROPERTIES TIMEOUT 30)
            set(DUST3D_OUTCOME_BINARY "${CMAKE_CURRENT_BINARY_DIR}/worker-outcome-values")
            add_custom_command(OUTPUT "${DUST3D_OUTCOME_BINARY}"
                COMMAND "${DUST3D_XCRUN}" swift-format lint --strict
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerOutcome.swift"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerOutcomeSmoke.swift"
                COMMAND "${DUST3D_XCRUN}" swiftc -swift-version 6 -strict-concurrency=complete
                    -warnings-as-errors -parse-as-library
                    -module-cache-path "${CMAKE_CURRENT_BINARY_DIR}/swift-module-cache"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerOutcome.swift"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerOutcomeSmoke.swift"
                    -o "${DUST3D_OUTCOME_BINARY}"
                DEPENDS tests/swift/WorkerOutcome.swift tests/swift/WorkerOutcomeSmoke.swift VERBATIM)
            add_custom_target(dust3d_worker_outcome_values ALL DEPENDS "${DUST3D_OUTCOME_BINARY}")
            add_test(NAME worker_outcome_values COMMAND "${DUST3D_OUTCOME_BINARY}")
            set_tests_properties(worker_outcome_values PROPERTIES TIMEOUT 30)
            set(DUST3D_XPC_FIXTURE_BINARIES)
            foreach(DUST3D_XPC_ROLE Host Service)
                string(TOLOWER "${DUST3D_XPC_ROLE}" DUST3D_XPC_ROLE_LOWER)
                set(DUST3D_XPC_BINARY "${CMAKE_CURRENT_BINARY_DIR}/worker-xpc-${DUST3D_XPC_ROLE_LOWER}")
                set(DUST3D_XPC_SUPPORT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerServiceAdmission.swift")
                if(DUST3D_XPC_ROLE STREQUAL "Service")
                    list(APPEND DUST3D_XPC_SUPPORT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerServiceOwnership.swift" "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerServiceScheduler.swift")
                else()
                    list(APPEND DUST3D_XPC_SUPPORT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerOutcome.swift" "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerXPCPeer.swift" "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerServiceQueue.swift")
                endif()
                add_custom_command(OUTPUT "${DUST3D_XPC_BINARY}"
                    COMMAND "${DUST3D_XCRUN}" swift-format lint --strict ${DUST3D_XPC_SUPPORT_SOURCES}
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerXPCCommon.swift"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerXPCChannels.swift"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerXPC${DUST3D_XPC_ROLE}.swift"
                    COMMAND "${DUST3D_XCRUN}" swiftc -swift-version 6 -strict-concurrency=complete
                        -warnings-as-errors -parse-as-library
                        -module-cache-path "${CMAKE_CURRENT_BINARY_DIR}/swift-module-cache"
                        -I "${DUST3D_WORKER_SWIFT_MODULE_DIRECTORY}" ${DUST3D_XPC_SUPPORT_SOURCES}
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerXPCCommon.swift"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerXPCChannels.swift"
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/WorkerXPC${DUST3D_XPC_ROLE}.swift"
                        "$<TARGET_FILE:dust3d_worker_runtime>" "$<TARGET_FILE:dust3d_worker_job_io>"
                        "$<TARGET_FILE:dust3d_worker_operation>" "$<TARGET_FILE:dust3d_worker_protocol>"
                        "$<TARGET_FILE:dust3d_engine>" "$<TARGET_FILE:dust3d_core>"
                        "$<TARGET_FILE:dust3d_remesher>" "$<TARGET_FILE:dust3d_triangle>"
                        -lc++ -lz -o "${DUST3D_XPC_BINARY}"
                    DEPENDS ${DUST3D_XPC_SUPPORT_SOURCES} tests/swift/WorkerXPCCommon.swift tests/swift/WorkerXPCChannels.swift "tests/swift/WorkerXPC${DUST3D_XPC_ROLE}.swift"
                        native/WorkerRuntime/include/Dust3DWorkerRuntime.h dust3d_worker_runtime
                        dust3d_worker_job_io dust3d_worker_operation dust3d_worker_protocol
                        dust3d_engine dust3d_core dust3d_remesher dust3d_triangle
                    VERBATIM)
                list(APPEND DUST3D_XPC_FIXTURE_BINARIES "${DUST3D_XPC_BINARY}")
            endforeach()
            add_custom_target(dust3d_worker_xpc_static_fixture ALL DEPENDS ${DUST3D_XPC_FIXTURE_BINARIES})
            add_test(NAME worker_xpc_static_mesh COMMAND "${Python3_EXECUTABLE}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/verify_worker_xpc_static.py"
                "${CMAKE_CURRENT_BINARY_DIR}/worker-xpc-host" "${CMAKE_CURRENT_BINARY_DIR}/worker-xpc-service"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures/Capsule.ds3" "${CMAKE_CURRENT_BINARY_DIR}")
            add_test(NAME worker_xpc_uv_mesh COMMAND "${Python3_EXECUTABLE}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/verify_worker_xpc_static.py"
                "${CMAKE_CURRENT_BINARY_DIR}/worker-xpc-host" "${CMAKE_CURRENT_BINARY_DIR}/worker-xpc-service"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures/Capsule.ds3" "${CMAKE_CURRENT_BINARY_DIR}" uv)
            add_test(NAME worker_xpc_socket_mesh COMMAND "${Python3_EXECUTABLE}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/verify_worker_xpc_static.py"
                "${CMAKE_CURRENT_BINARY_DIR}/worker-xpc-host" "${CMAKE_CURRENT_BINARY_DIR}/worker-xpc-service"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures/Capsule.ds3" "${CMAKE_CURRENT_BINARY_DIR}" mesh socket)
            add_test(NAME worker_xpc_socket_uv_mesh COMMAND "${Python3_EXECUTABLE}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/verify_worker_xpc_static.py"
                "${CMAKE_CURRENT_BINARY_DIR}/worker-xpc-host" "${CMAKE_CURRENT_BINARY_DIR}/worker-xpc-service"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures/Capsule.ds3" "${CMAKE_CURRENT_BINARY_DIR}" uv socket)
            set_tests_properties(worker_xpc_static_mesh worker_xpc_uv_mesh worker_xpc_socket_mesh worker_xpc_socket_uv_mesh PROPERTIES
                TIMEOUT 90 RESOURCE_LOCK dust3d_xpc_development_host)
        endif()
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
        set(DUST3D_SWIFT_OBJ_CONSUMER "${CMAKE_CURRENT_BINARY_DIR}/obj-export-swift")
        add_custom_command(OUTPUT "${DUST3D_SWIFT_OBJ_CONSUMER}"
            COMMAND "${DUST3D_XCRUN}" swift-format lint --strict "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/ObjExportSmoke.swift"
            COMMAND "${DUST3D_XCRUN}" swiftc -swift-version 6 -strict-concurrency=complete
                -warnings-as-errors -parse-as-library
                -module-cache-path "${CMAKE_CURRENT_BINARY_DIR}/swift-module-cache"
                -I "${DUST3D_SWIFT_MODULE_DIRECTORY}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/swift/ObjExportSmoke.swift"
                "$<TARGET_FILE:dust3d_engine>" "$<TARGET_FILE:dust3d_core>"
                "$<TARGET_FILE:dust3d_remesher>" "$<TARGET_FILE:dust3d_triangle>"
                -lc++ -lz -o "${DUST3D_SWIFT_OBJ_CONSUMER}"
            DEPENDS tests/swift/ObjExportSmoke.swift native/Engine/include/Dust3DBridge.h
                dust3d_engine dust3d_core dust3d_remesher dust3d_triangle
            VERBATIM)
        add_custom_target(dust3d_obj_export_swift ALL DEPENDS "${DUST3D_SWIFT_OBJ_CONSUMER}")
        add_test(NAME obj_export_swift COMMAND "${DUST3D_SWIFT_OBJ_CONSUMER}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/core/fixtures/Capsule.ds3")
        set_tests_properties(obj_export_swift PROPERTIES TIMEOUT 60)
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

// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "Dust3DCore",
    platforms: [.macOS(.v14)],
    products: [
        .executable(name: "dust3d-export", targets: ["Dust3DExport"]),
        .executable(name: "dust3d-worker", targets: ["Dust3DWorker"]),
        .library(name: "Dust3DEngine", type: .static, targets: ["Dust3DEngine"]),
        .library(name: "Dust3DWorkerRuntime", type: .static, targets: ["Dust3DWorkerRuntime"]),
    ],
    targets: [
        .executableTarget(
            name: "Dust3DExport", dependencies: ["Dust3DEngine"], path: "native/ExportCli", exclude: ["README.md"]
        ),
        .executableTarget(
            name: "Dust3DWorker",
            dependencies: ["Dust3DWorkerRuntime"],
            path: "native/Engine",
            sources: [
                "WorkerMain.cc", "WorkerResources.cc", "WorkerResourceLaunch.cc", "WorkerResourceSetup.cc",
                "WorkerUnixResources.cc",
            ],
            cxxSettings: [.define("DUST3D_WORKER_HAS_RESOURCE_SETUP", to: "1")]
        ),
        .target(
            name: "Dust3DWorkerRuntime",
            dependencies: ["Dust3DEngine"],
            path: "native",
            exclude: ["WorkerRuntime/README.md", "ExportCli/README.md"],
            sources: [
                "Engine/WorkerJobIO.cc", "Engine/WorkerOperation.cc", "Engine/WorkerUvOperation.cc",
                "Engine/WorkerSourceOperation.cc", "Engine/WorkerSourceRequest.cc",
                "Engine/WorkerIndexedCopyBudget.cc", "Engine/WorkerIndexedSourceOperation.cc",
                "Engine/WorkerIndexedSources.cc",
                "Engine/WorkerLegacyBaked.cc", "Engine/WorkerLegacyBakedCopy.cc", "Engine/WorkerMeshCopyInspection.cc",
                "Engine/WorkerSourceBakedCopy.cc", "Engine/WorkerSourceBaked.cc", "Engine/WorkerBakedTextures.cc",
                "Engine/WorkerIndexedMesh.cc",
                "Engine/WorkerSourcedMesh.cc",
                "Engine/WorkerSourceHints.cc",
                "Engine/WorkerProtocol.cc",
                "Engine/WorkerReplyCollector.cc",
                "Engine/WorkerMesh.cc",
                "Engine/WorkerFailure.cc", "Engine/WorkerAdmission.cc", "Engine/WorkerQueue.cc",
                "Engine/WorkerUv.cc", "Engine/WorkerUvMesh.cc", "Engine/WorkerUvRequest.cc",
                "WorkerRuntime/WorkerRuntimeBridge.cc", "WorkerRuntime/WorkerReplyBridge.cc",
                "WorkerRuntime/WorkerDecodedBridge.cc", "WorkerRuntime/WorkerRequestBridge.cc",
                "WorkerRuntime/WorkerQueueBridge.cc",
            ],
            publicHeadersPath: "WorkerRuntime/include",
            cxxSettings: [.headerSearchPath("Engine")]
        ),
        .target(
            name: "TriangleIntersection",
            path: "third_party/GuigueDevillers03",
            sources: ["tri_tri_intersect.c"],
            publicHeadersPath: "."
        ),
        .target(
            name: "AutoRemesher",
            path: "third_party/autoremesher",
            exclude: ["LICENSE", "README.md", "thirdparty/isotropicremesher/LICENSE"],
            sources: ["src/AutoRemesher", "thirdparty/isotropicremesher"],
            publicHeadersPath: "include",
            cxxSettings: [
                .headerSearchPath("tbbshim"), .headerSearchPath("include"),
                .headerSearchPath("thirdparty/isotropicremesher"), .headerSearchPath("../eigen"),
            ]
        ),
        .target(
            name: "Dust3DCore",
            dependencies: ["AutoRemesher", "TriangleIntersection"],
            path: "dust3d",
            sources: [
                "base", "mesh", "rig", "uv", "animation/animation_catalog.cc", "animation/animation_defaults.cc",
                "animation/parameter_catalog.cc", "animation/surface_material.cc", "animation/spring_step_admission.cc",
                "animation/biped/clip_metadata.cc",
            ],
            publicHeadersPath: "include",
            cxxSettings: [
                .headerSearchPath("../third_party/autoremesher/tbbshim"),
                .headerSearchPath(".."), .headerSearchPath("../third_party"),
                .headerSearchPath("../third_party/eigen"),
                .headerSearchPath("../third_party/rapidxml-1.13"),
                .headerSearchPath("../third_party/earcut.hpp/include"),
            ]
        ),
        .target(
            name: "Dust3DEngine",
            dependencies: ["Dust3DCore"],
            path: "native/Engine",
            sources: [
                "EngineSupport.cc", "AnimationCatalogBridge.cc",
                "ObjDocument.cc", "ObjGeometryPlan.cc",
                "FbxMaterialImagePlan.cc",
                "FbxMeshScene.cc",
                "FbxMeshScenePlan.cc",
                "FbxGeometrySelection.cc",
                "FbxAtlasGeometry.cc",
                "PngEncoding.cc",
                "FbxBakedMaterial.cc",
                "FbxExportBridge.cc", "ObjExportBridge.cc",
                "FbxDocument.cc",
                "FbxNode.cc",
                "FbxPropertyBlock.cc",
                "FbxAnimationMath.cc",
                "RigTransformValidation.cc",
                "SourceBakedMesh.cc", "ImportedSurface.cc", "ImportedAssets.cc", "ImportedModels.cc", "GlbReader.cc",
                "BakedMesh.cc",
                "Cancellation.cc",
                "Dust3DBridge.cc",
                "GenerationSession.cc",
                "IndexedMesh.cc",
                "RetainedPayload.cc",
                "SnapshotInvalidation.cc", "SourceProvenance.cc", "UvMetadata.cc",
                "DocumentReader.cc",
                "SnapshotValidator.cc",
            ],
            publicHeadersPath: "include",
            cxxSettings: [
                .headerSearchPath("../.."),
                .headerSearchPath("../../third_party/rapidxml-1.13"),
                .headerSearchPath("../../third_party"),
            ],
            linkerSettings: [.linkedLibrary("z")]
        ),
    ],
    cxxLanguageStandard: .cxx17
)

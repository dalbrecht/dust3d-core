// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "Dust3DCore",
    platforms: [.macOS(.v14)],
    products: [
        .executable(name: "dust3d-export", targets: ["Dust3DExport"]),
        .executable(name: "dust3d-xpc-service", targets: ["Dust3DWorkerServiceExecutable"]),
        .executable(name: "dust3d-worker", targets: ["Dust3DWorker"]),
        .library(name: "Dust3DEngine", type: .static, targets: ["Dust3DEngine"]),
        .library(name: "Dust3DWorkerPreview", type: .static, targets: ["Dust3DWorkerPreview"]),
        .library(name: "Dust3DWorkerService", type: .static, targets: ["Dust3DWorkerService"]),
        .library(name: "Dust3DWorkerXPC", type: .static, targets: ["Dust3DWorkerXPC"]),
        .library(name: "Dust3DWorkerClient", type: .static, targets: ["Dust3DWorkerClient"]),
        .library(name: "Dust3DWorkerRuntime", type: .static, targets: ["Dust3DWorkerRuntime"]),
        .library(name: "Dust3DWorkerAppleImages", type: .static, targets: ["Dust3DWorkerAppleImages"]),
        .library(name: "Dust3DImageMetadata", type: .static, targets: ["Dust3DImageMetadata"]),
    ],
    targets: [
        .target(
            name: "Dust3DWorkerPreview",
            dependencies: ["Dust3DWorkerRuntime", "Dust3DWorkerClient", "Dust3DWorkerXPC"],
            path: "native/WorkerPreview", exclude: ["README.md"]),
        .testTarget(
            name: "WorkerPreviewExecutorTests",
            dependencies: ["Dust3DWorkerPreview", "Dust3DWorkerXPC", "Dust3DWorkerClient"],
            path: "tests/WorkerPreview"),
        .executableTarget(
            name: "Dust3DWorkerServiceExecutable", dependencies: ["Dust3DWorkerService"],
            path: "native/WorkerServiceExecutable", exclude: ["README.md"]),
        .target(
            name: "Dust3DWorkerService",
            dependencies: ["Dust3DWorkerRuntime", "Dust3DWorkerAppleImages", "Dust3DWorkerXPC"],
            path: "native/WorkerService", exclude: ["README.md"],
            linkerSettings: [.linkedFramework("Security")]),
        .testTarget(
            name: "WorkerServicePackageTests", dependencies: ["Dust3DWorkerService"], path: "tests/WorkerService"),
        .target(
            name: "Dust3DWorkerXPC", dependencies: ["Dust3DWorkerClient"], path: "native/WorkerXPC",
            exclude: ["README.md"],
            linkerSettings: [.linkedFramework("Security")]),
        .testTarget(
            name: "WorkerXPCContractTests", dependencies: ["Dust3DWorkerXPC", "Dust3DWorkerClient"],
            path: "tests/WorkerXPC"),
        .target(name: "Dust3DWorkerClient", path: "native/WorkerClient", exclude: ["README.md"]),
        .testTarget(name: "WorkerClientPackageTests", dependencies: ["Dust3DWorkerClient"], path: "tests/WorkerClient"),
        .target(name: "Dust3DImageMetadata", path: "native/AppleImages/Metadata", exclude: ["README.md"]),
        .target(
            name: "Dust3DWorkerAppleImages", dependencies: ["Dust3DWorkerRuntime", "Dust3DImageMetadata"],
            path: "native/AppleImages", exclude: ["README.md", "provenance.json", "Metadata"],
            linkerSettings: [.linkedFramework("CoreGraphics"), .linkedFramework("ImageIO")]
        ),
        .executableTarget(
            name: "Dust3DExport", dependencies: ["Dust3DEngine"], path: "native/ExportCli", exclude: ["README.md"]
        ),
        .executableTarget(
            name: "Dust3DWorker",
            dependencies: ["Dust3DWorkerRuntime"],
            path: "native/Engine",
            sources: [
                "WorkerMain.cc"
            ],
            cxxSettings: [.define("DUST3D_WORKER_HAS_RESOURCE_SETUP", to: "1")]
        ),
        .target(
            name: "Dust3DWorkerRuntime",
            dependencies: ["Dust3DEngine"],
            path: "native",
            exclude: [
                "WorkerRuntime/README.md", "ExportCli/README.md", "AppleImages", "WorkerClient", "WorkerXPC",
                "WorkerService", "WorkerServiceExecutable", "WorkerPreview",
            ],
            sources: [
                "Engine/WorkerResources.cc", "Engine/WorkerResourceLaunch.cc", "Engine/WorkerResourceSetup.cc",
                "Engine/WorkerUnixResources.cc", "WorkerRuntime/WorkerResourcesBridge.cc",
                "WorkerRuntime/WorkerResourcesUnixBridge.cc",
                "Engine/WorkerJobIO.cc", "Engine/WorkerPreviewArchiveIO.cc", "Engine/WorkerPreviewSessionIO.cc",
                "Engine/WorkerOperation.cc",
                "Engine/WorkerUvOperation.cc",
                "Engine/WorkerSourceOperation.cc", "Engine/WorkerSourceRequest.cc",
                "Engine/WorkerIndexedCopyBudget.cc", "Engine/WorkerPreviewRange.cc", "Engine/WorkerPreviewSession.cc",
                "Engine/WorkerPreviewSourceCopy.cc", "Engine/WorkerPreviewFailure.cc",
                "Engine/WorkerIndexedSourceOperation.cc",
                "Engine/WorkerIndexedSources.cc", "Engine/WorkerGenerationStatistics.cc",
                "Engine/WorkerGenerationStatisticsCopy.cc", "Engine/WorkerNativeSession.cc",
                "Engine/WorkerConnectionSession.cc", "Engine/WorkerSessionOutput.cc",
                "Engine/WorkerSessionOutputCopy.cc", "Engine/WorkerSessionOperation.cc",
                "Engine/WorkerSourceBakeOperation.cc", "Engine/WorkerSourceBakeRequest.cc",
                "Engine/WorkerSourceProducerNative.cc", "Engine/WorkerSourceProducer.cc",
                "Engine/WorkerLegacyRequest.cc", "Engine/WorkerLegacyOperation.cc",
                "Engine/WorkerLegacyProducer.cc", "Engine/WorkerLegacyProducerNative.cc",
                "Engine/WorkerProceduralImages.cc", "Engine/WorkerProceduralImagesNative.cc",
                "Engine/WorkerImportedRequest.cc", "Engine/WorkerImportedOperation.cc",
                "Engine/WorkerImportedProducer.cc",
                "Engine/WorkerImportedProducerNative.cc",
                "Engine/WorkerImportedRender.cc", "Engine/WorkerImportedSurface.cc",
                "Engine/WorkerImportedSurfaceCopy.cc",
                "Engine/WorkerImportedMaterials.cc", "Engine/WorkerImportedMaterialsCopy.cc",
                "Engine/WorkerLegacyBaked.cc", "Engine/WorkerLegacyBakedCopy.cc", "Engine/WorkerMeshCopyInspection.cc",
                "Engine/WorkerSourceBakedCopy.cc", "Engine/WorkerSourceBaked.cc", "Engine/WorkerBakedTextures.cc",
                "Engine/WorkerIndexedMesh.cc",
                "Engine/WorkerSourcedMesh.cc",
                "Engine/WorkerSourceHints.cc", "Engine/WorkerSourceHintValidation.cc",
                "Engine/WorkerSessionOutputValidation.cc",
                "Engine/WorkerProtocol.cc", "Engine/WorkerPreviewArchive.cc", "Engine/WorkerPreviewChunk.cc",
                "Engine/WorkerPreviewChunkStream.cc",
                "Engine/WorkerReplyCollector.cc",
                "Engine/WorkerMesh.cc",
                "Engine/WorkerFailure.cc", "Engine/WorkerAdmission.cc", "Engine/WorkerQueue.cc",
                "Engine/WorkerUv.cc", "Engine/WorkerUvMesh.cc", "Engine/WorkerUvRequest.cc",
                "WorkerRuntime/WorkerBakedDecodedBridge.cc",
                "WorkerRuntime/WorkerImportedDecodedBridge.cc", "WorkerRuntime/WorkerImageDecoder.cc",
                "WorkerRuntime/WorkerPreviewArchiveBridge.cc", "WorkerRuntime/WorkerPreviewSessionBridge.cc",
                "WorkerRuntime/WorkerSessionBridge.cc",
                "WorkerRuntime/WorkerRuntimeBridge.cc",
                "WorkerRuntime/WorkerSessionOutputBridge.cc",
                "WorkerRuntime/WorkerReplyBridge.cc",
                "WorkerRuntime/WorkerDecodedBridge.cc", "WorkerRuntime/WorkerRequestBridge.cc",
                "WorkerRuntime/WorkerQueueBridge.cc",
            ],
            publicHeadersPath: "WorkerRuntime/include",
            cxxSettings: [.headerSearchPath("Engine"), .headerSearchPath("..")]
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

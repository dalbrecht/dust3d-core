// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "Dust3DCore",
    platforms: [.macOS(.v14)],
    products: [
        .library(name: "Dust3DEngine", type: .static, targets: ["Dust3DEngine"])
    ],
    targets: [
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
            sources: ["base", "mesh", "rig", "uv"],
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
                "FbxMaterialImagePlan.cc",
                "FbxMeshScene.cc",
                "FbxMeshScenePlan.cc",
                "FbxGeometrySelection.cc",
                "FbxAtlasGeometry.cc",
                "PngEncoding.cc",
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

// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_IMPORTED_MODELS_H_
#define DUST3D_NATIVE_IMPORTED_MODELS_H_
#include "GlbReader.h"
#include <dust3d/base/snapshot.h>
#include <map>
#include <string>
#include <vector>

namespace dust3d {
namespace native {
    /// Validated archive storage, borrowed only during ImportedModels::load.
    struct ModelAssetView {
        std::string name;
        const uint8_t* bytes;
        size_t size;
    };
    /// Primary import owners, independent of optional generation-cache observers.
    class ImportedModels {
    public:
        static constexpr size_t maximumPayloadBytes = 64 * 1024 * 1024;
        /// Resolve required ImportedModel parts and normalize brace aliases only in
        /// the derived generation snapshot. Throws DocumentError with original cause.
        void load(Snapshot& snapshot, const std::vector<ModelAssetView>& assets, std::string& warning);
        /// Inspect before takeGeometry: true when decoded glTF provides a linear vertex-color channel.
        bool hasVertexColors(const std::string& identity) const;
        /// Transfer geometry to one generator; material owners remain alive locally.
        std::map<std::string, MeshGenerator::ImportedModelData> takeGeometry();
        /// Transfer all decoded owners once; callers may discard geometry for metadata inspection.
        std::map<std::string, GlbAsset> takeAssets();

    private:
        std::map<std::string, GlbAsset> m_assets;
    };
}
}
#endif

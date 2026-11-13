// SPDX-License-Identifier: MIT
#ifndef DUST3D_EXPORT_PROCEDURAL_PNG_H_
#define DUST3D_EXPORT_PROCEDURAL_PNG_H_

#include "WorkerLegacyProducer.h"

namespace dust3d::export_cli {
class ProceduralPngDecoder final : public worker::ProceduralImageDecoder {
public:
    worker::ProceduralDecodedImage decode(const worker::ProceduralImageInput& input,
        uint64_t maximumEncodedBytes, uint64_t maximumDecodedBytes) override;
};
}

#endif

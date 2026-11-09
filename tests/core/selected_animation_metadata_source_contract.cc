// SPDX-License-Identifier: MIT
// Unregistered archive admission contract. No evaluator is needed.
#include "EngineInternal.h"
#include <cassert>
#include <dust3d/base/ds3_file.h>

using namespace dust3d;
using namespace dust3d::native;

void checkSelectedAnimationMetadata()
{
    const auto archiveWith = [](std::string_view animations) {
        const auto xml = "<canvas><animations>" + std::string(animations) + "</animations></canvas>";
        Ds3FileWriter writer;
        const bool added = writer.add("model.xml", "model", xml.data(), xml.size());
        assert(added);
        std::vector<uint8_t> archive;
        writer.save(archive);
        return archive;
    };
    const auto distinct = archiveWith(
        "<animation id='&#233;' type='FishIdle' durationSeconds='1'/>"
        "<animation id='e&#769;' type='FishIdle' durationSeconds='2'/>");
    assert(readSelectedAnimationMetadata(distinct.data(), distinct.size(), u8"é").at("durationSeconds") == "1");
    assert(readSelectedAnimationMetadata(distinct.data(), distinct.size(), u8"e\u0301").at("durationSeconds") == "2");

    const auto duplicate = archiveWith(
        "<animation id='&#233;' type='FishIdle'/>"
        "<animation id='&#233;' durationSeconds='2'/>");
    try {
        (void)readSelectedAnimationMetadata(duplicate.data(), duplicate.size(), u8"é");
        assert(false);
    } catch (const DocumentError& cause) {
        assert(cause.status() == DUST3D_STATUS_INVALID_DOCUMENT);
    }
}

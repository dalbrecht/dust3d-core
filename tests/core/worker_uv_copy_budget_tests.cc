// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerUvCopyBudget.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
void rejects(detail::UvCopyTextBudget& budget, uint64_t length, uint64_t previous)
{
    try {
        budget.charge(length);
    } catch (const UvPayloadError& cause) {
        require(cause.code() == UvPayloadErrorCode::limitExceeded && cause.what()[0], "Original typed text-quota cause retained");
        require(budget.chargedBytes() == previous, "Rejected text cannot change prior admission");
        return;
    }
    throw std::runtime_error("Excess aggregate copy text admitted");
}
void matchesCodecTextAdmission()
{
    UvPayload payload;
    payload.components = { "{11111111-2222-3333-4444-555555555555}" };
    payload.packedTextureSize = 64;
    UvTriangle triangle;
    triangle.known = 1;
    triangle.association = UvAssociation::component;
    triangle.componentIndex = 0;
    triangle.uv = { { { 0, 0 }, { 1, 0 }, { 0, 1 } } };
    payload.triangles = { triangle };
    UvChart chart;
    chart.width = 1;
    chart.height = 1;
    chart.color = { 1, 1, 1, 1 };
    chart.association = UvAssociation::component;
    chart.componentIndex = 0;
    chart.imageID = "{aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee}";
    payload.charts = { chart };
    const auto keyBytes = payload.components.front().size();
    const auto imageBytes = payload.charts.front().imageID.size();
    UvPayloadLimits exact;
    exact.maximumTextBytes = keyBytes + imageBytes;
    detail::UvCopyTextBudget copyBudget(exact.maximumTextBytes);
    copyBudget.charge(keyBytes);
    copyBudget.charge(imageBytes);
    const auto wire = encodeUvPayload(payload, 1, exact);
    require(copyBudget.chargedBytes() == exact.maximumTextBytes && !wire.empty(), "Pre-copy text allowance agrees with complete image-backed wire metadata");
    auto limited = exact;
    --limited.maximumTextBytes;
    detail::UvCopyTextBudget rejectedCopy(limited.maximumTextBytes);
    rejectedCopy.charge(keyBytes);
    rejects(rejectedCopy, imageBytes, keyBytes);
    try {
        (void)encodeUvPayload(payload, 1, limited);
    } catch (const UvPayloadError& cause) {
        require(cause.code() == UvPayloadErrorCode::limitExceeded, "Independent codec rejects the same aggregate text boundary");
        return;
    }
    throw std::runtime_error("Codec unexpectedly accepted excess chart text");
}

}
int main()
{
    try {
        matchesCodecTextAdmission();
        constexpr uint64_t componentKeyBytes = 38, chartImageBytes = 38;
        detail::UvCopyTextBudget exact(76);
        exact.charge(componentKeyBytes);
        exact.charge(chartImageBytes);
        require(exact.chargedBytes() == 76, "Mixed key/image text accepts exact aggregate quota");
        exact.charge(0);
        require(exact.chargedBytes() == 76, "Empty chart image text adds no retained bytes");

        detail::UvCopyTextBudget limited(75);
        limited.charge(38);
        rejects(limited, 38, 38);

        detail::UvCopyTextBudget repeated(113);
        repeated.charge(38);
        repeated.charge(38);
        rejects(repeated, 38, 76);

        detail::UvCopyTextBudget oversized(37);
        rejects(oversized, 38, 0);
        std::cout << "Pure UV pre-copy mixed-text budget contracts passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

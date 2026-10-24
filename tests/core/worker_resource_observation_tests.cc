// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerResourceObservation.h>
#include <stdexcept>

using namespace dust3d::worker;
namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
ResourceLaunch launch()
{
    return { ResourceRuntime::ordinary, { { 600, 600 }, { 16777216, 16777216 }, { 128, 128 }, std::nullopt } };
}
ResourceObservation decode(const std::string& bytes, const ResourceLaunch& requested)
{
    return decodeResourceObservation(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), requested);
}
template <typename Action>
void rejects(ResourceObservationErrorCode code, std::optional<ResourceKind> resource, Action action)
{
    try {
        action();
    } catch (const ResourceObservationError& error) {
        require(error.code() == code && error.resource() == resource && error.what()[0], "Original typed observation cause required");
        return;
    }
    throw std::runtime_error("Invalid resource observation admitted");
}
void ordinary()
{
    const std::string prefix = "resource-profile-v1 ordinary 600 600 16777216 16777216 0 0 128 128\n";
    const auto bytes = prefix + std::string("\0\xff\x1bopaque", 9);
    auto observed = decode(bytes, launch());
    require(observed.runtime == ResourceRuntime::ordinary && observed.diagnosticPrefixBytes == prefix.size(), "Only complete bounded resource prefix consumed; opaque tail remains independent");
    require(observed.effective[0].soft == 600 && observed.effective[1].hard == 16777216 && observed.effective[2].soft == 0 && observed.effective[3].hard == 128, "Effective caps preserved in resource order");
    observed = decode("resource-profile-v1 ordinary 1 2 3 4 0 0 6 8\n", launch());
    require(observed.effective[0].hard == 2 && observed.effective[3].soft == 6, "Stricter inherited caps are admissible");
    observed = decode("resource-profile-v1 ordinary 0 0 0 0 0 0 0 0\n", launch());
    require(observed.effective[0].soft == 0, "Zero inherited caps are not silently raised by admission");
    auto requested = launch();
    requested.runtime = ResourceRuntime::addressSanitized;
    observed = decode("resource-profile-v1 asan 600 600 16777216 16777216 0 0 128 128\n", requested);
    require(observed.runtime == ResourceRuntime::addressSanitized, "Instrumented runtime matches explicitly");
}
void malformed()
{
    rejects(ResourceObservationErrorCode::invalidReport, std::nullopt, [&] { decodeResourceObservation(nullptr, 0, launch()); });
    rejects(ResourceObservationErrorCode::invalidReport, std::nullopt, [&] { decodeResourceObservation(nullptr, 1, launch()); });
    for (const auto& text : { std::string(), std::string("resource-profile-v1 ordinary 1 1 1 1 0 0 1 1"), std::string(1024, 'x') + "\n", std::string("\n"), std::string("resource-profile-v2 ordinary 1 1 1 1 0 0 1 1\n"), std::string("resource-profile-v1 unknown 1 1 1 1 0 0 1 1\n"), std::string("resource-profile-v1  ordinary 1 1 1 1 0 0 1 1\n"), std::string("resource-profile-v1 ordinary 1 1 1 1 0 0 1 1 extra\n") })
        rejects(ResourceObservationErrorCode::invalidReport, std::nullopt, [&] { decode(text, launch()); });
    rejects(ResourceObservationErrorCode::runtimeMismatch, std::nullopt, [&] { decode("resource-profile-v1 asan 1 1 1 1 0 0 1 1\n", launch()); });
    for (const auto* number : { "01", "+1", "-1", "1x", "18446744073709551616" })
        rejects(ResourceObservationErrorCode::invalidReport, ResourceKind::cpuSeconds, [&] { decode(std::string("resource-profile-v1 ordinary ") + number + " 2 1 1 0 0 1 1\n", launch()); });
    rejects(ResourceObservationErrorCode::invalidReport, ResourceKind::cpuSeconds, [&] { decode("resource-profile-v1 ordinary 2 1 1 1 0 0 1 1\n", launch()); });
    const std::string changed[] = { "601 601 1 1 0 0 1 1", "1 1 16777217 16777217 0 0 1 1", "1 1 1 1 1 1 1 1", "1 1 1 1 0 0 129 129" };
    for (size_t index = 0; index < 4; ++index)
        rejects(ResourceObservationErrorCode::relaxedLimit, static_cast<ResourceKind>(index), [&] { decode("resource-profile-v1 ordinary " + changed[index] + "\n", launch()); });
    auto requested = launch();
    requested.profile.cpuSeconds = { 10, 20 };
    rejects(ResourceObservationErrorCode::relaxedLimit, ResourceKind::cpuSeconds, [&] { decode("resource-profile-v1 ordinary 11 12 1 1 0 0 1 1\n", requested); });
    rejects(ResourceObservationErrorCode::relaxedLimit, ResourceKind::cpuSeconds, [&] { decode("resource-profile-v1 ordinary 9 21 1 1 0 0 1 1\n", requested); });
    requested.profile.addressBytes = ResourceBudget { 100, 200 };
    try {
        decode("resource-profile-v1 ordinary 1 1 1 1 0 0 1 1\n", requested);
        throw std::runtime_error("Unsupported virtual activation admitted");
    } catch (const ResourceError& error) {
        require(error.code() == ResourceErrorCode::unsupportedResource && error.resource() == ResourceKind::addressBytes, "Unsupported profile preflight preserves original resource cause");
    }
}
}
int main()
{
    try {
        ordinary();
        malformed();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

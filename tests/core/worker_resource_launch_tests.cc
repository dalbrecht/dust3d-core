// SPDX-License-Identifier: MIT
#include <iostream>
#include <limits>
#include <native/Engine/WorkerResourceLaunch.h>
#include <stdexcept>

using namespace dust3d::worker;
namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
template <typename Action>
void rejects(ResourceLaunchErrorCode code, Action action)
{
    try {
        action();
    } catch (const ResourceLaunchError& error) {
        require(error.code() == code && error.what()[0], "Original typed launch error required");
        return;
    }
    throw std::runtime_error("Invalid resource launch admitted");
}
void roundTrips()
{
    ResourceLaunch launch { ResourceRuntime::ordinary, { { 600, 600 }, { 16777216, 16777216 }, { 128, 128 }, std::nullopt } };
    auto arguments = encodeResourceLaunch(launch);
    require(arguments == std::vector<std::string> { "--resource-profile-v1", "ordinary", "600", "600", "16777216", "16777216", "128", "128" }, "Canonical explicit version/runtime/unit budgets required");
    auto decoded = decodeResourceLaunch(arguments, ResourceRuntime::ordinary);
    require(decoded.runtime == launch.runtime && decoded.profile.cpuSeconds.soft == 600 && decoded.profile.fileBytes.hard == 16777216 && decoded.profile.descriptors.soft == 128 && !decoded.profile.addressBytes, "Mandatory budgets survive round trip without memory default");
    launch.profile.addressBytes = ResourceBudget { 1234, std::numeric_limits<uint64_t>::max() };
    arguments = encodeResourceLaunch(launch);
    decoded = decodeResourceLaunch(arguments, ResourceRuntime::ordinary);
    require(arguments.size() == 10 && decoded.profile.addressBytes->soft == 1234 && decoded.profile.addressBytes->hard == std::numeric_limits<uint64_t>::max(), "Optional finite virtual cap survives exact integer round trip");
    launch.runtime = ResourceRuntime::addressSanitized;
    try {
        encodeResourceLaunch(launch);
        throw std::runtime_error("Instrumented virtual cap admitted by encoder");
    } catch (const ResourceError& error) {
        require(error.code() == ResourceErrorCode::incompatibleRuntime, "Encoder preserves runtime/policy compatibility failure");
    }
    launch.profile.addressBytes.reset();
    arguments = encodeResourceLaunch(launch);
    require(arguments[1] == "asan" && decodeResourceLaunch(arguments, ResourceRuntime::addressSanitized).runtime == ResourceRuntime::addressSanitized, "Instrumented runtime has explicit identity");
    rejects(ResourceLaunchErrorCode::runtimeMismatch, [&] { decodeResourceLaunch(arguments, ResourceRuntime::ordinary); });
    rejects(ResourceLaunchErrorCode::runtimeMismatch, [&] { decodeResourceLaunch(arguments, ResourceRuntime::unknown); });
    rejects(ResourceLaunchErrorCode::runtimeMismatch, [&] { decodeResourceLaunch(arguments, static_cast<ResourceRuntime>(99)); });
}
void malformed()
{
    const auto valid = encodeResourceLaunch({ ResourceRuntime::ordinary, { { 600, 600 }, { 16777216, 16777216 }, { 128, 128 }, std::nullopt } });
    for (size_t size : { size_t { 0 }, size_t { 7 }, size_t { 9 }, size_t { 11 } }) {
        auto arguments = valid;
        arguments.resize(size);
        rejects(ResourceLaunchErrorCode::invalidArguments, [&] { decodeResourceLaunch(arguments, ResourceRuntime::ordinary); });
    }
    auto arguments = valid;
    arguments[0] = "--resource-profile-v2";
    rejects(ResourceLaunchErrorCode::unsupportedVersion, [&] { decodeResourceLaunch(arguments, ResourceRuntime::ordinary); });
    arguments = valid;
    arguments[1] = "unknown";
    rejects(ResourceLaunchErrorCode::invalidArguments, [&] { decodeResourceLaunch(arguments, ResourceRuntime::ordinary); });
    arguments[1] = std::string(33, 'a');
    rejects(ResourceLaunchErrorCode::invalidArguments, [&] { decodeResourceLaunch(arguments, ResourceRuntime::ordinary); });
    arguments[1] = std::string("ordinary\0tail", 13);
    rejects(ResourceLaunchErrorCode::invalidArguments, [&] { decodeResourceLaunch(arguments, ResourceRuntime::ordinary); });
    for (const std::string number : { "", "01", "+1", "-1", " 1", "1 ", "1x", "18446744073709551616" }) {
        arguments = valid;
        arguments[2] = number;
        rejects(ResourceLaunchErrorCode::invalidNumber, [&] { decodeResourceLaunch(arguments, ResourceRuntime::ordinary); });
    }
    arguments = valid;
    arguments[2] = "0";
    try {
        decodeResourceLaunch(arguments, ResourceRuntime::ordinary);
        throw std::runtime_error("Invalid zero policy admitted");
    } catch (const ResourceError& error) {
        require(error.code() == ResourceErrorCode::invalidPolicy && error.resource() == ResourceKind::cpuSeconds, "Policy errors are not replaced by generic launch errors");
    }
    try {
        encodeResourceLaunch({ ResourceRuntime::unknown, { { 1, 1 }, { 1, 1 }, { 1, 1 }, std::nullopt } });
        throw std::runtime_error("Unknown encoder runtime admitted");
    } catch (const ResourceError& error) {
        require(error.code() == ResourceErrorCode::incompatibleRuntime, "Unknown runtime fails before encoding");
    }
}
}
int main()
{
    try {
        roundTrips();
        malformed();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

// SPDX-License-Identifier: MIT
#include "WorkerResourceLaunch.h"
#include <charconv>
#include <limits>

namespace dust3d {
namespace worker {
    namespace {
        void validate(const ResourceLaunch& launch)
        {
            ResourceEnvironment environment {};
            environment.runtime = launch.runtime;
            for (auto& resource : environment.resources)
                resource = { true, std::numeric_limits<uint64_t>::max(), { std::nullopt, std::nullopt } };
            (void)planWorkerResources(launch.profile, environment);
        }
        uint64_t number(const std::string& text)
        {
            uint64_t value = 0;
            if (text.empty() || (text.size() > 1 && text[0] == '0'))
                throw ResourceLaunchError(ResourceLaunchErrorCode::invalidNumber, "Worker resource numbers must be canonical unsigned decimals");
            const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
            if (result.ec != std::errc {} || result.ptr != text.data() + text.size())
                throw ResourceLaunchError(ResourceLaunchErrorCode::invalidNumber, "Worker resource number is invalid or outside the integer range");
            return value;
        }
    }
    ResourceLaunchError::ResourceLaunchError(ResourceLaunchErrorCode code, const char* message)
        : std::runtime_error(message)
        , m_code(code)
    {
    }
    std::vector<std::string> encodeResourceLaunch(const ResourceLaunch& launch)
    {
        validate(launch);
        const auto& profile = launch.profile;
        std::vector<std::string> arguments { "--resource-profile-v1", launch.runtime == ResourceRuntime::ordinary ? "ordinary" : "asan",
            std::to_string(profile.cpuSeconds.soft), std::to_string(profile.cpuSeconds.hard),
            std::to_string(profile.fileBytes.soft), std::to_string(profile.fileBytes.hard),
            std::to_string(profile.descriptors.soft), std::to_string(profile.descriptors.hard) };
        if (profile.addressBytes) {
            arguments.push_back(std::to_string(profile.addressBytes->soft));
            arguments.push_back(std::to_string(profile.addressBytes->hard));
        }
        return arguments;
    }
    ResourceLaunch decodeResourceLaunch(const std::vector<std::string>& arguments, ResourceRuntime actualRuntime)
    {
        if (arguments.size() != 8 && arguments.size() != 10)
            throw ResourceLaunchError(ResourceLaunchErrorCode::invalidArguments, "Worker resource launch requires eight or ten explicit arguments");
        for (const auto& argument : arguments) {
            if (argument.size() > 32 || argument.find('\0') != std::string::npos)
                throw ResourceLaunchError(ResourceLaunchErrorCode::invalidArguments, "Worker resource argument exceeds supported token bounds");
        }
        if (arguments[0] != "--resource-profile-v1")
            throw ResourceLaunchError(ResourceLaunchErrorCode::unsupportedVersion, "Worker resource launch version is unsupported");
        const auto runtime = arguments[1] == "ordinary" ? ResourceRuntime::ordinary : arguments[1] == "asan" ? ResourceRuntime::addressSanitized
                                                                                                             : ResourceRuntime::unknown;
        if (runtime == ResourceRuntime::unknown)
            throw ResourceLaunchError(ResourceLaunchErrorCode::invalidArguments, "Worker resource runtime token is unsupported");
        if (runtime != actualRuntime)
            throw ResourceLaunchError(ResourceLaunchErrorCode::runtimeMismatch, "Worker resource runtime does not match the compiled helper");
        ResourceLaunch launch { runtime, { { number(arguments[2]), number(arguments[3]) }, { number(arguments[4]), number(arguments[5]) }, { number(arguments[6]), number(arguments[7]) }, std::nullopt } };
        if (arguments.size() == 10)
            launch.profile.addressBytes = ResourceBudget { number(arguments[8]), number(arguments[9]) };
        validate(launch);
        return launch;
    }
}
}

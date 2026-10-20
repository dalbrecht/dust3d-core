// SPDX-License-Identifier: MIT
#include <cstdlib>
#include <functional>
#include <iostream>
#include <json/json.hpp>
#include <new>
#include <stdexcept>
#include <string>

namespace {
thread_local size_t allocationCountdown = SIZE_MAX;
thread_local bool hasInjectedFailure = false;
}
void* operator new(size_t size)
{
    if (allocationCountdown != SIZE_MAX) {
        if (allocationCountdown == 0) {
            allocationCountdown = SIZE_MAX;
            hasInjectedFailure = true;
            throw std::bad_alloc();
        }
        --allocationCountdown;
    }
    if (void* value = std::malloc(size ? size : 1))
        return value;
    throw std::bad_alloc();
}
void* operator new[](size_t size) { return ::operator new(size); }
void operator delete(void* value) noexcept { std::free(value); }
void operator delete[](void* value) noexcept { std::free(value); }
void operator delete(void* value, size_t) noexcept { std::free(value); }
void operator delete[](void* value, size_t) noexcept { std::free(value); }

namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

size_t containAllocationFailures(const char* label, const std::function<void()>& operation)
{
    operation();
    size_t allocations = 0;
    for (size_t index = 0; index < 10000; ++index) {
        hasInjectedFailure = false;
        allocationCountdown = index;
        try {
            operation();
            allocationCountdown = SIZE_MAX;
            if (!hasInjectedFailure) {
                allocations = index;
                break;
            }
        } catch (const std::bad_alloc&) {
            allocationCountdown = SIZE_MAX;
            require(hasInjectedFailure, "JSON operation threw an unexpected allocation failure");
        }
    }
    require(allocations > 0, label);
    return allocations;
}
}

int main()
{
    try {
        const auto document = nlohmann::json::parse(R"({"asset":{"version":"2.0"}})");
        const auto& version = document.at("asset").at("version");
        const nlohmann::json number = 2;
        const std::string longLiteral(64, 'a');
        const nlohmann::json longVersion = longLiteral;
        require(!(version == nullptr) && number == 2 && !(number != 2), "Scalar JSON comparison changed");
        const size_t parseFailures = containAllocationFailures("JSON value parse did not allocate", [] {
            const auto parsed = nlohmann::json::parse(R"({"version":"2.0","values":[1,true,null]})");
            require(parsed.at("version") == "2.0" && parsed.at("values").is_array(), "Parsed JSON value changed");
        });
        const size_t failures = containAllocationFailures("glTF version literal comparison did not allocate", [&] {
            require(version == "2.0" && !(version != "2.0"), "Matching glTF version literal compared unequal");
            require(!(version == "1.0") && version != "1.0", "Different glTF version literal compared equal");
            require(!(version < "2.0") && !(version > "2.0") && version <= "2.0" && version >= "2.0",
                "Ordered glTF version literal comparison changed");
            require(longVersion == longLiteral.c_str() && !(longVersion != longLiteral.c_str()),
                "Long JSON string literal comparison changed");
        });
        std::cout << "JSON allocation failures propagated: parse " << parseFailures << ", comparison " << failures << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

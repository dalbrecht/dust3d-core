// SPDX-License-Identifier: MIT
#include <iostream>
#include <json/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>

using Json = nlohmann::json;

// Unevaluated expressions verify the mitigation without injecting failures.
static_assert(!noexcept(std::declval<const Json&>() == "2.0"), "String comparison conversion must be allowed to throw");
static_assert(!noexcept(std::declval<const Json&>() != "2.0"), "String comparison conversion must be allowed to throw");
static_assert(!noexcept(std::declval<const Json&>() < "2.0"), "String comparison conversion must be allowed to throw");
static_assert(!noexcept(std::declval<const Json&>() > "2.0"), "String comparison conversion must be allowed to throw");
static_assert(!noexcept(std::declval<const Json&>() <= "2.0"), "String comparison conversion must be allowed to throw");
static_assert(!noexcept(std::declval<const Json&>() >= "2.0"), "String comparison conversion must be allowed to throw");
static_assert(noexcept(std::declval<const Json&>() == 2), "Numeric scalar comparison must retain its no-throw contract");

namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
}

int main()
{
    try {
        const auto document = Json::parse(R"({"asset":{"version":"2.0"},"values":[1,true,null]})");
        const auto& version = document.at("asset").at("version");
        const Json number = 2;
        require(!(version == nullptr) && number == 2 && !(number != 2), "Scalar JSON comparison changed");
        require(document.at("values").is_array() && document.at("values").size() == 3, "Ordinary JSON parse changed");
        require(version == "2.0" && !(version != "2.0"), "Matching glTF version literal compared unequal");
        require(!(version == "1.0") && version != "1.0", "Different glTF version literal compared equal");
        require(!(version < "2.0") && !(version > "2.0") && version <= "2.0" && version >= "2.0",
            "Ordered glTF version literal comparison changed");
        const std::string longLiteral(64, 'a');
        const Json longVersion = longLiteral;
        require(longVersion == longLiteral.c_str() && !(longVersion != longLiteral.c_str()),
            "Long JSON string literal comparison changed");
        std::cout << "Ordinary JSON comparison contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

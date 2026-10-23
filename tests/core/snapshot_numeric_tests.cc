#include "EngineInternal.h"
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void rejectNumber(const std::string& value, const std::string& originalCause)
{
    dust3d::Snapshot snapshot;
    snapshot.canvas["originX"] = value;
    try {
        dust3d::native::validateSnapshot(snapshot, false, true);
    } catch (const dust3d::native::DocumentError& error) {
        require(error.status() == DUST3D_STATUS_INVALID_DOCUMENT, "Numeric rejection lost its document status");
        const std::string message = error.what();
        require(message.find("originX") != std::string::npos, "Numeric rejection lost its field name");
        require(message.find(originalCause) != std::string::npos, "Numeric rejection lost its original cause");
        return;
    }
    throw std::runtime_error("Invalid numeric metadata was accepted");
}
}

int main()
{
    try {
        // Capture this standard library's diagnostic rather than assume its wording.
        for (const auto* value : { "bad", "1e309", "1e-999" }) {
            std::string originalCause;
            try {
                std::stod(value);
            } catch (const std::invalid_argument& error) {
                originalCause = error.what();
            } catch (const std::out_of_range& error) {
                originalCause = error.what();
            }
            require(!originalCause.empty(), "Fixture must exercise numeric conversion rejection");
            rejectNumber(value, originalCause);
        }
        for (const auto* value : { "", "0", "0.25", "0,25", "-0.25", "1000000" }) {
            dust3d::Snapshot snapshot;
            snapshot.canvas["originX"] = value;
            dust3d::native::validateSnapshot(snapshot, false, true);
            require(snapshot.canvas.at("originX") == value, "Validation must preserve source spelling");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

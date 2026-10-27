// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerPreviewChunkStream.h>
#include <native/Engine/WorkerSessionOutput.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
template <class Action>
void reject(PreviewChunkErrorCode expected, Action action)
{
    try {
        action();
    } catch (const PreviewChunkError& cause) {
        require(cause.code() == expected && cause.what()[0], "Typed byte stream cause survives");
        return;
    }
    throw std::runtime_error("Invalid stream operation accepted");
}
struct SinkError : std::runtime_error {
    using std::runtime_error::runtime_error;
};
template <class Action>
void original(Action action)
{
    try {
        action();
    } catch (const SinkError& cause) {
        require(std::string(cause.what()) == "original sink cause", "Original callback cause retained");
        return;
    }
    throw std::runtime_error("Callback failure replaced or swallowed");
}
}
int main()
{
    try {
        PreviewChunkIdentity identity;
        identity.request.document[0] = 1;
        identity.request.request = 2;
        identity.connectionGeneration = 3;
        identity.direction = PreviewChunkDirection::session;
        PreviewChunkPolicy policy { 1024, 4 };
        const auto body = encodeSessionOutput({}, {});
        std::vector<uint8_t> wire;
        PreviewChunkWriter writer(identity, body.size(), policy, [&](const uint8_t* bytes, size_t size) { wire.insert(wire.end(), bytes, bytes + size); });
        writer.start();
        writer.append(nullptr, 0);
        for (size_t offset = 0; offset < body.size();) {
            const auto count = std::min(size_t(3), body.size() - offset);
            writer.append(body.data() + offset, count);
            offset += count;
        }
        writer.finish();
        require(writer.isFinished() && !writer.isFailed() && wire.size() == measurePreviewChunking(body.size(), policy).wireBytes, "Writer emits exact canonical physical framing from arbitrary small spans");
        std::vector<uint8_t> copied;
        size_t controls = 0;
        PreviewChunkReader reader(identity, policy, [&](const PreviewChunkHeader& header, const uint8_t* bytes, size_t size) {
            if (header.kind == PreviewChunkKind::data)
                copied.insert(copied.end(), bytes, bytes + size);
            else {
                require(!bytes && !size, "Control callbacks borrow no body");
                ++controls;
            }
        });
        reader.append(nullptr, 0);
        for (size_t offset = 0; offset < wire.size(); ++offset)
            reader.append(wire.data() + offset, 1);
        require(!reader.isFramed() && copied == body && controls == 2, "Complete byte stream stays provisional before actual EOF");
        reader.finishAtEOF();
        wire.clear();
        require(reader.isFramed() && !reader.isFailed() && std::holds_alternative<std::monostate>(decodeSessionOutput(copied.data(), copied.size(), {}).outcome), "Independent copied body survives reader workspace and still needs semantic decode");
        reject(PreviewChunkErrorCode::invalidSequence, [&] { reader.append(nullptr, 0); });
        require(reader.isFailed(), "Reader cannot be reused after completion");
        reject(PreviewChunkErrorCode::invalidSequence, [&] { writer.finish(); });
        require(writer.isFailed(), "Writer cannot emit another terminal marker");
        auto ignore = [](const PreviewChunkHeader&, const uint8_t*, size_t) { return; };
        auto ignoreBytes = [](const uint8_t*, size_t) { return; };
        reject(PreviewChunkErrorCode::invalidInput, [&] { PreviewChunkReader empty(identity, policy, {}); });
        reject(PreviewChunkErrorCode::invalidInput, [&] { PreviewChunkWriter empty(identity, 9, policy, {}); });
        reject(PreviewChunkErrorCode::invalidLimits, [&] { PreviewChunkReader invalid(identity, { 9, 65537 }, ignore); });
        reject(PreviewChunkErrorCode::invalidLimits, [&] { PreviewChunkWriter invalid(identity, 9, { 9, 65537 }, ignoreBytes); });
        PreviewChunkReader missing(identity, policy, ignore);
        reject(PreviewChunkErrorCode::invalidInput, [&] { missing.append(nullptr, 1); });
        require(missing.isFailed(), "Missing input seals reader");
        PreviewChunkWriter duplicateStart(identity, 9, policy, ignoreBytes);
        duplicateStart.start();
        reject(PreviewChunkErrorCode::invalidSequence, [&] { duplicateStart.start(); });
        PreviewChunkWriter earlyFinish(identity, 9, policy, ignoreBytes);
        reject(PreviewChunkErrorCode::invalidSequence, [&] { earlyFinish.finish(); });
        PreviewChunkWriter notStarted(identity, 9, policy, ignoreBytes);
        reject(PreviewChunkErrorCode::invalidSequence, [&] { notStarted.append(body.data(), 1); });
        PreviewChunkWriter noInput(identity, 9, policy, ignoreBytes);
        noInput.start();
        reject(PreviewChunkErrorCode::invalidInput, [&] { noInput.append(nullptr, 1); });
        PreviewChunkWriter shortBody(identity, 9, policy, ignoreBytes);
        shortBody.start();
        shortBody.append(body.data(), 3);
        reject(PreviewChunkErrorCode::payloadMismatch, [&] { shortBody.finish(); });
        std::vector<uint8_t> overlongWire;
        PreviewChunkWriter overlong(identity, 9, policy, [&](const uint8_t* bytes, size_t size) { overlongWire.insert(overlongWire.end(), bytes, bytes + size); });
        overlong.start();
        reject(PreviewChunkErrorCode::payloadMismatch, [&] { overlong.append(body.data(), 10); });
        require(overlongWire.size() == 80, "Overlong append rejects before publishing any of its body");
        PreviewChunkHeader begin { PreviewChunkKind::begin, identity, 0, 9, 0 };
        auto beginBytes = encodePreviewChunkHeader(begin, policy);
        PreviewChunkReader partialHeader(identity, policy, ignore);
        partialHeader.append(beginBytes.data(), 7);
        reject(PreviewChunkErrorCode::invalidSequence, [&] { partialHeader.finishAtEOF(); });
        auto malformed = beginBytes;
        malformed[0] = 0;
        PreviewChunkReader badHeader(identity, policy, ignore);
        reject(PreviewChunkErrorCode::invalidHeader, [&] { badHeader.append(malformed.data(), malformed.size()); });
        reject(PreviewChunkErrorCode::invalidHeader, [&] { badHeader.append(beginBytes.data(), beginBytes.size()); });
        require(badHeader.isFailed(), "Decode failure seals enclosing reader and retains original cause");
        auto foreign = begin;
        ++foreign.identity.connectionGeneration;
        const auto foreignBytes = encodePreviewChunkHeader(foreign, policy);
        PreviewChunkReader foreignReader(identity, policy, ignore);
        reject(PreviewChunkErrorCode::identityMismatch, [&] { foreignReader.append(foreignBytes.data(), foreignBytes.size()); });
        PreviewChunkReader partialBody(identity, policy, ignore);
        partialBody.append(beginBytes.data(), beginBytes.size());
        const auto dataHeader = encodePreviewChunkHeader({ PreviewChunkKind::data, identity, 0, 9, 4 }, policy);
        partialBody.append(dataHeader.data(), dataHeader.size());
        partialBody.append(body.data(), 2);
        reject(PreviewChunkErrorCode::invalidSequence, [&] { partialBody.finishAtEOF(); });
        PreviewChunkReader noEnd(identity, policy, ignore);
        noEnd.append(beginBytes.data(), beginBytes.size());
        reject(PreviewChunkErrorCode::invalidSequence, [&] { noEnd.finishAtEOF(); });
        std::vector<uint8_t> smallWire;
        PreviewChunkWriter small(identity, 9, policy, [&](const uint8_t* bytes, size_t size) { smallWire.insert(smallWire.end(), bytes, bytes + size); });
        small.start();
        small.append(body.data(), 9);
        small.finish();
        smallWire.push_back(0);
        PreviewChunkReader trailing(identity, policy, ignore);
        reject(PreviewChunkErrorCode::invalidSequence, [&] { trailing.append(smallWire.data(), smallWire.size()); });
        require(trailing.isFailed() && !trailing.isFramed(), "One trailing byte rejects without waiting for a full header");
        PreviewChunkWriter failedSink(identity, 9, policy, [](const uint8_t*, size_t size) { if (size != 80) throw SinkError("original sink cause"); });
        failedSink.start();
        original([&] { failedSink.append(body.data(), 4); });
        original([&] { failedSink.finish(); });
        PreviewChunkReader failedReader(identity, policy, [](const PreviewChunkHeader& header, const uint8_t*, size_t) { if (header.kind == PreviewChunkKind::data) throw SinkError("original sink cause"); });
        original([&] { failedReader.append(smallWire.data(), smallWire.size() - 1); });
        original([&] { failedReader.finishAtEOF(); });
        require(failedReader.isFailed() && failedSink.isFailed(), "Callback errors seal endpoints");
        PreviewChunkWriter* activeWriter = nullptr;
        PreviewChunkWriter reentrantWriter(identity, 9, policy, [&](const uint8_t*, size_t) { try { activeWriter->append(nullptr, 0); } catch (const PreviewChunkError&) { } });
        activeWriter = &reentrantWriter;
        reject(PreviewChunkErrorCode::invalidSequence, [&] { reentrantWriter.start(); });
        PreviewChunkWriter* guardedWriter = nullptr;
        size_t dataHeaders = 0, bodyCallbacks = 0;
        PreviewChunkWriter betweenCallbacks(identity, 4, policy, [&](const uint8_t*, size_t size) {
            if (size == 80) {
                ++dataHeaders;
                if (dataHeaders == 2) {
                    try {
                        guardedWriter->append(nullptr, 0);
                    } catch (const PreviewChunkError&) {
                    }
                }
            } else {
                ++bodyCallbacks;
            }
        });
        guardedWriter = &betweenCallbacks;
        betweenCallbacks.start();
        reject(PreviewChunkErrorCode::invalidSequence, [&] { betweenCallbacks.append(body.data(), 4); });
        require(betweenCallbacks.isFailed() && bodyCallbacks == 0, "Caught header reentry must stop before the body sink");
        PreviewChunkReader* activeReader = nullptr;
        PreviewChunkReader reentrantReader(identity, policy, [&](const PreviewChunkHeader&, const uint8_t*, size_t) { try { activeReader->append(nullptr, 0); } catch (const PreviewChunkError&) { } });
        activeReader = &reentrantReader;
        reject(PreviewChunkErrorCode::invalidSequence, [&] { reentrantReader.append(beginBytes.data(), beginBytes.size()); });
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

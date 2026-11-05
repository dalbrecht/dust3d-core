// SPDX-License-Identifier: MIT
#include "../Engine/CancellationToken.h"
#include "ExportCancellation.h"
#include "ExportReceipt.h"
#include <Dust3DBridge.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {
enum class ExitCode { usage = 2,
    native = 3,
    filesystem = 4,
    partialBatch = 5,
    receiptDelivery = 6,
    cancelled = 130 };
class ExportError : public std::runtime_error {
public:
    ExportError(ExitCode code, const std::string& cause)
        : std::runtime_error(cause)
        , m_code(code)
    {
    }
    int exitCode() const noexcept { return static_cast<int>(m_code); }

private:
    ExitCode m_code;
};
[[noreturn]] void filesystemFailure(const char* operation, int cause)
{
    throw ExportError(ExitCode::filesystem, std::string(operation) + ": " + std::error_code(cause, std::generic_category()).message() + " (errno " + std::to_string(cause) + ')');
}
using dust3d::export_cli::commitUnlessCancelled;
using dust3d::export_cli::ExportCancelled;

static_assert(std::atomic_bool::is_always_lock_free, "SIGINT requires a lock-free cancellation flag");
Dust3DCancellationToken cancellationToken;

void cancelExport(int)
{
    cancellationToken.cancelled.store(true, std::memory_order_relaxed);
}

bool isCancelled()
{
    return dust3d_cancellation_token_is_cancelled(&cancellationToken);
}

void installCancellationHandler()
{
    struct sigaction action { };
    action.sa_handler = cancelExport;
    if (sigemptyset(&action.sa_mask) < 0 || ::sigaction(SIGINT, &action, nullptr) < 0)
        filesystemFailure("Configure export cancellation", errno);
}

void checkCancellation()
{
    if (isCancelled())
        throw ExportCancelled();
}

class Descriptor {
public:
    explicit Descriptor(int value) noexcept
        : m_value(value)
    {
    }
    ~Descriptor()
    {
        if (m_value >= 0 && ::close(m_value) != 0)
            std::fprintf(stderr, "Descriptor cleanup failed (errno %d)\n", errno);
    }
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    int get() const noexcept { return m_value; }
    void closeForCommit()
    {
        // A failed close must not be retried against a potentially reused descriptor.
        if (::close(std::exchange(m_value, -1)) != 0)
            filesystemFailure("Close staged output", errno);
    }

private:
    int m_value;
};
struct Arguments {
    bool baked = false;
    size_t textureSize = 256;
    size_t maximumOutputBytes = 0;
    std::filesystem::path source;
    std::filesystem::path destination;
};
size_t parseNumber(const std::string& value)
{
    size_t number = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), number);
    if (result.ec != std::errc() || result.ptr != value.data() + value.size())
        throw ExportError(ExitCode::usage, "Expected a bounded unsigned decimal option value.");
    return number;
}
struct Invocation {
    std::vector<Arguments> jobs;
    bool isBatch = false;
    bool usesLegacyOutputs = false;
    bool jsonReceipts = false;
};
Invocation parseArguments(int argc, char** argv)
{
    Arguments result;
    Invocation invocation;
    std::vector<std::string> paths;
    std::vector<std::string> legacyOutputs;
    bool optionsEnded = false;
    bool hasTextureSize = false;
    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        if (!optionsEnded && argument == "--") {
            optionsEnded = true;
        } else if (!optionsEnded && (argument == "-o" || argument == "-output")) {
            if (++index == argc)
                throw ExportError(ExitCode::usage, "Missing legacy export target.");
            if (legacyOutputs.size() == 64)
                throw ExportError(ExitCode::usage, "Legacy export supports at most64targets.");
            legacyOutputs.emplace_back(argv[index]);
        } else if (!optionsEnded && argument == "--json") {
            invocation.jsonReceipts = true;
        } else if (!optionsEnded && argument == "--batch") {
            invocation.isBatch = true;
        } else if (!optionsEnded && argument == "--baked") {
            result.baked = true;
        } else if (!optionsEnded && (argument == "--texture-size" || argument == "--max-output-bytes")) {
            if (++index == argc)
                throw ExportError(ExitCode::usage, "Missing numeric export option value.");
            const auto value = parseNumber(argv[index]);
            if (argument == "--texture-size") {
                if (value < 1 || value > 8192)
                    throw ExportError(ExitCode::usage, "Texture size must be1...8192.");
                result.textureSize = value;
                hasTextureSize = true;
            } else {
                result.maximumOutputBytes = value;
            }
        } else if (!optionsEnded && argument.size() && argument[0] == '-') {
            throw ExportError(ExitCode::usage, "Unknown export option: " + argument);
        } else {
            if (paths.size() == 128)
                throw ExportError(ExitCode::usage, "Batch supports at most64input/output pairs.");
            paths.push_back(argument);
        }
    }
    if (!legacyOutputs.empty()) {
        if (invocation.isBatch || paths.size() != 1 || (hasTextureSize && !result.baked))
            throw ExportError(ExitCode::usage, "Legacy -o/-output requires one source and cannot mix with --batch.");
        invocation.isBatch = true;
        invocation.usesLegacyOutputs = true;
        result.source = paths.front();
        for (const auto& output : legacyOutputs) {
            result.destination = output;
            invocation.jobs.push_back(result);
        }
        return invocation;
    }
    if (paths.empty() || paths.size() % 2 || (!invocation.isBatch && paths.size() != 2) || (hasTextureSize && !result.baked))
        throw ExportError(ExitCode::usage, "Expected input/output pairs; multiple pairs require --batch; --texture-size requires --baked.");
    for (size_t index = 0; index < paths.size(); index += 2) {
        result.source = paths[index];
        result.destination = paths[index + 1];
        invocation.jobs.push_back(result);
    }
    return invocation;
}

struct Archive {
    std::vector<uint8_t> bytes;
    struct stat identity {};
};
Archive readArchive(const std::filesystem::path& path)
{
    Descriptor input(::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK));
    if (input.get() < 0)
        filesystemFailure("Open source archive", errno);
    Archive archive;
    if (::fstat(input.get(), &archive.identity) != 0)
        filesystemFailure("Inspect source archive", errno);
    if (!S_ISREG(archive.identity.st_mode))
        throw ExportError(ExitCode::usage, "Source archive must be a regular file.");
    constexpr size_t maximumBytes = 256 * 1024 * 1024;
    if (archive.identity.st_size < 0 || static_cast<uintmax_t>(archive.identity.st_size) > maximumBytes)
        throw ExportError(ExitCode::native, "Source archive exceeds256MiB.");
    archive.bytes.reserve(static_cast<size_t>(archive.identity.st_size));
    std::array<uint8_t, 65536> buffer {};
    for (;;) {
        const auto count = ::read(input.get(), buffer.data(), buffer.size());
        if (count < 0) {
            if (errno == EINTR)
                continue;
            filesystemFailure("Read source archive", errno);
        }
        if (!count)
            break;
        const auto length = static_cast<size_t>(count);
        if (length > maximumBytes - archive.bytes.size())
            throw ExportError(ExitCode::native, "Growing source archive exceeds256MiB.");
        const auto required = archive.bytes.size() + length;
        if (required > archive.bytes.capacity())
            archive.bytes.reserve(std::max(required, std::min(maximumBytes, archive.bytes.capacity() * 2)));
        if (archive.bytes.capacity() > maximumBytes)
            throw ExportError(ExitCode::native, "Source archive capacity exceeds256MiB.");
        archive.bytes.insert(archive.bytes.end(), buffer.begin(), buffer.begin() + length);
    }
    return archive;
}
std::string destinationExtension(const std::filesystem::path& destination)
{
    auto extension = destination.extension().string();
    for (auto& letter : extension) {
        if (letter >= 'A' && letter <= 'Z')
            letter = static_cast<char>(letter - 'A' + 'a');
    }
    return extension;
}
void validateDestination(const std::filesystem::path& destination, const struct stat& source)
{
    const auto extension = destinationExtension(destination);
    if (extension != ".fbx" && extension != ".obj")
        throw ExportError(ExitCode::usage, "Destination must have the .fbx or .obj extension.");
    const auto parent = destination.has_parent_path() ? destination.parent_path() : std::filesystem::path(".");
    struct stat parentInfo {};
    if (::stat(parent.c_str(), &parentInfo) != 0)
        filesystemFailure("Inspect destination directory", errno);
    if (!S_ISDIR(parentInfo.st_mode))
        filesystemFailure("Destination parent is not a directory", ENOTDIR);
    struct stat target {};
    if (::lstat(destination.c_str(), &target) == 0) {
        if (!S_ISREG(target.st_mode) || (target.st_dev == source.st_dev && target.st_ino == source.st_ino))
            throw ExportError(ExitCode::usage, "Destination must be a regular file distinct from the source; symlinks are not accepted.");
    } else if (errno != ENOENT) {
        filesystemFailure("Inspect destination", errno);
    }
}
class TemporaryOutput {
public:
    TemporaryOutput(std::string path, int descriptor) noexcept
        : descriptor(descriptor)
        , m_path(std::move(path))
    {
    }
    ~TemporaryOutput()
    {
        if (!m_committed && ::unlink(m_path.c_str()) != 0 && errno != ENOENT)
            std::fprintf(stderr, "Temporary output cleanup failed (errno %d)\n", errno);
    }
    TemporaryOutput(const TemporaryOutput&) = delete;
    TemporaryOutput& operator=(const TemporaryOutput&) = delete;
    void commit(const std::filesystem::path& destination)
    {
        descriptor.closeForCommit();
        commitUnlessCancelled(isCancelled, [&] {
            if (::rename(m_path.c_str(), destination.c_str()) != 0)
                filesystemFailure("Commit staged output", errno);
        });
        m_committed = true;
    }
    Descriptor descriptor;

private:
    std::string m_path;
    bool m_committed = false;
};
struct stat publish(const std::filesystem::path& destination, const struct stat& source, const uint8_t* bytes, size_t count)
{
    validateDestination(destination, source);
    const auto parent = destination.has_parent_path() ? destination.parent_path() : std::filesystem::path(".");
    auto pattern = (parent / ".dust3d-export-XXXXXX").string();
    const auto descriptor = ::mkstemp(pattern.data());
    if (descriptor < 0)
        filesystemFailure("Create staged output", errno);
    TemporaryOutput output(std::move(pattern), descriptor);
    if (::fcntl(descriptor, F_SETFD, FD_CLOEXEC) != 0)
        filesystemFailure("Protect staged descriptor", errno);
    for (size_t offset = 0; offset < count;) {
        checkCancellation();
        const auto written = ::write(descriptor, bytes + offset, std::min(count - offset, size_t { 65536 }));
        if (written < 0) {
            if (errno == EINTR)
                continue;
            filesystemFailure("Write staged output", errno);
        }
        if (!written)
            filesystemFailure("Write staged output made no progress", EIO);
        offset += static_cast<size_t>(written);
    }
    for (;;) {
        checkCancellation();
        if (::fsync(descriptor) == 0)
            break;
        if (errno != EINTR)
            filesystemFailure("Flush staged output", errno);
    }
    struct stat published {};
    if (::fstat(descriptor, &published) != 0)
        filesystemFailure("Inspect staged output", errno);
    validateDestination(destination, source);
    output.commit(destination);
    return published;
}
using dust3d::export_cli::publishWithReceipt;
using dust3d::export_cli::ReceiptDeliveryState;
using dust3d::export_cli::serializeReceipt;

struct ReceiptOutput {
    int descriptor = -1;
    size_t jobIndex = 1;
    size_t jobCount = 1;
    bool deliveryEnabled = true;

    std::string prepare(const Arguments& job, bool committed, size_t bytes, const char* diagnostic) const
    {
        if (descriptor < 0)
            return {};
        const auto extension = destinationExtension(job.destination);
        const auto format = extension == ".obj" ? "obj" : extension == ".fbx" ? "fbx" : "unknown";
        return serializeReceipt({ jobIndex, jobCount, job.source.string(), job.destination.string(),
            format, job.baked ? "baked" : "geometry", committed, bytes, diagnostic });
    }

    bool deliver(const std::string& record) const noexcept
    {
        if (!deliveryEnabled)
            return false;
        size_t offset = 0;
        while (offset < record.size()) {
            const auto written = ::write(descriptor, record.data() + offset, record.size() - offset);
            if (written < 0 && errno == EINTR)
                continue;
            if (written <= 0) {
                std::fprintf(stderr, "Job %zu/%zu receipt delivery failed (errno %d)\n",
                    jobIndex, jobCount, written == 0 ? EIO : errno);
                return false;
            }
            offset += static_cast<size_t>(written);
        }
        return true;
    }

    bool deliverFailure(const Arguments& job, const char* diagnostic) const noexcept
    {
        if (descriptor < 0)
            return true;
        try {
            return deliver(prepare(job, false, 0, diagnostic));
        } catch (const std::exception& cause) {
            std::fprintf(stderr, "Job %zu/%zu receipt preparation failed: %s\n",
                jobIndex, jobCount, cause.what());
            return false;
        }
    }
};

struct ExportResult {
    struct stat identity;
    bool receiptDelivered;
};

ExportResult publishExport(const Arguments& job, const struct stat& source, const uint8_t* bytes,
    size_t count, const char* warning, const ReceiptOutput& receipt)
{
    const auto record = receipt.prepare(job, true, count, warning);
    const auto result = publishWithReceipt(record, [&] { return publish(job.destination, source, bytes, count); },
        [&](const std::string& line) { return receipt.deliver(line); });
    if (receipt.descriptor < 0)
        std::printf("Exported %zu bytes\n", count);
    if (*warning)
        std::fprintf(stderr, "Source note: %s\n", warning);
    return { result.publication, result.delivered };
}

ExportResult exportFile(const Arguments& arguments, const ReceiptOutput& receipt)
{
    checkCancellation();
    const auto archive = readArchive(arguments.source);
    validateDestination(arguments.destination, archive.identity);
    if (destinationExtension(arguments.destination) == ".obj") {
        if (arguments.baked)
            throw ExportError(ExitCode::usage, "OBJ exports original geometry only; --baked requires FBX.");
        Dust3DObjExportOptions options {};
        options.struct_size = sizeof(options);
        options.cancellation = &cancellationToken;
        options.maximum_output_payload_bytes = arguments.maximumOutputBytes;
        Dust3DError error {};
        std::unique_ptr<Dust3DObjExport, decltype(&dust3d_obj_export_destroy)> owner(
            dust3d_obj_export_load(archive.bytes.data(), archive.bytes.size(), &options, &error), dust3d_obj_export_destroy);
        if (!owner && error.status == DUST3D_STATUS_CANCELLED)
            throw ExportCancelled();
        if (!owner)
            throw ExportError(ExitCode::native, "Native export status " + std::to_string(error.status) + ": " + error.message);
        return publishExport(arguments, archive.identity, dust3d_obj_export_bytes(owner.get()),
            dust3d_obj_export_byte_count(owner.get()), dust3d_obj_export_warning(owner.get()), receipt);
    }
    Dust3DFbxExportOptions options {};
    options.struct_size = sizeof(options);
    options.cancellation = &cancellationToken;
    options.mode = arguments.baked ? DUST3D_FBX_EXPORT_BAKED : DUST3D_FBX_EXPORT_GEOMETRY;
    options.maximum_output_payload_bytes = arguments.maximumOutputBytes;
    options.bake.struct_size = sizeof(options.bake);
    options.bake.atlas_texture_size = options.bake.color_size = options.bake.normal_size = options.bake.material_size = arguments.textureSize;
    Dust3DError error {};
    std::unique_ptr<Dust3DFbxExport, decltype(&dust3d_fbx_export_destroy)> owner(
        dust3d_fbx_export_load(archive.bytes.data(), archive.bytes.size(), &options, &error), dust3d_fbx_export_destroy);
    if (!owner && error.status == DUST3D_STATUS_CANCELLED)
        throw ExportCancelled();
    if (!owner)
        throw ExportError(ExitCode::native, "Native export status " + std::to_string(error.status) + ": " + error.message);
    return publishExport(arguments, archive.identity, dust3d_fbx_export_bytes(owner.get()),
        dust3d_fbx_export_byte_count(owner.get()), dust3d_fbx_export_warning(owner.get()), receipt);
}
struct PathIdentity {
    std::filesystem::path canonical;
    struct stat metadata {};
    bool exists = false;
};
PathIdentity pathIdentity(const std::filesystem::path& path)
{
    PathIdentity result;
    result.canonical = std::filesystem::weakly_canonical(path);
    if (::stat(path.c_str(), &result.metadata) == 0)
        result.exists = true;
    else if (errno != ENOENT && errno != ENOTDIR)
        filesystemFailure("Inspect batch path", errno);
    return result;
}
bool aliases(const PathIdentity& first, const PathIdentity& second)
{
    return first.canonical == second.canonical || (first.exists && second.exists && first.metadata.st_dev == second.metadata.st_dev && first.metadata.st_ino == second.metadata.st_ino);
}
std::vector<PathIdentity> preflightBatch(const std::vector<Arguments>& jobs)
{
    std::vector<PathIdentity> sources;
    std::vector<PathIdentity> destinations;
    for (const auto& job : jobs) {
        sources.push_back(pathIdentity(job.source));
        destinations.push_back(pathIdentity(job.destination));
    }
    // Protect every input and prevent competing output ownership before the first commit.
    for (size_t index = 0; index < destinations.size(); ++index) {
        for (const auto& source : sources) {
            if (aliases(destinations[index], source))
                throw ExportError(ExitCode::usage, "Batch output aliases an input archive.");
        }
        for (size_t previous = 0; previous < index; ++previous) {
            if (aliases(destinations[index], destinations[previous]))
                throw ExportError(ExitCode::usage, "Batch outputs must have distinct paths and identities.");
        }
    }
    return destinations;
}
int execute(const Invocation& invocation, int receiptDescriptor)
{
    ReceiptDeliveryState delivery;
    constexpr auto deliveryFailure = static_cast<int>(ExitCode::receiptDelivery);
    if (!invocation.isBatch) {
        const auto& job = invocation.jobs.front();
        const ReceiptOutput receipt { receiptDescriptor };
        try {
            const auto result = exportFile(job, receipt);
            delivery.record(result.receiptDelivered);
            return delivery.singleExitCode(0, deliveryFailure);
        } catch (const ExportCancelled& cause) {
            receipt.deliverFailure(job, cause.what());
            throw;
        } catch (const ExportError& cause) {
            delivery.record(receipt.deliverFailure(job, cause.what()));
            std::fprintf(stderr, "%s\n", cause.what());
            return delivery.singleExitCode(cause.exitCode(), deliveryFailure);
        } catch (const std::exception& cause) {
            delivery.record(receipt.deliverFailure(job, cause.what()));
            std::fprintf(stderr, "Export failed: %s\n", cause.what());
            return delivery.singleExitCode(static_cast<int>(ExitCode::filesystem), deliveryFailure);
        }
    }
    auto destinations = preflightBatch(invocation.jobs);
    // Allocate bookkeeping before publication so recording a committed inode cannot fail after rename.
    std::vector<bool> hasCommitted(invocation.jobs.size(), false);
    size_t failed = 0;
    for (size_t index = 0; index < invocation.jobs.size(); ++index) {
        const ReceiptOutput receipt { receiptDescriptor, index + 1, invocation.jobs.size(), delivery.enabled() };
        try {
            const auto current = pathIdentity(invocation.jobs[index].destination);
            for (size_t previous = 0; previous < index; ++previous) {
                if (hasCommitted[previous] && aliases(current, destinations[previous]))
                    throw ExportError(ExitCode::usage, "Batch destination aliases a previously committed output.");
            }
            const auto published = exportFile(invocation.jobs[index], receipt);
            delivery.record(published.receiptDelivered);
            destinations[index].metadata = published.identity;
            destinations[index].exists = true;
            hasCommitted[index] = true;
            if (receiptDescriptor < 0)
                std::printf("Job %zu/%zu committed\n", index + 1, invocation.jobs.size());
        } catch (const ExportCancelled& cause) {
            receipt.deliverFailure(invocation.jobs[index], cause.what());
            throw;
        } catch (const ExportError& cause) {
            ++failed;
            delivery.record(receipt.deliverFailure(invocation.jobs[index], cause.what()));
            std::fprintf(stderr, "Job %zu/%zu failed (exit %d): %s\n", index + 1, invocation.jobs.size(), cause.exitCode(), cause.what());
        } catch (const std::exception& cause) {
            ++failed;
            delivery.record(receipt.deliverFailure(invocation.jobs[index], cause.what()));
            std::fprintf(stderr, "Job %zu/%zu failed (exit 4): %s\n", index + 1, invocation.jobs.size(), cause.what());
        }
    }
    std::fprintf(stderr, "Batch: %zu committed, %zu failed\n", invocation.jobs.size() - failed, failed);
    const auto exportExit = failed ? (invocation.usesLegacyOutputs ? 1 : static_cast<int>(ExitCode::partialBatch)) : 0;
    return delivery.batchExitCode(exportExit, deliveryFailure);
}

}
int main(int argc, char** argv)
{
    try {
        if (argc == 2 && std::string(argv[1]) == "--help") {
            std::cout << "dust3d-export [--json] [--batch] [--baked] [--texture-size 1...8192] [--max-output-bytes N] [--] input.ds3 output.fbx|output.obj [input.ds3 output...]\nLegacy: dust3d-export input.ds3 -o output.obj [-output output.fbx ...]\n";
            return 0;
        }
        const auto invocation = parseArguments(argc, argv);
        installCancellationHandler();
        if (!invocation.jsonReceipts)
            return execute(invocation, -1);
        Descriptor receiptDescriptor(::fcntl(STDOUT_FILENO, F_DUPFD_CLOEXEC, STDERR_FILENO + 1));
        if (receiptDescriptor.get() < 0 || ::dup2(STDERR_FILENO, STDOUT_FILENO) < 0)
            filesystemFailure("Configure receipt output", errno);
        if (std::signal(SIGPIPE, SIG_IGN) == SIG_ERR)
            filesystemFailure("Configure receipt delivery", errno);
        return execute(invocation, receiptDescriptor.get());
    } catch (const ExportCancelled& cause) {
        std::fprintf(stderr, "%s\n", cause.what());
        return static_cast<int>(ExitCode::cancelled);
    } catch (const ExportError& cause) {
        std::fprintf(stderr, "%s\n", cause.what());
        return cause.exitCode();
    } catch (const std::exception& cause) {
        std::fprintf(stderr, "Export failed: %s\n", cause.what());
        return static_cast<int>(ExitCode::filesystem);
    }
}

#include <Dust3DBridge.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <dust3d/base/snapshot.h>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <pthread.h>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using MeshOwner = std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)>;
using SessionOwner = std::unique_ptr<Dust3DGenerationSession, decltype(&dust3d_generation_session_destroy)>;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

SessionOwner session(size_t budget = DUST3D_DEFAULT_CACHE_PAYLOAD_BYTES)
{
    Dust3DError error {};
    SessionOwner result(dust3d_generation_session_create(budget, &error), dust3d_generation_session_destroy);
    require(result && error.status == DUST3D_STATUS_OK, std::string("Cannot create session: ") + error.message);
    return result;
}

Dust3DGenerationStats stats(const Dust3DGenerationSession* owner)
{
    Dust3DGenerationStats result {};
    Dust3DError error {};
    require(dust3d_generation_session_get_stats(owner, &result, sizeof(result), &error) == DUST3D_STATUS_OK,
        std::string("Cannot read session stats: ") + error.message);
    require(error.status == DUST3D_STATUS_OK, "Successful stats must clear stale errors");
    return result;
}

std::vector<uint8_t> fixture(const std::string& directory, const std::string& name)
{
    std::ifstream file(directory + "/" + name + ".ds3", std::ios::binary);
    require(file.good(), "Cannot read generation fixture " + name);
    return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
}

MeshOwner load(Dust3DGenerationSession* owner, const std::vector<uint8_t>& bytes)
{
    Dust3DError error {};
    MeshOwner mesh(dust3d_generation_session_load(owner, bytes.data(), bytes.size(), &error), dust3d_mesh_destroy);
    require(mesh && error.status == DUST3D_STATUS_OK, std::string("Session generation failed: ") + error.message);
    return mesh;
}

using Triangle = std::array<int64_t, 30>;
std::multiset<Triangle> triangles(const Dust3DMesh* mesh)
{
    const auto count = dust3d_mesh_vertex_count(mesh);
    const auto* vertices = dust3d_mesh_vertices(mesh);
    require(vertices && count && count % 3 == 0, "Expected complete nonempty triangles");
    std::multiset<Triangle> result;
    for (size_t first = 0; first < count; first += 3) {
        std::array<Triangle, 3> rotations {};
        for (size_t rotation = 0; rotation < 3; ++rotation) {
            for (size_t corner = 0; corner < 3; ++corner) {
                const auto& vertex = vertices[first + (rotation + corner) % 3];
                const std::array<float, 10> attributes { vertex.position[0], vertex.position[1], vertex.position[2],
                    vertex.normal[0], vertex.normal[1], vertex.normal[2], vertex.color[0], vertex.color[1], vertex.color[2], vertex.color[3] };
                for (size_t attribute = 0; attribute < attributes.size(); ++attribute) {
                    require(std::isfinite(attributes[attribute]), "Generated attributes must be finite");
                    rotations[rotation][corner * 10 + attribute] = std::llround(double(attributes[attribute]) * 100000.0);
                }
            }
        }
        result.insert(*std::min_element(rotations.begin(), rotations.end()));
    }
    return result;
}

void requireFreshEquivalent(const Dust3DMesh* cached, const std::vector<uint8_t>& bytes)
{
    Dust3DError error {};
    MeshOwner fresh(dust3d_mesh_load(bytes.data(), bytes.size(), &error), dust3d_mesh_destroy);
    require(fresh && error.status == DUST3D_STATUS_OK, "Fresh reference generation must succeed");
    require(triangles(cached) == triangles(fresh.get()), "Cached generation must preserve oriented full-attribute triangles within 1e-5 buckets");
    require(dust3d_mesh_node_count(cached) == dust3d_mesh_node_count(fresh.get())
            && dust3d_mesh_part_count(cached) == dust3d_mesh_part_count(fresh.get()),
        "Cached source counts must match fresh generation");
    require(std::string(dust3d_mesh_warning(cached)) == dust3d_mesh_warning(fresh.get()), "Cached compatibility warnings must match fresh input");
}

void boundaries()
{
    Dust3DError error {};
    require(!dust3d_generation_session_create(std::numeric_limits<size_t>::max(), &error)
            && error.status == DUST3D_STATUS_LIMIT_EXCEEDED,
        "Unrepresentable/unsupported cache budgets must fail before allocation");
    require(!dust3d_generation_session_load(nullptr, nullptr, 0, &error)
            && error.status == DUST3D_STATUS_INVALID_DOCUMENT,
        "Null session loads must report invalid input");
    require(dust3d_generation_session_reset(nullptr, &error) == DUST3D_STATUS_INVALID_DOCUMENT,
        "Null resets must fail safely");
    auto owner = session();
    const auto initial = stats(owner.get());
    require(initial.maximum_cache_payload_bytes == DUST3D_DEFAULT_CACHE_PAYLOAD_BYTES
            && initial.retained_payload_bytes == 0 && initial.cache_outcome == DUST3D_CACHE_EMPTY,
        "New sessions start empty with the requested budget");
    alignas(Dust3DGenerationStats) std::array<uint8_t, sizeof(Dust3DGenerationStats)> output;
    output.fill(0xa5);
    require(dust3d_generation_session_get_stats(owner.get(), reinterpret_cast<Dust3DGenerationStats*>(output.data()), output.size() - 1, &error)
            == DUST3D_STATUS_LIMIT_EXCEEDED,
        "A short output buffer must fail before writing");
    require(std::all_of(output.begin(), output.end(), [](uint8_t value) { return value == 0xa5; }), "Short stats buffers must remain untouched");
    require(dust3d_generation_session_get_stats(owner.get(), nullptr, 0, &error) == DUST3D_STATUS_INVALID_DOCUMENT,
        "Null stats outputs must fail safely");
    dust3d_generation_session_destroy(nullptr);
}

void reuseAndIndependentOwnership(const std::string& directory)
{
    const auto bytes = fixture(directory, "Capsule");
    const auto original = bytes;
    auto owner = session();
    const auto first = load(owner.get(), bytes);
    const auto retained = stats(owner.get());
    require(retained.cache_outcome == DUST3D_CACHE_RETAINED && retained.retained_payload_bytes > 0
            && retained.retained_payload_bytes <= retained.maximum_cache_payload_bytes,
        "Successful generation must retain only bounded owned state");
    require(retained.rebuilt_components > 0 && retained.reused_components == 0, "Cold sessions must report actual component rebuilding");
    const auto second = load(owner.get(), bytes);
    require(stats(owner.get()).reused_components > 0, "Repeated input must actually reuse component geometry");
    requireFreshEquivalent(second.get(), bytes);
    require(triangles(first.get()) == triangles(second.get()), "Later generations cannot mutate earlier mesh owners");
    require(bytes == original, "Session preparation cannot inject dirty flags into source archive bytes");
    require(dust3d_generation_session_reset(owner.get(), nullptr) == DUST3D_STATUS_OK, "Reset accepts a null error output");
    require(stats(owner.get()).retained_payload_bytes == 0, "Reset releases all retained cache payload");
    owner.reset();
    requireFreshEquivalent(first.get(), bytes);
}

std::string model(const std::vector<uint8_t>& bytes)
{
    const std::string text(bytes.begin(), bytes.end());
    const auto lineEnd = text.find('\n');
    const auto space = text.rfind(' ', lineEnd);
    const auto offset = std::stoul(text.substr(space + 1, lineEnd - space - 1));
    return text.substr(offset);
}

std::vector<uint8_t> archive(const std::string& xml, const std::string& payload = "", const std::string& attribute = "", bool assetFirst = false, const std::string& assetType = "asset")
{
    const std::string prefix = "DUST3D 1.0 xml ";
    const std::string modelItem = "<model name='model.xml' offset='0' size='" + std::to_string(xml.size()) + "'/>";
    const std::string asset = payload.empty() ? "" : "<" + assetType + " name='images/test.bin' offset='" + std::to_string(xml.size()) + "' size='" + std::to_string(payload.size()) + "' " + attribute + "/>";
    const std::string header = "<ds3>" + (assetFirst ? asset + modelItem : modelItem + asset) + "</ds3>";
    std::ostringstream stream;
    stream << prefix << std::setfill('0') << std::setw(10) << prefix.size() + 12 + header.size() << "\r\n"
           << header << xml << payload;
    const auto result = stream.str();
    return { result.begin(), result.end() };
}

std::string change(std::string xml, const std::string& tag, const std::string& key, const std::string& value)
{
    const auto start = xml.find("<" + tag + " ");
    require(start != std::string::npos, "Missing synthetic edit tag");
    const auto end = xml.find('>', start);
    const auto attribute = xml.find(key + "=", start);
    if (attribute < end) {
        const auto quote = attribute + key.size() + 1;
        const auto finish = xml.find(xml[quote], quote + 1);
        xml.replace(quote + 1, finish - quote - 1, value);
    } else {
        xml.insert(start + tag.size() + 1, " " + key + "='" + value + "'");
    }
    return xml;
}

void editCorpus(const std::string& directory)
{
    for (const std::string name : { "Capsule", "Sword", "RoundedCrossing", "RoundedCrossingHard", "RoundedCrossingTranslated", "RoundedCrossingDiff", "SeparatedNearWeld" }) {
        auto owner = session();
        const auto bytes = fixture(directory, name);
        const auto xml = model(bytes);
        load(owner.get(), bytes);
        requireFreshEquivalent(load(owner.get(), bytes).get(), bytes);
        const std::vector<std::string> edits {
            change(xml, "node", "radius", "0.19"), change(xml, "node", "x", "0.63"),
            change(xml, "part", "deformThickness", "0.7"), change(xml, "part", "deformWidth", "0.8"),
            change(xml, "part", "xMirrored", "true"), change(xml, "part", "rounded", "false"),
            change(xml, "component", "color", "#cc6633"), change(xml, "component", "smoothCutoffDegrees", "15"),
            change(xml, "component", "__dirty", "false"), change(xml, "part", "__dirty", "false")
        };
        for (size_t index = 0; index < edits.size(); ++index) {
            const auto edited = archive(edits[index]);
            Dust3DError freshError {}, cachedError {};
            MeshOwner fresh(dust3d_mesh_load(edited.data(), edited.size(), &freshError), dust3d_mesh_destroy);
            MeshOwner cached(dust3d_generation_session_load(owner.get(), edited.data(), edited.size(), &cachedError), dust3d_mesh_destroy);
            const auto context = name + " edit " + std::to_string(index);
            require(freshError.status == cachedError.status && bool(fresh) == bool(cached), context + ": cached/fresh status mismatch");
            if (cached) {
                require(triangles(cached.get()) == triangles(fresh.get()), context + ": cached/fresh attributes differ");
            } else if (cachedError.status != DUST3D_STATUS_NO_GEOMETRY) {
                require(stats(owner.get()).cache_outcome == DUST3D_CACHE_RESET_AFTER_FAILURE
                        && stats(owner.get()).retained_payload_bytes == 0,
                    context + ": genuine failure retained cache state");
            }
            requireFreshEquivalent(load(owner.get(), bytes).get(), bytes);
        }
    }
}

void assetRevisions(const std::string& directory)
{
    auto owner = session();
    const auto xml = model(fixture(directory, "Capsule"));
    const auto first = archive(xml, "opaque", "revision='one'");
    load(owner.get(), first);
    const auto reordered = archive(xml, "opaque", "revision='one'", true);
    requireFreshEquivalent(load(owner.get(), reordered).get(), reordered);
    require(stats(owner.get()).reused_components > 0, "Archive item order alone must not invalidate identical assets");
    const auto shifted = archive(xml + "\n", "opaque", "revision='one'");
    load(owner.get(), shifted);
    require(stats(owner.get()).reused_components > 0, "Storage offset shifts alone must not invalidate identical assets");
    for (const auto& bytes : { archive(xml, "OPAQUE", "revision='one'"), archive(xml, "OPAQUE", "revision='two'"), archive(xml), first }) {
        requireFreshEquivalent(load(owner.get(), bytes).get(), bytes);
        require(stats(owner.get()).reused_components == 0, "Changed/added/removed assets and unknown attributes must flush caches");
    }
    auto small = session(1024);
    const auto large = archive(xml, std::string(2048, 'x'));
    requireFreshEquivalent(load(small.get(), large).get(), large);
    const auto state = stats(small.get());
    require(state.cache_outcome == DUST3D_CACHE_BYPASSED && state.retained_payload_bytes == 0 && state.cache_message[0],
        "Oversized assets must bypass retention while preserving the valid mesh");
}

void linkedProfilesAndSiblingReuse(const std::string& directory)
{
    const auto original = fixture(directory, "Sword");
    auto owner = session();
    const auto xml = model(original);
    auto changedProfile = xml;
    const auto profileCoordinate = changedProfile.find("x=\"0.378000\"");
    require(profileCoordinate != std::string::npos, "Sword profile provenance changed; audit the dependency edit");
    changedProfile.replace(profileCoordinate, std::string("x=\"0.378000\"").size(), "x=\"0.388000\"");
    const auto revised = archive(changedProfile, "same-owned-image");
    load(owner.get(), archive(xml, "same-owned-image"));
    requireFreshEquivalent(load(owner.get(), revised).get(), revised);
    require(stats(owner.get()).reused_components > 0 && stats(owner.get()).rebuilt_components > 0,
        "A linked profile edit must rebuild its consumer while reusing unrelated siblings and unchanged assets");
    const auto nodeLink = archive(change(xml, "node", "cutFace", "{85d6d957-b271-5202-b069-642e420b2a6c}"));
    requireFreshEquivalent(load(owner.get(), nodeLink).get(), nodeLink);
    requireFreshEquivalent(load(owner.get(), original).get(), original);
    auto reordered = xml;
    const auto first = reordered.find("<component ");
    const auto middle = reordered.find("</component>", first) + std::string("</component>").size();
    const auto last = reordered.find("</component>", middle) + std::string("</component>").size();
    const auto firstComponent = reordered.substr(first, middle - first);
    reordered.replace(first, last - first, reordered.substr(middle, last - middle) + firstComponent);
    const auto reorderedBytes = archive(reordered);
    requireFreshEquivalent(load(owner.get(), reorderedBytes).get(), reorderedBytes);
    requireFreshEquivalent(load(owner.get(), original).get(), original);
}

void profileAndWrappingEdits(const std::string& directory)
{
    auto owner = session();
    const auto xml = model(fixture(directory, "Capsule"));
    std::string grouped = xml;
    grouped.insert(grouped.find("<components>") + std::string("<components>").size(),
        "<component id='{cccccccc-cccc-4ccc-8ccc-cccccccccccc}'>");
    grouped.insert(grouped.find("</components>"), "</component>");
    grouped = change(grouped, "component", "wrapFaces", "64");
    std::vector<std::string> revisions {
        xml, change(xml, "part", "cutFace", "Quad"), change(xml, "node", "cutFace", "Triangle"),
        grouped, change(grouped, "component", "color", "#33cc66"),
        change(grouped, "component", "wrap", "Skin"), change(grouped, "component", "wrap", "Cloth")
    };
    auto cloth = change(grouped, "component", "wrap", "Cloth");
    cloth = change(cloth, "component", "wrapFaces", "64");
    revisions.push_back(cloth);
    revisions.push_back(change(cloth, "component", "wrapOffset", "0.02"));
    revisions.push_back(change(cloth, "component", "wrapOffset", "-1"));
    revisions.push_back(change(cloth, "component", "wrapOpenTop", "0.1"));
    revisions.push_back(change(cloth, "component", "wrapBindTo", "{bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb}"));
    // Reparent, remove the group, restore, and revisit previously cached wrapping states.
    revisions.push_back(xml);
    revisions.push_back(grouped);
    revisions.push_back(cloth);
    size_t successfulWrapping = 0;
    for (size_t index = 0; index < revisions.size(); ++index) {
        const auto bytes = archive(revisions[index]);
        for (size_t repeat = 0; repeat < 2; ++repeat) {
            Dust3DError freshError {}, cachedError {};
            MeshOwner fresh(dust3d_mesh_load(bytes.data(), bytes.size(), &freshError), dust3d_mesh_destroy);
            MeshOwner cached(dust3d_generation_session_load(owner.get(), bytes.data(), bytes.size(), &cachedError), dust3d_mesh_destroy);
            const auto context = "Profile/wrap revision " + std::to_string(index);
            require(freshError.status == cachedError.status && bool(fresh) == bool(cached), context + ": status differs");
            if (cached) {
                require(triangles(cached.get()) == triangles(fresh.get()), context + ": oriented attributes differ");
                if (index >= 5)
                    ++successfulWrapping;
            } else if (cachedError.status != DUST3D_STATUS_NO_GEOMETRY) {
                require(stats(owner.get()).cache_outcome == DUST3D_CACHE_RESET_AFTER_FAILURE, context + ": failure did not reset state");
            }
        }
    }
    require(successfulWrapping >= 6, "The wrapping corpus must exercise real successful cached generation");
}

void successfulEmpty(const std::string& directory)
{
    auto owner = session();
    auto xml = model(fixture(directory, "Capsule"));
    // A hidden source has no mesh contribution, while its source document remains valid.
    xml = change(xml, "part", "disabled", "true");
    const auto bytes = archive(xml);
    Dust3DError freshError {}, cachedError {};
    MeshOwner fresh(dust3d_mesh_load(bytes.data(), bytes.size(), &freshError), dust3d_mesh_destroy);
    MeshOwner cached(dust3d_generation_session_load(owner.get(), bytes.data(), bytes.size(), &cachedError), dust3d_mesh_destroy);
    require(!fresh && !cached && freshError.status == DUST3D_STATUS_NO_GEOMETRY && cachedError.status == freshError.status,
        "A valid empty source must preserve NO_GEOMETRY semantics");
    require(stats(owner.get()).cache_outcome == DUST3D_CACHE_RETAINED, "Successful empty results may retain bounded semantic state");
    cached.reset(dust3d_generation_session_load(owner.get(), bytes.data(), bytes.size(), &cachedError));
    require(!cached && cachedError.status == DUST3D_STATUS_NO_GEOMETRY && stats(owner.get()).cache_outcome == DUST3D_CACHE_RETAINED,
        "Repeated empty results cannot be treated as failed generations");
    const auto restored = fixture(directory, "Capsule");
    requireFreshEquivalent(load(owner.get(), restored).get(), restored);
}

class StackWorker {
public:
    explicit StackWorker(std::function<void()> operation)
    {
        auto task = std::make_unique<std::packaged_task<void()>>(std::move(operation));
        result = task->get_future();
        pthread_attr_t attributes;
        require(pthread_attr_init(&attributes) == 0, "Cannot initialize test worker attributes");
        // Apple Clang's unoptimized ASan+UBSan instrumentation expands the inherited
        // triangle-intersection frame from 336 bytes to over 3 MiB. Keep the real
        // concurrent boolean workload and instrumentation, with an explicit test stack.
        const int stackStatus = pthread_attr_setstacksize(&attributes, 16 * 1024 * 1024);
        const int createStatus = stackStatus ? stackStatus : pthread_create(&thread, &attributes, [](void* raw) -> void* {
            std::unique_ptr<std::packaged_task<void()>> owned(static_cast<std::packaged_task<void()>*>(raw));
            (*owned)();
            return nullptr; }, task.get());
        pthread_attr_destroy(&attributes);
        require(createStatus == 0, "Cannot create test worker with its required sanitizer stack");
        task.release();
    }
    ~StackWorker() { pthread_join(thread, nullptr); }
    StackWorker(const StackWorker&) = delete;
    StackWorker& operator=(const StackWorker&) = delete;
    std::future<void> result;

private:
    pthread_t thread {};
};

void concurrentCalls(const std::string& directory)
{
    const auto capsule = fixture(directory, "Capsule");
    const auto sword = fixture(directory, "Sword");
    auto shared = session();
    auto independent = session();
    std::vector<std::unique_ptr<StackWorker>> calls;
    for (size_t index = 0; index < 6; ++index) {
        calls.push_back(std::make_unique<StackWorker>([&, index] {
            const auto& bytes = index % 2 ? capsule : sword;
            for (size_t iteration = 0; iteration < 3; ++iteration) {
                if (index % 3 == 0) {
                    Dust3DError error {};
                    MeshOwner mesh(dust3d_mesh_load(bytes.data(), bytes.size(), &error), dust3d_mesh_destroy);
                    require(mesh && error.status == DUST3D_STATUS_OK, "Concurrent legacy generation failed");
                    requireFreshEquivalent(mesh.get(), bytes);
                } else if (index % 3 == 1) {
                    const auto mesh = load(shared.get(), bytes);
                    requireFreshEquivalent(mesh.get(), bytes);
                } else {
                    Dust3DError error {};
                    Dust3DSourceOptions options { sizeof(options), 0 };
                    std::unique_ptr<Dust3DSourcedMesh, decltype(&dust3d_sourced_mesh_destroy)> mesh(
                        dust3d_generation_session_load_sourced(index % 2 ? shared.get() : independent.get(), bytes.data(), bytes.size(), &options, &error), dust3d_sourced_mesh_destroy);
                    require(mesh && error.status == DUST3D_STATUS_OK, "Concurrent cached source generation failed");
                    requireFreshEquivalent(dust3d_sourced_mesh_geometry(mesh.get()), bytes);
                    require(dust3d_sourced_mesh_corner_count(mesh.get()) == dust3d_mesh_vertex_count(dust3d_sourced_mesh_geometry(mesh.get())), "Concurrent source hints stay aligned");
                }
            }
        }));
    }
    for (auto& call : calls)
        call->result.get();
}

void limitsAndFailureRecovery(const std::string& directory)
{
    const auto bytes = fixture(directory, "Capsule");
    for (const size_t budget : { size_t(0), size_t(1), size_t(16384) }) {
        auto owner = session(budget);
        const auto first = load(owner.get(), bytes);
        const auto second = load(owner.get(), bytes);
        const auto state = stats(owner.get());
        requireFreshEquivalent(second.get(), bytes);
        require(state.retained_payload_bytes == 0 && state.reused_components == 0, "Disabled/insufficient budgets cannot retain state");
        require(state.cache_outcome == (budget ? DUST3D_CACHE_BYPASSED : DUST3D_CACHE_DISABLED), "Budget outcomes must be explicit");
        if (budget)
            require(state.cache_status == DUST3D_STATUS_LIMIT_EXCEEDED && state.cache_message[0], "Cache-only rejection must surface its cause without failing generation");
    }
    auto owner = session();
    const auto previous = load(owner.get(), bytes);
    const std::vector<uint8_t> invalid { 1, 2, 3 };
    Dust3DError error {};
    require(!dust3d_generation_session_load(owner.get(), invalid.data(), invalid.size(), &error)
            && error.status == DUST3D_STATUS_INVALID_DOCUMENT && error.message[0],
        "Malformed input must preserve its original primary failure");
    const auto failed = stats(owner.get());
    require(failed.retained_payload_bytes == 0 && failed.cache_outcome == DUST3D_CACHE_RESET_AFTER_FAILURE,
        "Failed loads cannot leave partial cache state");
    const auto recovered = load(owner.get(), bytes);
    require(stats(owner.get()).reused_components == 0, "Recovery must start from a fresh cache");
    requireFreshEquivalent(recovered.get(), bytes);
    requireFreshEquivalent(previous.get(), bytes);
}

void sourcedSessions(const std::string& directory)
{
    using SourceOwner = std::unique_ptr<Dust3DSourcedMesh, decltype(&dust3d_sourced_mesh_destroy)>;
    const auto bytes = fixture(directory, "Capsule");
    Dust3DError error {};
    Dust3DSourceOptions options { sizeof(options), 0 };
    auto owner = session();
    auto sourced = [&](const std::vector<uint8_t>& input, size_t budget = 0) {
        Dust3DSourceOptions request { sizeof(request), budget };
        return SourceOwner(dust3d_generation_session_load_sourced(owner.get(), input.data(), input.size(), &request, &error), dust3d_sourced_mesh_destroy);
    };
    auto first = sourced(bytes);
    require(first && stats(owner.get()).reused_components == 0, "First sourced session must generate fresh");
    auto second = sourced(bytes);
    require(second && stats(owner.get()).reused_components > 0, "Source output must retain actual component reuse");
    const auto* geometry = dust3d_sourced_mesh_geometry(first.get());
    const auto count = dust3d_mesh_vertex_count(geometry);
    require(count > 0 && count == dust3d_sourced_mesh_corner_count(first.get()), "Sourced cached geometry aligns with copied corners");
    const std::string originalKey = dust3d_sourced_mesh_node_id(first.get(), 0);
    const auto retained = stats(owner.get()).retained_payload_bytes;
    Dust3DSourceOptions invalid { 0, 0 };
    require(!dust3d_generation_session_load_sourced(owner.get(), bytes.data(), bytes.size(), &invalid, &error)
            && error.status == DUST3D_STATUS_INVALID_DOCUMENT,
        "Invalid source options reject before changing session state");
    require(stats(owner.get()).retained_payload_bytes == retained, "Invalid options preserve prior cache/statistics");
    require(sourced(bytes) && stats(owner.get()).reused_components > 0, "Rejected options cannot flush a valid cache");
    require(!sourced(bytes, 1) && error.status == DUST3D_STATUS_LIMIT_EXCEEDED && error.message[0], "Source budget failure preserves its primary cause");
    require(stats(owner.get()).cache_outcome == DUST3D_CACHE_RESET_AFTER_FAILURE && stats(owner.get()).retained_payload_bytes == 0, "Failed source export cannot retain partial cache");
    require(sourced(bytes) && stats(owner.get()).reused_components == 0, "Source budget recovery starts fresh");
    const auto xml = model(bytes);
    for (const auto& input : { archive(change(xml, "node", "radius", ".12")), bytes, archive(xml, "opaque"), archive(xml, "OPAQUE") }) {
        const auto current = sourced(input);
        require(current != nullptr, "Edited/restored/asset-revised source generation must succeed");
        requireFreshEquivalent(dust3d_sourced_mesh_geometry(current.get()), input);
        require(dust3d_sourced_mesh_corner_count(current.get()) == dust3d_mesh_vertex_count(dust3d_sourced_mesh_geometry(current.get())), "Edits cannot misalign source corners");
    }
    std::string unbraced = originalKey.substr(1, originalKey.size() - 2);
    auto renamed = xml;
    size_t start = 0;
    while ((start = renamed.find(originalKey, start)) != std::string::npos) {
        renamed.replace(start, originalKey.size(), unbraced);
        start += unbraced.size();
    }
    const auto aliased = sourced(archive(renamed));
    bool foundCurrentKey = false;
    for (size_t index = 0; index < dust3d_sourced_mesh_identity_count(aliased.get()); ++index)
        foundCurrentKey = foundCurrentKey || unbraced == dust3d_sourced_mesh_node_id(aliased.get(), index);
    require(aliased && foundCurrentKey, "Original identity export must use current document spelling, never retained output identities");
    const auto hidden = archive(change(xml, "part", "disabled", "true"));
    require(!sourced(hidden) && error.status == DUST3D_STATUS_NO_GEOMETRY && stats(owner.get()).cache_outcome == DUST3D_CACHE_RETAINED, "Sourced session preserves successful empty generation");
    require(dust3d_generation_session_reset(owner.get(), &error) == DUST3D_STATUS_OK, "Session reset must succeed");
    owner.reset();
    require(originalKey == dust3d_sourced_mesh_node_id(first.get(), 0) && count == dust3d_sourced_mesh_corner_count(first.get()), "Source owner survives session changes/reset/destruction");
    requireFreshEquivalent(geometry, bytes);

    for (const auto budget : { size_t { 0 }, size_t { 1 } }) {
        auto limited = session(budget);
        SourceOwner result(dust3d_generation_session_load_sourced(limited.get(), bytes.data(), bytes.size(), &options, &error), dust3d_sourced_mesh_destroy);
        require(result != nullptr, "Disabled/insufficient cache still returns source output");
        const auto state = stats(limited.get());
        require(state.retained_payload_bytes == 0 && state.cache_outcome == (budget ? DUST3D_CACHE_BYPASSED : DUST3D_CACHE_DISABLED), "Source budget and cache budget remain independent");
    }
    owner = session();
    auto grouped = xml;
    grouped.insert(grouped.find("<components>") + std::string("<components>").size(),
        "<component id='{cccccccc-cccc-4ccc-8ccc-cccccccccccc}' wrap='Cloth' wrapFaces='64'>");
    grouped.insert(grouped.find("</components>"), "</component>");
    const auto wrappedBytes = archive(grouped);
    const auto wrapped = sourced(wrappedBytes);
    const auto cachedWrap = sourced(wrappedBytes);
    require(wrapped && cachedWrap && stats(owner.get()).reused_components > 0, "Weighted wrapping source output must reuse component geometry");
    const auto* wrappedCorners = dust3d_sourced_mesh_corners(cachedWrap.get());
    bool hasMultipleWeights = false;
    for (size_t index = 0; index < dust3d_sourced_mesh_corner_count(cachedWrap.get()); ++index)
        hasMultipleWeights = hasMultipleWeights || (wrappedCorners[index].kind == DUST3D_SOURCE_WEIGHTED_HINT && wrappedCorners[index].influence_count > 1);
    require(hasMultipleWeights, "Cache hits preserve complete multi-node wrapping influences");
    requireFreshEquivalent(dust3d_sourced_mesh_geometry(cachedWrap.get()), wrappedBytes);
    require(!sourced(archive(change(xml, "node", "x", "abc"))) && error.status == DUST3D_STATUS_INVALID_DOCUMENT && error.message[0], "Cached source parser failures preserve invalid-document classification and cause");
    require(!dust3d_generation_session_load_sourced(nullptr, nullptr, 0, &options, nullptr), "Null session/diagnostic must be safe");
}
}

int main(int argc, char** argv)
{
    try {
        require(argc == 2, "A fixture directory is required");
        sourcedSessions(argv[1]);
        boundaries();
        reuseAndIndependentOwnership(argv[1]);
        limitsAndFailureRecovery(argv[1]);
        editCorpus(argv[1]);
        assetRevisions(argv[1]);
        linkedProfilesAndSiblingReuse(argv[1]);
        profileAndWrappingEdits(argv[1]);
        successfulEmpty(argv[1]);
        concurrentCalls(argv[1]);
        std::cout << "Generation session contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

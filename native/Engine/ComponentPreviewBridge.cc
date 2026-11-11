#include "Cancellation.h"
#include "ComponentPreviewOutput.h"
#include "ImportedModels.h"
#include <mutex>
#include <set>

namespace {
using namespace dust3d;
using namespace dust3d::native;

template <typename Result, typename Copy>
Result* loadComponentPreviews(const uint8_t* bytes, size_t size, const Dust3DComponentPreviewID* ids,
    size_t count, size_t maximumBytes, const Dust3DCancellationToken* cancellation, Dust3DError* error, Copy copy)
{
    setError(error, DUST3D_STATUS_OK, "");
    try {
        if (!ids || count == 0 || count > 256)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Request between 1 and 256 component previews.");
        const auto limit = componentPreviewOutputLimit(maximumBytes);
        checkCancellation(cancellation);
        std::lock_guard<std::mutex> lock(engineMutex());
        checkCancellation(cancellation);

        std::string warning;
        ImportedModels imports;
        auto snapshot = readDocument(bytes, size, warning, nullptr, &imports, true);
        validateComponentPreviewHierarchy(*snapshot);
        std::vector<ComponentPreviewRequest> requests;
        requests.reserve(count);
        std::set<Uuid> requestedIds;
        for (size_t index = 0; index < count; ++index) {
            checkCancellation(cancellation);
            const auto& input = ids[index];
            if (!input.bytes || (input.length != 36 && input.length != 38))
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "A complete authored component identity is required.");
            const std::string authoredId(input.bytes, input.length);
            if (authoredId.find('\0') != std::string::npos)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "The component identity contains NUL.");
            const auto id = resolveComponentPreviewId(*snapshot, authoredId);
            if (!requestedIds.insert(id).second)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "The component preview request contains a duplicate identity.");
            const auto& authored = snapshot->components.at(authoredId);
            const auto image = authored.find("colorImageId");
            if (image != authored.end() && image->second.size() > limit)
                throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "The component texture reference exceeds the output quota.");
            if (image != authored.end() && image->second.find('\0') != std::string::npos)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "The component texture reference contains NUL.");
            requests.push_back({ id, image == authored.end() ? std::string() : image->second });
        }

        MeshGenerator generator(snapshot.get());
        snapshot.release();
        generator.setImportedModelData(imports.takeGeometry());
        checkCancellation(cancellation);
        generator.generate();
        checkCancellation(cancellation);

        // Cut Faces and disabled parts can have previews without a combined mesh.
        return copy(generator.generatedComponentPreviews(), requests, warning, limit, cancellation).release();
    } catch (const DocumentError& exception) {
        setError(error, exception.status(), exception.what());
    } catch (const std::bad_alloc&) {
        setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, "Not enough memory to prepare component previews.");
    } catch (const std::exception& exception) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, exception.what());
    } catch (...) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, "An unknown error occurred while preparing component previews.");
    }
    return nullptr;
}
}

Dust3DComponentPreview* dust3d_component_preview_load(const uint8_t* bytes, size_t size,
    const char* component_id, size_t component_id_length, size_t maximum_output_bytes,
    const Dust3DCancellationToken* cancellation, Dust3DError* error)
{
    const Dust3DComponentPreviewID id { component_id, component_id_length };
    return loadComponentPreviews<Dust3DComponentPreview>(bytes, size, &id, 1, maximum_output_bytes, cancellation, error,
        [](const auto& previews, const auto& requests, const auto& warning, auto limit, auto* token) {
            const auto& request = requests.front();
            const auto preview = previews.find(request.id);
            if (preview == previews.end())
                throw DocumentError(DUST3D_STATUS_NO_GEOMETRY, "The component generated no preview geometry.");
            return copyComponentPreview(preview->second, request.colorImageId, warning, limit, token);
        });
}

Dust3DComponentPreviewBatch* dust3d_component_preview_batch_load(const uint8_t* bytes, size_t size,
    const Dust3DComponentPreviewID* ids, size_t count, size_t maximum_output_bytes,
    const Dust3DCancellationToken* cancellation, Dust3DError* error)
{
    return loadComponentPreviews<Dust3DComponentPreviewBatch>(
        bytes, size, ids, count, maximum_output_bytes, cancellation, error, copyComponentPreviewBatch);
}

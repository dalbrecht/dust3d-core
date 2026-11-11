#include "Cancellation.h"
#include "ComponentPreviewOutput.h"
#include "ImportedModels.h"
#include <mutex>

Dust3DComponentPreview* dust3d_component_preview_load(const uint8_t* bytes, size_t size,
    const char* component_id, size_t component_id_length, size_t maximum_output_bytes,
    const Dust3DCancellationToken* cancellation, Dust3DError* error)
{
    using namespace dust3d;
    using namespace dust3d::native;

    setError(error, DUST3D_STATUS_OK, "");
    try {
        if (!component_id || (component_id_length != 36 && component_id_length != 38))
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "A complete authored component identity is required.");
        const std::string authoredId(component_id, component_id_length);
        if (authoredId.find('\0') != std::string::npos)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "The component identity contains NUL.");
        const auto limit = componentPreviewOutputLimit(maximum_output_bytes);
        checkCancellation(cancellation);
        std::lock_guard<std::mutex> lock(engineMutex());
        checkCancellation(cancellation);

        std::string warning;
        ImportedModels imports;
        auto snapshot = readDocument(bytes, size, warning, nullptr, &imports, true);
        const auto id = resolveComponentPreviewId(*snapshot, authoredId);
        const auto& authored = snapshot->components.at(authoredId);
        const auto image = authored.find("colorImageId");
        if (image != authored.end() && image->second.size() > limit)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "The component texture reference exceeds the output quota.");
        if (image != authored.end() && image->second.find('\0') != std::string::npos)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "The component texture reference contains NUL.");
        const std::string colorImageId = image == authored.end() ? std::string() : image->second;

        MeshGenerator generator(snapshot.get());
        snapshot.release();
        generator.setImportedModelData(imports.takeGeometry());
        checkCancellation(cancellation);
        generator.generate();
        checkCancellation(cancellation);

        // A Cut Face or disabled part can have a preview with no combined mesh.
        const auto& previews = generator.generatedComponentPreviews();
        const auto preview = previews.find(id);
        if (preview == previews.end())
            throw DocumentError(DUST3D_STATUS_NO_GEOMETRY, "The component generated no preview geometry.");
        return copyComponentPreview(preview->second, colorImageId, warning, limit, cancellation).release();
    } catch (const DocumentError& exception) {
        setError(error, exception.status(), exception.what());
    } catch (const std::bad_alloc&) {
        setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, "Not enough memory to prepare this component preview.");
    } catch (const std::exception& exception) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, exception.what());
    } catch (...) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, "An unknown error occurred while preparing the component preview.");
    }
    return nullptr;
}

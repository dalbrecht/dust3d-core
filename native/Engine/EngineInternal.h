#ifndef DUST3D_NATIVE_ENGINE_INTERNAL_H_
#define DUST3D_NATIVE_ENGINE_INTERNAL_H_

#include "include/Dust3DBridge.h"
#include <dust3d/base/snapshot.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace dust3d {
class RetainedPayloadBudget;
}

struct Dust3DMesh {
    void accountDynamicPayload(dust3d::RetainedPayloadBudget& budget) const noexcept;
    std::vector<Dust3DVertex> vertices;
    size_t nodeCount = 0;
    size_t partCount = 0;
    std::string warning;
};

struct Dust3DIndexedMesh {
    void accountDynamicPayload(dust3d::RetainedPayloadBudget& budget) const noexcept;
    std::vector<Dust3DVertex> vertices;
    std::vector<uint32_t> indices;
    size_t nodeCount = 0;
    size_t partCount = 0;
    std::string warning;
};

namespace dust3d {
namespace native {

    class ImportedModels;

    class DocumentError : public std::runtime_error {
    public:
        DocumentError(Dust3DStatus status, const std::string& message)
            : std::runtime_error(message)
            , m_status(status)
        {
        }
        Dust3DStatus status() const { return m_status; }

    private:
        Dust3DStatus m_status;
    };

    void setError(Dust3DError* error, Dust3DStatus status, const char* message) noexcept;
    std::unique_ptr<Dust3DIndexedMesh> buildIndexedMesh(const Dust3DMesh& source, size_t maximumBytes);

    // Callbacks borrow validated archive spans only for the current read; implementations
    // must isolate their own allocation failures from primary document decoding.
    class DocumentAssetObserver {
    public:
        virtual ~DocumentAssetObserver() = default;
        virtual void item(std::string_view type, std::string_view name, const uint8_t* data, size_t size) noexcept = 0;
        virtual void attribute(std::string_view name, std::string_view value) noexcept = 0;
    };
    std::unique_ptr<Snapshot> readDocument(const uint8_t* bytes, size_t size, std::string& warning,
        DocumentAssetObserver* observer = nullptr, ImportedModels* imports = nullptr);
    void validateSnapshot(Snapshot& snapshot, bool canImportModels = false);

}
}

#endif

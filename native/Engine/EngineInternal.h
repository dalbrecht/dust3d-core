#ifndef DUST3D_NATIVE_ENGINE_INTERNAL_H_
#define DUST3D_NATIVE_ENGINE_INTERNAL_H_

#include "include/Dust3DBridge.h"
#include <dust3d/base/snapshot.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

struct Dust3DMesh {
    std::vector<Dust3DVertex> vertices;
    size_t nodeCount = 0;
    size_t partCount = 0;
    std::string warning;
};

namespace dust3d {
namespace native {

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

    std::unique_ptr<Snapshot> readDocument(const uint8_t* bytes, size_t size, std::string& warning);
    void validateSnapshot(Snapshot& snapshot);

}
}

#endif

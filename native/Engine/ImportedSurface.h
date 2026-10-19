// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_IMPORTED_SURFACE_H_
#define DUST3D_NATIVE_IMPORTED_SURFACE_H_
#include "EngineInternal.h"
#include <dust3d/base/retained_payload_budget.h>
#include <map>
#include <set>

namespace dust3d {
class Object;
namespace native {
    struct SurfaceComponent {
        std::string identity;
        Dust3DSurfaceComponentKind kind;
        void accountDynamicPayload(RetainedPayloadBudget& budget) const noexcept { budget.accountDynamic(identity); }
    };
    /// Private generation observer. Object is borrowed only between prepare and finish.
    class ImportedSurfaceData {
    public:
        ImportedSurfaceData(const Snapshot& snapshot, const ImportedModels& imports, size_t maximumBytes);
        void prepare(const Object& object, const std::string& warning);
        void appendTriangle(size_t index);
        void finish(const Dust3DMesh& geometry);
        const std::vector<Dust3DImportedSurfaceTriangle>& triangles() const { return m_triangles; }
        const std::vector<std::string>& models() const { return m_models; }
        const std::vector<SurfaceComponent>& components() const { return m_components; }
        size_t payloadBytes() const { return m_payloadBytes; }

    private:
        void charge(size_t count, size_t width = 1);
        std::string copyIdentity(const std::string& value);
        void addComponent(const std::string& value, Dust3DSurfaceComponentKind kind);
        template <typename T>
        void reserve(std::vector<T>& values, size_t capacity)
        {
            if (capacity <= values.capacity())
                return;
            charge(capacity - values.capacity(), sizeof(T));
            values.reserve(capacity);
            charge(values.capacity() - capacity, sizeof(T));
        }
        size_t m_maximumBytes;
        RetainedPayloadBudget m_budget;
        size_t m_payloadBytes = 0;
        const Object* m_object = nullptr;
        std::vector<Dust3DImportedSurfaceTriangle> m_triangles;
        std::vector<std::string> m_models;
        std::vector<SurfaceComponent> m_components;
        // Lookup nodes are generation workspace, cleared before publication.
        std::map<std::string, size_t> m_modelIndices;
        std::map<std::string, size_t> m_componentIndices;
        std::set<std::string> m_linearModels;
    };
}
}
struct Dust3DImportedSurfaceMesh {
    std::unique_ptr<Dust3DMesh> geometry;
    std::unique_ptr<dust3d::native::ImportedSurfaceData> data;
};
#endif

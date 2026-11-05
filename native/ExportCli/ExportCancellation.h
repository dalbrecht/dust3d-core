// SPDX-License-Identifier: MIT
#ifndef DUST3D_EXPORT_CLI_EXPORT_CANCELLATION_H
#define DUST3D_EXPORT_CLI_EXPORT_CANCELLATION_H

#include <stdexcept>

namespace dust3d::export_cli {

class ExportCancelled : public std::runtime_error {
public:
    ExportCancelled()
        : std::runtime_error("Export cancelled.")
    {
    }
};

template <typename Cancelled, typename Commit>
void commitUnlessCancelled(Cancelled cancelled, Commit commit)
{
    if (cancelled())
        throw ExportCancelled();
    commit();
}

}

#endif

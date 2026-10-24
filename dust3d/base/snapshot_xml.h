/*
 *  Copyright (c) 2016-2021 Jeremy HU <jeremy-at-dust3d dot org>. All rights reserved. 
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:

 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.

 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#ifndef DUST3D_BASE_SNAPSHOT_XML_H_
#define DUST3D_BASE_SNAPSHOT_XML_H_

#include <cstddef>
#include <dust3d/base/snapshot.h>
#include <stdexcept>
#include <string>

namespace dust3d {

void loadSnapshotFromXmlString(Snapshot* snapshot, char* xmlString);
enum class SnapshotXmlWriteErrorCode { invalidLimits,
    invalidHierarchy,
    limitExceeded };

/// Typed rejection before publication; allocation failures propagate as standard C++ exceptions.
class SnapshotXmlWriteError : public std::runtime_error {
public:
    SnapshotXmlWriteError(SnapshotXmlWriteErrorCode code, const char* message)
        : std::runtime_error(message)
        , m_code(code)
    {
    }
    SnapshotXmlWriteErrorCode code() const noexcept { return m_code; }

private:
    SnapshotXmlWriteErrorCode m_code;
};

/// Callers may tighten these hard ceilings, not enlarge them.
struct SnapshotXmlWriteLimits {
    size_t maximumComponents = 65536;
    // Reserve two enclosing XML levels within the reader's 128-level bound.
    size_t maximumDepth = 126;
    size_t maximumXmlBytes = 16 * 1024 * 1024;
};

/// Validate hierarchy and exact encoded size without serializing or mutating a snapshot.
/// Throws SnapshotXmlWriteError for invalid metadata/limits, or standard allocation exceptions.
/// Example: validateSnapshotForXmlWrite(snapshot) before preparing an ordinary fixture.
void validateSnapshotForXmlWrite(const Snapshot& snapshot, const SnapshotXmlWriteLimits& limits = {});

/// Append compatible XML under default bounds. The destination is unchanged on any exception.
/// Throws SnapshotXmlWriteError or standard allocation exceptions; never recurses through components.
void saveSnapshotToXmlString(const Snapshot& snapshot, std::string& xmlString);

/// Append XML with stricter bounds, including any existing destination prefix in the byte budget.
/// Throws SnapshotXmlWriteError or standard allocation exceptions without partial publication.
void saveSnapshotToXmlString(const Snapshot& snapshot, std::string& xmlString, const SnapshotXmlWriteLimits& limits);

}

#endif

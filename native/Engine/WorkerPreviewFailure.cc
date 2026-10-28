// SPDX-License-Identifier: MIT
#include "WorkerPreviewFailure.h"
#include "WorkerIndexedSources.h"
#include "WorkerPreviewRange.h"
#include <algorithm>
#include <new>
namespace dust3d {
namespace worker {
    namespace {
        FailurePayload copyCause(const char* cause, FailureDomain domain, int32_t code, FailureStatus status)
        {
            const FailurePayloadLimits limits;
            const auto maximum = std::min(limits.maximumCauseBytes, limits.maximumWireBytes - detailedFailurePrefixBytes);
            size_t size = 0;
            while (size < maximum && cause[size])
                ++size;
            return { status, { cause, cause + size }, FailureDetail { domain, code, 0, static_cast<uint8_t>(cause[size] != 0) } };
        }
        FailureStatus status(bool isLimit) { return isLimit ? FailureStatus::limitExceeded : FailureStatus::generationFailed; }
    }
    FailurePayload copyPreviewGenerationFailure(const std::exception_ptr& exception)
    {
        if (!exception)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Preview failure requires an original exception.");
        try {
            std::rethrow_exception(exception);
        } catch (const IndexedMeshPayloadError& cause) {
            return copyCause(cause.what(), FailureDomain::indexedMesh, static_cast<int32_t>(cause.code()), status(cause.code() == IndexedMeshPayloadErrorCode::limitExceeded));
        } catch (const IndexedSourcesPayloadError& cause) {
            return copyCause(cause.what(), FailureDomain::indexedSources, static_cast<int32_t>(cause.code()), status(cause.code() == IndexedSourcesPayloadErrorCode::limitExceeded));
        } catch (const SourceHintPayloadError& cause) {
            return copyCause(cause.what(), FailureDomain::sourceHints, static_cast<int32_t>(cause.code()), status(cause.code() == SourceHintPayloadErrorCode::limitExceeded));
        } catch (const MeshPayloadError& cause) {
            return copyCause(cause.what(), FailureDomain::mesh, static_cast<int32_t>(cause.code()), status(cause.code() == MeshPayloadErrorCode::limitExceeded));
        } catch (const PreviewRangeError& cause) {
            return copyCause(cause.what(), FailureDomain::previewRange, static_cast<int32_t>(cause.code()), status(cause.code() == PreviewRangeErrorCode::nativeBudgetExceeded || cause.code() == PreviewRangeErrorCode::arithmeticOverflow || cause.code() == PreviewRangeErrorCode::addressSpaceExceeded));
        } catch (const std::bad_alloc& cause) {
            return copyCause(cause.what(), FailureDomain::standard, 1, FailureStatus::limitExceeded);
        } catch (const std::exception& cause) {
            return copyCause(cause.what(), FailureDomain::standard, 0, FailureStatus::generationFailed);
        } catch (...) {
            return copyCause("Preview copy threw without a standard diagnostic.", FailureDomain::standard, -1, FailureStatus::generationFailed);
        }
    }
}
}

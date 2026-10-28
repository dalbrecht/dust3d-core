// SPDX-License-Identifier: MIT
#include "WorkerSessionOutputValidation.h"
namespace dust3d {
namespace worker {
    namespace detail {
        namespace {
            [[noreturn]] void throwSessionOutputValidation(SessionOutputErrorCode code, const char* cause) { throw SessionOutputError(code, cause); }
        }
        void validateSessionOutputOutcome(const SessionOutput& value, uint32_t kind)
        {
            if (value.sourceIssue && (kind == 1 || (value.sourceIssue->status != FailureStatus::generationFailed && value.sourceIssue->status != FailureStatus::limitExceeded)))
                throwSessionOutputValidation(SessionOutputErrorCode::invalidOutcome, "Session source issue requires an ordinary fallback outcome and original status3/4.");
            if (value.statistics && value.statisticsIssue)
                throwSessionOutputValidation(SessionOutputErrorCode::invalidOutcome, "Session statistics cannot be both available and failed.");
            if (const auto* failure = std::get_if<FailurePayload>(&value.outcome); failure && failure->status == FailureStatus::noGeometry)
                throwSessionOutputValidation(SessionOutputErrorCode::invalidOutcome, "Session successful emptiness requires its explicit outcome.");
        }
        uint32_t sessionOutputKind(const SessionOutput& value)
        {
            if (value.outcome.valueless_by_exception())
                throwSessionOutputValidation(SessionOutputErrorCode::invalidOutcome, "Session outcome has no owned value.");
            constexpr uint32_t kinds[] { 3, 1, 2, 4 };
            return kinds[value.outcome.index()];
        }
    }
}
}

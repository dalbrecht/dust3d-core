// SPDX-License-Identifier: MIT
#include <Dust3DWorkerSession.h>
#include <iostream>
#include <memory>
#include <native/Engine/WorkerNativeSession.h>
#include <native/WorkerRuntime/WorkerSessionOutputInternal.h>
#include <stdexcept>
void require(bool value)
{
    if (!value)
        throw std::runtime_error("C session lifecycle contract");
}
int main()
{
    try {
        Dust3DWorkerSessionConfiguration configuration {};
        configuration.document[0] = 1;
        configuration.connection_generation = 9;
        configuration.maximum_cache_payload_bytes = 4096;
        Dust3DWorkerDecodeResult result {};
        require(!dust3d_worker_session_create(configuration, nullptr));
        using Session = std::unique_ptr<Dust3DWorkerSession, decltype(&dust3d_worker_session_destroy)>;
        Session owner(dust3d_worker_session_create(configuration, &result), dust3d_worker_session_destroy);
        require(owner && result.status == DUST3D_WORKER_DECODE_OK);
        require(!dust3d_worker_session_reset(owner.get(), 9, nullptr));
        require(!dust3d_worker_session_reset(owner.get(), 10, &result));
        require(result.status != DUST3D_WORKER_DECODE_OK && result.message[0]);
        require(dust3d_worker_session_reset(owner.get(), 9, &result));
        require(result.status == DUST3D_WORKER_DECODE_OK);
        require(!dust3d_worker_session_close(owner.get(), nullptr));
        require(dust3d_worker_session_reset(owner.get(), 9, &result));
        require(dust3d_worker_session_close(owner.get(), &result));
        require(dust3d_worker_session_close(owner.get(), &result));
        require(!dust3d_worker_session_reset(owner.get(), 9, &result));
        require(result.status != DUST3D_WORKER_DECODE_OK && result.message[0]);
        require(!dust3d_worker_session_reset(nullptr, 9, &result));
        require(result.status == DUST3D_WORKER_DECODE_PROTOCOL_REJECTED);
        require(!dust3d_worker_session_close(nullptr, &result));
        require(!dust3d_worker_session_close(owner.get(), nullptr));
        auto invalid = configuration;
        invalid.connection_generation = 0;
        require(!dust3d_worker_session_create(invalid, &result));
        require(result.detail_source == DUST3D_WORKER_DETAIL_SESSION_SCOPE);
        invalid = configuration;
        invalid.document[0] = 0;
        require(!dust3d_worker_session_create(invalid, &result));
        require(result.detail_source == DUST3D_WORKER_DETAIL_SESSION_SCOPE);
        invalid = configuration;
        invalid.maximum_cache_payload_bytes = 256ULL * 1024 * 1024 + 1;
        require(!dust3d_worker_session_create(invalid, &result));
        require(result.status == DUST3D_WORKER_DECODE_PROTOCOL_REJECTED);
        configuration.maximum_cache_payload_bytes = 0;
        Session disabled(dust3d_worker_session_create(configuration, &result), dust3d_worker_session_destroy);
        require(disabled && result.status == DUST3D_WORKER_DECODE_OK);
        require(dust3d_worker_session_reset(disabled.get(), 9, &result));
        require(dust3d_worker_session_close(disabled.get(), &result));
        // Synthetic typed diagnostics exercise copying only; no native failure is induced.
        dust3d::worker::FailurePayload failure;
        failure.cause = { 'c', 'a', 'u', 's', 'e' };
        failure.detail = dust3d::worker::FailureDetail { dust3d::worker::FailureDomain::native, 17, 17, 1 };
        try {
            throw dust3d::worker::NativeSessionError(failure);
        } catch (...) {
            dust3d::worker::bridge::rejectSessionOutputException(std::current_exception(), result);
        }
        require(result.detail_source == DUST3D_WORKER_DETAIL_NATIVE_SESSION && result.detail_code == 17);
        require(result.message_truncated == 1);
        failure.detail.reset();
        try {
            throw dust3d::worker::NativeSessionError(failure);
        } catch (...) {
            dust3d::worker::bridge::rejectSessionOutputException(std::current_exception(), result);
        }
        require(result.detail_code == static_cast<int32_t>(failure.status) && !result.message_truncated);
        owner.reset();
        dust3d_worker_session_destroy(nullptr);
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

/* SPDX-License-Identifier: MIT */
#include <Dust3DWorkerSessionOutput.h>
#include <stdint.h>
int main(void)
{
    uint8_t body[128] = { 0 };
    body[1] = 1;
    body[3] = 64;
    body[7] = 3;
    body[31] = 64;
    body[65] = 1;
    body[67] = 64;
    Dust3DWorkerDecodeResult result = { 0 };
    Dust3DWorkerSessionOutputLimits limits = { 128, 1024, 0 };
    Dust3DWorkerSessionOutput* owner = dust3d_worker_session_output_decode(body, sizeof(body), limits, &result);
    if (!owner || result.status != DUST3D_WORKER_DECODE_OK)
        return 1;
    const Dust3DWorkerDecodedReply* (*primary)(const Dust3DWorkerSessionOutput*) = dust3d_worker_session_output_primary;
    if (primary(owner) || dust3d_worker_session_output_summary(owner).outcome != DUST3D_WORKER_SESSION_EMPTY)
        return 2;
    dust3d_worker_session_output_destroy(owner);
    dust3d_worker_session_output_destroy(0);
    Dust3DWorkerRequestIdentity active = { 0 };
    Dust3DWorkerServiceObservation observed = { 0 };
    Dust3DWorkerServiceReceipt receipt = { 0 };
    if (dust3d_worker_session_service_admit(0, active, observed, receipt, limits, &result)
        || result.status != DUST3D_WORKER_DECODE_PROTOCOL_REJECTED)
        return 3;
    if (dust3d_worker_session_service_queue_admit(0, observed, receipt, limits, &result)
        || result.status != DUST3D_WORKER_DECODE_PROTOCOL_REJECTED)
        return 4;
    return 0;
}

/* SPDX-License-Identifier: MIT */
#include <Dust3DWorkerRuntime.h>
#include <string.h>
int main(void)
{
    uint8_t header[64] = { 'D', 'S', '3', 'W', 'O', 'R', 'K', 0 };
    header[9] = 1;
    header[11] = 64;
    header[17] = 1;
    header[19] = 1;
    header[31] = 42;
    header[40] = 1;
    Dust3DWorkerReplyResult framed;
    Dust3DWorkerReplyCollector* collector = dust3d_worker_reply_create(header, 64, (Dust3DWorkerWireLimits) { 64, 64, 64 }, &framed);
    if (!collector)
        return 1;
    uint8_t reply[82] = { 0 };
    memcpy(reply, header, 64);
    reply[17] = 3;
    reply[63] = 18;
    reply[65] = 1;
    reply[67] = 16;
    reply[69] = 3;
    reply[79] = 2;
    reply[80] = 255;
    if (dust3d_worker_reply_append(collector, reply, sizeof(reply)).status != DUST3D_WORKER_REPLY_READY
        || dust3d_worker_reply_finish(collector).status != DUST3D_WORKER_REPLY_FRAMED)
        return 2;
    Dust3DWorkerDecodeResult result;
    Dust3DWorkerDecodedReply* decoded = dust3d_worker_reply_decode(collector, 64, &result);
    if (!decoded || result.status != DUST3D_WORKER_DECODE_OK)
        return 3;
    dust3d_worker_reply_destroy(collector);
    Dust3DWorkerDecodedSummary summary = dust3d_worker_decoded_summary(decoded);
    size_t size = 0;
    const uint8_t* cause = dust3d_worker_decoded_failure_cause(decoded, &size);
    if (summary.kind != DUST3D_WORKER_DECODED_FAILURE || summary.failure_status != 3 || summary.vertex_count || !cause || size != 2 || cause[0] != 255 || cause[1] != 0)
        return 4;
    dust3d_worker_decoded_destroy(decoded);
    collector = dust3d_worker_service_reply_create(header, 64, 9, (Dust3DWorkerWireLimits) { 64, 64, 64 }, &framed);
    if (!collector || dust3d_worker_reply_append(collector, reply, sizeof(reply)).status != DUST3D_WORKER_REPLY_READY
        || dust3d_worker_reply_finish(collector).status != DUST3D_WORKER_REPLY_FRAMED)
        return 5;
    Dust3DWorkerRequestIdentity active = { { 0 }, 42, 0 };
    active.document[0] = 1;
    Dust3DWorkerServiceObservation observation = { 9, 101 };
    Dust3DWorkerServiceReceipt receipt = { 0 };
    receipt.operation = DUST3D_WORKER_OPERATION_MESH;
    receipt.request_identity = 42;
    receipt.process_identifier = 101;
    decoded = dust3d_worker_service_admit(collector, active, observation, receipt, 64, &result);
    dust3d_worker_reply_destroy(collector);
    if (!decoded || result.status != DUST3D_WORKER_DECODE_OK)
        return 6;
    cause = dust3d_worker_decoded_failure_cause(decoded, &size);
    if (!cause || size != 2 || cause[0] != 255 || cause[1] != 0)
        return 7;
    dust3d_worker_decoded_destroy(decoded);
    Dust3DWorkerImportedTriangle triangle;
    Dust3DWorkerImportedMaterial material;
    Dust3DWorkerImportedImage image;
    Dust3DWorkerImportedSummary imported = dust3d_worker_decoded_imported_summary(NULL);
    Dust3DWorkerFailureDetail detail = dust3d_worker_decoded_failure_detail(NULL);
    if (imported.has_value || detail.has_detail || dust3d_worker_decoded_imported_triangle(NULL, 0, &triangle)
        || dust3d_worker_decoded_imported_material(NULL, 0, &material) || dust3d_worker_decoded_imported_image(NULL, 0, &image))
        return 8;
    dust3d_worker_decoded_destroy(NULL);
    return 0;
}

// SPDX-License-Identifier: MIT
#ifndef DUST3D_WORKER_DECODED_INTERNAL_H_
#define DUST3D_WORKER_DECODED_INTERNAL_H_
#include "WorkerAdmission.h"
#include <Dust3DWorkerRuntime.h>
struct Dust3DWorkerDecodedReply {
    dust3d::worker::AdmittedReply value;
};
namespace dust3d::worker::bridge {
Dust3DWorkerDecodedReply* adoptDecodedReply(AdmittedReply value);
}
#endif

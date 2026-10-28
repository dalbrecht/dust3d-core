// SPDX-License-Identifier: MIT
import Dust3DWorkerXPC
import Foundation
import ObjectiveC
import XCTest

final class WorkerXPCContractTests: XCTestCase {
    func testInterfaceFactoryReturnsIndependentInstances() {
        let first = WorkerXPCContract.makeInterface()
        let second = WorkerXPCContract.makeInterface()
        XCTAssertFalse(first === second)
    }
    func testPublicProtocolRetainsPreviewAndLifecycleSelectors() {
        for selector in [
            #selector(Dust3DWorkerXPCProtocol.handshake(withReply:)),
            NSSelectorFromString(
                "servePreview:output:isStream:document:requestIdentity:revision:connectionGeneration:maximumCachePayloadBytes:archiveBytes:maximumIndexedBytes:maximumSourceBytes:maximumInputTransferBytes:inputChunkBytes:maximumOutputTransferBytes:outputChunkBytes:withReply:"
            ),
            #selector(Dust3DWorkerXPCProtocol.closeSession(_:withReply:)),
            #selector(Dust3DWorkerXPCProtocol.cancelQueued(_:withReply:)),
        ] {
            XCTAssertNotNil(protocol_getMethodDescription(Dust3DWorkerXPCProtocol.self, selector, true, true).name)
        }
    }
}

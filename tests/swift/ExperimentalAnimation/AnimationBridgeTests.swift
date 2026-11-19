// SPDX-License-Identifier: MIT
import Dust3DEngine
import Dust3DExperimentalAnimation
import XCTest

final class AnimationBridgeTests: XCTestCase {
    func testBothAnimationOwnersRejectEmptyArchivesWithTypedErrors() {
        var options = dust3d_experimental_fish_idle_default_options()
        XCTAssertEqual(options.version, 1)
        XCTAssertGreaterThan(options.rig.maximum_output_bytes, 0)
        var error = Dust3DError()
        let clip = dust3d_experimental_fish_idle_load(nil, 0, nil, 0, nil, 0, &options, &error)
        XCTAssertNil(clip)
        XCTAssertEqual(error.status, DUST3D_STATUS_INVALID_DOCUMENT)
        dust3d_experimental_fish_idle_destroy(clip)

        var rig = dust3d_generated_rig_default_options()
        let fbx = dust3d_experimental_fish_idle_fbx_create(nil, 0, nil, 0, nil, 0, &rig, 1024, &error)
        XCTAssertNil(fbx)
        XCTAssertEqual(error.status, DUST3D_STATUS_INVALID_DOCUMENT)
        dust3d_experimental_fish_idle_fbx_destroy(fbx)
    }
}

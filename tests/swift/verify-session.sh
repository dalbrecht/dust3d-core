#!/bin/bash
set -euo pipefail

repository_directory="$(cd "$(dirname "$0")/../.." && pwd)"
scratch_directory="$(mktemp -d "${TMPDIR:-/tmp}/dust3d-swift-session.XXXXXX")"
trap 'rm -rf "$scratch_directory"' EXIT

export CLANG_MODULE_CACHE_PATH="$scratch_directory/module-cache"
export SWIFTPM_MODULECACHE_OVERRIDE="$scratch_directory/module-cache"

cd "$repository_directory"
xcrun swift build --disable-sandbox --build-system native --scratch-path "$scratch_directory" -j 4
binary_directory="$(xcrun swift build --disable-sandbox --build-system native --scratch-path "$scratch_directory" --show-bin-path)"
xcrun swiftc -parse-as-library -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
    -Xcc "-fmodule-map-file=$binary_directory/Dust3DEngine.build/module.modulemap" \
    -I native/Engine/include tests/swift/GenerationSessionSmoke.swift \
    -L "$binary_directory" -lDust3DEngine -lc++ -o "$scratch_directory/session-smoke"
"$scratch_directory/session-smoke" tests/core/fixtures/Capsule.ds3

xcrun swiftc -parse-as-library -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
    -Xcc "-fmodule-map-file=$binary_directory/Dust3DEngine.build/module.modulemap" \
    -I native/Engine/include tests/swift/SourceProvenanceSmoke.swift \
    -L "$binary_directory" -lDust3DEngine -lc++ -o "$scratch_directory/source-smoke"
"$scratch_directory/source-smoke" tests/core/fixtures/Capsule.ds3

xcrun swiftc -parse-as-library -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
    -Xcc "-fmodule-map-file=$binary_directory/Dust3DEngine.build/module.modulemap" \
    -I native/Engine/include tests/swift/UvMetadataSmoke.swift \
    -L "$binary_directory" -lDust3DEngine -lc++ -o "$scratch_directory/uv-smoke"
"$scratch_directory/uv-smoke" tests/core/fixtures/Capsule.ds3
"$scratch_directory/uv-smoke" tests/core/fixtures/Sword.ds3

xcrun swiftc -parse-as-library -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
    -Xcc "-fmodule-map-file=$binary_directory/Dust3DEngine.build/module.modulemap" \
    -I native/Engine/include tests/swift/BakedMeshSmoke.swift \
    -L "$binary_directory" -lDust3DEngine -lc++ -o "$scratch_directory/bake-smoke"
"$scratch_directory/bake-smoke" tests/core/fixtures/Capsule.ds3
"$scratch_directory/bake-smoke" tests/core/fixtures/Sword.ds3

xcrun swiftc -parse-as-library -swift-version 6 -strict-concurrency=complete -warnings-as-errors \
    -Xcc "-fmodule-map-file=$binary_directory/Dust3DEngine.build/module.modulemap" \
    -I native/Engine/include tests/swift/AnimationCatalogSmoke.swift \
    -L "$binary_directory" -lDust3DEngine -lc++ -lz -o "$scratch_directory/animation-catalog-smoke"
"$scratch_directory/animation-catalog-smoke"

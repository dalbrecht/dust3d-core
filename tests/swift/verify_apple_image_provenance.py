# SPDX-License-Identifier: MIT
"""Check the recorded exact Apple source extraction; never fetch or execute sources."""
import hashlib
import json
from pathlib import Path
import sys


def main():
    root = Path(sys.argv[1])
    manifest = json.loads((root / 'provenance.json').read_text())
    if manifest['repository'] != 'dalbrecht/dust3d-swiftui' or manifest['license'] != 'MIT':
        raise ValueError('Unexpected source provenance')
    expected = {'Metadata/PNGContainerMetadata.swift', 'Metadata/JPEGContainerMetadata.swift', 'ImageSampleLayout.swift'}
    if {item['destination'] for item in manifest['files']} != expected:
        raise ValueError('Incomplete shared source extraction')
    for item in manifest['files']:
        if hashlib.sha256((root / item['destination']).read_bytes()).hexdigest() != item['sha256']:
            raise ValueError(f"Shared source drift requires explicit reconciliation: {item['destination']}")
    print('Three byte-identical MIT Apple source extractions verified')


if __name__ == '__main__':
    main()

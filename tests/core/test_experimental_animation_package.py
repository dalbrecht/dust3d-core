# SPDX-License-Identifier: MIT
"""Check the explicitly enabled SwiftPM animation source product."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which('swift'), 'SwiftPM is unavailable')
class ExperimentalAnimationPackageTests(unittest.TestCase):
    def test_runtime_product_requires_explicit_opt_in_and_has_disjoint_sources(self):
        for flag in (None, '0', '1'):
            with self.subTest(flag=flag):
                environment = dict(os.environ)
                environment.pop('DUST3D_EXPERIMENTAL_ANIMATION', None)
                if flag is not None:
                    environment['DUST3D_EXPERIMENTAL_ANIMATION'] = flag
                with tempfile.TemporaryDirectory() as scratch:
                    result = subprocess.run(
                        ['swift', 'package', '--disable-sandbox', '--package-path', str(ROOT),
                         '--scratch-path', scratch, 'dump-package'],
                        check=True, capture_output=True, text=True, env=environment)
                package = json.loads(result.stdout)
                products = {row['name'] for row in package['products']}
                targets = {row['name']: row for row in package['targets']}
                if flag != '1':
                    self.assertNotIn('Dust3DExperimentalAnimation', products)
                    self.assertNotIn('Dust3DExperimentalAnimation', targets)
                    continue
                self.assertIn('Dust3DExperimentalAnimation', products)
                bridge = targets['Dust3DExperimentalAnimation']
                self.assertEqual(bridge['publicHeadersPath'], 'native/Engine/ExperimentalInclude')
                sources = set(bridge['sources'])
                self.assertEqual(len(sources), 138)
                self.assertIn('dust3d/animation/animation_generator.cc', sources)
                self.assertIn('dust3d/animation/sound_event_detector.cc', sources)
                self.assertNotIn('dust3d/animation/sound_generator.cc', sources)
                self.assertNotIn('dust3d/animation/biped/clip_metadata.cc', sources)
                normal_engine = {'native/Engine/' + name for name in targets['Dust3DEngine']['sources']}
                self.assertFalse(sources & normal_engine)

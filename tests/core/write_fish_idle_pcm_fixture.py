# SPDX-License-Identifier: MIT
"""Write the generated private animation fixture for CMake."""
from pathlib import Path
import subprocess
import sys

generator = Path(__file__).with_name("generate_fish_idle_pcm_fixture.py")
result = subprocess.run([sys.executable, "-B", str(generator)], check=True, capture_output=True)
Path(sys.argv[1]).write_bytes(result.stdout)

#!/usr/bin/env python3
"""Exercise extended pipeline output against the actual C loader.
SPDX-License-Identifier: BSD-3-Clause
"""
import sys
from pathlib import Path
import subprocess
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import asset_pipeline as a

root = Path(__file__).resolve().parents[1]
v = [(-1, -1, 0, 0, 0, 1), (1, -1, 0, 0, 0, 1), (0, 1, 0, 0, 0, 1)]
texture = dict(width=2, height=2, rgba=bytes([255, 0, 0, 255, 0, 255, 0, 128,
                                           0, 0, 255, 0, 255, 255, 255, 255]), repeat=True)
mesh = a.Mesh(v, [(0, 1, 2, 0xffffff)], [(0, 0), (1, 0), (.5, 1)],
              [a.material('Checker', texture=0, specular=.5)], [texture], [0],
              [((0, 0, 0, 0), (1, 0, 0, 0))] * 3,
              [(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)], [0])
for compact in (False, True):
    target = root / 'build/native' / f'asset-{compact}.d3m'
    a.save(mesh, target, compact)
    subprocess.run([str(root / 'build/native/asset-tests.exe'), str(target)], check=True)
for name in ('metamask-v2', 'suzanne-v2', 'suzanne-v2-lod1', 'suzanne-v2-lod2'):
    subprocess.run([str(root / 'build/native/asset-tests.exe'),
                    str(root / 'sdcard/models' / (name + '.d3m'))], check=True)

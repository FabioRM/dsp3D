#!/usr/bin/env python3
"""Regenerate original, BSD-licensed importer examples and their SD asset packs.
SPDX-License-Identifier: BSD-3-Clause
"""
import base64
import json
import math
from pathlib import Path
import struct
import zlib
from import_models import Gltf, obj
from asset_pipeline import save, Mesh, cluster_lod
import convert_model

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'assets/source/examples'
DEST = ROOT / 'sdcard/models/examples'
SOURCE.mkdir(parents=True, exist_ok=True)
DEST.mkdir(parents=True, exist_ok=True)


def png():
    def chunk(kind, data):
        return struct.pack('>I', len(data))+kind+data+struct.pack('>I', zlib.crc32(kind+data))
    raw = bytearray()
    for y in range(16):
        raw.append(0)
        for x in range(16):
            raw.extend((249, 179, 75, 255) if (x//4+y//4) & 1 else (36, 107, 184, 255))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR', struct.pack('>IIBBBBB', 16, 16, 8, 6, 0, 0, 0))+chunk(b'IDAT', zlib.compress(raw))+chunk(b'IEND', b'')


def main():
    (SOURCE / 'checker.png').write_bytes(png())
    (SOURCE / 'checker.mtl').write_text('# SPDX-License-Identifier: BSD-3-Clause\nnewmtl Checker\nKd 1 1 1\nKs .2 .2 .2\nmap_Kd checker.png\n', encoding='ascii')
    (SOURCE / 'checker.obj').write_text('# SPDX-License-Identifier: BSD-3-Clause\nmtllib checker.mtl\n'
        'v -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\nvn 0 0 1\n'
        'vt 0 0\nvt 2 0\nvt 2 2\nvt 0 2\nusemtl Checker\nf 1/1/1 2/2/1 3/3/1 4/4/1\n', encoding='ascii')
    save(obj(SOURCE / 'checker.obj'), DEST / 'checker.d3m', True)
    binary, views, accessors = bytearray(), [], []
    def accessor(values, kind, component=5126):
        binary.extend(b'\0'*(-len(binary) % 4))
        start = len(binary)
        fmt = {5126: 'f', 5123: 'H'}[component]
        for value in values:
            binary.extend(struct.pack('<'+fmt*len(value), *value))
        views.append(dict(buffer=0, byteOffset=start, byteLength=len(binary)-start))
        accessors.append(dict(bufferView=len(views)-1, componentType=component, count=len(values), type=kind))
        return len(accessors)-1
    positions = [(x*.6-.3, y*.25-1, 0) for y in range(9) for x in range(2)]
    normals = [(0, 0, 1)]*18
    weights = [(1-max(0, min(1, (y-2)/4)), max(0, min(1, (y-2)/4)), 0, 0) for y in range(9) for x in range(2)]
    indices = [(i,) for y in range(8) for i in (y*2, y*2+1, y*2+3, y*2, y*2+3, y*2+2)]
    attrs = dict(POSITION=accessor(positions, 'VEC3'), NORMAL=accessor(normals, 'VEC3'),
        TEXCOORD_0=accessor([(x, y/8) for y in range(9) for x in range(2)], 'VEC2'),
        JOINTS_0=accessor([(0, 1, 0, 0)]*18, 'VEC4', 5123), WEIGHTS_0=accessor(weights, 'VEC4'))
    index = accessor(indices, 'SCALAR', 5123)
    bind = accessor([(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)]*2, 'MAT4')
    times = accessor([(i,) for i in range(5)], 'SCALAR')
    rotations = accessor([(0, 0, math.sin(a/2), math.cos(a/2)) for a in (0, .8, 0, -.8, 0)], 'VEC4')
    doc = dict(asset=dict(version='2.0', generator='dsp3D generate_examples.py', copyright='BSD-3-Clause, Fabio Angeletti'),
        buffers=[dict(byteLength=len(binary), uri='data:application/octet-stream;base64,'+base64.b64encode(binary).decode())],
        bufferViews=views, accessors=accessors,
        images=[dict(uri='checker.png')], textures=[dict(source=0)],
        materials=[dict(name='Ribbon checker', doubleSided=True, pbrMetallicRoughness=dict(
            baseColorTexture=dict(index=0), roughnessFactor=.8, metallicFactor=0))],
        meshes=[dict(primitives=[dict(attributes=attrs, indices=index, material=0)])],
        nodes=[dict(mesh=0, skin=0), dict(children=[2]), {}],
        scenes=[dict(nodes=[0, 1])], scene=0,
        skins=[dict(joints=[1, 2], inverseBindMatrices=bind)],
        animations=[dict(name='Ribbon wave', samplers=[dict(input=times, output=rotations)],
            channels=[dict(sampler=0, target=dict(node=2, path='rotation'))])])
    source = SOURCE / 'ribbon.gltf'
    source.write_text(json.dumps(doc, indent=2)+'\n', encoding='utf8')
    Gltf(source).export(DEST / 'ribbon.d3m', True)
    # Reproduce the embedded compact Suzanne and its deterministic clustered LODs.
    v, f, _, _ = convert_model.convert(ROOT / 'assets/source/suzanne.stl', 180, 0x79b8e8)
    mesh = Mesh(v, f)
    save(mesh, ROOT / 'sdcard/models/suzanne-v2.d3m', True, ROOT / 'assets/generated', 'suzanne_compact')
    for i, grid in enumerate((8, 4), 1):
        save(cluster_lod(mesh, grid), ROOT / f'sdcard/models/suzanne-v2-lod{i}.d3m', True,
             ROOT / 'assets/generated', f'suzanne_lod{i}')


if __name__ == '__main__':
    main()

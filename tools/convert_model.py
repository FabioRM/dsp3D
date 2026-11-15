#!/usr/bin/env python3
"""Deterministic STL -> indexed dsp3D mesh. Python standard library only.
SPDX-License-Identifier: BSD-3-Clause
Copyright (c) 2026 Fabio Angeletti
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import zlib


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def unit(a):
    length = math.sqrt(sum(x*x for x in a))
    if not length:
        raise ValueError("Zero-length normal")
    return tuple(x/length for x in a)


def read_stl(path):
    data = path.read_bytes()
    count = struct.unpack_from('<I', data, 80)[0] if len(data) >= 84 else 0
    if len(data) >= 84 and len(data) == 84 + count*50:
        faces = []
        for i in range(count):
            record = struct.unpack_from('<12fH', data, 84+i*50)
            faces.append([tuple(record[j:j+3]) for j in (3, 6, 9)])
    else:
        text = data.decode('ascii')
        if not text.lstrip().startswith('solid') or 'endsolid' not in text:
            raise ValueError("Expected an ASCII or binary STL file")
        vertices = []
        for line in text.splitlines():
            fields = line.split()
            if fields and fields[0] == 'vertex':
                if len(fields) != 4:
                    raise ValueError("Malformed STL vertex")
                vertices.append(tuple(float(x) for x in fields[1:]))
        if len(vertices) % 3:
            raise ValueError("Incomplete STL triangle")
        faces = [vertices[i:i+3] for i in range(0, len(vertices), 3)]
    if not faces or any(not math.isfinite(x) for f in faces for v in f for x in v):
        raise ValueError("Empty mesh or non-finite coordinates")
    return faces


def convert(path, crease=55.0, color=0xe89032, z_up=True):
    faces = read_stl(path)
    points = [v for f in faces for v in f]
    center = tuple((min(v[i] for v in points)+max(v[i] for v in points))*0.5 for i in range(3))
    extent = max(max(v[i] for v in points)-min(v[i] for v in points) for i in range(3))
    if extent <= 0:
        raise ValueError("Mesh has no extent")
    def transform(v):
        x, y, z = ((v[i]-center[i])*2.0/extent for i in range(3))
        return tuple(round(k, 7) for k in ((x, z, -y) if z_up else (x, y, z)))
    clean, normals, weighted = [], [], []
    for face in faces:
        face = [transform(v) for v in face]
        normal = cross(sub(face[1], face[0]), sub(face[2], face[0]))
        if sum(x*x for x in normal) <= 1e-18:
            continue
        clean.append(face)
        weighted.append(normal)
        normals.append(unit(normal))
    if not clean:
        raise ValueError("Mesh contains only degenerate triangles")
    adjacent = {}
    for i, face in enumerate(clean):
        for v in face:
            adjacent.setdefault(v, []).append(i)
    vertices, triangles, lookup = [], [], {}
    threshold = math.cos(math.radians(crease))
    for i, face in enumerate(clean):
        indices = []
        for v in face:
            contributing = [j for j in adjacent[v] if sum(a*b for a, b in zip(normals[i], normals[j])) >= threshold-1e-8]
            normal = unit(tuple(sum(weighted[j][k] for j in contributing) for k in range(3)))
            normal = tuple(round(x, 7) for x in normal)
            key = v + normal
            if key not in lookup:
                lookup[key] = len(vertices)
                vertices.append(key)
            indices.append(lookup[key])
        triangles.append((*indices, color))
    payload = b''.join(struct.pack('<6f', *v) for v in vertices)
    payload += b''.join(struct.pack('<4I', *t) for t in triangles)
    blob = struct.pack('<4s7I', b'DSP3', 1, len(vertices), len(triangles), len(payload), zlib.crc32(payload), 0, 0) + payload
    report = {'source': path.name, 'source_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
              'source_triangles': len(faces), 'triangles': len(triangles), 'vertices': len(vertices),
              'removed_degenerate': len(faces)-len(clean), 'bytes': len(blob),
              'crease_degrees': crease, 'color': f'{color:06x}', 'z_up_to_y_up': z_up}
    return vertices, triangles, blob, report


def export(path, output, symbol, crease, color, z_up=True, c_dir=None):
    if not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', symbol):
        raise ValueError("Invalid C symbol")
    vertices, triangles, blob, report = convert(path, crease, color, z_up)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(blob)
    output.with_suffix('.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    if c_dir:
        c_dir.mkdir(parents=True, exist_ok=True)
        header = f'#ifndef DSP3D_ASSET_{symbol.upper()}_H\n#define DSP3D_ASSET_{symbol.upper()}_H\n#include "dsp3D.h"\nextern const dsp3d_mesh {symbol};\n#endif\n'
        lines = [f'/* Generated from {path.name}; see assets/README.md for provenance. */', f'#include "{symbol}.h"',
                 'static const dsp3d_vertex vertices[] = {']
        def ff(x):
            return f'{x:.8e}f'
        for v in vertices:
            lines.append('    {{'+','.join(ff(x) for x in v[:3])+'},{'+','.join(ff(x) for x in v[3:])+'}},')
        lines += ['};', 'static const dsp3d_triangle triangles[] = {']
        lines += ['    {{'+','.join(str(x) for x in t[:3])+'},0x'+f'{t[3]:06x}'+'u},' for t in triangles]
        lines += ['};', f'const dsp3d_mesh {symbol} = {{vertices,triangles,{len(vertices)},{len(triangles)}}};', '']
        (c_dir / f'{symbol}.h').write_text(header, encoding='utf-8')
        (c_dir / f'{symbol}.c').write_text('\n'.join(lines), encoding='utf-8')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--symbol', default='model')
    parser.add_argument('--crease', type=float, default=55)
    parser.add_argument('--color', type=lambda s: int(s, 16), default=0xe89032)
    parser.add_argument('--keep-axes', action='store_true')
    parser.add_argument('--c-dir', type=Path)
    args = parser.parse_args()
    if not 0 <= args.crease <= 180 or not 0 <= args.color <= 0xffffff:
        parser.error('Crease must be 0..180 and color must be 000000..ffffff')
    try:
        print(json.dumps(export(args.input, args.output, args.symbol, args.crease, args.color, not args.keep_axes, args.c_dir), indent=2))
    except (ValueError, OSError, UnicodeError, struct.error) as exc:
        parser.exit(1, f'Conversion failed: {exc}\n')


if __name__ == '__main__':
    main()

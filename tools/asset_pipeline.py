#!/usr/bin/env python3
"""Extended dsp3D assets: prepared geometry, compact vertices, LOD and thumbnails.
SPDX-License-Identifier: BSD-3-Clause
"""
import argparse
import copy
import json
import math
from pathlib import Path
import re
import struct
import zlib
import convert_model


class Mesh:
    def __init__(self, vertices, triangles, uv=None, materials=None, textures=None,
                 material_indices=None, skin=None, inverse_bind=None, joints=None):
        self.vertices = vertices
        self.triangles = triangles
        self.uv = uv
        self.materials = materials or []
        self.textures = textures or []
        self.material_indices = material_indices
        self.skin = skin
        self.inverse_bind = inverse_bind
        self.joints = joints


def material(name='Default', color=0xffffff, texture=-1, specular=0, emissive=0, opacity=255, flags=0):
    return dict(name=name, color=color, texture=texture, specular=specular,
                emissive=emissive, opacity=opacity, flags=flags)


def bounds(mesh):
    if not mesh.vertices or not mesh.triangles:
        raise ValueError('Empty mesh')
    if any(not math.isfinite(x) for v in mesh.vertices for x in v):
        raise ValueError('Non-finite vertex')
    minimum = tuple(min(v[i] for v in mesh.vertices) for i in range(3))
    maximum = tuple(max(v[i] for v in mesh.vertices) for i in range(3))
    return minimum, maximum


def face_normals(mesh):
    result = []
    for face in mesh.triangles:
        if any(i < 0 or i >= len(mesh.vertices) for i in face[:3]):
            raise ValueError('Triangle index out of range')
        p, q, r = (mesh.vertices[i][:3] for i in face[:3])
        n = convert_model.cross(convert_model.sub(q, p), convert_model.sub(r, p))
        result.append(convert_model.unit(n) if sum(x*x for x in n) > 1e-20 else (0, 1, 0))
    return result


def thumbnail(mesh, size=32):
    """Small orthographic flat preview; runtime rendering is independently tested."""
    lo, hi = bounds(mesh)
    center = tuple((a+b)*0.5 for a, b in zip(lo, hi))
    extent = max(b-a for a, b in zip(lo, hi)) or 1
    points = []
    for vertex in mesh.vertices:
        x, y, z = ((vertex[i]-center[i])/extent for i in range(3))
        x, z = x*0.94+z*0.34, z*0.94-x*0.34
        y, z = y*0.98+z*0.20, z*0.98-y*0.20
        points.append((size/2+x*size*0.8, size/2-y*size*0.8, z))
    pixels = [0x08a4]*(size*size)
    depth = [-math.inf]*(size*size)
    normals = face_normals(mesh)
    for index, face in enumerate(mesh.triangles):
        a, b, c = (points[i] for i in face[:3])
        def edge(p, q, x, y):
            return (q[0]-p[0])*(y-p[1])-(q[1]-p[1])*(x-p[0])
        area = edge(a, b, c[0], c[1])
        if abs(area) < 1e-10:
            continue
        color = face[3]
        if mesh.materials:
            color = mesh.materials[mesh.material_indices[index] if mesh.material_indices else 0]['color']
        shade = 0.25+0.75*abs(normals[index][2])
        red, green, blue = (int(((color >> shift) & 255)*shade) for shift in (16, 8, 0))
        rgb = (red >> 3) << 11 | (green >> 2) << 5 | (blue >> 3)
        for y in range(max(0, int(min(a[1], b[1], c[1]))), min(size, int(max(a[1], b[1], c[1]))+1)):
            for x in range(max(0, int(min(a[0], b[0], c[0]))), min(size, int(max(a[0], b[0], c[0]))+1)):
                w = (edge(b, c, x+0.5, y+0.5)/area, edge(c, a, x+0.5, y+0.5)/area,
                     edge(a, b, x+0.5, y+0.5)/area)
                if min(w) >= 0:
                    z = sum(w[i]*v[2] for i, v in enumerate((a, b, c)))
                    if z > depth[y*size+x]:
                        depth[y*size+x], pixels[y*size+x] = z, rgb
    return struct.pack('<'+'H'*len(pixels), *pixels)


def encode(mesh, compact=False, with_thumbnail=True):
    lo, hi = bounds(mesh)
    nv, nf = len(mesh.vertices), len(mesh.triangles)
    chunks = []
    def chunk(name, values, count, stride):
        if len(values) != count*stride or not count:
            raise ValueError(f'Invalid {name} chunk')
        chunks.append((name.encode('ascii'), values, count, stride))
    if compact:
        center = tuple((a+b)*0.5 for a, b in zip(lo, hi))
        extent = tuple((b-a)*0.5 for a, b in zip(lo, hi))
        def q(x):
            return max(-32767, min(32767, round(x*32767)))
        packed = b''
        for v in mesh.vertices:
            p = [q((v[i]-center[i])/extent[i]) if extent[i] else 0 for i in range(3)]
            n = [q(x) for x in convert_model.unit(v[3:6])]
            packed += struct.pack('<6h', *p, *n)
        chunk('VQ16', packed, nv, 12)
    else:
        chunk('VRTX', b''.join(struct.pack('<6f', *v) for v in mesh.vertices), nv, 24)
    chunk('TRIS', b''.join(struct.pack('<4I', *f) for f in mesh.triangles), nf, 16)
    chunk('FNOR', b''.join(struct.pack('<3f', *n) for n in face_normals(mesh)), nf, 12)
    if mesh.uv is not None:
        if len(mesh.uv) != nv or any(not math.isfinite(x) for uv in mesh.uv for x in uv):
            raise ValueError('Invalid texture coordinates')
        chunk('UV00', b''.join(struct.pack('<2f', *uv) for uv in mesh.uv), nv, 8)
    if mesh.material_indices is not None:
        if len(mesh.material_indices) != nf or any(i < 0 or i >= len(mesh.materials) for i in mesh.material_indices):
            raise ValueError('Invalid material indices')
        chunk('MIDX', b''.join(struct.pack('<I', i) for i in mesh.material_indices), nf, 4)
    if mesh.materials:
        data = b''
        for m in mesh.materials:
            if not 0 <= m['opacity'] <= 255 or not 0 <= m['specular'] <= 1 or not 0 <= m['emissive'] <= 1 or not -1 <= m['texture'] < len(mesh.textures):
                raise ValueError('Invalid material')
            data += struct.pack('<IIffIIII', m['color'], m['texture'] & 0xffffffff, m['specular'],
                                m['emissive'], m['opacity'], m['flags'], 0, 0)
        chunk('MATL', data, len(mesh.materials), 32)
        chunk('NAME', b''.join(m['name'].encode('ascii', errors='replace')[:31].ljust(32, b'\0') for m in mesh.materials), len(mesh.materials), 32)
    if mesh.textures:
        texinfo, texels, alpha = b'', b'', b''
        for tex in mesh.textures:
            width, height, rgba = tex['width'], tex['height'], tex['rgba']
            if not 0 < width <= 2047 or not 0 < height <= 2047 or len(rgba) != width*height*4:
                raise ValueError('Invalid texture dimensions')
            first, afirst = len(texels)//2, 0xffffffff
            values = rgba[3::4]
            if any(a != 255 for a in values):
                afirst = len(alpha)
                alpha += values
            for i in range(0, len(rgba), 4):
                r, g, b, _ = rgba[i:i+4]
                texels += struct.pack('<H', (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3))
            texinfo += struct.pack('<6I', width, height, first, afirst, int(tex.get('repeat', False)), 0)
        chunk('TEXI', texinfo, len(mesh.textures), 24)
        chunk('TEXD', texels, len(texels)//2, 2)
        if alpha:
            chunk('ALPH', alpha, len(alpha), 1)
    if mesh.skin is not None:
        if len(mesh.skin) != nv or not mesh.joints or len(mesh.joints) > 64 or len(mesh.inverse_bind) != len(mesh.joints):
            raise ValueError('Invalid skin')
        for joint, weight in mesh.skin:
            if len(joint) != 4 or len(weight) != 4 or any(j < 0 or j >= len(mesh.joints) for j in joint) or abs(sum(weight)-1) > 0.01:
                raise ValueError('Invalid skin weights or joints')
        chunk('SKIN', b''.join(struct.pack('<4H4f', *j, *w) for j, w in mesh.skin), nv, 24)
        chunk('IBND', b''.join(struct.pack('<16f', *m) for m in mesh.inverse_bind), len(mesh.joints), 64)
        chunk('JNOD', b''.join(struct.pack('<I', j) for j in mesh.joints), len(mesh.joints), 4)
    if with_thumbnail:
        chunk('THMB', thumbnail(mesh), 32*32, 2)
    directory, body = bytearray(), bytearray()
    offset = 64+len(chunks)*16
    for name, values, count, stride in chunks:
        directory += struct.pack('<4s3I', name, offset+len(body), count, stride)
        body += values
        body += b'\0'*(-len(body) % 4)
    payload = bytes(directory+body)
    return struct.pack('<4s7I8f', b'DSP3', 2, 64+len(payload), zlib.crc32(payload), nv, nf, len(chunks), 0,
                       *lo, *hi, 0, 0)[:56] + struct.pack('<2I', 32 if with_thumbnail else 0, 32 if with_thumbnail else 0) + payload


def cluster_lod(mesh, grid):
    if mesh.skin is not None:
        raise ValueError('Offline LOD of skinned meshes is not supported; retain the original skin')
    lo, hi = bounds(mesh)
    extent = max(b-a for a, b in zip(lo, hi)) or 1
    cells = [tuple(round((v[i]-lo[i])/extent*grid) for i in range(3)) for v in mesh.vertices]
    positions = {}
    for cell, vertex in zip(cells, mesh.vertices):
        positions.setdefault(cell, []).append(vertex[:3])
    centers = {cell: tuple(sum(p[i] for p in values)/len(values) for i in range(3)) for cell, values in positions.items()}
    groups, remap, vertices, uv = {}, [], [], []
    for i, cell in enumerate(cells):
        key = (cell, tuple(round(x*256) for x in mesh.uv[i]) if mesh.uv else ())
        if key not in groups:
            groups[key] = len(vertices)
            vertices.append((*centers[cell], *mesh.vertices[i][3:]))
            if mesh.uv:
                uv.append(mesh.uv[i])
        remap.append(groups[key])
    faces, material_ids, seen = [], [], set()
    for i, face in enumerate(mesh.triangles):
        indices = tuple(remap[j] for j in face[:3])
        material_id = mesh.material_indices[i] if mesh.material_indices else 0
        key = (tuple(sorted(indices)), face[3], material_id)
        if len(set(indices)) < 3 or key in seen:
            continue
        a, b, c = (vertices[j][:3] for j in indices)
        n = convert_model.cross(convert_model.sub(b, a), convert_model.sub(c, a))
        if sum(x*x for x in n) < 1e-16:
            continue
        seen.add(key)
        faces.append((*indices, face[3]))
        material_ids.append(material_id)
    return Mesh(vertices, faces, uv if mesh.uv else None, copy.deepcopy(mesh.materials),
                mesh.textures, material_ids if mesh.material_indices else None)


def save(mesh, output, compact=False, c_dir=None, symbol='asset'):
    blob = encode(mesh, compact)
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(blob)
    report = dict(version=2, vertices=len(mesh.vertices), triangles=len(mesh.triangles), bytes=len(blob),
                  compact=compact, materials=[m['name'] for m in mesh.materials], textures=len(mesh.textures), joints=len(mesh.joints or []), bounds=bounds(mesh))
    output.with_suffix('.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    if c_dir:
        if not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', symbol):
            raise ValueError('Invalid C symbol')
        directory = Path(c_dir)
        directory.mkdir(parents=True, exist_ok=True)
        words = struct.unpack('<'+'I'*(len(blob)//4), blob)
        header = f'#ifndef DSP3D_{symbol.upper()}_H\n#define DSP3D_{symbol.upper()}_H\n#include <stddef.h>\n#include <stdint.h>\nextern const uint32_t {symbol}_data[];\nextern const size_t {symbol}_size;\n#endif\n'
        lines = [f'/* Generated by tools/asset_pipeline.py. */', f'#include "{symbol}.h"', f'const uint32_t {symbol}_data[] = {{']
        lines += ['    '+','.join(f'0x{x:08x}u' for x in words[i:i+8])+',' for i in range(0, len(words), 8)]
        lines += ['};', f'const size_t {symbol}_size = {len(blob)};', '']
        (directory/f'{symbol}.h').write_text(header, encoding='utf-8')
        (directory/f'{symbol}.c').write_text('\n'.join(lines), encoding='utf-8')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--compact', action='store_true')
    parser.add_argument('--crease', type=float, default=60)
    parser.add_argument('--color', type=lambda x: int(x, 16), default=0x79b8e8)
    parser.add_argument('--lods', type=int, choices=range(4), default=0)
    parser.add_argument('--c-dir', type=Path)
    parser.add_argument('--symbol', default='asset')
    args = parser.parse_args()
    try:
        if args.input.suffix.lower() != '.stl':
            raise ValueError('This entry point currently expects STL')
        v, f, _, _ = convert_model.convert(args.input, args.crease, args.color)
        mesh = Mesh(v, f)
        print(json.dumps(save(mesh, args.output, args.compact, args.c_dir, args.symbol), indent=2))
        previous = mesh
        for i in range(args.lods):
            target = len(mesh.triangles)/(2**(i+1))
            candidates = [cluster_lod(mesh, grid) for grid in (32, 24, 16, 12, 8, 6, 4, 3, 2)]
            candidates = [m for m in candidates if m.triangles and len(m.triangles) < len(previous.triangles)]
            if not candidates:
                break
            previous = min(candidates, key=lambda m: abs(len(m.triangles)-target))
            save(previous, args.output.with_stem(args.output.stem+f'-lod{i+1}'), args.compact)
    except (OSError, ValueError, struct.error) as exc:
        parser.exit(1, f'Asset conversion failed: {exc}\n')


if __name__ == '__main__':
    main()

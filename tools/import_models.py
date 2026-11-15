#!/usr/bin/env python3
"""Dependency-free OBJ/MTL and glTF 2.0 import for the dsp3D asset pipeline.
glTF reference: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html
SPDX-License-Identifier: BSD-3-Clause
"""
import base64
import bisect
import copy
import json
import math
from pathlib import Path
import shlex
import struct
import zlib
from urllib.parse import unquote
from asset_pipeline import Mesh, material
from convert_model import cross, sub, unit


def local_uri(folder, uri):
    if uri.startswith('data:'):
        header, payload = uri.split(',', 1)
        if not header.endswith(';base64'):
            raise ValueError('Only base64 data URIs are supported')
        return base64.b64decode(payload, validate=True)
    if ':' in uri or uri.startswith(('/', '\\')):
        raise ValueError('External resources must be relative local files')
    path = (folder / unquote(uri)).resolve()
    if not path.is_relative_to(folder.resolve()):
        raise ValueError('Resource escapes the model directory')
    return path.read_bytes()


def image(data):
    """Read noninterlaced 8-bit PNG (gray/RGB/gray-alpha/RGBA) or uncompressed BMP."""
    if data[:8] == b'\x89PNG\r\n\x1a\n':
        offset, compressed, width, height, channels = 8, bytearray(), 0, 0, 0
        while offset < len(data):
            length, kind = struct.unpack_from('>I4s', data, offset)
            value = data[offset+8:offset+8+length]
            if len(value) != length or offset+length+12 > len(data):
                raise ValueError('Truncated PNG')
            if zlib.crc32(kind+value) != struct.unpack_from('>I', data, offset+8+length)[0]:
                raise ValueError('PNG CRC mismatch')
            if kind == b'IHDR':
                width, height, bits, color, compression, filtering, interlace = struct.unpack('>IIBBBBB', value)
                if bits != 8 or color not in (0, 2, 4, 6) or compression or filtering or interlace:
                    raise ValueError('PNG requires noninterlaced 8-bit gray/RGB/GA/RGBA; export palette PNG as RGBA')
                channels = {0: 1, 2: 3, 4: 2, 6: 4}[color]
            elif kind == b'IDAT':
                compressed.extend(value)
            elif kind == b'tRNS':
                raise ValueError('PNG tRNS is unsupported; export RGBA')
            elif kind == b'IEND':
                break
            offset += length+12
        if not 0 < width <= 2047 or not 0 < height <= 2047 or not channels:
            raise ValueError('Invalid texture dimensions')
        expected = height*(width*channels+1)
        decoder = zlib.decompressobj()
        raw = decoder.decompress(compressed, expected+1)
        if len(raw) != expected or not decoder.eof:
            raise ValueError('PNG decompressed size mismatch')
        rgba, previous, offset = bytearray(), bytearray(width*channels), 0
        for _ in range(height):
            filtering = raw[offset]
            row = bytearray(raw[offset+1:offset+1+width*channels])
            offset += 1+len(row)
            if filtering > 4:
                raise ValueError('Invalid PNG filter')
            for i in range(len(row)):
                left = row[i-channels] if i >= channels else 0
                up = previous[i]
                corner = previous[i-channels] if i >= channels else 0
                prediction = left+up-corner
                distances = (abs(prediction-left), abs(prediction-up), abs(prediction-corner))
                paeth = (left, up, corner)[distances.index(min(distances))]
                row[i] = (row[i]+(0, left, up, (left+up)//2, paeth)[filtering]) & 255
            for i in range(0, len(row), channels):
                p = row[i:i+channels]
                rgba.extend((p[0], p[0], p[0], 255) if channels == 1 else
                            (p[0], p[0], p[0], p[1]) if channels == 2 else
                            (*p, 255) if channels == 3 else p)
            previous = row
        return dict(width=width, height=height, rgba=bytes(rgba), repeat=False)
    if data[:2] == b'BM':
        pixel_offset = struct.unpack_from('<I', data, 10)[0]
        size, width, signed_height, planes, bits, compression = struct.unpack_from('<IiiHHI', data, 14)
        height = abs(signed_height)
        if size < 40 or planes != 1 or bits not in (24, 32) or compression or not 0 < width <= 2047 or not 0 < height <= 2047:
            raise ValueError('BMP requires uncompressed 24/32-bit pixels')
        stride = (width*(bits//8)+3) & ~3
        if pixel_offset+stride*height > len(data):
            raise ValueError('Truncated BMP')
        rgba = bytearray()
        for y in range(height):
            row = y if signed_height < 0 else height-1-y
            for x in range(width):
                b, g, r = data[pixel_offset+row*stride+x*(bits//8):][:3]
                rgba.extend((r, g, b, 255))
        return dict(width=width, height=height, rgba=bytes(rgba), repeat=False)
    raise ValueError('Texture format unsupported; convert to 8-bit RGBA PNG or 24-bit BMP')


def rgb(values):
    return sum(round(max(0, min(1, float(v)))*255) << shift for v, shift in zip(values, (16, 8, 0)))


def read_mtl(path, materials, textures):
    current = None
    for line in path.read_text(encoding='utf-8-sig').splitlines():
        parts = shlex.split(line, comments=True)
        if not parts:
            continue
        key, values = parts[0], parts[1:]
        if key == 'newmtl':
            current = material(' '.join(values))
            materials[current['name']] = current
        elif current is not None:
            if key == 'Kd':
                current['color'] = rgb(values[:3])
            elif key == 'Ks':
                current['specular'] = max(0, min(1, max(map(float, values[:3]))))
            elif key == 'Ke':
                current['emissive'] = max(0, min(1, max(map(float, values[:3]))))
            elif key in ('d', 'Tr'):
                current['opacity'] = round(max(0, min(1, 1-float(values[0]) if key == 'Tr' else float(values[0])))*255)
            elif key == 'map_Kd':
                if not values or values[0].startswith('-'):
                    raise ValueError('MTL map_Kd options are unsupported; bake them into the texture')
                textures.append(image(local_uri(path.parent, ' '.join(values))))
                textures[-1]['repeat'] = True
                current['texture'] = len(textures)-1


def obj(path):
    positions, normals, texcoords, vertices, uv, faces, ids = [], [], [], [], [], [], []
    materials, textures, lookup, names = {'Default': material()}, [], {}, ['Default']
    active = 'Default'
    def index(value, count):
        number = int(value)
        number = number-1 if number > 0 else count+number
        if not 0 <= number < count:
            raise ValueError('OBJ index out of range')
        return number
    for line in path.read_text(encoding='utf-8-sig').splitlines():
        parts = shlex.split(line, comments=True)
        if not parts:
            continue
        key, values = parts[0], parts[1:]
        if key == 'v':
            positions.append(tuple(map(float, values[:3])))
        elif key == 'vn':
            normals.append(unit(tuple(map(float, values[:3]))))
        elif key == 'vt':
            u, v = map(float, values[:2])
            texcoords.append((u, 1-v))
        elif key == 'mtllib':
            for name in values:
                target = (path.parent / name).resolve()
                if not target.is_relative_to(path.parent.resolve()):
                    raise ValueError('MTL escapes model directory')
                read_mtl(target, materials, textures)
        elif key == 'usemtl':
            active = ' '.join(values)
            if active not in materials:
                raise ValueError(f'Undefined OBJ material: {active}')
            if active not in names:
                names.append(active)
        elif key == 'f':
            polygon = []
            for value in values:
                fields = value.split('/')
                p = index(fields[0], len(positions))
                t = index(fields[1], len(texcoords)) if len(fields) > 1 and fields[1] else None
                n = index(fields[2], len(normals)) if len(fields) > 2 and fields[2] else None
                polygon.append((p, t, n))
            if len(polygon) < 3:
                raise ValueError('OBJ face needs three vertices')
            for i in range(1, len(polygon)-1):
                corners = (polygon[0], polygon[i], polygon[i+1])
                a, b, c = [positions[v[0]] for v in corners]
                normal = unit(cross(sub(b, a), sub(c, a)))
                face = []
                for p, t, n in corners:
                    norm = normals[n] if n is not None else normal
                    tag = (p, t, norm)
                    if tag not in lookup:
                        lookup[tag] = len(vertices)
                        vertices.append((*positions[p], *norm))
                        uv.append(texcoords[t] if t is not None else (0, 0))
                    face.append(lookup[tag])
                faces.append((*face, materials[active]['color']))
                ids.append(names.index(active))
    return Mesh(vertices, faces, uv, [materials[n] for n in names], textures, ids)


IDENTITY = (1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)


class Gltf:
    def __init__(self, path):
        self.path, binary = path, None
        data = path.read_bytes()
        if path.suffix.lower() == '.glb':
            magic, version, size = struct.unpack_from('<4sII', data)
            if magic != b'glTF' or version != 2 or size != len(data):
                raise ValueError('Invalid GLB header')
            offset, document = 12, None
            while offset < size:
                length, kind = struct.unpack_from('<II', data, offset)
                payload = data[offset+8:offset+8+length]
                if len(payload) != length or length % 4:
                    raise ValueError('Invalid GLB chunk')
                if kind == 0x4e4f534a and document is None:
                    document = json.loads(payload)
                elif kind == 0x004e4942 and binary is None:
                    binary = payload
                offset += length+8
            self.doc = document
        else:
            self.doc = json.loads(data)
        d = self.doc
        if not d or d.get('asset', {}).get('version') != '2.0':
            raise ValueError('glTF 2.0 required')
        unsupported = set(d.get('extensionsRequired', []))-{'KHR_materials_unlit'}
        if unsupported:
            raise ValueError('Unsupported required glTF extensions: '+', '.join(sorted(unsupported)))
        self.buffers = []
        for b in d.get('buffers', []):
            payload = local_uri(path.parent, b['uri']) if 'uri' in b else binary
            if payload is None or len(payload) < b['byteLength']:
                raise ValueError('Missing/truncated glTF buffer')
            self.buffers.append(payload[:b['byteLength']])
        self.textures, self.materials = [], []
        for t in d.get('textures', []):
            im = d['images'][t['source']]
            payload = local_uri(path.parent, im['uri']) if 'uri' in im else self.view(im['bufferView'])
            tex = image(payload)
            sampler = d.get('samplers', [])[t['sampler']] if 'sampler' in t else {}
            wrap_s, wrap_t = sampler.get('wrapS', 10497), sampler.get('wrapT', 10497)
            if wrap_s != wrap_t or wrap_s not in (10497, 33071):
                raise ValueError('Texture wrapping requires matching REPEAT or CLAMP_TO_EDGE axes')
            tex['repeat'] = wrap_s == 10497
            self.textures.append(tex)
        for m in d.get('materials', []):
            pbr = m.get('pbrMetallicRoughness', {})
            color = pbr.get('baseColorFactor', [1, 1, 1, 1])
            tex = pbr.get('baseColorTexture', {})
            if tex.get('texCoord', 0) != 0 or tex.get('extensions'):
                raise ValueError('Only untransformed TEXCOORD_0 is supported')
            if any(key in m for key in ('normalTexture', 'occlusionTexture', 'emissiveTexture')) or 'metallicRoughnessTexture' in pbr:
                raise ValueError('Bake normal/occlusion/emissive/metallic-roughness textures before import')
            if m.get('alphaMode', 'OPAQUE') == 'MASK':
                raise ValueError('Alpha MASK requires offline baking; use BLEND or OPAQUE')
            # Runtime uses Lambert + optional Blinn specular, not the glTF PBR BRDF.
            flags = 1 if 'KHR_materials_unlit' in m.get('extensions', {}) else 0
            flags |= 4 if m.get('doubleSided', False) else 0
            texture_index = tex.get('index', -1)
            if texture_index >= 0 and m.get('alphaMode', 'OPAQUE') == 'OPAQUE':
                opaque = copy.deepcopy(self.textures[texture_index])
                pixels = bytearray(opaque['rgba'])
                pixels[3::4] = bytes([255])*(opaque['width']*opaque['height'])
                opaque['rgba'] = bytes(pixels)
                texture_index = len(self.textures)
                self.textures.append(opaque)
            self.materials.append(material(m.get('name', 'Material'), rgb(color[:3]), texture_index,
                                           (1-pbr.get('roughnessFactor', 1))*.8,
                                           max(m.get('emissiveFactor', [0, 0, 0])),
                                           round(color[3]*255) if m.get('alphaMode') == 'BLEND' else 255, flags))
        self.materials.append(material('Default'))

    def view(self, index):
        v = self.doc['bufferViews'][index]
        start, length = v.get('byteOffset', 0), v['byteLength']
        buffer = self.buffers[v['buffer']]
        if start < 0 or length < 0 or start+length > len(buffer):
            raise ValueError('Buffer view outside buffer')
        return buffer[start:start+length]

    def accessor(self, index):
        a = self.doc['accessors'][index]
        fmt = {5120: 'b', 5121: 'B', 5122: 'h', 5123: 'H', 5125: 'I', 5126: 'f'}[a['componentType']]
        width = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}[a['type']]
        size, count = struct.calcsize('<'+fmt*width), a['count']
        if not 0 < count <= 4_000_000:
            raise ValueError('Accessor count outside converter limits')
        def read(view, offset, stride, n, format_string):
            values = self.view(view)
            size = struct.calcsize(format_string)
            if offset < 0 or stride < size or offset+(n-1)*stride+size > len(values):
                raise ValueError('Accessor outside buffer view')
            return [struct.unpack_from(format_string, values, offset+i*stride) for i in range(n)]
        if 'bufferView' in a:
            stride = self.doc['bufferViews'][a['bufferView']].get('byteStride', size)
            result = read(a['bufferView'], a.get('byteOffset', 0), stride, count, '<'+fmt*width)
        else:
            result = [(0,)*width for _ in range(count)]
        if 'sparse' in a:
            s = a['sparse']
            indices, values = s['indices'], s['values']
            if not 0 < s['count'] <= count:
                raise ValueError('Invalid sparse count')
            ifmt = {5121: 'B', 5123: 'H', 5125: 'I'}[indices['componentType']]
            ind = read(indices['bufferView'], indices.get('byteOffset', 0), struct.calcsize(ifmt), s['count'], '<'+ifmt)
            val = read(values['bufferView'], values.get('byteOffset', 0), size, s['count'], '<'+fmt*width)
            previous = -1
            for (i,), value in zip(ind, val):
                if not previous < i < count:
                    raise ValueError('Invalid sparse indices')
                result[i], previous = value, i
        if a.get('normalized') and fmt != 'f':
            scale = {'b': 127, 'B': 255, 'h': 32767, 'H': 65535}.get(fmt)
            if scale is None:
                raise ValueError('Invalid normalized component')
            result = [tuple(max(-1, v/scale) for v in row) for row in result]
        if any(not math.isfinite(v) for row in result for v in row):
            raise ValueError('Non-finite accessor')
        return result

    def mesh(self, index, skin_index=None):
        vertices, faces, uv, ids, weights = [], [], [], [], []
        for primitive in self.doc['meshes'][index]['primitives']:
            if primitive.get('targets') or primitive.get('extensions'):
                raise ValueError('Morph targets and compressed primitives require offline baking')
            attrs = primitive['attributes']
            pos = self.accessor(attrs['POSITION'])
            norm = self.accessor(attrs['NORMAL']) if 'NORMAL' in attrs else None
            coords = self.accessor(attrs['TEXCOORD_0']) if 'TEXCOORD_0' in attrs else [(0, 0)]*len(pos)
            if norm is not None and len(norm) != len(pos) or len(coords) != len(pos):
                raise ValueError('Attribute counts differ')
            indices = [v[0] for v in self.accessor(primitive['indices'])] if 'indices' in primitive else list(range(len(pos)))
            kind = primitive.get('mode', 4)
            if kind == 4:
                if len(indices) % 3:
                    raise ValueError('Incomplete triangle')
                triangles = [indices[i:i+3] for i in range(0, len(indices), 3)]
            elif kind == 5:
                triangles = [(indices[i+(i & 1)], indices[i+1-(i & 1)], indices[i+2]) for i in range(len(indices)-2)]
            elif kind == 6:
                triangles = [(indices[0], indices[i], indices[i+1]) for i in range(1, len(indices)-1)]
            else:
                raise ValueError('Only triangle, strip and fan primitives are supported')
            material_id = primitive.get('material', len(self.materials)-1)
            if not 0 <= material_id < len(self.materials):
                raise ValueError('Material index out of range')
            joints = self.accessor(attrs['JOINTS_0']) if skin_index is not None else None
            w = self.accessor(attrs['WEIGHTS_0']) if skin_index is not None else None
            if joints is not None and (len(joints) != len(pos) or len(w) != len(pos) or 'JOINTS_1' in attrs):
                raise ValueError('Skin requires exactly four joint weights per vertex')
            # Duplicate corners only when normals are absent, preserving hard faces.
            base = len(vertices)
            if norm is not None:
                vertices.extend((*p, *unit(n)) for p, n in zip(pos, norm))
                uv.extend(coords)
                if joints is not None:
                    weights.extend((tuple(map(int, j)), tuple(x/sum(weight) for x in weight)) for j, weight in zip(joints, w))
            for triangle in triangles:
                if any(not isinstance(i, int) or not 0 <= i < len(pos) for i in triangle):
                    raise ValueError('Triangle index out of range')
                if len(set(triangle)) < 3:
                    continue
                if norm is None:
                    a, b, c = [pos[i] for i in triangle]
                    n = unit(cross(sub(b, a), sub(c, a)))
                    face = []
                    for i in triangle:
                        face.append(len(vertices))
                        vertices.append((*pos[i], *n))
                        uv.append(coords[i])
                        if joints is not None:
                            weights.append((tuple(map(int, joints[i])), tuple(x/sum(w[i]) for x in w[i])))
                else:
                    face = [base+i for i in triangle]
                faces.append((*face, self.materials[material_id]['color']))
                ids.append(material_id)
        skin, bind, joint_nodes = None, None, None
        if skin_index is not None:
            s = self.doc['skins'][skin_index]
            joint_nodes, skin = s['joints'], weights
            matrices = self.accessor(s['inverseBindMatrices']) if 'inverseBindMatrices' in s else [IDENTITY]*len(joint_nodes)
            bind = [tuple(m[c*4+r] for r in range(4) for c in range(4)) for m in matrices]
        return Mesh(vertices, faces, uv, copy.deepcopy(self.materials), copy.deepcopy(self.textures), ids, skin, bind, joint_nodes)

    def export(self, output, compact, c_dir=None, symbol='asset'):
        from asset_pipeline import save
        d, catalog, mapping, nodes = self.doc, [], {}, copy.deepcopy(self.doc.get('nodes', []))
        if len(nodes) > 128:
            raise ValueError('Scene exceeds 128 nodes')
        if any('matrix' in node for node in nodes):
            raise ValueError('Export node transforms as TRS before import (matrix nodes are unsupported)')
        if any(mesh.get('weights') for mesh in d.get('meshes', [])):
            raise ValueError('Morph weights require offline baking')
        for node in nodes:
            if 'mesh' in node:
                key = (node['mesh'], node.get('skin'))
                if key not in mapping:
                    mapping[key] = len(catalog)
                    catalog.append(self.mesh(*key))
                node['mesh_id'] = mapping[key]
        if not catalog:
            catalog = [self.mesh(i) for i in range(len(d.get('meshes', [])))]
        if not catalog:
            raise ValueError('No triangle meshes in glTF')
        paths = []
        for i, mesh in enumerate(catalog):
            path = output if i == 0 else output.with_stem(output.stem+f'-mesh{i}')
            save(mesh, path, compact, c_dir if i == 0 else None, symbol)
            paths.append(path.name)
        parents = [-1]*len(nodes)
        for i, node in enumerate(nodes):
            for child in node.get('children', []):
                if not 0 <= child < len(nodes) or parents[child] != -1 or child == i:
                    raise ValueError('Invalid node hierarchy')
                parents[child] = i
        for i in range(len(nodes)):
            visited, parent = set(), i
            while parent != -1:
                if parent in visited:
                    raise ValueError('Node cycle')
                visited.add(parent)
                parent = parents[parent]
        node_bytes = bytearray()
        for i, n in enumerate(nodes):
            pose = (*n.get('translation', [0, 0, 0]), *n.get('rotation', [0, 0, 0, 1]), *n.get('scale', [1, 1, 1]))
            node_bytes += struct.pack('<iI10fII', parents[i], n.get('mesh_id', 0xffffffff), *pose, 1, 0)
        animations = d.get('animations', [])
        if not animations:
            animations = [{}]
        for number, animation in enumerate(animations):
            tracks, keys, duration = bytearray(), bytearray(), 0
            channels = {}
            for channel in animation.get('channels', []):
                target = channel['target']
                if target['path'] not in ('translation', 'rotation', 'scale'):
                    raise ValueError('Morph animation is unsupported')
                sampler = animation['samplers'][channel['sampler']]
                times = [v[0] for v in self.accessor(sampler['input'])]
                values = self.accessor(sampler['output'])
                method = sampler.get('interpolation', 'LINEAR')
                if method not in ('LINEAR', 'STEP'):
                    raise ValueError('Bake CUBICSPLINE animation to LINEAR before import')
                if len(times) != len(values) or any(a >= b for a, b in zip(times, times[1:])):
                    raise ValueError('Invalid animation sampler')
                channels.setdefault(target['node'], {})[target['path']] = (times, values, method)
                duration = max(duration, times[-1])
            for node, curves in sorted(channels.items()):
                if not 0 <= node < len(nodes):
                    raise ValueError('Animated node out of range')
                # Bake complete TRS tracks at 30 Hz, retaining every original key time.
                times = sorted({0, duration, *(min(duration, i/30) for i in range(math.ceil(duration*30)+1)),
                                *(t for curve in curves.values() for t in curve[0])})
                first = len(keys)//44
                for t in times:
                    pose = []
                    for path, default in (('translation', [0, 0, 0]), ('rotation', [0, 0, 0, 1]), ('scale', [1, 1, 1])):
                        value = nodes[node].get(path, default)
                        if path in curves:
                            ct, cv, method = curves[path]
                            right = bisect.bisect_right(ct, t)
                            left = max(0, right-1)
                            right = min(len(ct)-1, right)
                            f = (t-ct[left])/(ct[right]-ct[left]) if ct[right] > ct[left] and method != 'STEP' else 0
                            a, b = cv[left], cv[right]
                            if path == 'rotation' and sum(x*y for x, y in zip(a, b)) < 0:
                                b = [-x for x in b]
                            if path == 'rotation':
                                dot = max(-1, min(1, sum(x*y for x, y in zip(a, b))))
                                angle = math.acos(dot)
                                value = [x*(1-f)+y*f for x, y in zip(a, b)] if angle < 1e-5 else [
                                    (x*math.sin((1-f)*angle)+y*math.sin(f*angle))/math.sin(angle) for x, y in zip(a, b)]
                                length = math.sqrt(sum(x*x for x in value))
                                value = [x/length for x in value]
                            else:
                                value = [x*(1-f)+y*f for x, y in zip(a, b)]
                        pose.extend(value)
                    keys += struct.pack('<11f', t, *pose)
                tracks += struct.pack('<4I', node, first, len(times), int(all(c[2] == 'STEP' for c in curves.values())))
            payload = node_bytes+tracks+keys
            header = struct.pack('<4s6IfII', b'D3SC', 1, len(nodes), len(tracks)//16, len(keys)//44,
                                 len(payload), zlib.crc32(payload), duration, 1, 0)
            scene_path = output.with_suffix('.d3s') if number == 0 else output.with_stem(output.stem+f'-animation{number}').with_suffix('.d3s')
            scene_path.write_bytes(header+payload)
        output.with_suffix('.scene.json').write_text(json.dumps(dict(models=paths, nodes=len(nodes),
            animations=len(d.get('animations', [])), shading='Lambert/Blinn approximation of base color and roughness'), indent=2)+'\n', encoding='utf8')
        return catalog[0]

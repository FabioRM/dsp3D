"""Importer fixtures exercise file formats without third-party Python modules.
SPDX-License-Identifier: BSD-3-Clause
"""
import base64
import json
from pathlib import Path
import struct
import subprocess
import sys
import unittest
import zlib
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import asset_pipeline as a
import import_models as im

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/native/imports'
OUT.mkdir(parents=True, exist_ok=True)


def png():
    def chunk(kind, data):
        return struct.pack('>I', len(data))+kind+data+struct.pack('>I', zlib.crc32(kind+data))
    return (b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR', struct.pack('>IIBBBBB', 2, 1, 8, 6, 0, 0, 0))+
            chunk(b'IDAT', zlib.compress(b'\0\xff\0\0\xff\0\xff\0\x80'))+chunk(b'IEND', b''))


def fixture():
    data = struct.pack('<9f3H', -1, -1, 0, 1, -1, 0, 0, 1, 0, 0, 1, 2)
    data += b'\0'*(-len(data) % 4)
    time_offset = len(data)
    data += struct.pack('<2f6f', 0, 1, 0, 0, 0, 1, 0, 0)
    doc = dict(asset=dict(version='2.0'), buffers=[dict(byteLength=len(data), uri='data:application/octet-stream;base64,'+base64.b64encode(data).decode())],
        bufferViews=[dict(buffer=0, byteOffset=0, byteLength=36), dict(buffer=0, byteOffset=36, byteLength=6),
                     dict(buffer=0, byteOffset=time_offset, byteLength=8), dict(buffer=0, byteOffset=time_offset+8, byteLength=24)],
        accessors=[dict(bufferView=0, componentType=5126, count=3, type='VEC3'),
                   dict(bufferView=1, componentType=5123, count=3, type='SCALAR'),
                   dict(bufferView=2, componentType=5126, count=2, type='SCALAR'),
                   dict(bufferView=3, componentType=5126, count=2, type='VEC3')],
        meshes=[dict(primitives=[dict(attributes=dict(POSITION=0), indices=1, material=0)])],
        materials=[dict(name='Red', doubleSided=True, pbrMetallicRoughness=dict(baseColorFactor=[1, 0, 0, 1]))],
        nodes=[dict(mesh=0)], scenes=[dict(nodes=[0])], scene=0,
        animations=[dict(samplers=[dict(input=2, output=3)], channels=[dict(sampler=0, target=dict(node=0, path='translation'))])])
    return doc, data


class ImportTests(unittest.TestCase):
    def load_c(self, mesh, name):
        path = OUT / (name+'.d3m')
        a.save(mesh, path, True)
        subprocess.run([str(ROOT / 'build/native/asset-tests.exe'), str(path)], check=True)

    def test_png_and_crc(self):
        tex = im.image(png())
        self.assertEqual(tex['rgba'], bytes([255, 0, 0, 255, 0, 255, 0, 128]))
        bad = bytearray(png())
        bad[20] ^= 1
        with self.assertRaises(ValueError):
            im.image(bad)

    def test_obj_mtl_negative_indices_polygon(self):
        (OUT / 'texture.png').write_bytes(png())
        (OUT / 'test.mtl').write_text('newmtl Paint\nKd 0.8 0.4 0.2\nKs .2 .2 .2\nd 0.75\nmap_Kd texture.png\n')
        path = OUT / 'test.obj'
        path.write_text('mtllib test.mtl\nv -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nusemtl Paint\nf -4/1 -3/2 -2/3 -1/4\n')
        mesh = im.obj(path)
        self.assertEqual(len(mesh.triangles), 2)
        self.assertEqual(mesh.materials[1]['opacity'], 191)
        self.load_c(mesh, 'obj')

    def test_gltf_glb_animation(self):
        doc, binary = fixture()
        for glb in (False, True):
            path = OUT / ('triangle.glb' if glb else 'triangle.gltf')
            if glb:
                del doc['buffers'][0]['uri']
                content = json.dumps(doc).encode()
                content += b' '*(-len(content) % 4)
                binary += b'\0'*(-len(binary) % 4)
                path.write_bytes(struct.pack('<4sII', b'glTF', 2, 12+8+len(content)+8+len(binary))+
                    struct.pack('<II', len(content), 0x4e4f534a)+content+struct.pack('<II', len(binary), 0x004e4942)+binary)
            else:
                path.write_text(json.dumps(doc))
            output = OUT / ('glb.d3m' if glb else 'gltf.d3m')
            mesh = im.Gltf(path).export(output, True)
            self.load_c(mesh, 'glb' if glb else 'gltf')
            scene = output.with_suffix('.d3s').read_bytes()
            self.assertEqual(zlib.crc32(scene[40:]), struct.unpack_from('<I', scene, 24)[0])
            self.assertEqual(struct.unpack_from('<I', scene, 12)[0], 1)
            subprocess.run([str(ROOT / 'build/native/scene-asset-tests.exe'), str(output), str(output.with_suffix('.d3s'))], check=True)

    def test_sparse_and_rejection(self):
        doc, _ = fixture()
        path = OUT / 'sparse.gltf'
        # Zero base values patched by sparse indices 0,1,2 from the index view.
        doc['accessors'][0].pop('bufferView')
        doc['accessors'][0]['sparse'] = dict(count=3, indices=dict(bufferView=1, componentType=5123), values=dict(bufferView=0))
        path.write_text(json.dumps(doc))
        self.assertEqual(im.Gltf(path).accessor(0)[0], (-1, -1, 0))
        doc['extensionsRequired'] = ['KHR_draco_mesh_compression']
        path.write_text(json.dumps(doc))
        with self.assertRaises(ValueError):
            im.Gltf(path)
        with self.assertRaises(ValueError):
            im.local_uri(OUT, '../outside.bin')


if __name__ == '__main__':
    unittest.main()

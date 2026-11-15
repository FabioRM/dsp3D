"""Model conversion regression tests; standard library only."""
import importlib.util
import math
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('converter', ROOT / 'tools/convert_model.py')
converter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(converter)


class ConversionTests(unittest.TestCase):
    def test_generated_models_are_reproducible(self):
        for name, crease, color, count in [('metamask', 0, 0xed8a28, 188), ('suzanne', 65, 0x79b8e8, 968)]:
            vertices, faces, blob, report = converter.convert(ROOT / f'assets/source/{name}.stl', crease, color)
            self.assertEqual(blob, (ROOT / f'sdcard/models/{name}.d3m').read_bytes())
            self.assertEqual(len(faces), count)
            self.assertLess(len(vertices), count * 3)
            self.assertEqual(struct.unpack_from('<I', blob, 20)[0], zlib.crc32(blob[32:]))
            for vertex in vertices:
                self.assertAlmostEqual(sum(x*x for x in vertex[3:]), 1.0, places=5)
            self.assertEqual(report['removed_degenerate'], 0)

    def test_bad_stl(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'bad.stl'
            for data in [b'', b'not an STL', b'solid x\nvertex nan 0 0\nendsolid x',
                         b'solid x\nvertex 0 0 0\nendsolid x']:
                path.write_bytes(data)
                with self.assertRaises((ValueError, UnicodeError)):
                    converter.convert(path)

    def test_binary_detection_does_not_depend_on_header(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'solid.stl'
            record = struct.pack('<12fH', 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0)
            path.write_bytes(b'solid misleading header'.ljust(80, b'\0') + struct.pack('<I', 1) + record)
            self.assertEqual(len(converter.read_stl(path)), 1)


if __name__ == '__main__':
    unittest.main()

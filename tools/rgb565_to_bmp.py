#!/usr/bin/env python3
"""Convert a raw little-endian RGB565 framebuffer to BMP or PNG (no dependencies)."""
import argparse
from pathlib import Path
import struct
import zlib


def convert(source, destination, width, height):
    raw = Path(source).read_bytes()
    if width <= 0 or height <= 0 or len(raw) != width*height*2:
        raise ValueError('Framebuffer dimensions do not match its byte length')
    if Path(destination).suffix.lower() == '.png':
        pixels = bytearray()
        for y in range(height):
            pixels.append(0)
            for x in range(width):
                value = struct.unpack_from('<H', raw, (y*width+x)*2)[0]
                pixels.extend(((value >> 11)*255//31, ((value >> 5) & 63)*255//63, (value & 31)*255//31))
        def chunk(kind, data):
            return struct.pack('>I', len(data))+kind+data+struct.pack('>I', zlib.crc32(kind+data))
        image = b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR', struct.pack('>2I5B', width, height, 8, 2, 0, 0, 0))
        image += chunk(b'IDAT', zlib.compress(pixels, 9))+chunk(b'IEND', b'')
        Path(destination).write_bytes(image)
        return
    stride = (width*3+3) & ~3
    header = struct.pack('<2sIHHI', b'BM', 54+stride*height, 0, 0, 54)
    header += struct.pack('<IiiHHIIiiII', 40, width, height, 1, 24, 0, stride*height, 2835, 2835, 0, 0)
    with Path(destination).open('wb') as output:
        output.write(header)
        for y in reversed(range(height)):
            row = bytearray()
            for x in range(width):
                value = struct.unpack_from('<H', raw, (y*width+x)*2)[0]
                row.extend(((value & 31)*255//31, ((value >> 5) & 63)*255//63, (value >> 11)*255//31))
            output.write(row)
            output.write(b'\0'*(stride-len(row)))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    parser.add_argument('width', type=int)
    parser.add_argument('height', type=int)
    args = parser.parse_args()
    convert(args.source, args.destination, args.width, args.height)

#!/usr/bin/env python3
"""Convert a raw little-endian RGB565 framebuffer to a BMP (no dependencies)."""
import argparse
from pathlib import Path
import struct


def convert(source, destination, width, height):
    raw = Path(source).read_bytes()
    if width <= 0 or height <= 0 or len(raw) != width*height*2:
        raise ValueError('Framebuffer dimensions do not match its byte length')
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

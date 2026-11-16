#!/usr/bin/env python3
"""Capture the active LTDC framebuffer through the repository-local SEGGER DLL.
The core is briefly halted and resumed without resetting the application.
SPDX-License-Identifier: BSD-3-Clause
"""
import argparse
import ctypes as c
from pathlib import Path
import struct
from rgb565_to_bmp import convert


class Probe:
    def __init__(self, board):
        root = Path(__file__).resolve().parent
        self.dll = c.WinDLL(str(root / 'local/programmer/bin/JLink_x64.dll'))
        d = self.dll
        d.JLINKARM_Open.restype = c.c_char_p
        d.JLINKARM_ExecCommand.argtypes = [c.c_char_p, c.c_void_p, c.c_int]
        d.JLINKARM_ReadMemU32.argtypes = [c.c_uint32, c.c_uint32, c.c_void_p, c.c_void_p]
        d.JLINKARM_WriteMem.argtypes = [c.c_uint32, c.c_uint32, c.c_void_p]
        self.halted = False
        error = d.JLINKARM_Open()
        if error:
            raise RuntimeError(error.decode(errors='replace'))
        try:
            reply = c.create_string_buffer(256)
            device = b'STM32F769NI' if board == 'f769' else b'STM32F746NG'
            if d.JLINKARM_ExecCommand(b'Device = '+device, reply, len(reply)) < 0:
                raise RuntimeError(reply.value.decode(errors='replace'))
            d.JLINKARM_TIF_Select(1)
            d.JLINKARM_SetSpeed(4000)
            if d.JLINKARM_Connect() < 0:
                raise RuntimeError('J-Link target connection failed')
            if self.word(0xe0042000) & 0xfff != (0x451 if board == 'f769' else 0x449):
                raise RuntimeError('Connected MCU does not match requested board')
        except Exception:
            self.close()
            raise

    def word(self, address):
        return struct.unpack('<I', self.read(address, 4))[0]

    def read(self, address, size):
        if size % 4:
            raise ValueError('Word-aligned read size required')
        data = (c.c_uint32 * (size//4))()
        if self.dll.JLINKARM_ReadMemU32(address, size//4, data, None) != size//4:
            raise RuntimeError('Incomplete target memory read')
        return bytes(data)

    def halt(self):
        if self.dll.JLINKARM_Halt() < 0:
            raise RuntimeError('Cannot halt target')
        self.halted = True

    def close(self):
        if self.halted:
            self.dll.JLINKARM_Go()
            self.halted = False
        self.dll.JLINKARM_Close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board', choices=['f746', 'f769'], default='f769')
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    width, height = (800, 480) if args.board == 'f769' else (480, 272)
    probe = Probe(args.board)
    try:
        probe.halt()
        address = probe.word(0x400168ac)
        if not 0xc0000000 <= address <= 0xc0000000+width*height*2:
            raise RuntimeError('LTDC framebuffer is outside the viewer buffers')
        data = probe.read(address, width*height*2)
    finally:
        probe.close()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    raw = args.output.with_suffix('.rgb565')
    raw.write_bytes(data)
    convert(raw, args.output, width, height)
    print(f'Saved {args.output} from framebuffer 0x{address:08x}')


if __name__ == '__main__':
    main()

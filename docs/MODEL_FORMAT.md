# dsp3D binary model v1

All fields are little-endian. Header size is 32 bytes; payload begins on a
four-byte boundary. Floating-point values are IEEE-754 binary32.

| Offset | Field | Encoding |
| --- | --- | --- |
| 0 | Magic | ASCII `DSP3` |
| 4 | Version | uint32, 1 |
| 8 | Vertex count | uint32 |
| 12 | Triangle count | uint32 |
| 16 | Payload bytes | uint32 |
| 20 | Payload CRC | CRC-32/ISO-HDLC, same as Python zlib.crc32 |
| 24 | Reserved | two zero uint32 values |

The payload contains `vertex_count` records of six floats (position XYZ, unit
normal XYZ), followed by `triangle_count` records of four uint32 values
(three zero-based vertex indices and color `0x00RRGGBB`). There is no padding.

`dsp3d_model_open` checks alignment, endianness, exact length, count arithmetic,
reserved fields, CRC, finite coordinates, unit normals, and index bounds. On
failure it clears the output mesh. It returns a view into the input buffer;
the application owns that buffer and keeps it alive. Build the library with
`-fno-strict-aliasing` when using this zero-copy byte-buffer interface, as the
provided builds do. Embedded demo assets use typed C structures instead.

The firmware viewer additionally limits file size and vertex count to the
configured SDRAM arena and scratch capacity. It mounts the card read-only and
does not parse STL on the microcontroller.

#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Independently verify fresh known-positive PNG outputs using Python zlib/CRC.

Usage: python3 -B -O THIS_FILE /absolute/path/to/dust3d_png_encoding_tests
Only this controlled encoder's two authored RGBA fixtures reach decompression.
"""
import binascii
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib


def require(condition, cause):
    if not condition:
        raise ValueError(cause)


def verify_png_bytes(data, width, height, pixels):
    """Check a controlled encoder output against independently supplied RGBA bytes."""
    require(data[:8] == b'\x89PNG\r\n\x1a\n', 'PNG signature changed')
    offset = 8
    chunks = []
    compressed = bytearray()
    while offset < len(data):
        require(offset + 12 <= len(data), 'truncated PNG chunk')
        size = struct.unpack_from('>I', data, offset)[0]
        kind = data[offset+4:offset+8]
        end = offset + 8 + size
        require(end + 4 <= len(data), 'chunk exceeds output')
        body = data[offset+8:end]
        checksum = struct.unpack_from('>I', data, end)[0]
        require(binascii.crc32(kind + body) & 0xffffffff == checksum, 'chunk CRC differs')
        chunks.append(kind)
        if kind == b'IHDR':
            require(body == struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0), 'RGBA header differs')
        elif kind == b'IDAT':
            compressed.extend(body)
        elif kind == b'IEND':
            require(not body and end + 4 == len(data), 'invalid terminal chunk')
        else:
            raise ValueError('unexpected metadata chunk')
        offset = end + 4
    require(chunks[0:1] == [b'IHDR'] and chunks[-1:] == [b'IEND'], 'chunk order differs')
    require(chunks[1:-1] and all(kind == b'IDAT' for kind in chunks[1:-1]), 'data chunks missing/reordered')
    decoder = zlib.decompressobj()
    raw = decoder.decompress(compressed) + decoder.flush()
    require(decoder.eof and not decoder.unused_data, 'zlib stream not complete')
    row_bytes = width * 4
    expected = b''.join(b'\0' + pixels[row*row_bytes:(row+1)*row_bytes] for row in range(height))
    require(raw == expected, 'pixel bytes, alpha or row order differ')


def verify(path, width, height, pixels):
    verify_png_bytes(path.read_bytes(), width, height, pixels)


def main(executable):
    small = bytes([255, 0, 0, 128, 0, 255, 0, 64, 0, 0, 255, 32, 1, 2, 3, 0])
    large = bytearray()
    state = 123456789
    for _ in range(128 * 129 * 4):
        state = (state * 1664525 + 1013904223) & 0xffffffff
        large.append(state >> 24)
    with tempfile.TemporaryDirectory(dir=os.environ.get('DUST3D_BUILD_ROOT')) as directory:
        path = Path(directory) / 'known-positive.png'
        subprocess.run([executable, str(path)], check=True, timeout=30)
        verify(path, 2, 2, small)
        subprocess.run([executable, str(path), 'large'], check=True, timeout=30)
        verify(path, 128, 129, large)
    print('Independent PNG header/CRC/deflate/RGBA/alpha/row-order acceptance passed')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('usage: verify_png_encoding.py PNG_TEST_EXECUTABLE')
    main(sys.argv[1])

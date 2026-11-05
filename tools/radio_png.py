"""Decode non-interlaced 8-bit RGB/RGBA PNGs into top-down RGBA bytes."""
import struct
import zlib


def rgba_png(data):
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('Not a PNG')
    offset = 8
    chunks = []
    header = None
    while offset + 12 <= len(data):
        size = struct.unpack_from('>I', data, offset)[0]
        end = offset + 12 + size
        if end > len(data):
            raise ValueError('Truncated PNG')
        kind = data[offset+4:offset+8]
        content = data[offset+8:offset+8+size]
        crc = struct.unpack_from('>I', data, offset+8+size)[0]
        if zlib.crc32(kind + content) != crc:
            raise ValueError('PNG CRC mismatch')
        if kind == b'IHDR':
            header = struct.unpack('>IIBBBBB', content)
        elif kind == b'IDAT':
            chunks.append(content)
        elif kind == b'IEND':
            break
        offset = end
    if not header:
        raise ValueError('Missing PNG header')
    width, height, depth, color, compression, filtering, interlace = header
    if not width or not height or depth != 8 or color not in (2, 6) or (compression, filtering, interlace) != (0, 0, 0):
        raise ValueError('Expected non-interlaced 8-bit RGB or RGBA PNG')
    bpp = 3 if color == 2 else 4
    stride = width * bpp
    raw = zlib.decompress(b''.join(chunks))
    if len(raw) != height * (stride + 1):
        raise ValueError('Unexpected PNG pixel size')
    previous = bytearray(stride)
    output = bytearray(width * height * 4)
    for y in range(height):
        start = y * (stride + 1)
        kind = raw[start]
        if kind > 4:
            raise ValueError('Unsupported PNG row filter')
        row = bytearray(raw[start+1:start+1+stride])
        for i in range(stride):
            left = row[i-bpp] if i >= bpp else 0
            above = previous[i]
            corner = previous[i-bpp] if i >= bpp else 0
            if kind == 1:
                predictor = left
            elif kind == 2:
                predictor = above
            elif kind == 3:
                predictor = (left + above) // 2
            elif kind == 4:
                p = left + above - corner
                pa, pb, pc = abs(p-left), abs(p-above), abs(p-corner)
                predictor = left if pa <= pb and pa <= pc else above if pb <= pc else corner
            else:
                predictor = 0
            row[i] = (row[i] + predictor) & 255
        for x in range(width):
            source = x*bpp
            target = (y*width+x)*4
            output[target:target+3] = row[source:source+3]
            output[target+3] = row[source+3] if bpp == 4 else 255
        previous = row
    return width, height, bytes(output)

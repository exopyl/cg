"""Generate the directory tree icons of peles as 32-bit uncompressed TGA.

The RmlUi GL2 backend only decodes uncompressed 24/32-bit TGA, hence the
format. Shapes are rasterised with 8x8 supersampling; no external asset.

Usage: python make_tree_icons.py <output_dir> [preview.png]
"""
import math
import struct
import sys
import zlib
from pathlib import Path

SS = 8


def hexcolor(h, a=255):
    h = h.lstrip('#')
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), a)


def rrect(x0, y0, x1, y1, r):
    def inside(x, y):
        if x < x0 or x > x1 or y < y0 or y > y1:
            return False
        cx = min(max(x, x0 + r), x1 - r)
        cy = min(max(y, y0 + r), y1 - r)
        return (x - cx) ** 2 + (y - cy) ** 2 <= r * r
    return inside


def polygon(points):
    def inside(x, y):
        n = len(points)
        c = False
        j = n - 1
        for i in range(n):
            xi, yi = points[i]
            xj, yj = points[j]
            if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
                c = not c
            j = i
        return c
    return inside


def polyline(points, width):
    half = width / 2.0

    def seg_dist(px, py, a, b):
        ax, ay = a
        bx, by = b
        dx, dy = bx - ax, by - ay
        t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)))
        return math.hypot(px - (ax + t * dx), py - (ay + t * dy))

    def inside(x, y):
        return any(seg_dist(x, y, points[i], points[i + 1]) <= half
                   for i in range(len(points) - 1))
    return inside


def circle(cx, cy, r):
    return lambda x, y: (x - cx) ** 2 + (y - cy) ** 2 <= r * r


def render(w, h, layers):
    """layers: list of (shape, rgba). Straight-alpha 'over' compositing."""
    pixels = []
    for py in range(h):
        row = []
        for px in range(w):
            r = g = b = a = 0.0
            for shape, (cr, cg, cb, ca) in layers:
                hits = 0
                for sy in range(SS):
                    for sx in range(SS):
                        if shape(px + (sx + 0.5) / SS, py + (sy + 0.5) / SS):
                            hits += 1
                sa = hits / (SS * SS) * ca / 255.0
                if sa <= 0.0:
                    continue
                na = sa + a * (1.0 - sa)
                r = (cr * sa + r * a * (1.0 - sa)) / na
                g = (cg * sa + g * a * (1.0 - sa)) / na
                b = (cb * sa + b * a * (1.0 - sa)) / na
                a = na
            row.append((round(r), round(g), round(b), round(a * 255)))
        pixels.append(row)
    return pixels


def write_tga(path, pixels):
    h = len(pixels)
    w = len(pixels[0])
    # id length, colour map type, image type 2 (uncompressed true colour),
    # colour map spec (5 bytes), x/y origin, width, height, 32 bpp,
    # descriptor 0x28 = 8 alpha bits + top-to-bottom row order.
    header = struct.pack('<BBB5sHHHHBB', 0, 0, 2, b'\0' * 5, 0, 0, w, h, 32, 0x28)
    body = bytearray()
    for row in pixels:
        for r, g, b, a in row:
            body += bytes((b, g, r, a))
    Path(path).write_bytes(header + bytes(body))


def write_preview_png(path, images, scale=8, bg=(0x26, 0x26, 0x28)):
    width = sum(len(p[0]) * scale + scale * 2 for p in images)
    height = max(len(p) for p in images) * scale
    canvas = [[bg for _ in range(width)] for _ in range(height)]
    ox = 0
    for p in images:
        for y, row in enumerate(p):
            for x, (r, g, b, a) in enumerate(row):
                al = a / 255.0
                c = tuple(round(v * al + bgv * (1 - al)) for v, bgv in zip((r, g, b), bg))
                for yy in range(scale):
                    for xx in range(scale):
                        canvas[y * scale + yy][ox + x * scale + xx] = c
        ox += len(p[0]) * scale + scale * 2
    raw = b''.join(b'\0' + bytes(v for px in row for v in px) for row in canvas)

    def chunk(tag, data):
        return (struct.pack('>I', len(data)) + tag + data +
                struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff))
    png = (b'\x89PNG\r\n\x1a\n' +
           chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)) +
           chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))
    Path(path).write_bytes(png)


CHEVRON = hexcolor('#a8a8ae')
FOLDER_BACK = hexcolor('#c08f3c')
FOLDER_FRONT = hexcolor('#e2b55a')
FOLDER_FLAP = hexcolor('#eec56c')
DRIVE_TOP = hexcolor('#a4aab2')
DRIVE_FACE = hexcolor('#7a8088')
DRIVE_LED = hexcolor('#62c862')


def icons():
    return {
        'tree-chevron-closed': render(12, 16, [
            (polyline([(4.6, 4.8), (7.8, 8.0), (4.6, 11.2)], 1.6), CHEVRON)]),
        'tree-chevron-open': render(12, 16, [
            (polyline([(2.8, 6.4), (6.0, 9.6), (9.2, 6.4)], 1.6), CHEVRON)]),
        'tree-folder-closed': render(16, 16, [
            (rrect(1.0, 2.5, 7.0, 6.0, 1.0), FOLDER_BACK),
            (rrect(1.0, 4.0, 15.0, 13.5, 1.0), FOLDER_BACK),
            (rrect(1.0, 5.5, 15.0, 13.5, 1.0), FOLDER_FRONT)]),
        'tree-folder-open': render(16, 16, [
            (rrect(1.0, 2.5, 7.0, 6.0, 1.0), FOLDER_BACK),
            (rrect(1.0, 4.0, 13.5, 13.5, 1.0), FOLDER_BACK),
            (polygon([(3.6, 7.0), (15.6, 7.0), (13.2, 13.5), (0.6, 13.5)]), FOLDER_FLAP)]),
        'tree-drive': render(16, 16, [
            (rrect(1.0, 4.5, 15.0, 11.5, 1.2), DRIVE_TOP),
            (rrect(1.0, 8.0, 15.0, 11.5, 1.2), DRIVE_FACE),
            (circle(12.2, 9.75, 0.95), DRIVE_LED)]),
    }


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    images = icons()
    for name, pixels in images.items():
        write_tga(out / (name + '.tga'), pixels)
        print('wrote', out / (name + '.tga'))
    if len(sys.argv) > 2:
        write_preview_png(sys.argv[2], list(images.values()))


if __name__ == '__main__':
    main()

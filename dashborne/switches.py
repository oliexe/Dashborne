"""Render original geometric switch assets with no external build dependencies."""
import struct
import zlib


def png(width, height, pixels):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    rows = b''.join(b'\0' + pixels[y*width*4:(y+1)*width*4] for y in range(height))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b''))


def write_switches(destination):
    destination.mkdir(parents=True, exist_ok=True)
    for name, track, knob, center in [
        ('off', (48, 50, 56), (195, 199, 209), 18),
        ('on', (107, 119, 133), (241, 243, 245), 54),
        ('disabled', (25, 26, 29), (76, 79, 87), 18),
    ]:
        pixels = bytearray()
        for y in range(36):
            for x in range(72):
                sums = [0, 0, 0]; covered = 0
                for sy in range(4):
                    for sx in range(4):
                        px, py = x+(sx+.5)/4, y+(sy+.5)/4
                        nearest = min(54, max(18, px))
                        if (px-nearest)**2 + (py-18)**2 > 18**2:
                            continue
                        color = knob if (px-center)**2 + (py-18)**2 <= 13**2 else track
                        covered += 1
                        for i in range(3): sums[i] += color[i]
                pixels.extend([round(s/covered) if covered else 0 for s in sums] + [round(covered*255/16)])
        (destination / ('toggle-' + name + '.png')).write_bytes(png(72, 36, pixels))


def write_boot_button(destination):
    """Antialiased rounded backgrounds for navigation and the footer action."""
    for name, width, height, radius, color in [
        ('continue', 300, 48, 14, (91, 102, 115)),
        ('continue-pressed', 300, 48, 14, (74, 84, 96)),
        ('nav-idle', 400, 64, 14, (0, 0, 0)),
        ('nav-selected', 400, 64, 14, (23, 26, 30)),
        ('nav-pressed', 400, 64, 14, (30, 34, 39)),
        ('content-panel', 1060, 376, 20, (7, 8, 10)),
    ]:
        pixels = bytearray()
        for y in range(height):
            for x in range(width):
                if radius <= x < width-radius or radius <= y < height-radius:
                    pixels.extend((*color, 255))
                    continue
                covered = 0
                for sy in range(4):
                    for sx in range(4):
                        px, py = x+(sx+.5)/4, y+(sy+.5)/4
                        cx = min(width-radius, max(radius, px))
                        cy = min(height-radius, max(radius, py))
                        covered += (px-cx)**2 + (py-cy)**2 <= radius**2
                pixels.extend((*color, round(covered*255/16)))
        (destination / (name + '.png')).write_bytes(png(width, height, pixels))

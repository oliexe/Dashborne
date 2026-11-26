"""Generate neutral direction-cue lighting; no vehicle artwork is included."""
from pathlib import Path
import math
from PIL import Image

def generate(directory):
    directory.mkdir(parents=True, exist_ok=True)
    width, height = 220, 313
    for reverse in (False, True):
        image = Image.new('RGBA', (width, height))
        for y in range(height):
            depth = y / (height - 1)
            spread = 35 + 55 * (depth if reverse else 1 - depth)
            fade = math.sin(math.pi * depth) ** 1.4
            for x in range(width):
                alpha = round(42 * fade * math.exp(-((x - width / 2) / spread) ** 4))
                image.putpixel((x, y), (255, 255, 255, alpha))
        image.save(directory / ('reverse-glow.png' if reverse else 'forward-glow.png'), optimize=True)

if __name__ == '__main__':
    generate(Path(__file__).resolve().parent / 'assets')

# Builds sprite-replacements/spr_pressspace.png: "Press B to continue" from the game's own
# "Press Space to continue" letters, with a hand-pixeled B in the same style.
# Usage: python3 make_pressb.py <original spr_pressspace png> <out png>
import sys
from PIL import Image
src = Image.open(sys.argv[1]).convert('RGBA'); W, H = src.size
white = [[src.getpixel((x, y))[3] > 128 and sum(src.getpixel((x, y))[:3]) > 384 for x in range(W)] for y in range(H)]
def cols(a, b): return [[white[y][x] for x in range(a, b)] for y in range(H)]
press = cols(0, 42)       # "Press"
rest = cols(92, W)        # "to continue"
B = ["######..", "##..###.", "##..###.", "##..###.", "##..###.", "#######.", "##...###", "##...###", "##...###", "##...###", "########", "#######."]
bglyph = [[False] * 8 for _ in range(H)]
for i, row in enumerate(B):
    for x, c in enumerate(row): bglyph[2 + i][x] = c == '#'
space = lambda n: [[False] * n for _ in range(H)]
parts = [press, space(4), bglyph, space(5), rest]
mask = [sum((p[y] for p in parts), []) for y in range(H)]
mw = len(mask[0]); off = W - mw  # right-aligned: the sprite's origin is its bottom-right corner
out = Image.new('RGBA', (W, H), (0, 0, 0, 0))
isw = lambda x, y: 0 <= y < H and 0 <= x - off < mw and mask[y][x - off]
for y in range(H):
    for x in range(W):
        if isw(x, y): out.putpixel((x, y), (255, 255, 255, 255)); continue
        # 2 px black outline with clipped corners, like the original lettering
        if any(isw(x + dx, y + dy) for dx in range(-2, 3) for dy in range(-2, 3) if abs(dx) + abs(dy) <= 3):
            out.putpixel((x, y), (0, 0, 0, 255))
out.save(sys.argv[2])
print('text width', mw, 'offset', off)

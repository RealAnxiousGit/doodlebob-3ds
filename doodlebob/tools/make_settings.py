# Builds sprite-replacements/spr_fullscreen_<frame>.png (8 frames): "Settings" in the game's own button lettering,
# replacing "Fullscreen On/OFF" (a 3DS has no fullscreen toggle; the runner turns that button into the Settings menu).
# Every letter is cut from the same animation frame of another button, so the grow animation matches:
#   S, e, t from "Select" (spr_levselect), i from "Quit" (spr_quitgame), n, s from "Fullscreen" (spr_fullscreen),
#   and the G from "Game" (spr_newgame) stands in for g: in this font capitals and lowercase are the same height.
# Usage: python make_settings.py <dir with the exported frames, e.g. spr_levselect_0.png> <out dir>
# (Export the frames with tools/export-menu-art.csx.)
import os
import sys
from collections import deque
from PIL import Image

src, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
FRAMES = 8


def load(name, f):
    return Image.open(os.path.join(src, f'{name}_{f}.png')).convert('RGBA')


def word_runs(im):
    """Column runs of non-transparent pixels (the words; letters inside a word touch)."""
    a = im.split()[3].load()
    cols = [any(a[x, y] > 0 for y in range(im.height)) for x in range(im.width)]
    runs, start = [], None
    for x, c in enumerate(cols + [False]):
        if c and start is None:
            start = x
        if not c and start is not None:
            runs.append((start, x - 1))
            start = None
    return runs


# (sprite, word index, letter span in that word on frame 7)
LETTERS = {
    'S': ('spr_levselect', 3, 0, 36),
    'e': ('spr_levselect', 3, 36, 68),
    't': ('spr_levselect', 3, 134, 164),
    'i': ('spr_quitgame', 0, 64, 79),
    'n': ('spr_fullscreen', 0, 229, 262),
    'G': ('spr_newgame', 1, 0, 44),
    's': ('spr_fullscreen', 0, 93, 119),
}
WORD = 'SettinGs'


def is_white(p):
    return p[3] > 128 and p[0] > 200 and p[1] > 200 and p[2] > 200


def outline_width(im):
    """Black pixels between the outside and the white fill, at the middle row of the first letter."""
    px = im.load()
    y = im.height // 2
    x = 0
    while x < im.width and px[x, y][3] == 0:
        x += 1
    n = 0
    while x < im.width and px[x, y][3] > 0 and not is_white(px[x, y]):
        n += 1
        x += 1
    return max(2, n)


def cut_letter(im, x0, x1, ow):
    """The letter whose white fill is biggest in columns x0..x1, with its own outline (pixels near that fill)."""
    px = im.load()
    W, H = im.size
    seen = set()
    best = []
    for sx in range(max(0, x0), min(W, x1 + 1)):
        for sy in range(H):
            if (sx, sy) in seen or not is_white(px[sx, sy]):
                continue
            comp, q = [], deque([(sx, sy)])
            seen.add((sx, sy))
            while q:
                x, y = q.popleft()
                comp.append((x, y))
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if 0 <= nx < W and 0 <= ny < H and (nx, ny) not in seen and is_white(px[nx, ny]):
                        seen.add((nx, ny))
                        q.append((nx, ny))
            inside = sum(1 for x, _ in comp if x0 <= x <= x1)
            if inside > len([p for p in best if x0 <= p[0] <= x1]):
                best = comp
    # On some frames neighbouring fills touch (the "ls" in "Fullscreen"): keep only the letter's own columns.
    fill = set(p for p in best if x0 <= p[0] <= x1)
    r = ow + 1
    keep = set()
    for x, y in fill:
        for dx in range(-r, r + 1):
            for dy in range(-r, r + 1):
                if abs(dx) + abs(dy) <= r + 1:
                    keep.add((x + dx, y + dy))
    xs = [x for x, _ in keep]
    ys = [y for _, y in keep]
    bx0, by0, bx1, by1 = max(0, min(xs)), max(0, min(ys)), min(W - 1, max(xs)), min(H - 1, max(ys))
    glyph = Image.new('RGBA', (bx1 - bx0 + 1, by1 - by0 + 1), (0, 0, 0, 0))
    g = glyph.load()
    for x, y in keep:
        if not (bx0 <= x <= bx1 and by0 <= y <= by1) or px[x, y][3] == 0:
            continue
        # Outline pixels near this letter, and only this letter's own white (not a touching neighbour's).
        if is_white(px[x, y]) and (x, y) not in fill:
            continue
        g[x - bx0, y - by0] = px[x, y]
    fx = [x for x, _ in fill]
    return glyph, by1, min(fx) - bx0, max(fx) - bx0


ref_runs = {name: word_runs(load(name, 7)) for name in {v[0] for v in LETTERS.values()}}
union = None
for f in range(FRAMES):
    target = load('spr_fullscreen', f)  # canvas size and baseline come from the frame being replaced
    W, H = target.size
    bottom = max(y for y in range(H) if any(target.getpixel((x, y))[3] > 0 for x in range(W)))
    ow = outline_width(load('spr_levselect', f))
    glyphs = []
    for ch in WORD:
        name, word, a, b = LETTERS[ch]
        im = load(name, f)
        runs = word_runs(im)
        ra, rb = ref_runs[name][word]
        fa, fb = runs[word]
        k = (fb - fa) / (rb - ra)
        glyphs.append(cut_letter(im, fa + round(a * k), fa + round(b * k), ow))
    # Letters touch outline to outline, as in the game: two outlines between neighbouring fills.
    gap = ow * 2
    total = 0
    xs = []
    for i, (g, _, fl, fr) in enumerate(glyphs):
        x = 0 if i == 0 else xs[-1] + glyphs[i - 1][3] + gap - fl
        xs.append(x)
    total = xs[-1] + glyphs[-1][0].width
    left = (W - total) // 2
    canvas = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    # Draw right to left so each letter's outline sits under the white of the letter before it.
    for (g, gb, _, _), x in reversed(list(zip(glyphs, xs))):
        layer = Image.new('RGBA', (W, H), (0, 0, 0, 0))
        layer.paste(g, (left + x, bottom - g.height + 1))
        canvas = Image.alpha_composite(layer, canvas)
    canvas.save(os.path.join(out, f'spr_fullscreen_{f}.png'))
    box = canvas.getbbox()
    union = box if union is None else (min(union[0], box[0]), min(union[1], box[1]), max(union[2], box[2]), max(union[3], box[3]))
    print(f'frame {f}: outline {ow}px, text {total}px wide in {W}x{H}')
# Opaque area over all frames (inclusive right/bottom), for tools/patch-data.csx.
with open(os.path.join(out, 'spr_fullscreen.margins'), 'w') as fh:
    fh.write(f'{union[0]} {union[1]} {union[2] - 1} {union[3] - 1}\n')
print('margins', union)

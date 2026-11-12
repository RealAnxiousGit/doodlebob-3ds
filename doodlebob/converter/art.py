# The port's art, made on the player's PC from their own copy of the game (nothing from the game ships with the tool).
# Same results as tools/make_settings.py, make_pressb.py, make_icon.py and make_banner.py.
import array
import io
import wave
from collections import deque

from PIL import Image, ImageDraw, ImageFilter


# ---------- "Settings" button (replaces "Fullscreen On/OFF", 8 grow frames) ----------

def _word_runs(im):
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
_LETTERS = {
    'S': ('spr_levselect', 3, 0, 36),
    'e': ('spr_levselect', 3, 36, 68),
    't': ('spr_levselect', 3, 134, 164),
    'i': ('spr_quitgame', 0, 64, 79),
    'n': ('spr_fullscreen', 0, 229, 262),
    'G': ('spr_newgame', 1, 0, 44),
    's': ('spr_fullscreen', 0, 93, 119),
}
_WORD = 'SettinGs'


def _is_white(p):
    return p[3] > 128 and p[0] > 200 and p[1] > 200 and p[2] > 200


def _outline_width(im):
    px = im.load()
    y = im.height // 2
    x = 0
    while x < im.width and px[x, y][3] == 0:
        x += 1
    n = 0
    while x < im.width and px[x, y][3] > 0 and not _is_white(px[x, y]):
        n += 1
        x += 1
    return max(2, n)


def _cut_letter(im, x0, x1, ow):
    px = im.load()
    W, H = im.size
    seen = set()
    best = []
    for sx in range(max(0, x0), min(W, x1 + 1)):
        for sy in range(H):
            if (sx, sy) in seen or not _is_white(px[sx, sy]):
                continue
            comp, q = [], deque([(sx, sy)])
            seen.add((sx, sy))
            while q:
                x, y = q.popleft()
                comp.append((x, y))
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if 0 <= nx < W and 0 <= ny < H and (nx, ny) not in seen and _is_white(px[nx, ny]):
                        seen.add((nx, ny))
                        q.append((nx, ny))
            inside = sum(1 for x, _ in comp if x0 <= x <= x1)
            if inside > len([p for p in best if x0 <= p[0] <= x1]):
                best = comp
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
        if _is_white(px[x, y]) and (x, y) not in fill:
            continue
        g[x - bx0, y - by0] = px[x, y]
    fx = [x for x, _ in fill]
    return glyph, by1, min(fx) - bx0, max(fx) - bx0


def settings_button(dw):
    """Returns ({('spr_fullscreen', frame): image}, (left, top, right, bottom) margins)."""
    load = dw.sprite_frame
    names = {v[0] for v in _LETTERS.values()}
    ref_runs = {name: _word_runs(load(name, 7)) for name in names}
    frames = {}
    union = None
    for f in range(8):
        target = load('spr_fullscreen', f)
        W, H = target.size
        bottom = target.getbbox()[3] - 1
        ow = _outline_width(load('spr_levselect', f))
        glyphs = []
        cache = {}
        for ch in _WORD:
            name, word, a, b = _LETTERS[ch]
            im = cache.setdefault(name, load(name, f))
            runs = _word_runs(im)
            ra, rb = ref_runs[name][word]
            fa, fb = runs[word]
            k = (fb - fa) / (rb - ra)
            glyphs.append(_cut_letter(im, fa + round(a * k), fa + round(b * k), ow))
        gap = ow * 2
        xs = []
        for i, (g, _, fl, fr) in enumerate(glyphs):
            xs.append(0 if i == 0 else xs[-1] + glyphs[i - 1][3] + gap - fl)
        total = xs[-1] + glyphs[-1][0].width
        left = (W - total) // 2
        canvas = Image.new('RGBA', (W, H), (0, 0, 0, 0))
        for (g, gb, _, _), x in reversed(list(zip(glyphs, xs))):
            layer = Image.new('RGBA', (W, H), (0, 0, 0, 0))
            layer.paste(g, (left + x, bottom - g.height + 1))
            canvas = Image.alpha_composite(layer, canvas)
        frames[('spr_fullscreen', f)] = canvas
        box = canvas.getbbox()
        union = box if union is None else (min(union[0], box[0]), min(union[1], box[1]),
                                           max(union[2], box[2]), max(union[3], box[3]))
    return frames, (union[0], union[1], union[2] - 1, union[3] - 1)


# ---------- "Press B to continue" (replaces "Press Space to continue") ----------

_B = ["######..", "##..###.", "##..###.", "##..###.", "##..###.", "#######.", "##...###", "##...###", "##...###",
      "##...###", "########", "#######."]


def press_b(dw):
    src = dw.sprite_frame('spr_pressspace', 0)
    W, H = src.size
    px = src.load()
    white = [[px[x, y][3] > 128 and sum(px[x, y][:3]) > 384 for x in range(W)] for y in range(H)]

    def cols(a, b):
        return [[white[y][x] for x in range(a, b)] for y in range(H)]

    bglyph = [[False] * 8 for _ in range(H)]
    for i, row in enumerate(_B):
        for x, c in enumerate(row):
            bglyph[2 + i][x] = c == '#'

    def space(n):
        return [[False] * n for _ in range(H)]

    parts = [cols(0, 42), space(4), bglyph, space(5), cols(92, W)]
    mask = [sum((p[y] for p in parts), []) for y in range(H)]
    mw = len(mask[0])
    off = W - mw
    out = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    o = out.load()

    def isw(x, y):
        return 0 <= y < H and 0 <= x - off < mw and mask[y][x - off]

    for y in range(H):
        for x in range(W):
            if isw(x, y):
                o[x, y] = (255, 255, 255, 255)
            elif any(isw(x + dx, y + dy) for dx in range(-2, 3) for dy in range(-2, 3) if abs(dx) + abs(dy) <= 3):
                o[x, y] = (0, 0, 0, 255)
    return out


# ---------- HOME Menu icon (48x48) ----------

def icon(dw):
    src = dw.sprite_frame('spr_titlebob', 0)
    crop = src.crop((20, 85, 390, 455))
    a = crop.split()[3]
    dark = Image.eval(crop.convert('L'), lambda v: 255 if v < 90 else 0)
    dark = Image.composite(dark, Image.new('L', crop.size, 0), a)
    thick = dark.filter(ImageFilter.MaxFilter(7))
    bg = Image.new('RGBA', crop.size, (0, 0, 0, 0))
    ImageDraw.Draw(bg).rounded_rectangle((0, 0, crop.size[0] - 1, crop.size[1] - 1), radius=60,
                                         fill=(66, 160, 235, 255))
    img = Image.alpha_composite(bg, crop)
    img = Image.composite(Image.new('RGBA', crop.size, (0, 0, 0, 255)), img, thick)
    return img.resize((48, 48), Image.LANCZOS)


# ---------- CIA banner (256x128, transparent) and its 2.9 s sound ----------

def _fit(img, w=None, h=None):
    if w is not None:
        h = round(img.height * w / img.width)
    else:
        w = round(img.width * h / img.height)
    return img.resize((w, h), Image.LANCZOS)


def _shadow(img, radius, alpha):
    a = img.split()[3].filter(ImageFilter.GaussianBlur(radius)).point(lambda v: int(v * alpha))
    s = Image.new('RGBA', img.size, (0, 0, 0, 0))
    s.putalpha(a)
    return s


def banner(dw):
    """Returns (256x128 RGBA image, WAV bytes)."""
    W, H = 256, 128
    out = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    logo = _fit(dw.sprite_frame('spr_title2', 0), w=214)
    out.alpha_composite(_shadow(logo, 2, 0.6), (8, 8))
    out.alpha_composite(logo, (6, 5))
    bob = dw.sprite_frame('spr_titlebobsmall', 0)
    bob = _fit(bob.crop(bob.getbbox()), h=104)
    bx, by = W - bob.width - 2, H - bob.height + 2
    out.alpha_composite(_shadow(bob, 3, 0.55), (bx + 3, by + 3))
    out.alpha_composite(bob, (bx, by))

    with wave.open(io.BytesIO(dw.sound_bytes('msc_title'))) as w:
        rate, ch, width = w.getframerate(), w.getnchannels(), w.getsampwidth()
        if width != 2:
            raise ValueError('title theme is not 16-bit audio')
        pcm = array.array('h', w.readframes(int(rate * 2.9)))
    frames = len(pcm) // ch
    fade = int(rate * 0.5)
    stereo = array.array('h')
    for i in range(frames):
        gain = min(1.0, (frames - i) / fade)
        left = pcm[i * ch]
        right = pcm[i * ch + 1] if ch > 1 else left
        stereo.append(int(left * gain))
        stereo.append(int(right * gain))
    buf = io.BytesIO()
    with wave.open(buf, 'wb') as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(stereo.tobytes())
    return out, buf.getvalue()

# Builds the CIA's HOME Menu banner (256x128) and banner sound from the game's own title art and music.
# Usage: python3 make_banner.py <art dir> <out dir>
#   <art dir> holds spr_title2.png, spr_titlebobsmall.png and msc_title.wav,
#   exported from data.win with tools/export-banner-art.csx.
# Writes banner.png, banner.wav (first 2.9 s of the title theme, faded out) and banner_preview.png (3x).
import os
import sys
import wave
import array
from PIL import Image, ImageFilter

src, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
W, H = 256, 128


def load(name):
    return Image.open(os.path.join(src, name)).convert('RGBA')


def fit(img, w=None, h=None):
    if w is not None:
        h = round(img.height * w / img.width)
    else:
        w = round(img.width * h / img.height)
    return img.resize((w, h), Image.LANCZOS)


def shadow(img, radius, alpha):
    a = img.split()[3].filter(ImageFilter.GaussianBlur(radius))
    a = a.point(lambda v: int(v * alpha))
    s = Image.new('RGBA', img.size, (0, 0, 0, 0))
    s.putalpha(a)
    return s


# Transparent background: only the logo and DoodleBob (with soft shadows) show on the HOME Menu.
banner = Image.new('RGBA', (W, H), (0, 0, 0, 0))

# The logo across the top, DoodleBob in front of its right half.
logo = fit(load('spr_title2.png'), w=214)
lx, ly = 6, 5
banner.alpha_composite(shadow(logo, 2, 0.6), (lx + 2, ly + 3))
banner.alpha_composite(logo, (lx, ly))

bob = load('spr_titlebobsmall.png')
bob = bob.crop(bob.getbbox())
bob = fit(bob, h=104)
bx, by = W - bob.width - 2, H - bob.height + 2
banner.alpha_composite(shadow(bob, 3, 0.55), (bx + 3, by + 3))
banner.alpha_composite(bob, (bx, by))

banner.save(os.path.join(out, 'banner.png'))
# Preview at 3x on a checkerboard, so the transparent areas are visible.
big = banner.resize((W * 3, H * 3), Image.NEAREST)
board = Image.new('RGBA', big.size, (200, 200, 200, 255))
for y in range(0, big.height, 24):
    for x in range(0, big.width, 24):
        if (x // 24 + y // 24) % 2:
            board.paste((150, 150, 150, 255), (x, y, x + 24, y + 24))
board.alpha_composite(big)
board.save(os.path.join(out, 'banner_preview.png'))

# Sound: the title theme's opening, 2.9 s (the HOME Menu limit is about 3 s), with a 0.5 s fade, stereo 16-bit.
with wave.open(os.path.join(src, 'msc_title.wav')) as w:
    rate, ch, width = w.getframerate(), w.getnchannels(), w.getsampwidth()
    assert width == 2, 'expected 16-bit audio'
    n = int(rate * 2.9)
    pcm = array.array('h', w.readframes(n))
frames = len(pcm) // ch
fade = int(rate * 0.5)
stereo = array.array('h')
for i in range(frames):
    gain = min(1.0, (frames - i) / fade)
    left = pcm[i * ch]
    right = pcm[i * ch + 1] if ch > 1 else left
    stereo.append(int(left * gain))
    stereo.append(int(right * gain))
with wave.open(os.path.join(out, 'banner.wav'), 'wb') as w:
    w.setnchannels(2)
    w.setsampwidth(2)
    w.setframerate(rate)
    w.writeframes(stereo.tobytes())
print('banner.png', banner.size, 'banner.wav', round(frames / rate, 2), 's at', rate)

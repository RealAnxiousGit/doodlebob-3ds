# Builds the 48x48 3DS home-menu icon from the game's title-screen DoodleBob (spr_titlebob).
# Usage: python3 make_icon.py <spr_titlebob png> <out icon.png> [preview png]
import sys
from PIL import Image, ImageFilter, ImageDraw
src = Image.open(sys.argv[1]).convert('RGBA')
crop = src.crop((20, 85, 390, 455))           # head, body and most of the pencil
# Thicken the doodle lines so they survive the 8x downscale.
a = crop.split()[3]
dark = Image.eval(crop.convert('L'), lambda v: 255 if v < 90 else 0)
dark = Image.composite(dark, Image.new('L', crop.size, 0), a)
thick = dark.filter(ImageFilter.MaxFilter(7))
bg = Image.new('RGBA', crop.size, (0, 0, 0, 0))
d = ImageDraw.Draw(bg)
d.rounded_rectangle((0, 0, crop.size[0] - 1, crop.size[1] - 1), radius=60, fill=(66, 160, 235, 255))  # title-screen sky blue
img = Image.alpha_composite(bg, crop)
black = Image.new('RGBA', crop.size, (0, 0, 0, 255))
img = Image.composite(black, img, thick)
icon = img.resize((48, 48), Image.LANCZOS)
icon.save(sys.argv[2])
if len(sys.argv) > 3: icon.resize((192, 192), Image.NEAREST).save(sys.argv[3])
print('alpha at body centre:', src.getpixel((190, 300))[3])

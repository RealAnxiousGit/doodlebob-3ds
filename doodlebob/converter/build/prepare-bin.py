# Fills converter/bin with what the converter ships (no game data in any of it):
#   doodlebob.elf / doodlebob.3dsx  the runner (the .3dsx with its icon area blanked; the converter adds the icon)
#   doodlebob.rsf                   CIA settings
#   makerom.exe, bannertool.exe     CIA tools (Windows builds from their GitHub releases, MIT)
#   n3ds-preprocess.exe             built by build/build-preprocess-win.sh
#   How to install.txt, licenses/
# Run after building the runner: py converter/build/prepare-bin.py
import os
import shutil
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
CONV = os.path.dirname(HERE)
PROJECT = os.path.dirname(CONV)
RUNNER = r'\\wsl.localhost\Ubuntu\home\claude\doodlebob-3ds\cinnamon'
BIN = os.path.join(CONV, 'bin')
os.makedirs(os.path.join(BIN, 'licenses'), exist_ok=True)

shutil.copy2(os.path.join(RUNNER, 'build', 'n3ds', 'cinnamon.elf'), os.path.join(BIN, 'doodlebob.elf'))
data = bytearray(open(os.path.join(RUNNER, 'build', 'n3ds', 'cinnamon.3dsx'), 'rb').read())
assert data[:4] == b'3DSX' and struct.unpack_from('<H', data, 4)[0] >= 0x2C
off, size = struct.unpack_from('<II', data, 0x20)
assert data[off:off + 4] == b'SMDH'
data[off:off + size] = b'SMDH' + bytes(size - 4)  # icon and titles come from the player's copy
open(os.path.join(BIN, 'doodlebob.3dsx'), 'wb').write(data)

shutil.copy2(os.path.join(PROJECT, 'tools', 'doodlebob.rsf'), os.path.join(BIN, 'doodlebob.rsf'))
shutil.copy2(os.path.join(CONV, 'vendor', 'makerom', 'makerom.exe'), BIN)
shutil.copy2(os.path.join(CONV, 'vendor', 'bannertool', 'bannertool-1.2.3-windows', 'bannertool.exe'), BIN)
shutil.copy2(os.path.join(CONV, 'How to install.txt'), BIN)
shutil.copy2(os.path.join(CONV, 'vendor', 'bannertool', 'bannertool-1.2.3-windows', 'LICENSE.txt'),
             os.path.join(BIN, 'licenses', 'bannertool-LICENSE.txt'))
shutil.copy2(os.path.join(CONV, 'vendor', 'makerom', 'LICENSE.txt'), os.path.join(BIN, 'licenses', 'makerom-LICENSE.txt'))
shutil.copy2(os.path.join(RUNNER, 'LICENSE'), os.path.join(BIN, 'licenses', 'cinnamon-LICENSE.txt'))
assert os.path.isfile(os.path.join(BIN, 'n3ds-preprocess.exe')), 'build n3ds-preprocess.exe first'
for n in sorted(os.listdir(BIN)):
    p = os.path.join(BIN, n)
    print(f'{n:28} {os.path.getsize(p) if os.path.isfile(p) else "":>12}')

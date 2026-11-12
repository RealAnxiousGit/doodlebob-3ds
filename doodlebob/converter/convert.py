# The conversion itself: the player's copy of DoodleBob and the Magic Pencil DX: Doodler's Cut -> 3DS files.
# Used by gui.py; also runs from a command line: python convert.py <game .exe or data.win> <output folder>
import hashlib
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

from dwfile import DataWin
import art

VERSION = (0, 3, 0)
# data.win of the supported release (Doodler's Cut, March 2021). The patch and the port's fixes are made for it.
DATA_SHA256 = 'b5bee0c8360adf3b585f3d0877e1d3d9db03d25204e20cd60cd51bacfe3ab331'
SD_FOLDER = os.path.join('SD card', '3ds', 'DoodleBob')
NO_WINDOW = 0x08000000 if os.name == 'nt' else 0  # CREATE_NO_WINDOW: no console windows flashing up


class ConvertError(Exception):
    pass


def resource(*parts):
    """Files shipped inside the converter (bin/), whether running as a script or as the packed .exe."""
    base = getattr(sys, '_MEIPASS', os.path.dirname(os.path.abspath(__file__)))
    return os.path.join(base, 'bin', *parts)


def run(args, log, cwd=None, on_line=None):
    proc = subprocess.Popen(args, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            creationflags=NO_WINDOW, text=True, errors='replace')
    tail = []
    for line in proc.stdout:
        line = line.rstrip()
        tail = (tail + [line])[-15:]
        if on_line:
            on_line(line)
    if proc.wait() != 0:
        log('\n'.join(tail))
        raise ConvertError(f'{os.path.basename(args[0])} failed (exit code {proc.returncode})')


# ---------- finding data.win ----------

def _carve_cab(exe_bytes):
    """IExpress packages (one .exe holding the game folder) carry a Microsoft cabinet: return it, or None."""
    i = exe_bytes.find(b'MSCF\0\0\0\0')
    while i >= 0:
        size = struct.unpack_from('<I', exe_bytes, i + 8)[0]
        if 0 < size <= len(exe_bytes) - i:
            return exe_bytes[i:i + size]
        i = exe_bytes.find(b'MSCF\0\0\0\0', i + 1)
    return None


def _carve_form(exe_bytes):
    """Single-exe GameMaker builds keep data.win inside the .exe (a FORM chunk starting with GEN8)."""
    i = exe_bytes.find(b'FORM')
    while i >= 0:
        if exe_bytes[i + 8:i + 12] == b'GEN8':
            size = struct.unpack_from('<I', exe_bytes, i + 4)[0] + 8
            if size <= len(exe_bytes) - i:
                return exe_bytes[i:i + size]
        i = exe_bytes.find(b'FORM', i + 1)
    return None


def find_data_win(game_path, work, log):
    if os.path.basename(game_path).lower() == 'data.win':
        return game_path
    beside = os.path.join(os.path.dirname(game_path), 'data.win')
    if os.path.isfile(beside):
        log(f'Using data.win next to {os.path.basename(game_path)}')
        return beside
    with open(game_path, 'rb') as f:
        exe = f.read()
    cab = _carve_cab(exe)
    if cab is not None:
        log('Unpacking the game from the .exe...')
        cab_path = os.path.join(work, 'game.cab')
        out = os.path.join(work, 'game')
        os.makedirs(out, exist_ok=True)
        with open(cab_path, 'wb') as f:
            f.write(cab)
        expand = os.path.join(os.environ.get('SystemRoot', r'C:\Windows'), 'System32', 'expand.exe')
        run([expand, cab_path, '-F:*', out], lambda s: None)
        for root, _, names in os.walk(out):
            for n in names:
                if n.lower() == 'data.win':
                    return os.path.join(root, n)
        raise ConvertError('The .exe unpacked, but there was no data.win inside.')
    form = _carve_form(exe)
    if form is not None:
        path = os.path.join(work, 'data.win')
        with open(path, 'wb') as f:
            f.write(form)
        return path
    raise ConvertError("Couldn't find the game's data in that file. Pick the game's .exe as you downloaded it, "
                       'or data.win from the game folder.')


# ---------- 3DS files ----------

def _smdh(icon_png, work, log):
    out = os.path.join(work, 'icon.icn')
    run([resource('bannertool.exe'), 'makesmdh', '-s', 'DoodleBob 3DS',
         '-l', 'DoodleBob and the Magic Pencil DX (personal port)', '-p', 'Ethan Wake fangame',
         '-i', icon_png, '-o', out], log)
    with open(out, 'rb') as f:
        return out, f.read()


def _write_3dsx(smdh, path):
    """The runner .3dsx ships with an empty icon area; fill in the icon made from the player's copy."""
    with open(resource('doodlebob.3dsx'), 'rb') as f:
        data = bytearray(f.read())
    if data[:4] != b'3DSX' or struct.unpack_from('<H', data, 4)[0] < 0x2C:
        raise ConvertError('runner .3dsx has no icon area')
    offset, size = struct.unpack_from('<II', data, 0x20)
    if size != len(smdh):
        raise ConvertError('unexpected icon size')
    data[offset:offset + size] = smdh
    with open(path, 'wb') as f:
        f.write(data)


def convert(game_path, out_dir, log=print, progress=lambda f: None):
    """Converts the game. progress(f) gets 0..1. Returns the paths it wrote."""
    os.makedirs(out_dir, exist_ok=True)
    work = tempfile.mkdtemp(prefix='doodlebob3ds-')
    try:
        progress(0.01)
        data_path = find_data_win(game_path, work, log)
        log('Checking the game version...')
        h = hashlib.sha256()
        with open(data_path, 'rb') as f:
            for block in iter(lambda: f.read(1 << 20), b''):
                h.update(block)
        if h.hexdigest() != DATA_SHA256:
            raise ConvertError("This isn't the supported version of the game. The converter needs "
                               "DoodleBob and the Magic Pencil DX: Doodler's Cut (the March 2021 release), unmodified.")
        progress(0.04)

        log('Making the 3DS menu art from your copy...')
        dw = DataWin(data_path)
        frames, margins = art.settings_button(dw)
        frames[('spr_pressspace', 0)] = art.press_b(dw)
        icon_png = os.path.join(work, 'icon.png')
        art.icon(dw).save(icon_png)
        banner_png, banner_wav = os.path.join(work, 'banner.png'), os.path.join(work, 'banner.wav')
        img, wav = art.banner(dw)
        img.save(banner_png)
        with open(banner_wav, 'wb') as f:
            f.write(wav)
        progress(0.08)

        log('Patching the game data...')
        patched = os.path.join(work, 'data.win')
        dw.write_patched(patched, frames, {'spr_fullscreen': margins})
        del dw
        progress(0.12)

        sd = os.path.join(out_dir, SD_FOLDER)
        if os.path.isdir(sd):
            for name in ('gfx', 'audio'):
                shutil.rmtree(os.path.join(sd, name), ignore_errors=True)
        os.makedirs(sd, exist_ok=True)
        log('Converting graphics and sound for the 3DS (this takes a few minutes)...')
        tools = os.path.join(work, 'tools')
        os.makedirs(tools)
        pre = os.path.join(tools, 'n3ds-preprocess.exe')
        shutil.copy2(resource('n3ds-preprocess.exe'), pre)  # it keeps its staging files next to itself
        pages = re.compile(r'Wrote packed atlas page (\d+)')

        def on_line(line):
            m = pages.search(line)
            if m:
                progress(0.12 + 0.6 * min(1.0, (int(m.group(1)) + 1) / 480))
            elif 'loudness' in line:
                progress(0.75)

        run([pre, patched, sd, '--native-t3x'], log, cwd=tools, on_line=on_line)
        shutil.copy2(patched, os.path.join(sd, 'data.win'))
        progress(0.88)

        log('Building DoodleBob.3dsx and DoodleBob.cia...')
        icn_path, smdh = _smdh(icon_png, work, log)
        _write_3dsx(smdh, os.path.join(sd, 'DoodleBob.3dsx'))
        bnr = os.path.join(work, 'banner.bnr')
        run([resource('bannertool.exe'), 'makebanner', '-i', banner_png, '-a', banner_wav, '-o', bnr], log)
        cia = os.path.join(out_dir, 'DoodleBob.cia')
        major, minor, micro = (str(v) for v in VERSION)
        run([resource('makerom.exe'), '-f', 'cia', '-o', cia, '-target', 't', '-exefslogo',
             '-elf', resource('doodlebob.elf'), '-rsf', resource('doodlebob.rsf'), '-icon', icn_path,
             '-banner', bnr, '-major', major, '-minor', minor, '-micro', micro], log, cwd=work)
        shutil.copy2(resource('How to install.txt'), os.path.join(out_dir, 'How to install.txt'))
        progress(1.0)
        log('Done.')
        return {'cia': cia, '3dsx': os.path.join(sd, 'DoodleBob.3dsx'), 'sd': sd}
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print('usage: convert.py <game .exe or data.win> <output folder>')
        sys.exit(2)
    try:
        print(convert(sys.argv[1], sys.argv[2], progress=lambda f: None))
    except ConvertError as e:
        print('Error:', e)
        sys.exit(1)

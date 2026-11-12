# Minimal reader and patcher for this game's data.win (GameMaker: Studio 1.4, bytecode 16).
# Reads sprites, backgrounds, texture pages and embedded sounds; writes a copy with replaced sprite frames and margins.
import io
import struct

from PIL import Image

PNG_MAGIC = b'\x89PNG\r\n\x1a\n'
BLOB_ALIGN = 128  # texture PNGs start on 128-byte boundaries in the original file


class TexItem:
    """A texture page item (TPAG): where a frame's pixels sit on a texture page."""

    def __init__(self, data, off):
        (self.src_x, self.src_y, self.src_w, self.src_h, self.tgt_x, self.tgt_y, self.tgt_w, self.tgt_h,
         self.bound_w, self.bound_h, self.page) = struct.unpack_from('<11H', data, off)

    @property
    def trimmed(self):
        return self.tgt_w < self.bound_w or self.tgt_h < self.bound_h


class DataWin:
    def __init__(self, path):
        with open(path, 'rb') as f:
            self.data = f.read()
        d = self.data
        if d[:4] != b'FORM':
            raise ValueError('not a GameMaker data file')
        self.chunks = {}
        self.chunk_order = []
        o = 8
        while o < len(d):
            name = d[o:o + 4].decode('ascii', 'replace')
            size = self.u32(o + 4)
            self.chunks[name] = (o, size)
            self.chunk_order.append(name)
            o += 8 + size
        self._pages = {}
        self.sprites = {}
        for p in self.ptrlist('SPRT'):
            self.sprites[self.cstr(self.u32(p))] = p
        self.backgrounds = {}
        for p in self.ptrlist('BGND'):
            self.backgrounds[self.cstr(self.u32(p))] = p
        self.sounds = {}
        for p in self.ptrlist('SOND'):
            self.sounds[self.cstr(self.u32(p))] = p

    def u32(self, o):
        return struct.unpack_from('<I', self.data, o)[0]

    def i32(self, o):
        return struct.unpack_from('<i', self.data, o)[0]

    def cstr(self, ptr):
        if ptr == 0:
            return None
        return self.data[ptr:self.data.index(b'\0', ptr)].decode('utf-8', 'replace')

    def ptrlist(self, chunk):
        start = self.chunks[chunk][0] + 8
        return [self.u32(start + 4 + 4 * i) for i in range(self.u32(start))]

    # ---- texture pages ----
    def page_blob(self, index):
        entries = self.ptrlist('TXTR')
        start = self.u32(entries[index] + 4)
        assert self.data[start:start + 8] == PNG_MAGIC, 'texture page is not a PNG'
        return start

    def page_image(self, index):
        if index not in self._pages:
            self._pages[index] = Image.open(io.BytesIO(self.data[self.page_blob(index):])).convert('RGBA')
        return self._pages[index]

    # ---- sprites ----
    def sprite_items(self, name):
        p = self.sprites[name]
        count = self.i32(p + 0x38)
        if count < 0:
            raise ValueError('unexpected sprite format (newer GameMaker)')
        return [TexItem(self.data, self.u32(p + 0x3C + 4 * i)) for i in range(count)]

    def item_image(self, item):
        """The frame at its full size, transparent around the stored (trimmed) area, like UndertaleModTool's export."""
        page = self.page_image(item.page)
        part = page.crop((item.src_x, item.src_y, item.src_x + item.src_w, item.src_y + item.src_h))
        if (item.tgt_w, item.tgt_h) != (item.src_w, item.src_h):
            part = part.resize((item.tgt_w, item.tgt_h), Image.BILINEAR)
        out = Image.new('RGBA', (item.bound_w, item.bound_h), (0, 0, 0, 0))
        out.paste(part, (item.tgt_x, item.tgt_y))
        return out

    def sprite_frame(self, name, frame=0):
        return self.item_image(self.sprite_items(name)[frame])

    def background_image(self, name):
        p = self.backgrounds[name]
        return self.item_image(TexItem(self.data, self.u32(p + 16)))

    # ---- sounds ----
    def sound_bytes(self, name):
        """The embedded file (WAV or OGG) of a sound."""
        p = self.sounds[name]
        audio_id = self.i32(p + 32)
        a = self.ptrlist('AUDO')[audio_id]
        return self.data[a + 4:a + 4 + self.u32(a)]

    # ---- patching ----
    def write_patched(self, path, frames, margins):
        """Writes a copy with sprite frames replaced and sprite margins set.
        frames: {(sprite, frame): RGBA image, full frame size}; margins: {sprite: (left, top, right, bottom)}."""
        d = bytearray(self.data)
        for name, (left, top, right, bottom) in margins.items():
            p = self.sprites[name]
            struct.pack_into('<4i', d, p + 12, left, right, bottom, top)  # stored as left, right, bottom, top
        changed = set()
        for (name, frame), img in frames.items():
            item = self.sprite_items(name)[frame]
            img = img.convert('RGBA')
            if img.size == (item.bound_w, item.bound_h) and item.trimmed:
                # Most frames are stored trimmed: keep only the stored area (the art must fit inside it).
                box = img.getbbox()
                if box and (box[0] < item.tgt_x or box[1] < item.tgt_y or box[2] > item.tgt_x + item.tgt_w
                            or box[3] > item.tgt_y + item.tgt_h):
                    raise ValueError(f'{name} frame {frame}: new art is larger than the stored area')
                img = img.crop((item.tgt_x, item.tgt_y, item.tgt_x + item.tgt_w, item.tgt_y + item.tgt_h))
            if img.size != (item.src_w, item.src_h):
                raise ValueError(f'{name} frame {frame}: expected {item.src_w}x{item.src_h}, got {img.size}')
            page = self.page_image(item.page)
            page.paste((0, 0, 0, 0), (item.src_x, item.src_y, item.src_x + item.src_w, item.src_y + item.src_h))
            page.paste(img, (item.src_x, item.src_y))
            changed.add(item.page)

        # Rebuild the texture PNGs (the end of TXTR), then shift the sounds (AUDO, the last chunk) by the size change.
        if self.chunk_order[-2:] != ['TXTR', 'AUDO']:
            raise ValueError('unexpected chunk order')
        txtr_off, txtr_size = self.chunks['TXTR']
        entries = self.ptrlist('TXTR')
        starts = [self.u32(e + 4) for e in entries]
        region_start = starts[0]
        region_end = txtr_off + 8 + txtr_size
        blobs = bytearray()
        new_starts = []
        for i, s in enumerate(starts):
            end = starts[i + 1] if i + 1 < len(starts) else region_end
            if i in changed:
                buf = io.BytesIO()
                self._pages[i].save(buf, 'PNG', optimize=False, compress_level=6)
                blob = buf.getvalue()
            else:
                blob = bytes(self.data[s:end])
            new_starts.append(region_start + len(blobs))
            blobs += blob
            if i + 1 < len(starts):
                blobs += b'\0' * (-len(blobs) % BLOB_ALIGN)
        # Keep the sounds on the same alignment: pad so the size change is a multiple of BLOB_ALIGN.
        delta = (region_start + len(blobs)) - region_end
        pad = -delta % BLOB_ALIGN
        blobs += b'\0' * pad
        delta += pad
        for e, s in zip(entries, new_starts):
            struct.pack_into('<I', d, e + 4, s)
        struct.pack_into('<I', d, txtr_off + 4, txtr_size + delta)
        audo_off, audo_size = self.chunks['AUDO']
        audo = bytearray(d[audo_off:audo_off + 8 + audo_size])
        n = struct.unpack_from('<I', audo, 8)[0]
        for i in range(n):
            ptr = struct.unpack_from('<I', audo, 12 + 4 * i)[0]
            struct.pack_into('<I', audo, 12 + 4 * i, ptr + delta)
        out = bytes(d[:region_start]) + bytes(blobs) + bytes(audo)
        out = bytearray(out)
        struct.pack_into('<I', out, 4, len(out) - 8)
        with open(path, 'wb') as f:
            f.write(out)
        return len(changed)

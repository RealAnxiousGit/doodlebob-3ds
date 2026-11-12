import struct, sys, json, os

path = sys.argv[1]
outdir = sys.argv[2]
data = open(path, 'rb').read()
u32 = lambda o: struct.unpack_from('<I', data, o)[0]
i32 = lambda o: struct.unpack_from('<i', data, o)[0]

def cstr(ptr):
    if ptr == 0: return None
    end = data.index(b'\0', ptr)
    return data[ptr:end].decode('utf-8', 'replace')

assert data[0:4] == b'FORM'
chunks = {}
o = 8
while o < len(data):
    name = data[o:o+4].decode('ascii', 'replace'); size = u32(o+4)
    chunks[name] = (o+8, size)
    o += 8 + size
print('Chunks:', ', '.join(f'{k}={v[1]:,}' for k, v in chunks.items()))

g = chunks['GEN8'][0]
bc = data[g+1]
print('Bytecode version:', bc)
print('Filename:', cstr(u32(g+4)), '| Config:', cstr(u32(g+8)))
print('Display name:', cstr(u32(g+0x28)))
major, minor, release, build = (u32(g+0x2C+4*i) for i in range(4))
print(f'IDE version field: {major}.{minor}.{release}.{build}')
print('Default window:', u32(g+0x3C), 'x', u32(g+0x40))

def ptrlist(chunk):
    s, _ = chunks[chunk]
    n = u32(s)
    return [u32(s+4+4*i) for i in range(n)]

# CODE
if chunks.get('CODE', (0, 0))[1] == 0:
    print('CODE chunk empty -> YYC (native) build')
else:
    entries = ptrlist('CODE')
    total = sum(u32(e+4) for e in entries)
    print(f'Code entries: {len(entries)}, bytecode bytes: {total:,}')

# counts
for c in ['SOND','SPRT','BGND','PATH','SCPT','FONT','TMLN','OBJT','ROOM','TPAG','TXTR','AUDO','EXTN','SHDR']:
    if c in chunks and chunks[c][1] >= 4:
        print(f'{c}: {u32(chunks[c][0])}')

# textures
tex = []
for e in ptrlist('TXTR'):
    dp = u32(e+4) if bc >= 0 else 0
    if data[dp:dp+8] == b'\x89PNG\r\n\x1a\n':
        w, h = struct.unpack_from('>II', data, dp+16)
        tex.append((w, h))
    else:
        tex.append(None)
print('Texture pages:', len(tex), 'sizes:', sorted(set(tex), key=str))
vram = sum(w*h*4 for t in tex if t for (w, h) in [t])
print(f'Texture RAM if all loaded as RGBA8: {vram/1048576:.1f} MB')

# audio
aud = ptrlist('AUDO')
sizes = [u32(a) for a in aud]
print(f'Embedded audio: {len(aud)} files, {sum(sizes)/1048576:.1f} MB')
kinds = {}
for a in aud:
    hdr = data[a+4:a+8]
    kinds[hdr] = kinds.get(hdr, 0) + 1
print('Audio formats:', kinds)
# sounds referencing external files
ext = []
for sp in ptrlist('SOND'):
    nm, flags, typ, fn = cstr(u32(sp)), u32(sp+4), cstr(u32(sp+8)), cstr(u32(sp+12))
    embedded = flags & 1
    if not embedded: ext.append((nm, fn))
print('Sounds streamed from external files:', len(ext))

# extensions
if 'EXTN' in chunks and chunks['EXTN'][1] >= 4:
    for e in ptrlist('EXTN'):
        print('Extension:', cstr(u32(e)), cstr(u32(e+4)), cstr(u32(e+8)))

# shaders
if 'SHDR' in chunks and chunks['SHDR'][1] >= 4:
    print('Shaders:', [cstr(u32(e)) for e in ptrlist('SHDR')])

# FUNC: functions referenced by bytecode
s, size = chunks['FUNC']
n = u32(s)
funcs = []
for i in range(n):
    e = s+4+12*i
    funcs.append((cstr(u32(e)), u32(e+4)))
script_names = set()
for sp in ptrlist('SCPT'):
    script_names.add(cstr(u32(sp)))
builtins = sorted({f for f, _ in funcs if f not in script_names})
os.makedirs(outdir, exist_ok=True)
with open(os.path.join(outdir, 'builtins.txt'), 'w') as fh:
    fh.write('\n'.join(builtins))
print('Distinct functions referenced:', len(funcs), '| builtins (non-script):', len(builtins))

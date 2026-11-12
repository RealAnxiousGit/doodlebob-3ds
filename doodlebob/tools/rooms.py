"""Print each room's size, speed and views from a GMS 1.4 data.win.
Usage: python rooms.py <data.win>"""
import struct, sys

data = open(sys.argv[1], 'rb').read()
u32 = lambda o: struct.unpack_from('<I', data, o)[0]
i32 = lambda o: struct.unpack_from('<i', data, o)[0]

def cstr(p):
    return data[p:data.index(b'\0', p)].decode('utf-8', 'replace') if p else ''

o, chunks = 8, {}
while o < len(data):
    chunks[data[o:o+4]] = o + 8
    o += 8 + u32(o + 4)

s = chunks[b'ROOM']
speeds = {}
for i in range(u32(s)):
    r = u32(s + 4 + 4 * i)
    name, w, h, speed, flags = cstr(u32(r)), u32(r + 8), u32(r + 12), u32(r + 16), u32(r + 36)
    speeds[speed] = speeds.get(speed, 0) + 1
    views_ptr = u32(r + 44)
    vs = []
    if flags & 1:
        for vi in range(u32(views_ptr)):
            v = u32(views_ptr + 4 + 4 * vi)
            en, vx, vy, vw, vh, px, py, pw, ph = (i32(v + 4 * k) for k in range(9))
            if en:
                vs.append(f'view{vi} {vx},{vy} {vw}x{vh} -> port {px},{py} {pw}x{ph}')
    print(f'{i:2} {name:24} {w}x{h} speed={speed} views={"on" if flags & 1 else "off"} {"; ".join(vs)}')
print('room speeds:', speeds)

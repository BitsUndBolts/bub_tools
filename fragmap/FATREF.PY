"""Reference: list split files/folders of a FAT12/16 image (partition offset in bytes)."""
import struct, sys
img, off = sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 0
d = open(img, 'rb').read()[off:]
bps, spc, nres, nfats, nroot, tot16, _, spf = struct.unpack_from('<HBHBHHBH', d, 11)
tot = tot16 or struct.unpack_from('<I', d, 32)[0]
root = nres + nfats * spf; data = root + (nroot * 32 + 511) // 512
nclus = (tot - data) // spc; fat12 = nclus < 4085
fat = d[nres * 512:(nres + spf) * 512]
def nxt(c):
    if fat12:
        v = struct.unpack_from('<H', fat, c + c // 2)[0]
        v = v >> 4 if c & 1 else v & 0xFFF
        return 0xFFFF if v >= 0xFF8 else v
    return struct.unpack_from('<H', fat, c * 2)[0]
def chain(c):
    out = []
    while 2 <= c <= nclus + 1 and len(out) <= nclus:
        out.append(c); c = nxt(c)
        if c >= 0xFFF8: break
    return out
res = []; files = dirs = 0
def walk(sectors, path):
    global files, dirs
    for s in sectors:
        for i in range(16):
            e = d[s * 512 + i * 32: s * 512 + i * 32 + 32]
            if e[0] == 0: return
            if e[0] in (0xE5, 0x2E) or e[11] == 0x0F or e[11] & 8: continue
            n = e[0:8].rstrip(b' ').decode('cp437'); x = e[8:11].rstrip(b' ').decode('cp437')
            p = path + '\\' + n + ('.' + x if x else '')
            st, size = struct.unpack_from('<HI', e, 26)
            ch = chain(st) if st else []
            pieces = 1 + sum(1 for a, b in zip(ch, ch[1:]) if b != a + 1) if ch else 0
            if e[11] & 0x10:
                dirs += 1
                if pieces > 1: res.append((pieces, len(ch), '<DIR>', p + '\\'))
                walk([data + (c - 2) * spc + k for c in ch for k in range(spc)], p)
            else:
                files += 1
                if pieces > 1: res.append((pieces, len(ch), f'{size:,}', p))
walk(range(root, data), '')
print(f'FAT{12 if fat12 else 16} clusters {nclus} spc {spc} files {files} dirs {dirs} split {len(res)}')
for r in sorted(res, key=lambda r: (-r[0], r[3])): print(r[0], r[1], r[2], 'C:' + r[3])

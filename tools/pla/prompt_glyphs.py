# Pokémon Legends: Arceus button icons that follow the player's bindings (issue: the game asked for B while A was the
# button to press). The controller overlays (textures/prompts/<set>/romfs, mounted by runtime/services/fs.c) are icon
# mods made for one set of bindings each - the Xbox one by position (the Switch's A drawn as Xbox B, the east button),
# the PlayStation one by confirm-at-the-bottom (A as cross). Each button icon is a texture of its own
# (button_icon_A_00, button_icon_B_01...), so the runtime's texture replacement by content hash
# (runtime/nvn/vulkan/textures.c, the prompts set) can redraw it: for every icon texture of Switch button <slot>
# (A B X Y) and every controller face button <pad> (south east west north) this writes
#   textures/prompts/<set>/<slot>-<pad>/<hash>.png
# - that texture drawn with <pad>'s symbol, in the texture's own encoding (rg: red/green masks the shader colours,
# as BC5 / BC3; white: full colour; mono: one channel, BC4). The runtime loads the four folders the bindings name
# (bind_pad_a = south -> A-south ...). The title's "Press <A>" image gets the <pad> icon pasted over its button disc.
# Hashes: runtime/nvn/tex_hash.h tex_hash_full of the texture's bytes as stored in the BNTX (what the game uploads).
#
# usage (from the repository root, Pillow numpy texture2ddecoder):
#   python tools/pla/prompt_glyphs.py --ooz build/ooz/ooz.dll --set xbox
#   python tools/pla/prompt_glyphs.py --ooz build/ooz/ooz.dll --set playstation
# (build/ooz/ooz.dll: tools/za/dist.sh builds it from third_party/ooz. Some large entries of the overlay's .gfpak files
# don't decode with ooz - they hold no button icons of their own: every icon there is also in a file that does.)
import argparse, ctypes, os, shutil, struct, sys
import numpy as np
import texture2ddecoder
from PIL import Image

ap = argparse.ArgumentParser()
ap.add_argument('--ooz', required=True)
ap.add_argument('--set', required=True, choices=['xbox', 'playstation'])
ap.add_argument('--root', default='textures/prompts')
args = ap.parse_args()

ooz = ctypes.CDLL(os.path.abspath(args.ooz))
ooz.ooz_decompress.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p, ctypes.c_size_t]
M64 = (1 << 64) - 1
PADS = ('south', 'east', 'west', 'north')

# What each icon texture of the overlays shows (first 8 hex digits of its hash -> the pad button whose symbol it is).
# Xbox: A south, B east, X west, Y north; PlayStation: cross south, circle east, square west, triangle north.
SHOWS = {
    'xbox': {
        '9d040525': 'south', 'd7c003a8': 'south', '42417111': 'south', '89495a90': 'south', '98efce5f': 'south',
        'b3829957': 'east', '9bd93f98': 'east', '889a139a': 'east', '077844c2': 'east', '99acd200': 'east',
        '132eae3a': 'west', '0cb5a1c9': 'west',
        '8b26575e': 'north', '6a8413cc': 'north', '20da939e': 'north',
    },
    'playstation': {
        '059790ec': 'south', '6cbdc211': 'south', '9301190d': 'south', '8282efa7': 'south', '0110fdfe': 'south',
        '578eb2cb': 'east', '7fd5a854': 'east', 'e854eb19': 'east', '945f7f90': 'east',
        '75559303': 'west', 'a2ecc970': 'west', 'a831e3f5': 'west',
        '66c897a8': 'north', '1a1a48e5': 'north', '3237626a': 'north',
        '89495a90': None,  # (status screen: the game's own "A" left in both overlays - redrawn, never a source)
    },
}[args.set]


def tex_hash_full(p):  # runtime/nvn/tex_hash.h
    n = len(p)
    h = [0x9E3779B97F4A7C15 ^ n, 0xC2B2AE3D27D4EB4F, 0x165667B19E3779F9, 0x27D4EB2F165667C5,
         0x85EBCA77C2B2AE63, 0xFF51AFD7ED558CCD, 0xC4CEB9FE1A85EC53, 0x94D049BB133111EB]
    for row in np.frombuffer(p[:n - n % 64], dtype='<u8').reshape(-1, 8).tolist():
        for k in range(8):
            x = ((h[k] ^ row[k]) * 0x100000001B3) & M64
            h[k] = x ^ (x >> 29)
    r = 0
    for k in range(8):
        r = ((r ^ h[k]) * 0x9E3779B97F4A7C15) & M64
        r ^= r >> 32
    for b in p[n - n % 64:]:
        r = ((r ^ b) * 0x100000001B3) & M64
    return r ^ (r >> 31)


def gfpak(d, label):
    nf, fi = struct.unpack_from('<I', d, 0x10)[0], struct.unpack_from('<Q', d, 0x18)[0]
    for i in range(nf):
        lvl, comp, dsz, csz, pad, off = struct.unpack_from('<HHIIIQ', d, fi + i * 0x18)
        raw = bytes(d[off:off + csz])
        if comp == 0 or csz == dsz:
            yield i, raw
            continue
        out = ctypes.create_string_buffer(dsz + 256)
        if comp == 3 and ooz.ooz_decompress(raw, len(raw), out, dsz) == dsz:
            yield i, out.raw[:dsz]
        else:
            print(f'  ({label} entry {i}: not decoded, compression {comp}, {dsz} bytes)', file=sys.stderr)


def sarc(d):
    E = '<' if d[6:8] == b'\xff\xfe' else '>'
    hdr = struct.unpack_from(E + 'H', d, 4)[0]
    data_off = struct.unpack_from(E + 'I', d, 0xC)[0]
    n = struct.unpack_from(E + 'H', d, hdr + 6)[0]
    for i in range(n):
        h, attr, s, e = struct.unpack_from(E + 'IIII', d, hdr + 12 + 16 * i)
        yield d[data_off + s:data_off + e]


def bntx_textures(d):  # tools/za/prompt_textures.py
    cnt = struct.unpack_from('<I', d, 0x24)[0]
    infos = struct.unpack_from('<Q', d, 0x28)[0]
    for i in range(cnt):
        t = struct.unpack_from('<Q', d, infos + 8 * i)[0]
        fmt = struct.unpack_from('<I', d, t + 0x1C)[0]
        w, h = struct.unpack_from('<II', d, t + 0x24)
        bh = struct.unpack_from('<I', d, t + 0x34)[0]
        size = struct.unpack_from('<I', d, t + 0x50)[0]
        no = struct.unpack_from('<Q', d, t + 0x60)[0]
        m0 = struct.unpack_from('<Q', d, struct.unpack_from('<Q', d, t + 0x70)[0])[0]
        name = d[no + 2:no + 2 + struct.unpack_from('<H', d, no)[0]].decode('latin1')
        yield name, w, h, fmt, bh, d[m0:m0 + size]


def deswizzle_np(src, wb, hb, bpb, bh_log2):  # tools/za/prompt_textures.py
    gob_h, row = 8 << bh_log2, wb * bpb
    y, x = np.mgrid[0:hb, 0:row]
    by, byo = divmod(y, gob_h)
    gy, gyo = divmod(byo, 8)
    gx, gxo = divmod(x, 64)
    o = (by * ((row + 63) // 64) + gx) * gob_h * 64 + gy * 512 + (gxo // 32) * 256 + (gyo // 2) * 64 + \
        ((gxo % 32) // 16) * 32 + (gyo % 2) * 16 + gxo % 16
    a = np.frombuffer(src, np.uint8)
    return np.where(o < len(a), a[np.minimum(o, len(a) - 1)], 0).astype(np.uint8).tobytes()


DEC = {0x1a: ('bc1', 8), 0x1c: ('bc3', 16), 0x1d: ('bc4', 8), 0x1e: ('bc5', 16), 0x20: ('bc7', 16)}


def decode(w, h, fmt, bh, data):
    kind = fmt >> 8
    if kind == 0x0b:
        return Image.frombytes('RGBA', (w, h), deswizzle_np(data, w, h, 4, bh)[:w * h * 4])
    if kind not in DEC: return None
    name, bpb = DEC[kind]
    wb, hb = (w + 3) // 4, (h + 3) // 4
    raw = getattr(texture2ddecoder, 'decode_' + name)(deswizzle_np(data, wb, hb, bpb, bh), wb * 4, hb * 4)
    return Image.frombytes('RGBA', (wb * 4, hb * 4), raw, 'raw', 'BGRA').crop((0, 0, w, h))


def encoding(im):  # how the shader reads it: rg masks, one channel, or colour
    a = np.asarray(im)
    if a[..., 2].max() == 0 and a[..., 3].min() == 255:
        return 'mono' if a[..., 1].max() == 0 else 'rg'
    return 'white'


icons, titles = {}, {}  # hash -> (slot, image)


def walk(d):
    if d[:4] == b'SARC':
        for sub in sarc(d): walk(sub)
    elif d[:4] == b'BNTX':
        for name, w, h, fmt, bh, data in bntx_textures(d):
            base = name.split('^')[0]
            if base.startswith('grptxt_title_key_guid_') or (base.startswith('button_icon_') and base[12:13] in 'ABXY' and base[13:14] == '_'):
                hsh = tex_hash_full(data)
                if hsh in icons or hsh in titles: continue
                im = decode(w, h, fmt, bh, data)
                if im is None: continue
                (titles if base.startswith('grptxt') else icons)[hsh] = (base[12:13], im)


romfs = os.path.join(args.root, args.set, 'romfs')
for dp, _, fns in os.walk(romfs):
    for fn in sorted(fns):
        p = os.path.join(dp, fn)
        d = open(p, 'rb').read()
        if d[:8] == b'GFLXPACK':
            for i, sub in gfpak(d, os.path.relpath(p, romfs)): walk(sub)
        else:
            walk(d)

# the symbols: each pad button in each encoding, from icons whose symbol is known (SHOWS)
lib = {}
for hsh, (slot, im) in sorted(icons.items()):
    pad = SHOWS.get(f'{hsh:016x}'[:8], 'unknown')
    if pad == 'unknown':
        sys.exit(f'error: icon {slot} {hsh:016x} ({im.size[0]}x{im.size[1]}, {encoding(im)}) is not in SHOWS: '
                 f'look at it and add what it shows')
    if pad: lib.setdefault((encoding(im), im.size), {}).setdefault(pad, im)

out = os.path.join(args.root, args.set)
for slot in 'ABXY':
    for pad in PADS: shutil.rmtree(os.path.join(out, f'{slot}-{pad}'), ignore_errors=True)
made = 0
for hsh, (slot, im) in sorted(icons.items()):
    have = lib.get((encoding(im), im.size), {})
    for pad in PADS:
        if pad not in have: continue  # (no such symbol in this encoding: the icon stays as it is for that binding)
        os.makedirs(os.path.join(out, f'{slot}-{pad}'), exist_ok=True)
        have[pad].save(os.path.join(out, f'{slot}-{pad}', f'{hsh:016x}.png'))
        made += 1

# the title's "Press <A>": the colour icon of that pad button over the button disc (a light disc, ~38 px, in the
# text's line - wherever the language puts it)
white = {pad: im for (enc, size), m in lib.items() if enc == 'white' for pad, im in m.items()}
for hsh, (slot, im) in sorted(titles.items()):
    a = np.asarray(im).astype(int)
    disc = (a[..., 3] > 200) & (a[..., 0] > 200) & (a[..., 1] > 200) & (a.sum(-1) > 3 * 200)
    ys, xs = np.nonzero(disc)
    if not len(xs):
        print(f'  title {hsh:016x}: no button disc found', file=sys.stderr)
        continue
    # the disc: the light column run around the image's lightest blob - its bounding box, made square
    x0, x1, y0, y1 = xs.min(), xs.max(), ys.min(), ys.max()
    if x1 - x0 > 2 * (y1 - y0):  # (light text too: keep the blob around the densest column)
        col = np.bincount(xs, minlength=a.shape[1])
        cx = int(np.argmax(np.convolve(col, np.ones(y1 - y0 + 1), 'same')))
        x0, x1 = cx - (y1 - y0) // 2, cx + (y1 - y0) // 2
    size = max(x1 - x0, y1 - y0) + 3
    cx, cy = (x0 + x1) // 2, (y0 + y1) // 2
    for pad, icon in white.items():
        t = im.copy()
        ic = icon.resize((size, size), Image.LANCZOS)
        t.alpha_composite(ic, (cx - size // 2, cy - size // 2))
        os.makedirs(os.path.join(out, f'A-{pad}'), exist_ok=True)
        t.save(os.path.join(out, f'A-{pad}', f'{hsh:016x}.png'))
        made += 1
print(f'{args.set}: {len(icons)} icon textures, {len(titles)} title images -> {made} images in {out}/<slot>-<pad>/')

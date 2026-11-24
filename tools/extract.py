import sys, os, zlib, struct, io, json
sys.path.insert(0,'/home/claude/work')
from swfparse import load, tags, NAMES
from PIL import Image

src = sys.argv[1]; out = sys.argv[2]
os.makedirs(out+'/bitmaps', exist_ok=True); os.makedirs(out+'/sounds', exist_ok=True)
s, v, body, pos, fps, frames, nb = load(src)
ts = tags(body, pos)
jpegtables = b''
exports = {}
info = {'bitmaps':{}, 'sounds':{}, 'sprites':{}}

def fix_jpeg(d):
    # SWF JPEG may have erroneous header: FF D9 FF D8 at start
    if d[:4] == b'\xff\xd9\xff\xd8': d = d[4:]
    return d

for t, d in ts:
    if t == 8: jpegtables = d
for t, d in ts:
    if t == 56:
        n = struct.unpack('<H', d[:2])[0]; p = 2
        for _ in range(n):
            cid = struct.unpack('<H', d[p:p+2])[0]; p += 2
            e = d.index(b'\0', p); exports[cid] = d[p:e].decode('latin1'); p = e+1
for t, d in ts:
    try:
        if t == 6:
            cid = struct.unpack('<H', d[:2])[0]
            data = fix_jpeg(jpegtables)[:-2] + fix_jpeg(d[2:])[2:] if jpegtables else fix_jpeg(d[2:])
            im = Image.open(io.BytesIO(data)).convert('RGBA')
            im.save(f'{out}/bitmaps/{cid}.png'); info['bitmaps'][cid] = im.size
        elif t == 21:
            cid = struct.unpack('<H', d[:2])[0]
            im = Image.open(io.BytesIO(fix_jpeg(d[2:]))).convert('RGBA')
            im.save(f'{out}/bitmaps/{cid}.png'); info['bitmaps'][cid] = im.size
        elif t == 35:
            cid = struct.unpack('<H', d[:2])[0]
            alen = struct.unpack('<I', d[2:6])[0]
            jpg = fix_jpeg(d[6:6+alen]); alpha = zlib.decompress(d[6+alen:])
            im = Image.open(io.BytesIO(jpg)).convert('RGBA')
            if len(alpha) >= im.size[0]*im.size[1]:
                a = Image.frombytes('L', im.size, alpha[:im.size[0]*im.size[1]])
                im.putalpha(a)
            im.save(f'{out}/bitmaps/{cid}.png'); info['bitmaps'][cid] = im.size
        elif t in (20, 36):
            cid, fmt, w, h = struct.unpack('<HBHH', d[:7])
            p = 7; ctsize = 0
            if fmt == 3: ctsize = d[p]+1; p += 1
            raw = zlib.decompress(d[p:])
            if fmt == 3:
                bpp = 3 if t == 20 else 4
                pal = raw[:ctsize*bpp]; idx = raw[ctsize*bpp:]
                stride = (w+3)//4*4
                im = Image.new('RGBA', (w,h))
                px = im.load()
                for y in range(h):
                    for x in range(w):
                        i = idx[y*stride+x]
                        if t == 20: r,g,b = pal[i*3:i*3+3]; a = 255
                        else: r,g,b,a = pal[i*4:i*4+4]
                        px[x,y] = (r,g,b,a)
            elif fmt == 5:
                if t == 36:
                    # ARGB premultiplied
                    arr = bytearray(raw[:w*h*4])
                    im = Image.new('RGBA', (w,h))
                    px = bytearray(w*h*4)
                    for i in range(w*h):
                        a,r,g,b = arr[i*4:i*4+4]
                        if a: r=min(255,r*255//a); g=min(255,g*255//a); b=min(255,b*255//a)
                        px[i*4:i*4+4] = bytes((r,g,b,a))
                    im = Image.frombytes('RGBA', (w,h), bytes(px))
                else:
                    arr = raw[:w*h*4]
                    px = bytearray()
                    for i in range(w*h):
                        px += bytes((arr[i*4+1], arr[i*4+2], arr[i*4+3], 255))
                    im = Image.frombytes('RGBA', (w,h), bytes(px))
            else:
                continue
            im.save(f'{out}/bitmaps/{cid}.png'); info['bitmaps'][cid] = (w,h)
        elif t == 14:
            cid = struct.unpack('<H', d[:2])[0]
            flags = d[2]; fmt = flags >> 4; rate = [5512,11025,22050,44100][(flags>>2)&3]
            snd16 = (flags>>1)&1; stereo = flags&1
            cnt = struct.unpack('<I', d[3:7])[0]
            if fmt == 2:
                open(f'{out}/sounds/{cid}.mp3','wb').write(d[9:])
                info['sounds'][cid] = {'fmt':'mp3','rate':rate,'stereo':stereo,'samples':cnt}
            else:
                open(f'{out}/sounds/{cid}.raw','wb').write(d[7:])
                info['sounds'][cid] = {'fmt':fmt,'rate':rate,'stereo':stereo,'samples':cnt,'bits16':snd16}
    except Exception as ex:
        print('fail tag', t, ex)

info['exports'] = exports
json.dump(info, open(out+'/info.json','w'), indent=1)
print('bitmaps', len(info['bitmaps']), 'sounds', len(info['sounds']), 'exports', len(exports))

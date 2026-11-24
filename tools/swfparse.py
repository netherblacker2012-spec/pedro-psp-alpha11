import zlib, struct, sys, collections

def load(path):
    d = open(path,'rb').read()
    sig = d[:3]
    if sig == b'CWS':
        body = zlib.decompress(d[8:])
    elif sig == b'FWS':
        body = d[8:]
    else:
        raise Exception('unsupported '+str(sig))
    # rect
    nbits = body[0] >> 3
    rect_bytes = (5 + nbits*4 + 7)//8
    pos = rect_bytes
    fps = struct.unpack('<H', body[pos:pos+2])[0]/256.0
    frames = struct.unpack('<H', body[pos+2:pos+4])[0]
    pos += 4
    return d[3], d[3+1:4], body, pos, fps, frames, nbits

def tags(body, pos):
    out = []
    n = len(body)
    while pos < n:
        h = struct.unpack('<H', body[pos:pos+2])[0]; pos += 2
        t = h >> 6; l = h & 0x3f
        if l == 0x3f:
            l = struct.unpack('<I', body[pos:pos+4])[0]; pos += 4
        out.append((t, body[pos:pos+l]))
        pos += l
        if t == 0: break
    return out

NAMES = {0:'End',1:'ShowFrame',2:'DefineShape',4:'PlaceObject',5:'RemoveObject',6:'DefineBits',8:'JPEGTables',9:'SetBackgroundColor',
 10:'DefineFont',11:'DefineText',12:'DoAction',14:'DefineSound',15:'StartSound',20:'DefineBitsLossless',21:'DefineBitsJPEG2',22:'DefineShape2',
 26:'PlaceObject2',28:'RemoveObject2',32:'DefineShape3',34:'DefineButton2',35:'DefineBitsJPEG3',36:'DefineBitsLossless2',37:'DefineEditText',39:'DefineSprite',
 41:'ProductInfo',43:'FrameLabel',45:'SoundStreamHead2',46:'DefineMorphShape',48:'DefineFont2',56:'ExportAssets',57:'ImportAssets',
 59:'DoInitAction',62:'DefineFontInfo2',69:'FileAttributes',70:'PlaceObject3',71:'ImportAssets2',72:'DoABC_old',73:'DefineFontAlignZones',74:'CSMTextSettings',
 75:'DefineFont3',76:'SymbolClass',77:'Metadata',78:'DefineScalingGrid',82:'DoABC',83:'DefineShape4',84:'DefineMorphShape2',86:'DefineSceneAndFrameLabelData',
 87:'DefineBinaryData',88:'DefineFontName',90:'DefineBitsJPEG4'}

if __name__ == '__main__':
    p = sys.argv[1]
    s, v, body, pos, fps, frames, nb = load(p)
    print(p, 'version', body and open(p,'rb').read()[3], 'fps', fps, 'frames', frames)
    ts = tags(body, pos)
    c = collections.Counter(NAMES.get(t, str(t)) for t,_ in ts)
    for k,v in c.most_common(): print(f'  {k}: {v}')

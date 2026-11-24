import sys, struct, json, math, io, zlib
import numpy as np
from PIL import Image
sys.path.insert(0,'/home/claude/work')
from swfparse import load, tags

SRC="/home/claude/pedro/MFPMaxGamesMjE4MQ==.swf"
s,v,body,pos,fps,nframes,nb=load(SRC)
TS=tags(body,pos)
BM_DIR='/home/claude/work/main/bitmaps/'
_bm={}
def bitmap(cid):
    if cid not in _bm:
        try: _bm[cid]=np.array(Image.open(BM_DIR+f'{cid}.png').convert('RGBA'),dtype=np.uint8)
        except Exception: _bm[cid]=None
    return _bm[cid]

class BR:
    def __init__(s,d,p=0): s.d=d; s.p=p; s.b=0
    def al(s): 
        if s.b: s.b=0; s.p+=1
    def u8(s): s.al(); v=s.d[s.p]; s.p+=1; return v
    def u16(s): s.al(); v=struct.unpack_from('<H',s.d,s.p)[0]; s.p+=2; return v
    def u32(s): s.al(); v=struct.unpack_from('<I',s.d,s.p)[0]; s.p+=4; return v
    def bits(s,n):
        v=0
        for _ in range(n):
            byte=s.d[s.p]; v=(v<<1)|((byte>>(7-s.b))&1); s.b+=1
            if s.b==8: s.b=0; s.p+=1
        return v
    def sb(s,n):
        v=s.bits(n)
        if n and v&(1<<(n-1)): v-=1<<n
        return v
    def rect(s):
        n=s.bits(5); r=[s.sb(n) for _ in range(4)]; s.al(); return r
    def matrix(s):
        sx=sy=1.0; r0=r1=0.0
        if s.bits(1): n=s.bits(5); sx=s.sb(n)/65536; sy=s.sb(n)/65536
        if s.bits(1): n=s.bits(5); r0=s.sb(n)/65536; r1=s.sb(n)/65536
        n=s.bits(5); tx=s.sb(n); ty=s.sb(n); s.al()
        return (sx,r0,r1,sy,tx/20.0,ty/20.0)   # px

def mmul(a,b):  # a*b : apply b first then a. matrices (a,b,c,d,tx,ty) x' = a x + c y + tx ; y' = b x + d y + ty (SWF: sx,r0 ->b, r1->c)
    A=np.array([[a[0],a[2],a[4]],[a[1],a[3],a[5]],[0,0,1]])
    B=np.array([[b[0],b[2],b[4]],[b[1],b[3],b[5]],[0,0,1]])
    C=A@B
    return (C[0,0],C[1,0],C[0,1],C[1,1],C[0,2],C[1,2])
IDENT=(1,0,0,1,0,0)

# ---------- shapes ----------
class Shape: pass
SHAPES={}
def parse_color(r,alpha):
    rr,g,b=r.u8(),r.u8(),r.u8(); a=r.u8() if alpha else 255
    return (rr,g,b,a)
def parse_fillstyles(r,ver):
    n=r.u8()
    if n==0xff and ver>=2: n=r.u16()
    out=[]
    for _ in range(n):
        t=r.u8()
        if t==0: out.append(('solid',parse_color(r,ver>=3)))
        elif t in (0x10,0x12,0x13):
            m=r.matrix(); r.al()
            if t==0x13:
                pass
            sp=r.bits(2); il=r.bits(2); cnt=r.bits(4); r.al()
            cols=[]
            for _ in range(cnt):
                ratio=r.u8(); c=parse_color(r,ver>=3); cols.append((ratio,c))
            if t==0x13: r.u16()
            out.append(('grad',t,m,cols))
        elif t in (0x40,0x41,0x42,0x43):
            cid=r.u16(); m=r.matrix(); r.al()
            out.append(('bitmap',cid,m,t))
        else:
            out.append(('solid',(0,0,0,0)))
    return out
def parse_linestyles(r,ver):
    n=r.u8()
    if n==0xff: n=r.u16()
    for _ in range(n):
        r.u16()
        parse_color(r,ver>=3)
def parse_shape(t,d):
    ver={2:1,22:2,32:3,83:4}[t]
    r=BR(d); cid=r.u16(); bounds=r.rect()
    if t==83:
        r.rect(); r.u8()
    fills=parse_fillstyles(r,ver); parse_linestyles(r,ver)
    r.al()
    nfb=r.bits(4); nlb=r.bits(4)
    x=y=0; f0=f1=0; ls=0
    base=0
    edges=[]  # (fillstyle_global_idx, [(x0,y0,x1,y1)...]) built as flat segments list with style
    segs=[]   # (x0,y0,x1,y1,f0,f1)
    allfills=list(fills)
    fbase=0
    while True:
        if r.bits(1)==0:
            flags=r.bits(5)
            if flags==0: break
            if flags&1:
                n=r.bits(5); x=r.sb(n); y=r.sb(n)
            if flags&2: f0=r.bits(nfb)+ (fbase if r.d is not None and False else 0)
            if flags&4: f1=r.bits(nfb)
            if flags&8: ls=r.bits(nlb)
            if flags&16:
                r.al()
                nf=parse_fillstyles(r,ver); parse_linestyles(r,ver)
                r.al(); fbase=len(allfills); allfills+=nf
                nfb=r.bits(4); nlb=r.bits(4)
                # new style set replaces numbering
            # record fill with base offset
            cur_f0=(fbase+f0-1) if f0 else -1
            cur_f1=(fbase+f1-1) if f1 else -1
        else:
            if r.bits(1):  # straight
                n=r.bits(4)+2
                if r.bits(1): dx=r.sb(n); dy=r.sb(n)
                else:
                    if r.bits(1): dx=0; dy=r.sb(n)
                    else: dx=r.sb(n); dy=0
                segs.append((x,y,x+dx,y+dy,cur_f0 if 'cur_f0' in dir() else -1,cur_f1 if 'cur_f1' in dir() else -1,None))
                x+=dx; y+=dy
            else:
                n=r.bits(4)+2
                cx=r.sb(n); cy=r.sb(n); ax=r.sb(n); ay=r.sb(n)
                p1=(x+cx,y+cy); p2=(p1[0]+ax,p1[1]+ay)
                # flatten
                steps=6; px,py=x,y
                for i in range(1,steps+1):
                    tt=i/steps
                    qx=(1-tt)**2*x+2*(1-tt)*tt*p1[0]+tt*tt*p2[0]
                    qy=(1-tt)**2*y+2*(1-tt)*tt*p1[1]+tt*tt*p2[1]
                    segs.append((px,py,qx,qy,cur_f0,cur_f1,None)); px,py=qx,qy
                x,y=p2
    sh=Shape(); sh.id=cid; sh.bounds=[b/20 for b in bounds]; sh.fills=allfills; sh.segs=segs
    return sh
for t,d in TS:
    if t in (2,22,32,83):
        try:
            cid=struct.unpack('<H',d[:2])[0]
            SHAPES[cid]=(t,d)
        except Exception: pass
_parsed={}
def get_shape(cid):
    if cid not in _parsed:
        try: _parsed[cid]=parse_shape(*SHAPES[cid])
        except Exception as ex: _parsed[cid]=None
    return _parsed[cid]

# ---------- sprites ----------
SPR={}
def parse_placeobj2(dd):
    f=dd[0]; depth=struct.unpack_from('<H',dd,1)[0]; r=BR(dd,3)
    ch=m=None; alpha=None; name=None
    if f&2: ch=r.u16()
    if f&4: m=r.matrix()
    if f&8:
        r.al(); ha=r.bits(1); hm=r.bits(1); n=r.bits(4)
        mul=[256,256,256,256]; 
        if hm: mul=[r.sb(n) for _ in range(4)]
        if ha: [r.sb(n) for _ in range(4)]
        r.al(); alpha=mul[3]/256.0
    if f&0x10: r.u16()
    if f&0x20:
        e=dd.index(b'\0',r.p); name=dd[r.p:e].decode('latin1')
    return dict(move=bool(f&1),depth=depth,ch=ch,m=m,alpha=alpha,name=name)
def build_timeline(tgs):
    frames=[]; dl={}
    for t,d in tgs:
        if t==26:
            p=parse_placeobj2(d); dp=p['depth']
            if p['move'] and dp in dl:
                cur=dict(dl[dp])
                if p['ch'] is not None: cur['ch']=p['ch']
                if p['m'] is not None: cur['m']=p['m']
                if p['alpha'] is not None: cur['alpha']=p['alpha']
                dl[dp]=cur
            else:
                dl[dp]=dict(ch=p['ch'],m=p['m'] or IDENT,alpha=p['alpha'] if p['alpha'] is not None else 1.0,name=p['name'])
        elif t==28:
            dl.pop(struct.unpack('<H',d[:2])[0],None)
        elif t==1:
            frames.append({k:dict(v) for k,v in dl.items()})
    return frames
for t,d in TS:
    if t==39:
        cid=struct.unpack('<H',d[:2])[0]
        SPR[cid]=d[4:]
_tl={}
def sprite_tl(cid):
    if cid not in _tl:
        _tl[cid]=build_timeline(tags(SPR[cid],0))
    return _tl[cid]
ROOT_TL=build_timeline(TS)

# ---------- raster ----------
class Canvas:
    def __init__(s,x0,y0,w,h,scale):
        s.x0=x0; s.y0=y0; s.w=int(w); s.h=int(h); s.scale=scale
        s.buf=np.zeros((s.h,s.w,4),dtype=np.float32)  # premultiplied
    def to_img(s):
        a=s.buf[...,3:4]; rgb=np.where(a>1e-4, s.buf[...,:3]/np.maximum(a,1e-4),0)
        out=np.concatenate([rgb,a*255],axis=-1) if False else None
        arr=np.concatenate([np.clip(rgb,0,255),np.clip(a,0,1)*255],axis=-1).astype(np.uint8)
        return Image.fromarray(arr,'RGBA')

def coverage(segs, style, M, cv):
    """even-odd scanline coverage mask for given fill style, 4x vertical supersample, in canvas px."""
    H,W=cv.h,cv.w
    A=np.array([[M[0],M[2]],[M[1],M[3]]]); T=np.array([M[4],M[5]])
    pts=[]
    for (x0,y0,x1,y1,f0,f1,_) in segs:
        if f0==style or f1==style:
            if f0==f1: continue
            pts.append((x0/20,y0/20,x1/20,y1/20))
    if not pts: return None
    P=np.array(pts)
    a=(A@P[:,0:2].T).T+T; b=(A@P[:,2:4].T).T+T
    a=(a-[cv.x0,cv.y0])*cv.scale; b=(b-[cv.x0,cv.y0])*cv.scale
    ymin=max(0,int(math.floor(min(a[:,1].min(),b[:,1].min()))))
    ymax=min(H,int(math.ceil(max(a[:,1].max(),b[:,1].max()))))
    xmin_all=min(a[:,0].min(),b[:,0].min()); xmax_all=max(a[:,0].max(),b[:,0].max())
    if ymax<=ymin or xmax_all<0 or xmin_all>W: return None
    SS=3
    mask=np.zeros((H,W),dtype=np.float32)
    ya=a[:,1]; yb=b[:,1]
    lo=np.minimum(ya,yb); hi=np.maximum(ya,yb)
    for y in range(ymin,ymax):
        acc=None
        for k in range(SS):
            sy=y+(k+0.5)/SS
            sel=(lo<=sy)&(hi>sy)&(ya!=yb)
            if not sel.any(): continue
            t=(sy-ya[sel])/(yb[sel]-ya[sel]); xs=a[sel,0]+t*(b[sel,0]-a[sel,0])
            xs=np.sort(xs)
            if len(xs)%2: xs=xs[:-1]
            for i in range(0,len(xs),2):
                xl,xr=xs[i],xs[i+1]
                if xr<0 or xl>W: continue
                xl=max(xl,0); xr=min(xr,W)
                il=int(xl); ir=int(xr)
                if il==ir: mask[y,il]+=(xr-xl)/SS
                else:
                    mask[y,il]+=(il+1-xl)/SS; mask[y,ir if ir<W else W-1]+=(xr-ir)/SS if ir<W else 0
                    if ir>il+1: mask[y,il+1:ir]+=1.0/SS
    return np.clip(mask,0,1)

def draw_shape(cv, cid, M, alpha, tint=None):
    sh=get_shape(cid)
    if sh is None: return
    styles=set()
    for sg in sh.segs:
        styles.add(sg[4]); styles.add(sg[5])
    styles.discard(-1)
    for st in sorted(styles):
        fill=sh.fills[st] if st<len(sh.fills) else None
        if fill is None: continue
        mask=coverage(sh.segs,st,M,cv)
        if mask is None: continue
        ys,xs=np.nonzero(mask)
        if len(ys)==0: continue
        y0,y1,x0,x1=ys.min(),ys.max()+1,xs.min(),xs.max()+1
        m=mask[y0:y1,x0:x1]
        if fill[0]=='solid':
            r,g,b,a=fill[1]; col=np.zeros((y1-y0,x1-x0,4),dtype=np.float32); col[...]=[r,g,b,a]
        elif fill[0]=='bitmap':
            bm=bitmap(fill[1])
            if bm is None: continue
            FM=fill[2]  # fill matrix in px (shape space), maps texel(px) -> shape px? texel*? we use: shape = FM * (texel*1) where FM includes 20x? 
            # fill matrix maps texel coords (in twips units: 1 texel=20 twips) to shape twips. FM translation parsed /20 => px. scale: sx maps texel->twip => px = sx*texel/... 
            # FM here already has tx/ty in px; scales are raw (twips per texel) -> px per texel = sx/20.
            F=(FM[0]/20,FM[1]/20,FM[2]/20,FM[3]/20,FM[4],FM[5])
            C=mmul(M,F)   # texel -> canvas(px before cv scale)
            # canvas pixel (cx,cy) -> world: wx=cx/scale+x0
            # inverse
            A=np.array([[C[0],C[2]],[C[1],C[3]]]); T=np.array([C[4],C[5]])
            det=np.linalg.det(A)
            if abs(det)<1e-9: continue
            Ai=np.linalg.inv(A)
            gy,gx=np.mgrid[y0:y1,x0:x1]
            wx=(gx+0.5)/cv.scale+cv.x0; wy=(gy+0.5)/cv.scale+cv.y0
            tx=Ai[0,0]*(wx-T[0])+Ai[0,1]*(wy-T[1]); ty=Ai[1,0]*(wx-T[0])+Ai[1,1]*(wy-T[1])
            H,W=bm.shape[:2]
            if fill[3] in (0x40,0x42):  # repeating
                ix=np.floor(tx).astype(int)%W; iy=np.floor(ty).astype(int)%H
            else:
                ix=np.clip(np.floor(tx).astype(int),0,W-1); iy=np.clip(np.floor(ty).astype(int),0,H-1)
            col=bm[iy,ix].astype(np.float32)
        else:
            # gradient: approximate with average colour
            cols=fill[3]; c=cols[len(cols)//2][1]
            col=np.zeros((y1-y0,x1-x0,4),dtype=np.float32); col[...]=c
        a=(col[...,3]/255.0)*m*alpha
        pm=col[...,:3]*a[...,None]
        dst=cv.buf[y0:y1,x0:x1]
        dst[...,:3]=pm+dst[...,:3]*(1-a[...,None])
        dst[...,3]=a+dst[...,3]*(1-a)

SKIP_CHARS=set()
def render_char(cv, cid, M, alpha=1.0, frame=0, depth=0):
    if cid in SKIP_CHARS: return
    if cid in SHAPES: draw_shape(cv,cid,M,alpha); return
    if cid in SPR:
        if depth>12: return
        tl=sprite_tl(cid)
        if not tl: return
        fr=tl[min(frame,len(tl)-1)]
        for dp in sorted(fr):
            e=fr[dp]
            if e['ch'] is None: continue
            render_char(cv,e['ch'],mmul(M,e['m']),alpha*e['alpha'],0,depth+1)
        return
    # bitmaps referenced directly? not here

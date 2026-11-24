import flashrender as fr, numpy as np, json
from PIL import Image
OUT='/home/claude/work/psp/'
S=0.68; SG=S*0.5
fr.SKIP_CHARS.clear()
sprites={}   # name -> (img, px, py)
def grab(name,cid,frame,scale=SG,pad=170):
    c=fr.Canvas(-pad,-pad*1.3,pad*2*scale,pad*2.3*scale,scale)
    fr.render_char(c,cid,(1,0,0,1,0,0),1.0,frame)
    im=c.to_img(); bb=im.getbbox()
    if not bb: return False
    px,py=pad*scale,pad*1.3*scale
    sprites[name]=(im.crop(bb),px-bb[0],py-bb[1]); return True
groups={}
def add(group,cid,step):
    n=len(fr.sprite_tl(cid)); names=[]
    for i in range(0,n,step):
        nm=f'{group}{len(names)}'
        if grab(nm,cid,i): names.append(nm)
    groups[group]=names
add('gidle',294,4)     # 40 frames -> 10
add('gwalk',295,3)     # 57 -> 19
add('gdeadb',307,2)    # 24 -> 12  (hit from the front: falls backwards)
add('gdeadf',308,2)    # 28 -> 14  (hit from behind: falls forwards)
add('gaim',312,8)      # single aim pose (frame 8 of gangster = idx3 below)
grab('gaim0',312,3); groups['gaim']=['gaim0']
# muzzle flash (10 frames), a bit bigger than the original 25% so it reads on a small screen
add('muz',335,1) if False else None
for i in range(10):
    if grab(f'muz{i}',335,i,scale=S*0.5,pad=80): groups.setdefault('muz',[]).append(f'muz{i}')
# shelf pack into 512x512
order=[n for g in groups.values() for n in g]
x=y=rowh=0; AW=AH=512; place={}
for n in order:
    im,px,py=sprites[n]
    if x+im.width>AW: x=0; y+=rowh+1; rowh=0
    place[n]=(x,y,im.width,im.height,int(round(px)),int(round(py))); x+=im.width+1; rowh=max(rowh,im.height)
used=y+rowh
print('atlas height used',used,'frames',len(order))
assert used<=AH
atlas=Image.new('RGBA',(AW,AH),(0,0,0,0))
for n,(px_,py_,w_,h_,_,_) in place.items(): atlas.paste(sprites[n][0],(px_,py_))
atlas.save(OUT+'assets/gang.png',optimize=True)
steps={'gidle':4,'gwalk':3,'gdeadb':2,'gdeadf':2}
h=['#ifndef GANG_H\n#define GANG_H']
for g,names in groups.items():
    cname=g.upper()
    rects=','.join('{%d,%d,%d,%d,%d,%d}'%place[n] for n in names)
    h.append(f'#define N_{cname} {len(names)}\nstatic const Spr GA_{cname}[N_{cname}]={{{rects}}};')
h.append('#endif')
open(OUT+'src/gang.h','w').write('\n'.join(h)+'\n')
print({g:len(n) for g,n in groups.items()})

import flashrender as fr, numpy as np, json, shutil, os
from PIL import Image
OUT='/home/claude/work/psp/assets/'
S=0.68
x0,y0,w,h=-3000,300,5800,640
W=int(w*S); H=int(h*S)
f=fr.ROOT_TL[11]
# ---- level background ----
fr.SKIP_CHARS.clear(); fr.SKIP_CHARS.update({822,634,705,731,742,312,738})
cv=fr.Canvas(x0,y0,W,H,S)
for dp,e in sorted(f.items()):
    if e['ch'] is None or e['name'] in ('playerL','playerB','crosshair','HUD'): continue
    fr.render_char(cv,e['ch'],e['m'],e['alpha'])
im=cv.to_img()
bg=Image.new('RGB',im.size,(0,0,0)); bg.paste(im,(0,0),im)
bg.save(OUT+'level1.png',optimize=True)
# ---- collision (lvl 878) ----
cv2=fr.Canvas(x0,y0,W,H,S)
for dp,e in sorted(f.items()):
    if e['name']=='lvl': fr.render_char(cv2,e['ch'],e['m'],1.0)
a=(np.array(cv2.to_img())[...,3]>40)
packed=np.packbits(a,axis=1)
open(OUT+'level1.col','wb').write(packed.tobytes())
meta=dict(x0=x0,y0=y0,w=W,h=H,scale=S,stride=packed.shape[1])
# ---- entities ----
fr.SKIP_CHARS.clear()
enemies=[(e['m'][4],e['m'][5]) for dp,e in sorted(f.items()) if e['ch']==312]
pl=[e for e in f.values() if e['name']=='playerL'][0]['m']
meta['player']=(pl[4],pl[5]); meta['enemies']=enemies
# ---- sprite atlas ----
SG=S*0.5   # in-game sprite scale (placements are 0.5)
sprites={}
def grab(name,cid,frame,pad=140,flip=False):
    c=fr.Canvas(-pad,-pad*1.3,pad*2*SG,pad*2.3*SG,SG)
    fr.render_char(c,cid,(1,0,0,1,0,0),1.0,frame)
    im=c.to_img(); bb=im.getbbox()
    if not bb: return
    px,py=pad*SG,pad*1.3*SG   # registration pixel in canvas
    im=im.crop(bb); sprites[name]=(im,px-bb[0],py-bb[1])
for i in range(7): grab(f'body{i}',731,i)
# legs run cycle from clip 697 (30 frames)
leg_tl=fr.sprite_tl(697); print('697 frames',len(leg_tl))
for i in range(0,len(leg_tl),2): grab(f'leg{i//2}',697,i)
grab('legidle',705,0)
for i in (0,3): grab(f'gang{i}',312,i)
# pack atlas
names=list(sprites)
x=y=rowh=0; AW=512; place={}
for n in names:
    im,px,py=sprites[n]
    if x+im.width>AW: x=0; y+=rowh+1; rowh=0
    place[n]=(x,y,im.width,im.height,px,py); x+=im.width+1; rowh=max(rowh,im.height)
AH=y+rowh+1
atlas=Image.new('RGBA',(AW,AH),(0,0,0,0))
for n,(px_,py_,w_,h_,_,_) in place.items(): atlas.paste(sprites[n][0],(px_,py_))
atlas.save(OUT+'atlas.png',optimize=True)
json.dump(dict(meta=meta,atlas=place,legcount=len(range(0,len(leg_tl),2))),open('/home/claude/work/psp/meta.json','w'),indent=1)
print('level',W,H,'atlas',AW,AH,'enemies',len(enemies),'player',meta['player'])

import json, struct, subprocess, os, sys
base='/home/claude/work/main'
info=json.load(open(base+'/info.json'))
os.makedirs(base+'/wav', exist_ok=True)
def flv(data, rate_code=2):
    hdr = b'FLV\x01\x04' + struct.pack('>I', 9) + struct.pack('>I', 0)
    # audio: fmt 6 (Nellymoser), rate 3=44k.. flv codes: 0=5.5k,1=11k,2=22k,3=44k ; 16bit=1 ; mono=0
    flags = (6<<4)|(rate_code<<2)|(1<<1)|0
    out = hdr
    # split into chunks of 64 bytes*N per tag, with timestamps
    blk = 64
    n = len(data)//blk
    per = 4
    t = 0
    i = 0
    while i < n:
        c = min(per, n-i)
        payload = bytes([flags]) + data[i*blk:(i+c)*blk]
        ts = int(i*256/22050*1000)
        tag = bytes([8]) + struct.pack('>I', len(payload))[1:] + struct.pack('>I', ts)[1:] + bytes([(ts>>24)&255]) + b'\0\0\0' + payload
        out += tag + struct.pack('>I', len(tag))
        i += c
    return out
ok=0
for cid,s in info['sounds'].items():
    raw=open(f'{base}/sounds/{cid}.raw','rb').read()
    open('/tmp/t.flv','wb').write(flv(raw))
    r=subprocess.run(['ffmpeg','-y','-loglevel','error','-i','/tmp/t.flv','-ar','22050','-ac','1',f'{base}/wav/{cid}.wav'],capture_output=True,text=True)
    if r.returncode==0 and os.path.exists(f'{base}/wav/{cid}.wav'): ok+=1
    else: print(cid, r.stderr[:200])
print('converted', ok, 'of', len(info['sounds']))

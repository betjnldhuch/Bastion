import struct,sys,os
def chunks(b,off,end):
    while off+12<=end:
        t,s,v=struct.unpack_from('<III',b,off)
        yield t,off+12,min(off+12+s,end)
        off+=12+s
CONTAINERS={0x10,0x14,0x0E,0x1A,0x07,0x08,0x03,0x0F}
def dff_textures(b):
    out=[]
    def walk(off,end,depth=0):
        for t,s,e in chunks(b,off,end):
            if t==0x06:
                names=[b[cs:ce].split(b'\0')[0].decode('latin1') for ct,cs,ce in chunks(b,s,e) if ct==0x02]
                if names: out.append(names[0])
            elif t in CONTAINERS: walk(s,e,depth+1)
    walk(0,len(b))
    return out
FMT={0x100:'DXT/1555',0x200:'565',0x300:'4444',0x500:'8888',0x600:'888'}
def txd_list(b):
    res=[]
    for t,s,e in chunks(b,0,len(b)):
        if t!=0x16: continue
        for ct,cs,ce in chunks(b,s,e):
            if ct!=0x15: continue
            for st,ss,se in chunks(b,cs,ce):
                if st!=0x01: continue
                plat,flt=struct.unpack_from('<II',b,ss)
                name=b[ss+8:ss+40].split(b'\0')[0].decode('latin1')
                mask=b[ss+40:ss+72].split(b'\0')[0].decode('latin1')
                rf,d3d,w,h,dep,mips,rt,cf=struct.unpack_from('<IIHHBBBB',b,ss+72)
                res.append((name,mask,w,h,hex(rf&0xf00),d3d.to_bytes(4,'little') if d3d>255 else d3d,cf))
                break
    return res
if __name__=='__main__':
    d=sys.argv[1]
    for f in sorted(os.listdir(d)):
        b=open(os.path.join(d,f),'rb').read()
        if f.endswith('.txd'):
            print('TXD',f); [print('   ',x) for x in txd_list(b)]
    for f in sorted(os.listdir(d)):
        b=open(os.path.join(d,f),'rb').read()
        if f.endswith('.dff'):
            print('DFF',f, sorted(set(dff_textures(b))))

import numpy as np
def _565(c):
    return np.array([((c>>11)&31)*255//31,((c>>5)&63)*255//63,(c&31)*255//31])
def dxt_decode(data,w,h,kind):
    out=np.zeros((h,w,4),np.uint8); bs=8 if kind=='DXT1' else 16; p=0
    for by in range(0,h,4):
        for bx in range(0,w,4):
            blk=data[p:p+bs]; p+=bs
            alpha=None
            if kind=='DXT3':
                a=int.from_bytes(blk[:8],'little'); alpha=[((a>>(4*i))&15)*17 for i in range(16)]; blk=blk[8:]
            c0,c1,bits=struct.unpack('<HHI',blk[:8])
            p0,p1=_565(c0),_565(c1)
            if c0>c1 or kind!='DXT1': cols=[p0,p1,(2*p0+p1)//3,(p0+2*p1)//3]; a3=255
            else: cols=[p0,p1,(p0+p1)//2,np.array([0,0,0])]; a3=0
            for i in range(16):
                idx=(bits>>(2*i))&3; y,x=by+i//4,bx+i%4
                if y<h and x<w:
                    out[y,x,:3]=cols[idx]; out[y,x,3]=alpha[i] if alpha else (a3 if idx==3 else 255)
    return out
def txd_images(b):
    from PIL import Image
    res={}
    for t,s,e in chunks(b,0,len(b)):
        if t!=0x16: continue
        for ct,cs,ce in chunks(b,s,e):
            if ct!=0x15: continue
            for st,ss,se in chunks(b,cs,ce):
                if st!=0x01: continue
                name=b[ss+8:ss+40].split(b'\0')[0].decode('latin1')
                rf,d3d,w,h,dep,mips,rt,cf=struct.unpack_from('<IIHHBBBB',b,ss+72)
                kind=d3d.to_bytes(4,'little').decode('latin1')
                size=struct.unpack_from('<I',b,ss+88)[0]
                data=b[ss+92:ss+92+size]
                if kind in('DXT1','DXT3'): res[name]=Image.fromarray(dxt_decode(data,w,h,kind),'RGBA')
                break
    return res

# Replace textures inside an existing RW TXD with A8R8G8B8 rasters (with mipmaps).
import struct, sys
from PIL import Image
import os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import rw
def hdr(t,data,ver): return struct.pack('<III',t,len(data),ver)+data
def native(name,mask,img,filt,ver):
    img=img.convert('RGBA'); w,h=img.size
    mips=[]; cur=img
    while True:
        r,g,b,a=cur.split(); mips.append(Image.merge('RGBA',(b,g,r,a)).tobytes())
        if cur.width==1 and cur.height==1: break
        cur=cur.resize((max(1,cur.width//2),max(1,cur.height//2)),Image.LANCZOS)
    hasalpha=any(p<255 for p in img.getchannel('A').getdata())
    s=struct.pack('<II',9,filt)+name.encode().ljust(32,b'\0')+mask.encode().ljust(32,b'\0')
    s+=struct.pack('<IIHHBBBB',0x0500|0x8000,21,w,h,32,len(mips),4,1 if hasalpha else 0)
    for m in mips: s+=struct.pack('<I',len(m))+m
    return hdr(0x15,hdr(0x01,s,ver)+hdr(0x03,b'',ver),ver)
def patch(src,dst,repl):
    b=open(src,'rb').read()
    out=b''; done=set()
    for t,s,e in rw.chunks(b,0,len(b)):
        ver=struct.unpack_from('<I',b,s-4)[0]
        if t!=0x16: out+=b[s-12:e]; continue
        body=b''
        for ct,cs,ce in rw.chunks(b,s,e):
            cver=struct.unpack_from('<I',b,cs-4)[0]
            if ct==0x15:
                st=next(x for x in rw.chunks(b,cs,ce) if x[0]==1); ss=st[1]
                filt=struct.unpack_from('<I',b,ss+4)[0]
                name=b[ss+8:ss+40].split(b'\0')[0].decode('latin1'); mask=b[ss+40:ss+72].split(b'\0')[0].decode('latin1')
                if name in repl:
                    body+=native(name,mask,repl[name],filt,cver); done.add(name); continue
            body+=b[cs-12:ce]
        out+=hdr(0x16,body,ver)
    missing=set(repl)-done
    assert not missing, missing
    open(dst,'wb').write(out)

# Read textures of a TXD into PIL images: {name: Image}. Supports DXT1/DXT3 and 8888 (D3D8/D3D9).
def _c565(c): return ((c>>11&31)*255//31,(c>>5&63)*255//63,(c&31)*255//31)
def _dxt(data,w,h,kind):
    im=Image.new('RGBA',(w,h)); px=im.load(); o=0; bs=8 if kind==1 else 16
    for by in range(0,h,4):
        for bx in range(0,w,4):
            blk=data[o:o+bs]; o+=bs
            c0,c1,bits=struct.unpack('<HHI',blk[bs-8:]); a,b=_c565(c0),_c565(c1)
            if c0>c1 or kind!=1:
                cols=[a+(255,),b+(255,),tuple((2*x+y)//3 for x,y in zip(a,b))+(255,),tuple((x+2*y)//3 for x,y in zip(a,b))+(255,)]
            else:
                cols=[a+(255,),b+(255,),tuple((x+y)//2 for x,y in zip(a,b))+(255,),(0,0,0,0)]
            for i in range(16):
                x,y=bx+i%4,by+i//4
                if x<w and y<h: px[x,y]=cols[bits>>(2*i)&3]
    return im
def read(src):
    b=open(src,'rb').read(); res={}
    for t,s,e in rw.chunks(b,0,len(b)):
        if t!=0x16: continue
        for ct,cs,ce in rw.chunks(b,s,e):
            if ct!=0x15: continue
            ss=next(x for x in rw.chunks(b,cs,ce) if x[0]==1)[1]
            name=b[ss+8:ss+40].split(b'\0')[0].decode('latin1')
            fourcc=struct.unpack_from('<I',b,ss+76)[0]; w,h,dep=struct.unpack_from('<HHB',b,ss+80)
            p=ss+88+(1024 if dep==8 else 0); sz=struct.unpack_from('<I',b,p)[0]; data=b[p+4:p+4+sz]
            if name.startswith('bst_v_') and fourcc in (0x31545844,0x35545844):
                # Vehicle paint and decals use DXT1 and DXT5, including interpolated alpha.
                im=Image.frombytes('RGBA',(w,h),data,'bcn',1 if fourcc==0x31545844 else 3)
            elif fourcc in (0x31545844,0x33545844): im=_dxt(data,w,h,1 if fourcc==0x31545844 else 3)
            elif dep==32:
                im=Image.frombytes('RGBA',(w,h),data[:w*h*4]); r,g,bb,a=im.split(); im=Image.merge('RGBA',(bb,g,r,a))
            else: continue
            res[name]=im
    return res

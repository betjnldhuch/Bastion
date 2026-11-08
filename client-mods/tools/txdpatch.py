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

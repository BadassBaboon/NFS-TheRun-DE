import struct, sys
EXE=r"E:\Need for Speed The Run\Need For Speed The Run.exe"
d=open(EXE,'rb').read()
pe=struct.unpack_from('<I',d,0x3C)[0]
nsec=struct.unpack_from('<H',d,pe+6)[0]
opt=struct.unpack_from('<H',d,pe+20)[0]
base=struct.unpack_from('<I',d,pe+24+28)[0]
secs=[]
so=pe+24+opt
for i in range(nsec):
    o=so+i*40
    name=d[o:o+8].rstrip(b'\0').decode(errors='replace')
    vsz,va,rsz,ptr=struct.unpack_from('<IIII',d,o+8)
    secs.append((name,va,vsz,ptr,rsz))

def va2fo(va):
    rva=va-base
    for name,sva,vsz,ptr,rsz in secs:
        if sva<=rva<sva+max(vsz,rsz):
            off=rva-sva
            if off<rsz: return ptr+off
    return None

def u32(va):
    fo=va2fo(va)
    return struct.unpack_from('<I',d,fo)[0] if fo is not None else None
def u16(va):
    fo=va2fo(va)
    return struct.unpack_from('<H',d,fo)[0] if fo is not None else None
def cstr(va):
    fo=va2fo(va)
    if fo is None: return None
    e=d.index(b'\0',fo)
    return d[fo:e].decode(errors='replace')

typecache={}
def typename(tva):
    if tva in typecache: return typecache[tva]
    n=None
    try:
        tid=u32(tva)          # TypeInfo -> TypeInfoData
        if tid: n=cstr(u32(tid))
    except Exception: pass
    typecache[tva]=n or ("type_%08X"%tva)
    return typecache[tva]

def dump(clsname, typeinfo_va):
    cid=u32(typeinfo_va)
    if not cid: return None
    nm=cstr(u32(cid))
    cnt=u16(cid+4)
    arr=u32(cid+0x18)
    if not arr or not cnt or cnt>2000: return None
    out=[]
    for i in range(cnt):
        e=arr+i*12
        npv=u32(e); off=u16(e+6); tv=u32(e+8)
        fn=cstr(npv) if npv else None
        if not fn: continue
        out.append((off, fn, typename(tv) if tv else '?'))
    out.sort()
    return nm,out

CLASSES=[
 ("GameRenderSettings",0x2AACCBC),("WorldRenderSettings",0x2AE6E98),
 ("VisualEnvironmentSettings",0x2AE6E4C),("GlobalPostProcessSettings",0x2AA26AC),
 ("EmitterSystemSettings",0x2AD2B34),("DebrisSystemSettings",0x2AE735C),
 ("VegetationSystemSettings",0x2AE741C),("EnlightenRuntimeSettings",0x2AE5FF8),
 ("VisualTerrainSettings",0x2AAF720),("DecalSettings",0x2AA1CA4),
 ("EffectManagerSettings",0x2ABD588),("NfsGameSettings",0x2ADF658),
 ("TextureSettings",0x2AA0A38),("MeshSettings",0x2AA22C8),
 ("PhysicsSettings",0x2A9EB70),("GameTimeSettings",0x2AB54DC),
]
target=sys.argv[1] if len(sys.argv)>1 else None
for cn,ti in CLASSES:
    if target and target.lower() not in cn.lower(): continue
    r=dump(cn,ti)
    if not r: print("### %s: could not parse"%cn); continue
    nm,fields=r
    print("### %s  (%d fields)"%(nm,len(fields)))
    for off,fn,tn in fields:
        print("  0x%03X  %-34s %s"%(off,fn,tn))
    print()

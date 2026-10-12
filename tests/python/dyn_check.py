import numpy as np
sr=44100.0
dB=lambda x:10**(x/20); toDb=lambda x:20*np.log10(max(x,1e-5))
def fet(x,inDb=12,ratio=8,att=0.2,rel=250,mix=1.0,out=0.0,auto=True,allb=False):
    slope=1-1/ratio; thr=-16.0
    if allb: att*=3; rel*=0.6
    aC=np.exp(-1/(att*1e-3*sr)); rF=np.exp(-1/(rel*.25e-3*sr)); rS=np.exp(-1/(rel*1e-3*sr))
    grF=grS=0.0; y=np.zeros_like(x); grs=np.zeros_like(x)
    refGr=max(0,-10+inDb-thr)*slope
    levelG=dB(refGr-inDb) if auto else dB(inDb)      # <- proposed Auto Gain: level-match a -10 dBFS peak
    for i,s in enumerate(x):
        lvl=toDb(abs(s)*dB(inDb)); t=max(0,lvl-thr)*slope
        cF=aC if t>grF else rF; grF=cF*grF+(1-cF)*t
        cS=aC if t>grS else rS; grS=cS*grS+(1-cS)*t
        gr=.35*grF+.65*grS; grs[i]=gr
        y[i]=s*(1-mix)+(4*np.tanh(s*levelG*dB(-gr)*dB(out)/4))*mix
    return y,grs
def opto(x,peak=40,gain=0,limit=False):
    slope=1-1/(100 if limit else 3); thr=-6-peak*0.36; knee=10.0
    det=np.exp(-1/(.005*sr)); aT=np.exp(-1/(.010*sr)); rF=np.exp(-1/(.015*sr)); memA=np.exp(-1/(3*sr))
    env=fast=slow=mem=0.0; grs=np.zeros_like(x); y=np.zeros_like(x)
    for i,s in enumerate(x):
        env=max(abs(s),env*det); over=toDb(env)-thr
        gt=over*slope if 2*over>knee else (slope*(over+knee/2)**2/(2*knee) if 2*over>-knee else 0.0)
        mem=memA*mem+(1-memA)*min(1,max(0,gt/20)); rS=np.exp(-1/((0.5+4.5*mem)*sr))
        cF=aT if gt>fast else rF; cS=aT if gt>slow else rS
        fast=cF*fast+(1-cF)*gt; slow=cS*slow+(1-cS)*gt
        gr=.5*(fast+slow); grs[i]=gr; y[i]=s*dB(-gr)*dB(gain)
    return y,grs
t=lambda n:np.arange(int(n*sr))/sr
tone=lambda lvl,n:dB(lvl)*np.sin(2*np.pi*220*t(n))
print("== FET comp (1176-style) ==")
for lv in (-30,-20,-14,-10,-6):
    y,gr=fet(tone(lv,1.0)); print(" tone peak %4d dBFS -> steady GR %5.1f dB, out peak %6.1f dBFS (auto gain on)"%(lv,gr[-3000:].mean(),toDb(np.abs(y[-3000:]).max())))
y0,_=fet(tone(-10,1.0),auto=False); print(" auto OFF at -10 dBFS: out peak %.1f dBFS (hardware-like: input drive raises level)"%toDb(np.abs(y0[-3000:]).max()))
yo,_=fet(tone(-10,1.0),inDb=0); print(" Input=0 dB, -10 dBFS in: GR %.1f dB"%_[-3000:].mean())
for r in (4,8,12,20):
    _,gr=fet(tone(-12,1.0),ratio=r); print(" ratio %2d:1 at -12 dBFS peak -> GR %.1f dB (static curve says %.1f)"%(r,gr[-3000:].mean(),max(0,-12+12+16)*(1-1/r)))
sig=np.concatenate([tone(-10,1.0),tone(-60,1.5)])
_,gr=fet(sig); g0=gr[int(sr)-2000:int(sr)].mean(); k=int(sr)
half=np.argmax(gr[k:]<g0/2)/sr*1000; tenth=np.argmax(gr[k:]<g0*0.1)/sr*1000
print(" release (250 ms setting): GR from %.1f dB falls to 50%% in %.0f ms, to 10%% in %.0f ms"%(g0,half,tenth))
_,gr=fet(tone(-10,0.05),att=0.02); print(" attack 0.02 ms: GR after 1 ms %.1f dB of %.1f"%(gr[int(.001*sr)],gr[-100:].mean()))
_,gr=fet(tone(-10,0.05),att=0.8); print(" attack 0.8 ms: GR after 1 ms %.1f dB (slower)"%gr[int(.001*sr)])
print("== Opto comp (LA-2A-style) ==")
for pk in (20,40,60,80):
    y,gr=opto(tone(-10,2.0),peak=pk); print(" Peak Reduction %2d, tone -10 dBFS: GR %5.1f dB"%(pk,gr[-3000:].mean()))
for lim in (False,True):
    _,gr=opto(tone(-10,2.0),peak=40,limit=lim); print(" mode %s: GR %.1f dB"%("Limit" if lim else "Compress",gr[-3000:].mean()))
for dur in (0.3,3.0):
    sig=np.concatenate([tone(-10,dur),tone(-70,8)]); _,gr=opto(sig); k=int(dur*sr); g0=gr[k-100]
    h=np.argmax(gr[k:]<g0/2)/sr*1000; q=np.argmax(gr[k:]<g0*.1)/sr
    print(" after %.1f s of compression (%.1f dB): 50%% release in %.0f ms, 10%% in %.1f s"%(dur,g0,h,q))
print("   references: ~60-70 ms for the first half, then 0.5-5 s for the rest depending on how deep it was")

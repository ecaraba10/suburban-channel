import numpy as np
from scipy.signal import lfilter, freqz
sr=44100.0
db=lambda x:10**(x/20)
# --- JUCE IIR coefficient formulas (RBJ) ---
def lowshelf(f,Q,g):
    A=np.sqrt(g); w=2*np.pi*max(f,2)/sr; c=np.cos(w); s=np.sin(w); beta=s*np.sqrt(A)/Q
    b=[A*((A+1)-(A-1)*c+beta), 2*A*((A-1)-(A+1)*c), A*((A+1)-(A-1)*c-beta)]
    a=[(A+1)+(A-1)*c+beta, -2*((A-1)+(A+1)*c), (A+1)+(A-1)*c-beta]; return np.array(b)/a[0], np.array(a)/a[0]
def highshelf(f,Q,g):
    A=np.sqrt(g); w=2*np.pi*max(f,2)/sr; c=np.cos(w); s=np.sin(w); beta=s*np.sqrt(A)/Q
    b=[A*((A+1)+(A-1)*c+beta), -2*A*((A-1)+(A+1)*c), A*((A+1)+(A-1)*c-beta)]
    a=[(A+1)-(A-1)*c+beta, 2*((A-1)-(A+1)*c), (A+1)-(A-1)*c-beta]; return np.array(b)/a[0], np.array(a)/a[0]
def peak(f,Q,g):
    A=np.sqrt(g); w=2*np.pi*max(f,2)/sr; al=np.sin(w)/(2*Q); c2=-2*np.cos(w)
    b=[1+al*A,c2,1-al*A]; a=[1+al/A,c2,1-al/A]; return np.array(b)/a[0], np.array(a)/a[0]
def hpf(f,Q):
    w=2*np.pi*f/sr; c=np.cos(w); al=np.sin(w)/(2*Q)
    b=[(1+c)/2,-(1+c),(1+c)/2]; a=[1+al,-2*c,1-al]; return np.array(b)/a[0], np.array(a)/a[0]
def resp(filters,fs):
    h=np.ones(len(fs),complex)
    for b,a in filters:
        _,hh=freqz(b,a,worN=2*np.pi*np.array(fs)/sr); h*=hh
    return 20*np.log10(np.abs(h)+1e-12)
def at(filters,f): return resp(filters,[f])[0]

print("== API 550B-style EQ (550B steps) ==")
pq=lambda g:0.55+abs(g)*0.12
print(" LF shelf +12 @100: gain @20Hz %.1f, @100Hz %.1f, @10k %.1f dB"%(at([lowshelf(100,.7,db(12))],20),at([lowshelf(100,.7,db(12))],100),at([lowshelf(100,.7,db(12))],1e4)))
p=[peak(3000,pq(12),db(12))]; print(" HMF +12 @3k: @3k %.1f, @1k %.1f, @10k %.1f dB"%(at(p,3000),at(p,1000),at(p,1e4)))
p=[peak(3000,pq(2),db(2))]; q2=[peak(3000,pq(12),db(12))]
bw=lambda f:(lambda fr:(fr[np.where(resp(f,np.geomspace(100,2e4,4000))>=resp(f,[3000])[0]-3)[0][-1]]-fr[np.where(resp(f,np.geomspace(100,2e4,4000))>=resp(f,[3000])[0]-3)[0][0]]))(np.geomspace(100,2e4,4000))
print(" proportional Q: -3dB bandwidth @+2dB %.0f Hz vs @+12dB %.0f Hz (narrower at higher gain = expected)"%(bw(p),bw(q2)))

print("== SSL 4000 E-style EQ ==")
h1=[(np.array([1,-1]),np.array([1,0]))]  # placeholder
print(" HF shelf +15 @8k: @16k %.1f dB; bell +15 @8k Q.8: @8k %.1f, @2k %.1f"%(at([highshelf(8000,.7,db(15))],16000),at([peak(8000,.8,db(15))],8000),at([peak(8000,.8,db(15))],2000)))
for q in (0.5,1,4):
    f=[peak(3000,q,db(10))]; print(" HMF +10 dB, Q=%.1f: bandwidth(-3dB) %.0f Hz"%(q,bw(f)))
print(" HPF 18dB/oct @100: @50Hz %.1f, @100 %.1f, @400 %.1f"%tuple(at([hpf(100,1.0)],f)+(-20*np.log10(np.sqrt(1+(100/f)**2)) if True else 0) for f in (50,100,400)))

print("== Pultec EQP-1A-style ==")
def pult(lf,lb,la,hf,hb,bw_,haf,ha):
    fl=[]
    if lb>0: fl.append(lowshelf(lf,.6,db(lb*1.35)))
    if la>0: fl.append(lowshelf(lf*2,.9,db(-la*1.75)))
    if hb>0: fl.append(peak(hf,2.0-bw_*0.17,db(hb*1.8)))
    if ha>0: fl.append(highshelf(haf,.5,db(-ha*1.6)))
    return fl
print(" low boost 10 @60: DC %.1f dB (spec +13.5)"%at(pult(60,10,0,5000,0,5,10000,0),20))
print(" low atten 10 @60: DC %.1f dB (spec -17.5)"%at(pult(60,0,10,5000,0,5,10000,0),20))
print(" high boost 10 @10k: peak %.1f dB (spec +18)"%at(pult(60,0,0,10000,10,5,10000,0),10000))
print(" high atten 10 @10k: @20k %.1f dB (spec up to -16)"%at(pult(60,0,0,10000,0,5,10000,10),20000))
fs=np.geomspace(20,1000,800)
for lf in (30,60,100):
    r=resp(pult(lf,10,10,5000,0,5,10000,0),fs)
    print(" low-end trick @%d Hz (both full): max %.1f dB at %.0f Hz, min %.1f dB at %.0f Hz"%(lf,r.max(),fs[r.argmax()],r.min(),fs[r.argmin()]))
print("   open-source EQP-1A docs: boosting+cutting at 30 Hz gives a boost near 80 Hz and a dip near 200 Hz")

import numpy as np, itertools
exec(open('dsp_check.py').read().split('print("== API')[0])
B,C=13.5,17.5
def net(f,fb,Qb,fc,Qc,lb=B,lc=C,fs=None):
    fs=np.geomspace(f/4,f*60,900) if fs is None else fs
    fl=[]
    if lb>0: fl.append(lowshelf(f*fb,Qb,db(lb)))
    if lc>0: fl.append(lowshelf(f*fc,Qc,db(-lc)))
    return fs/f, resp(fl,fs)
best=[]
for fb,Qb,fc,Qc in itertools.product([1.5,2,3,4],[.5,.7,1.0,1.4,2.0],[3,4,6,8,12],[.5,.7,1.0,1.4,2.0]):
    x,r=net(30,fb,Qb,fc,Qc)
    w1=(x>=2)&(x<=4); w2=(x>=5)&(x<=10)
    bump=r[w1].max(); dip=r[w2].min(); far=r[x>40].max()
    xs,rb=net(30,fb,Qb,fc,Qc,lc=0); xc,rc=net(30,fb,Qb,fc,Qc,lb=0)
    over_b=rb.max()-B; over_c=-(rc.min()+C)    # overshoot of single curves beyond their shelf gain
    if bump>1.5 and dip<-2 and abs(far)<0.7 and over_b<3.5 and over_c<3.5:
        best.append((bump-dip-0.3*(over_b+over_c),fb,Qb,fc,Qc,bump,dip,over_b,over_c,r[0]))
best.sort(reverse=True)
print("score fb Qb fc Qc | bump dip | single-boost overshoot, single-cut overshoot | DC net")
for b in best[:8]: print("%.1f %s %s %s %s | %+.1f %+.1f | %.1f %.1f | %.1f"%b)

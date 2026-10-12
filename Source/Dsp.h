#pragma once
// Suburban Channel DSP core. Pure C++17 (no JUCE) so it can be compiled and tested on its own: see tests/.
// Every module processes up to two channels in place: float** d, nCh, n.
#include <algorithm>
#include <cmath>
#include <vector>

// Windows headers can define min/max macros that break std::min/std::max.
#ifdef min
 #undef min
#endif
#ifdef max
 #undef max
#endif

namespace bgtr
{
constexpr double kPi = 3.14159265358979323846;

inline float  dB2g (float db) { return std::pow (10.f, db * 0.05f); }
inline float  g2dB (float g)  { return 20.f * std::log10 (std::max (g, 1.0e-5f)); }
inline float  clampf (float v, float lo, float hi) { return std::min (std::max (v, lo), hi); }
inline int    clampi (int v, int lo, int hi)       { return std::min (std::max (v, lo), hi); }
inline double limF (double f, double sr)           { return std::min (f, sr * 0.45); }

// Published panel values of the hardware this plugin follows.
inline constexpr float kApiGainDb[11] = { -12.f, -9.f, -6.f, -4.f, -2.f, 0.f, 2.f, 4.f, 6.f, 9.f, 12.f };
inline constexpr float kApiLf [7] = { 30.f, 40.f, 50.f, 100.f, 200.f, 300.f, 400.f };
inline constexpr float kApiLmf[7] = { 75.f, 150.f, 180.f, 240.f, 500.f, 700.f, 1000.f };
inline constexpr float kApiHmf[7] = { 800.f, 1500.f, 3000.f, 5000.f, 8000.f, 10000.f, 12500.f };
inline constexpr float kApiHf [7] = { 2500.f, 5000.f, 7500.f, 10000.f, 12500.f, 15000.f, 20000.f };
inline constexpr float kPtLf [4] = { 20.f, 30.f, 60.f, 100.f };
inline constexpr float kPtHf [7] = { 3000.f, 4000.f, 5000.f, 8000.f, 10000.f, 12000.f, 16000.f };
inline constexpr float kPtHaf[3] = { 5000.f, 10000.f, 20000.f };

//==============================================================================
// Biquads (RBJ cookbook, the same formulas JUCE's IIR::Coefficients use). Real-time safe: no allocation.
struct Coef { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

inline Coef makeCoef (double b0, double b1, double b2, double a0, double a1, double a2)
{
    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}
inline Coef lowShelf (double sr, double f, double Q, double gain)
{
    const double A = std::sqrt (gain), w = 2 * kPi * std::max (f, 2.0) / sr, c = std::cos (w), s = std::sin (w), beta = s * std::sqrt (A) / Q;
    return makeCoef (A * ((A + 1) - (A - 1) * c + beta), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - beta),
                     (A + 1) + (A - 1) * c + beta, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - beta);
}
inline Coef highShelf (double sr, double f, double Q, double gain)
{
    const double A = std::sqrt (gain), w = 2 * kPi * std::max (f, 2.0) / sr, c = std::cos (w), s = std::sin (w), beta = s * std::sqrt (A) / Q;
    return makeCoef (A * ((A + 1) + (A - 1) * c + beta), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - beta),
                     (A + 1) - (A - 1) * c + beta, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - beta);
}
inline Coef peakEq (double sr, double f, double Q, double gain)
{
    const double A = std::sqrt (gain), w = 2 * kPi * std::max (f, 2.0) / sr, al = std::sin (w) / (2 * Q), c2 = -2 * std::cos (w);
    return makeCoef (1 + al * A, c2, 1 - al * A, 1 + al / A, c2, 1 - al / A);
}
inline Coef highPass (double sr, double f, double Q)
{
    const double w = 2 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2 * Q);
    return makeCoef ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
}
inline Coef lowPass (double sr, double f, double Q)
{
    const double w = 2 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2 * Q);
    return makeCoef ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
}
inline Coef highPass1 (double sr, double f)
{
    const double K = std::tan (kPi * f / sr);
    return { 1 / (1 + K), -1 / (1 + K), 0, (K - 1) / (1 + K), 0 };
}

struct Biquad
{
    Coef c;
    double z1[2] { 0, 0 }, z2[2] { 0, 0 };
    void set (const Coef& n) { c = n; }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    inline float tick (float x, int ch)
    {
        const double in = x, y = c.b0 * in + z1[ch];
        z1[ch] = c.b1 * in - c.a1 * y + z2[ch];
        z2[ch] = c.b2 * in - c.a2 * y;
        return (float) y;
    }
};

//==============================================================================
// Smoothing, to avoid zipper noise on knob moves.
struct Smooth
{
    float v = 0.f;
    bool primed = false;
    void reset() { primed = false; }
    inline float tick (float target, float a)          // per sample
    {
        if (! primed) { v = target; primed = true; }
        v += (target - v) * a;
        return v;
    }
    inline float block (float target, float a)         // per block
    {
        if (! primed || std::fabs (target - v) < 1.0e-4f * std::max (1.f, std::fabs (target))) { v = target; primed = true; }
        else v += (target - v) * a;
        return v;
    }
};
inline float coefMs (double sr, float ms)          { return 1.f - std::exp (-1.f / (std::max (ms, 0.01f) * 0.001f * (float) sr)); }
inline float coefBlock (double sr, int n, float ms) { return 1.f - std::exp (-(float) n / (ms * 0.001f * (float) sr)); }

//==============================================================================
// First-order antiderivative anti-aliasing for tanh: far less aliasing than plain tanh, zero latency.
struct TanhAdaa
{
    double uPrev = 0.0;
    void reset() { uPrev = 0.0; }
    static inline double logCosh (double x) { x = std::fabs (x); return x + std::log1p (std::exp (-2.0 * x)) - 0.6931471805599453; }
    inline float tick (float u)
    {
        const double du = (double) u - uPrev;
        const double y = std::fabs (du) < 1.0e-5 ? std::tanh (0.5 * ((double) u + uPrev)) : (logCosh (u) - logCosh (uPrev)) / du;
        uPrev = u;
        return (float) y;
    }
};

//==============================================================================
struct Gate
{
    struct P { float threshDb = -50.f, rangeDb = 30.f, releaseMs = 80.f; };
    float env = 0.f, gain = 1.f;
    bool open = false;
    void reset() { env = 0.f; gain = 1.f; open = false; }

    void process (float** d, int nCh, int n, double sr, const P& p)
    {
        const float openT = dB2g (p.threshDb), closeT = openT * 0.7f, floorG = dB2g (-p.rangeDb), fs = (float) sr;
        const float det = std::exp (-1.f / (0.005f * fs));
        const float atk = 1.f - std::exp (-1.f / (0.0005f * fs));
        const float rel = 1.f - std::exp (-1.f / (std::max (1.f, p.releaseMs) * 0.001f * fs));
        for (int i = 0; i < n; ++i)
        {
            float lvl = 0.f;
            for (int c = 0; c < nCh; ++c) lvl = std::max (lvl, std::fabs (d[c][i]));
            env = std::max (lvl, env * det);
            if (open) { if (env < closeT) open = false; }
            else if (env > openT) open = true;
            const float target = open ? 1.f : floorG;
            gain += (target - gain) * (target > gain ? atk : rel);
            for (int c = 0; c < nCh; ++c) d[c][i] *= gain;
        }
    }
};

//==============================================================================
// FET compressor following the UREI 1176 as documented: fixed threshold with the Input knob driving it,
// ratios 4/8/12/20/All, 20-800 us attack, 50-1100 ms release, FET distortion that grows with gain reduction,
// Class-A output stage. Auto Gain level-matches a -10 dBFS peak; off = hardware behavior (input raises level).
struct FetComp
{
    struct P { float inDb = 12.f; int ratio = 1; float attackMs = 0.2f, releaseMs = 250.f, mix = 1.f, outDb = 0.f, scHpf = 20.f; bool autoGain = true; };
    float grF = 0.f, grS = 0.f, dcIn[2] { 0, 0 }, dcOut[2] { 0, 0 };
    TanhAdaa ad[2];
    Biquad sc;
    Smooth sMix, sOut;
    void reset() { grF = grS = 0.f; dcIn[0] = dcIn[1] = dcOut[0] = dcOut[1] = 0.f; ad[0].reset(); ad[1].reset(); sc.reset(); sMix.reset(); sOut.reset(); }

    float process (float** d, int nCh, int n, double sr, const P& p)
    {
        static const float ratios[5] = { 4.f, 8.f, 12.f, 20.f, 16.f };
        const int ri = clampi (p.ratio, 0, 4);
        const bool all = (ri == 4);
        const float inG = dB2g (p.inDb), slope = 1.f - 1.f / ratios[ri], thr = -16.f, fs = (float) sr;
        const float atk = p.attackMs * (all ? 3.f : 1.f), rel = p.releaseMs * (all ? 0.6f : 1.f);
        const float aC = std::exp (-1.f / (atk * 0.001f * fs));
        const float rF = std::exp (-1.f / (rel * 0.25f * 0.001f * fs)), rS = std::exp (-1.f / (rel * 0.001f * fs));
        const float refGr = std::max (0.f, -10.f + p.inDb - thr) * slope;
        const float levelG = p.autoGain ? dB2g (refGr) : inG;
        const bool useSc = p.scHpf > 21.f;
        if (useSc) sc.set (highPass (sr, limF (p.scHpf, sr), 0.707));
        const float a = coefMs (sr, 15.f), outT = dB2g (p.outDb);
        float gr = grF * 0.35f + grS * 0.65f;

        for (int i = 0; i < n; ++i)
        {
            float pk = 0.f;
            for (int c = 0; c < nCh; ++c)
            {
                const float s = d[c][i], det = useSc ? sc.tick (s, c) : s;
                pk = std::max (pk, std::fabs (det));
            }
            const float lvl = g2dB (pk * inG), target = std::max (0.f, lvl - thr) * slope;
            const float cF = target > grF ? aC : rF, cS = target > grS ? aC : rS;
            grF = cF * grF + (1.f - cF) * target;
            grS = cS * grS + (1.f - cS) * target;
            gr = 0.35f * grF + 0.65f * grS;

            const float g = dB2g (-gr), depth = clampf (gr / 20.f, 0.f, 1.f);
            const float mix = sMix.tick (p.mix, a), outG = sOut.tick (outT, a);
            for (int c = 0; c < nCh; ++c)
            {
                const float x = d[c][i];
                float y = x * levelG * g;
                y += 0.08f * depth * y * y;                              // FET divider: even-order distortion grows with GR
                const float o = y - dcIn[c] + 0.9995f * dcOut[c];        // remove the DC the squared term creates
                dcIn[c] = y; dcOut[c] = o;
                y = 4.f * ad[c].tick (o * outG * 0.25f);                 // Class-A output stage, mild
                d[c][i] = x * (1.f - mix) + y * mix;
            }
        }
        return gr;
    }
};

//==============================================================================
// Opto compressor following the Teletronix LA-2A as documented: Peak Reduction / Gain / Compress-Limit,
// ~10 ms attack, two-stage release (~60 ms to the first half, then 0.5-5 s that lengthens after heavy or long
// compression), compress about 3:1 and limit about 100:1, gentle tube-style coloration.
struct OptoComp
{
    struct P { float peak = 40.f, gainDb = 0.f; bool limit = false; float scHpf = 20.f; };
    float env = 0.f, fast = 0.f, slow = 0.f, mem = 0.f, dcIn[2] { 0, 0 }, dcOut[2] { 0, 0 };
    TanhAdaa ad[2];
    Biquad sc;
    Smooth sGain;
    void reset() { env = fast = slow = mem = 0.f; dcIn[0] = dcIn[1] = dcOut[0] = dcOut[1] = 0.f; ad[0].reset(); ad[1].reset(); sc.reset(); sGain.reset(); }

    float process (float** d, int nCh, int n, double sr, const P& p)
    {
        const float fs = (float) sr, slope = 1.f - 1.f / (p.limit ? 100.f : 3.f), thr = -6.f - p.peak * 0.36f, knee = 10.f;
        const float det = std::exp (-1.f / (0.005f * fs)), aT = std::exp (-1.f / (0.010f * fs));
        const float rFast = std::exp (-1.f / (0.015f * fs)), memA = std::exp (-1.f / (3.0f * fs));
        const float rSlow = std::exp (-1.f / ((0.5f + 4.5f * mem) * fs));
        const bool useSc = p.scHpf > 21.f;
        if (useSc) sc.set (highPass (sr, limF (p.scHpf, sr), 0.707));
        const float kT = 0.7f, bias = 0.08f, bOff = std::tanh (bias), norm = 1.f / (kT * (1.f - bOff * bOff));
        const float a = coefMs (sr, 15.f), gT = dB2g (p.gainDb);
        float gr = 0.5f * (fast + slow);

        for (int i = 0; i < n; ++i)
        {
            float pk = 0.f;
            for (int c = 0; c < nCh; ++c)
            {
                const float s = d[c][i], dt = useSc ? sc.tick (s, c) : s;
                pk = std::max (pk, std::fabs (dt));
            }
            env = std::max (pk, env * det);
            const float over = g2dB (env) - thr;
            float gt = 0.f;
            if (2.f * over > knee)       gt = over * slope;
            else if (2.f * over > -knee) gt = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.f * knee);

            mem = memA * mem + (1.f - memA) * clampf (gt / 20.f, 0.f, 1.f);
            const float cF = gt > fast ? aT : rFast, cS = gt > slow ? aT : rSlow;
            fast = cF * fast + (1.f - cF) * gt;
            slow = cS * slow + (1.f - cS) * gt;
            gr = 0.5f * (fast + slow);

            const float g = dB2g (-gr) * sGain.tick (gT, a);
            for (int c = 0; c < nCh; ++c)
            {
                const float y = (ad[c].tick (kT * d[c][i] * g + bias) - bOff) * norm;
                const float o = y - dcIn[c] + 0.9995f * dcOut[c];
                dcIn[c] = y; dcOut[c] = o;
                d[c][i] = o;
            }
        }
        return gr;
    }
};

//==============================================================================
// Bus compressor following the SSL G-series mix-bus compressor as documented: stereo-linked VCA-style, ratios
// 2/4/10:1, attack 0.1/0.3/1/3/10/30 ms, release 0.1/0.3/0.6/1.2 s or a program-dependent Auto, makeup 0-15 dB,
// the effective threshold drops as the ratio is lowered. The stepped times are the published 10-90% values, so the
// time constants are time/2.2 (the same ratio comes out of the RC values on the Gyraf clone schematic). Auto uses
// a fast and a slow release stage weighted by how long the compression has been engaged.
// Approximated: feed-forward detector (the hardware is mostly feed-back), the soft knee and the ratio-dependent
// threshold offsets (-4/-2/0 dB for 2/4/10:1).
struct BusComp
{
    struct P { float threshDb = -12.f; int ratio = 1, attack = 3, release = 4; float makeupDb = 0.f, mix = 1.f, scHpf = 20.f; };
    float grA = 0.f, grB = 0.f, mem = 0.f;
    Biquad sc;
    Smooth sMk, sMix;
    void reset() { grA = grB = mem = 0.f; sc.reset(); sMk.reset(); sMix.reset(); }

    float process (float** d, int nCh, int n, double sr, const P& p)
    {
        static const float ratios[3] = { 2.f, 4.f, 10.f }, atkMs[6] = { 0.1f, 0.3f, 1.f, 3.f, 10.f, 30.f }, relMs[4] = { 100.f, 300.f, 600.f, 1200.f };
        const int ri = clampi (p.ratio, 0, 2);
        const float fs = (float) sr, slope = 1.f - 1.f / ratios[ri], knee = 6.f;
        const float thr = p.threshDb - (ri == 0 ? 4.f : ri == 1 ? 2.f : 0.f);
        const float aC = std::exp (-1.f / (atkMs[clampi (p.attack, 0, 5)] / 2.2f * 0.001f * fs));
        const bool autoRel = p.release >= 4;
        const float rSingle = std::exp (-1.f / (relMs[clampi (p.release, 0, 3)] / 2.2f * 0.001f * fs));
        const float rFast = std::exp (-1.f / (0.060f * fs)), rSlow = std::exp (-1.f / (0.62f * fs));
        const float memUp = std::exp (-1.f / (1.0f * fs)), memDown = std::exp (-1.f / (4.0f * fs));   // memory builds in ~1 s, fades in ~4 s
        const bool useSc = p.scHpf > 21.f;
        if (useSc) sc.set (highPass (sr, limF (p.scHpf, sr), 0.707));
        const float a = coefMs (sr, 15.f), mkT = dB2g (p.makeupDb);
        float gr = grA;

        for (int i = 0; i < n; ++i)
        {
            float pk = 0.f;
            for (int c = 0; c < nCh; ++c)
            {
                const float s = d[c][i], dt = useSc ? sc.tick (s, c) : s;
                pk = std::max (pk, std::fabs (dt));
            }
            const float over = g2dB (pk) - thr;
            float gt = 0.f;
            if (2.f * over > knee)       gt = over * slope;
            else if (2.f * over > -knee) gt = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.f * knee);

            if (autoRel)
            {
                const float mt = clampf (gt / 8.f, 0.f, 1.f), mc = mt > mem ? memUp : memDown;
                mem = mc * mem + (1.f - mc) * mt;
                const float w = 0.15f + 0.85f * mem;     // short peaks release fast, sustained compression releases slowly
                const float cF = gt > grA ? aC : rFast, cS = gt > grB ? aC : rSlow;
                grA = cF * grA + (1.f - cF) * gt;
                grB = cS * grB + (1.f - cS) * gt;
                gr = (1.f - w) * grA + w * grB;
            }
            else
            {
                const float c = gt > grA ? aC : rSingle;
                grA = c * grA + (1.f - c) * gt;
                grB = grA;
                gr = grA;
            }
            const float g = dB2g (-gr), mk = sMk.tick (mkT, a), mix = sMix.tick (p.mix, a);
            for (int c = 0; c < nCh; ++c)
            {
                const float x = d[c][i];
                d[c][i] = x * (1.f - mix) + x * g * mk * mix;
            }
        }
        return gr;
    }
};

//==============================================================================
// API 550B-style EQ: stepped frequencies and gains as on the panel (indices into the tables above),
// reciprocal boost/cut, shelf/peak switch on LF and HF, proportional Q. Plus an extra high-pass.
struct ProcessEq
{
    struct P { float hpf = 20.f; int lf = 3, lg = 5, lmf = 4, lmg = 5, hmf = 2, hmg = 5, hf = 3, hg = 5; bool lShelf = true, hShelf = true; };
    Biquad bHpf, bLf, bLmf, bHmf, bHf;
    Smooth s[9];
    void reset() { bHpf.reset(); bLf.reset(); bLmf.reset(); bHmf.reset(); bHf.reset(); for (auto& x : s) x.reset(); }

    void process (float** d, int nCh, int n, double sr, const P& p)
    {
        const float a = coefBlock (sr, n, 30.f);
        auto pq = [] (float g) { return 0.55 + std::fabs (g) * 0.12; };
        const float lf = s[0].block (kApiLf[clampi (p.lf, 0, 6)], a), lg = s[1].block (kApiGainDb[clampi (p.lg, 0, 10)], a);
        const float lmf = s[2].block (kApiLmf[clampi (p.lmf, 0, 6)], a), lmg = s[3].block (kApiGainDb[clampi (p.lmg, 0, 10)], a);
        const float hmf = s[4].block (kApiHmf[clampi (p.hmf, 0, 6)], a), hmg = s[5].block (kApiGainDb[clampi (p.hmg, 0, 10)], a);
        const float hf = s[6].block (kApiHf[clampi (p.hf, 0, 6)], a), hg = s[7].block (kApiGainDb[clampi (p.hg, 0, 10)], a);
        const float hp = s[8].block (p.hpf, a);
        const bool useHpf = hp > 21.f;
        if (useHpf) bHpf.set (highPass (sr, limF (hp, sr), 0.707));
        bLf.set (p.lShelf ? lowShelf (sr, limF (lf, sr), 0.7, dB2g (lg)) : peakEq (sr, limF (lf, sr), pq (lg), dB2g (lg)));
        bLmf.set (peakEq (sr, limF (lmf, sr), pq (lmg), dB2g (lmg)));
        bHmf.set (peakEq (sr, limF (hmf, sr), pq (hmg), dB2g (hmg)));
        bHf.set (p.hShelf ? highShelf (sr, limF (hf, sr), 0.7, dB2g (hg)) : peakEq (sr, limF (hf, sr), pq (hg), dB2g (hg)));
        for (int c = 0; c < nCh; ++c)
            for (int i = 0; i < n; ++i)
            {
                float x = d[c][i];
                if (useHpf) x = bHpf.tick (x, c);
                d[c][i] = bHf.tick (bHmf.tick (bLmf.tick (bLf.tick (x, c), c), c), c);
            }
    }
};

//==============================================================================
// SSL 4000 E-style EQ: LF 30-450 Hz, LMF 0.2-2.5 kHz, HMF 0.6-7 kHz, HF 1.5-16 kHz, +/-15 dB, bell option on
// LF/HF, parametric Q on the mids, 18 dB/oct HPF 16-600 Hz (the original stops at 350; extended for lo-fi radio
// sounds), 12 dB/oct LPF 3-22 kHz (fully out = bypassed).
struct SolidEq
{
    struct P { float hpf = 16.f, lpf = 22000.f, lf = 100.f, lg = 0.f, lmf = 600.f, lmg = 0.f, lmq = 1.f, hmf = 3000.f, hmg = 0.f, hmq = 1.f, hf = 8000.f, hg = 0.f; bool lBell = false, hBell = false; };
    Biquad bH1, bH2, bLp, bLf, bLmf, bHmf, bHf;
    Smooth s[12];
    void reset() { bH1.reset(); bH2.reset(); bLp.reset(); bLf.reset(); bLmf.reset(); bHmf.reset(); bHf.reset(); for (auto& x : s) x.reset(); }

    void process (float** d, int nCh, int n, double sr, const P& p)
    {
        const float a = coefBlock (sr, n, 30.f);
        const float hp = s[0].block (p.hpf, a), lp = s[1].block (p.lpf, a), lf = s[2].block (p.lf, a), lg = s[3].block (p.lg, a);
        const float lmf = s[4].block (p.lmf, a), lmg = s[5].block (p.lmg, a), lmq = s[6].block (p.lmq, a);
        const float hmf = s[7].block (p.hmf, a), hmg = s[8].block (p.hmg, a), hmq = s[9].block (p.hmq, a);
        const float hf = s[10].block (p.hf, a), hg = s[11].block (p.hg, a);
        const bool useHpf = hp > 17.f, useLpf = lp < 21900.f;
        if (useHpf) { bH1.set (highPass1 (sr, limF (hp, sr))); bH2.set (highPass (sr, limF (hp, sr), 1.0)); }
        if (useLpf) bLp.set (lowPass (sr, limF (lp, sr), 0.707));
        bLf.set (p.lBell ? peakEq (sr, limF (lf, sr), 0.8, dB2g (lg)) : lowShelf (sr, limF (lf, sr), 0.7, dB2g (lg)));
        bLmf.set (peakEq (sr, limF (lmf, sr), lmq, dB2g (lmg)));
        bHmf.set (peakEq (sr, limF (hmf, sr), hmq, dB2g (hmg)));
        bHf.set (p.hBell ? peakEq (sr, limF (hf, sr), 0.8, dB2g (hg)) : highShelf (sr, limF (hf, sr), 0.7, dB2g (hg)));
        for (int c = 0; c < nCh; ++c)
            for (int i = 0; i < n; ++i)
            {
                float x = d[c][i];
                if (useHpf) x = bH2.tick (bH1.tick (x, c), c);
                x = bHf.tick (bHmf.tick (bLmf.tick (bLf.tick (x, c), c), c), c);
                if (useLpf) x = bLp.tick (x, c);
                d[c][i] = x;
            }
    }
};

//==============================================================================
// Pultec EQP-1A-style: panel frequencies and maximum boost/cut from the published specs (low +13.5 / -17.5 dB,
// high boost +18 dB with bandwidth, high atten -16 dB). The passive network is approximated with a resonant
// boost shelf and a gentler cut shelf whose corners and Q are fitted so that boosting and cutting at 30 Hz together
// gives a boost near 80 Hz and a dip near 200 Hz (the documented "low-end trick"). Optional clean tube make-up amp.
struct PulseEq
{
    struct P { int lf = 2; float lb = 0.f, la = 0.f; int hf = 4; float hb = 0.f, bw = 5.f; int haf = 1; float ha = 0.f; bool amp = true; };
    Biquad bLb, bLa, bHb, bHa;
    TanhAdaa ad[2];
    Smooth s[8];
    void reset() { bLb.reset(); bLa.reset(); bHb.reset(); bHa.reset(); ad[0].reset(); ad[1].reset(); for (auto& x : s) x.reset(); }

    void process (float** d, int nCh, int n, double sr, const P& p)
    {
        const float a = coefBlock (sr, n, 30.f);
        const float lf = s[0].block (kPtLf[clampi (p.lf, 0, 3)], a), lb = s[1].block (p.lb, a), la = s[2].block (p.la, a);
        const float hf = s[3].block (kPtHf[clampi (p.hf, 0, 6)], a), hb = s[4].block (p.hb, a), bw = s[5].block (p.bw, a);
        const float haf = s[6].block (kPtHaf[clampi (p.haf, 0, 2)], a), ha = s[7].block (p.ha, a);
        bLb.set (lowShelf  (sr, limF (lf * 4.1, sr),  1.4, dB2g ( lb * 1.35f)));
        bLa.set (lowShelf  (sr, limF (lf * 3.55, sr), 0.6, dB2g (-la * 1.75f)));
        bHb.set (peakEq    (sr, limF (hf, sr), 2.0 - bw * 0.17, dB2g (hb * 1.8f)));
        bHa.set (highShelf (sr, limF (haf, sr), 0.5, dB2g (-ha * 1.6f)));

        constexpr float k = 0.548f;       // x - (k^2/3) x^3 with k^2/3 ~ 0.1 -> ~0.15% THD at -12 dBFS (spec: 0.15% at +10 dBm)
        for (int c = 0; c < nCh; ++c)
            for (int i = 0; i < n; ++i)
            {
                float x = bHa.tick (bHb.tick (bLa.tick (bLb.tick (d[c][i], c), c), c), c);
                if (p.amp) x = ad[c].tick (k * x) / k;
                d[c][i] = x;
            }
    }
};

//==============================================================================
// Tube/console-style saturation: biased tanh (even harmonics) with anti-aliasing and a DC blocker.
struct Sat
{
    struct P { float driveDb = 6.f, bias = 0.1f, mix = 1.f, outDb = 0.f; };
    float dcIn[2] { 0, 0 }, dcOut[2] { 0, 0 };
    TanhAdaa ad[2];
    Smooth sDrive, sMix, sOut;
    void reset() { dcIn[0] = dcIn[1] = dcOut[0] = dcOut[1] = 0.f; ad[0].reset(); ad[1].reset(); sDrive.reset(); sMix.reset(); sOut.reset(); }

    void process (float** d, int nCh, int n, double sr, const P& p, bool antiAlias = true)
    {
        const float a = coefMs (sr, 15.f), bOff = std::tanh (p.bias), dT = dB2g (p.driveDb), oT = dB2g (p.outDb);
        for (int i = 0; i < n; ++i)
        {
            const float drive = sDrive.tick (dT, a), mix = sMix.tick (p.mix, a), outG = sOut.tick (oT, a), comp = 1.f / std::sqrt (drive);
            for (int c = 0; c < nCh; ++c)
            {
                const float x = d[c][i], u = x * drive + p.bias;
                const float y = ((antiAlias ? ad[c].tick (u) : std::tanh (u)) - bOff) * comp;
                const float o = y - dcIn[c] + 0.9995f * dcOut[c];
                dcIn[c] = y; dcOut[c] = o;
                d[c][i] = (x * (1.f - mix) + o * mix) * outG;
            }
        }
    }
};

//==============================================================================
// Mid/side width plus a Haas delay on the right channel.
struct Width
{
    std::vector<float> buf;
    int pos = 0;
    Smooth sW, sD;
    void prepare (double sr) { buf.assign ((size_t) (sr * 0.05) + 8, 0.f); pos = 0; sW.reset(); sD.reset(); }
    void reset() { std::fill (buf.begin(), buf.end(), 0.f); pos = 0; sW.reset(); sD.reset(); }

    void process (float** d, int nCh, int n, double sr, float width, float haasMs)
    {
        if (nCh < 2 || buf.empty()) return;
        const int sz = (int) buf.size();
        const float a = coefMs (sr, 15.f), dT = std::min (haasMs * 0.001f * (float) sr, (float) (sz - 3));
        for (int i = 0; i < n; ++i)
        {
            const float w = sW.tick (width, a), dl = sD.tick (dT, a);
            const float m = 0.5f * (d[0][i] + d[1][i]), s = 0.5f * (d[0][i] - d[1][i]) * w;
            buf[(size_t) pos] = m - s;
            const int i0 = (int) dl;
            const float fr = dl - (float) i0;
            int r0 = pos - i0; if (r0 < 0) r0 += sz;
            int r1 = r0 - 1;   if (r1 < 0) r1 += sz;
            d[0][i] = m + s;
            d[1][i] = buf[(size_t) r0] * (1.f - fr) + buf[(size_t) r1] * fr;
            pos = (pos + 1) % sz;
        }
    }
};
} // namespace bgtr

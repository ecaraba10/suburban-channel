// Standalone tests for the Suburban Channel DSP core (no JUCE needed).
//   g++ -std=c++17 -O2 -Wall -Wextra tests/dsp_test.cpp -o dsp_test && ./dsp_test
#include "../Source/Dsp.h"
#include <cstdio>
#include <random>
using namespace bgtr;

static int fails = 0, checks = 0;
static void check (bool ok, const char* what, double value, const char* expect)
{
    ++checks; if (! ok) ++fails;
    std::printf ("  [%s] %-58s %9.2f  (expect %s)\n", ok ? "PASS" : "FAIL", what, value, expect);
}
static std::vector<float> sine (double f, float lvlDb, double sec, double sr)
{
    std::vector<float> x ((size_t) (sec * sr));
    for (size_t i = 0; i < x.size(); ++i) x[i] = dB2g (lvlDb) * (float) std::sin (2 * kPi * f * (double) i / sr);
    return x;
}
template <class F> static void runMono (std::vector<float>& x, F fn, int blk = 256)
{
    for (size_t i = 0; i < x.size(); i += (size_t) blk)
    {
        int n = (int) std::min ((size_t) blk, x.size() - i);
        float* ch[1] = { x.data() + i };
        fn (ch, 1, n);
    }
}
static double rms (const std::vector<float>& x, size_t from, size_t to)
{
    double s = 0; for (size_t i = from; i < to; ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / (double) (to - from));
}
static double peakDb (const std::vector<float>& x, size_t from, size_t to)
{
    float m = 0; for (size_t i = from; i < to; ++i) m = std::max (m, std::fabs (x[i]));
    return g2dB (m);
}
template <class F> static double gainAt (F fn, double f, double sr, float lvl = -30.f)
{
    auto x = sine (f, lvl, f < 100 ? 3.0 : 1.5, sr), y = x;
    runMono (y, fn);
    const size_t a = x.size() / 2, b = x.size();
    return 20 * std::log10 (rms (y, a, b) / rms (x, a, b));
}
static double goertzel (const std::vector<float>& x, double f, double sr, size_t from, size_t to)
{
    const double w = 2 * kPi * f / sr, c = 2 * std::cos (w); double s1 = 0, s2 = 0;
    for (size_t i = from; i < to; ++i) { double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) * 2.0 / (double) (to - from);
}

int main()
{
    const double sr = 44100.0;

    std::printf ("API 550B-style EQ\n");
    { ProcessEq e; ProcessEq::P p; p.lf = 3; p.lg = 10; p.lShelf = true;
      auto fn = [&] (float** d, int c, int n) { e.process (d, c, n, sr, p); };
      check (gainAt (fn, 30, sr) > 10.5, "LF shelf +12 dB @100 Hz: gain at 30 Hz", gainAt (fn, 30, sr), "> 10.5 dB");
      check (std::fabs (gainAt (fn, 10000, sr)) < 0.5, "LF shelf: gain at 10 kHz", gainAt (fn, 10000, sr), "~ 0 dB"); }
    { ProcessEq e; ProcessEq::P p; p.hmf = 2; p.hmg = 10;                      // 3 kHz, +12
      auto fn = [&] (float** d, int c, int n) { e.process (d, c, n, sr, p); };
      check (std::fabs (gainAt (fn, 3000, sr) - 12.f) < 0.7, "HMF +12 dB @3 kHz: gain at 3 kHz", gainAt (fn, 3000, sr), "12 dB"); }
    { ProcessEq e; ProcessEq::P p; p.lmg = 0; p.lmf = 4;                       // 500 Hz, -12 (reciprocal)
      auto fn = [&] (float** d, int c, int n) { e.process (d, c, n, sr, p); };
      check (std::fabs (gainAt (fn, 500, sr) + 12.f) < 0.7, "LMF -12 dB @500 Hz: gain at 500 Hz", gainAt (fn, 500, sr), "-12 dB"); }

    std::printf ("SSL 4000 E-style EQ\n");
    { SolidEq e; SolidEq::P p; p.hg = 15.f; p.hf = 8000.f;
      auto fn = [&] (float** d, int c, int n) { e.process (d, c, n, sr, p); };
      check (gainAt (fn, 16000, sr) > 14.0, "HF shelf +15 dB @8 kHz: gain at 16 kHz", gainAt (fn, 16000, sr), "> 14 dB"); }
    { SolidEq e; SolidEq::P p; p.lpf = 3000.f;
      auto fn = [&] (float** d, int c, int n) { e.process (d, c, n, sr, p); };
      check (gainAt (fn, 12000, sr) < -18.0, "LPF 3 kHz: gain at 12 kHz", gainAt (fn, 12000, sr), "< -18 dB"); }
    { SolidEq e; SolidEq::P p; p.hpf = 100.f;
      auto fn = [&] (float** d, int c, int n) { e.process (d, c, n, sr, p); };
      check (std::fabs (gainAt (fn, 50, sr) + 18.1) < 2.0, "HPF 100 Hz (18 dB/oct): gain at 50 Hz", gainAt (fn, 50, sr), "~ -18 dB"); }
    { SolidEq e; SolidEq::P p; p.hmg = 10; p.hmq = 4;   // narrow vs wide
      auto fn = [&] (float** d, int c, int n) { e.process (d, c, n, sr, p); };
      double narrow = gainAt (fn, 4500, sr); SolidEq e2; SolidEq::P p2; p2.hmg = 10; p2.hmq = 0.5f;
      auto fn2 = [&] (float** d, int c, int n) { e2.process (d, c, n, sr, p2); };
      double wide = gainAt (fn2, 4500, sr);
      check (wide > narrow + 3, "HMF Q: wide curve boosts 4.5 kHz more than narrow", wide - narrow, "> 3 dB"); }

    std::printf ("Lo-fi radio band-limit (400 Hz high-pass, 4 kHz low-pass)\n");
    { SolidEq e; SolidEq::P p; p.hpf = 400.f; p.lpf = 4000.f; p.hmf = 1300.f; p.hmg = 3.f;
      auto fn = [&] (float** d, int c, int n) { e.process (d, c, n, sr, p); };
      check (gainAt (fn, 200, sr) < -12.0, "below the window: gain at 200 Hz", gainAt (fn, 200, sr), "< -12 dB");
      check (std::fabs (gainAt (fn, 400, sr) + 3.0) < 2.0, "high-pass corner: gain at 400 Hz", gainAt (fn, 400, sr), "~ -3 dB");
      check (gainAt (fn, 1300, sr) > 0.5, "mid bump: gain at 1.3 kHz", gainAt (fn, 1300, sr), "> 0.5 dB");
      check (std::fabs (gainAt (fn, 4000, sr) + 3.0) < 2.5, "low-pass corner: gain at 4 kHz", gainAt (fn, 4000, sr), "~ -3 dB");
      check (gainAt (fn, 8000, sr) < -9.0, "above the window: gain at 8 kHz", gainAt (fn, 8000, sr), "< -9 dB"); }

    std::printf ("Pultec EQP-1A-style (96 kHz so 20 kHz can be measured)\n");
    { const double s96 = 96000.0;
      { PulseEq e; PulseEq::P p; p.lf = 2; p.lb = 10; p.amp = false;
        auto fn = [&] (float** d, int c, int n) { e.process (d, c, n, s96, p); };
        check (gainAt (fn, 20, s96) > 12.0, "low boost 10 @60 Hz: gain at 20 Hz", gainAt (fn, 20, s96), "~ +13.5 dB (>= 12)"); }
      { PulseEq e; PulseEq::P p; p.lf = 2; p.la = 10; p.amp = false;
        auto fn = [&] (float** d, int c, int n) { e.process (d, c, n, s96, p); };
        check (gainAt (fn, 20, s96) < -16.0, "low atten 10 @60 Hz: gain at 20 Hz", gainAt (fn, 20, s96), "~ -17.5 dB (<= -16)"); }
      { PulseEq e; PulseEq::P p; p.hf = 4; p.hb = 10; p.amp = false;
        auto fn = [&] (float** d, int c, int n) { e.process (d, c, n, s96, p); };
        check (std::fabs (gainAt (fn, 10000, s96) - 18.0) < 1.0, "high boost 10 @10 kHz: gain at 10 kHz", gainAt (fn, 10000, s96), "+18 dB"); }
      { PulseEq e; PulseEq::P p; p.haf = 2; p.ha = 10; p.amp = false;
        auto fn = [&] (float** d, int c, int n) { e.process (d, c, n, s96, p); };
        check (gainAt (fn, 42000, s96) < -15.0 && gainAt (fn, 42000, s96) > -17.0, "high atten 10 @20 kHz: shelf plateau at 42 kHz", gainAt (fn, 42000, s96), "~ -16 dB"); }
      // low-end trick at 30 Hz: boost + cut together -> boost near 80 Hz, dip near 200 Hz (open-source EQP-1A docs)
      PulseEq e; PulseEq::P p; p.lf = 1; p.lb = 10; p.la = 10; p.amp = false;
      auto fn = [&] (float** d, int c, int n) { e.process (d, c, n, s96, p); };
      double bestG = -99, bestF = 0, worstG = 99, worstF = 0;
      for (double f = 40; f <= 600; f *= 1.06) { double g = gainAt (fn, f, s96); if (g > bestG) { bestG = g; bestF = f; } if (f > 120 && g < worstG) { worstG = g; worstF = f; } }
      check (bestF > 65 && bestF < 100 && bestG > 2.5, "low-end trick @30 Hz: boost peak frequency", bestF, "65-100 Hz, > +2.5 dB");
      check (worstF > 160 && worstF < 240 && worstG < -3.0, "low-end trick @30 Hz: dip frequency", worstF, "160-240 Hz, < -3 dB"); }
    { PulseEq e; PulseEq::P p; p.amp = true;
      auto x = sine (1000, -12.f, 1.0, sr); auto y = x; runMono (y, [&] (float** d, int c, int n) { e.process (d, c, n, sr, p); });
      const size_t a = 20000, b = 20000 + 32768 > y.size() ? y.size() : 20000 + 32768;
      double h3 = goertzel (y, 3000, sr, a, b) / goertzel (y, 1000, sr, a, b);
      check (h3 < 0.004 && h3 > 0.0002, "tube amp stage: 3rd harmonic at -12 dBFS (ratio)", h3 * 100, "~0.15% (0.02-0.4 %)"); }

    std::printf ("Gate\n");
    { Gate g; Gate::P p; auto lo = sine (200, -20.f, 0.5, sr), hi = sine (200, -65.f, 0.5, sr);
      auto fn = [&] (float** d, int c, int n) { g.process (d, c, n, sr, p); };
      auto a = lo; runMono (a, fn); auto b = hi; runMono (b, fn);
      check (std::fabs (peakDb (a, 10000, a.size()) - peakDb (lo, 10000, lo.size())) < 0.2, "loud signal passes unchanged", peakDb (a, 10000, a.size()) - peakDb (lo, 10000, lo.size()), "~ 0 dB");
      check (peakDb (b, 15000, b.size()) - peakDb (hi, 15000, hi.size()) < -24, "signal below threshold is attenuated", peakDb (b, 15000, b.size()) - peakDb (hi, 15000, hi.size()), "~ -30 dB"); }

    std::printf ("FET compressor (1176-style)\n");
    { FetComp c; FetComp::P p; p.ratio = 1;
      auto x = sine (220, -12.f, 1.5, sr); auto y = x; float gr = 0;
      runMono (y, [&] (float** d, int ch, int n) { gr = c.process (d, ch, n, sr, p); });
      check (gr > 13.0 && gr < 15.0, "8:1, -12 dBFS tone: gain reduction (dB)", gr, "~14 (static curve)"); }
    for (int r = 0; r < 4; ++r)
    { FetComp c; FetComp::P p; p.ratio = r; static const float R[4] = { 4, 8, 12, 20 };
      auto x = sine (220, -12.f, 1.5, sr); auto y = x; float gr = 0;
      runMono (y, [&] (float** d, int ch, int n) { gr = c.process (d, ch, n, sr, p); });
      const float expect = 16.f * (1.f - 1.f / R[r]); char b[64]; std::snprintf (b, sizeof b, "ratio %g:1 gain reduction (dB)", R[r]);
      check (std::fabs (gr - expect) < 1.0, b, gr, "static curve +-1"); }
    { FetComp c; FetComp::P p; auto x = sine (220, -10.f, 1.5, sr); auto y = x;
      runMono (y, [&] (float** d, int ch, int n) { c.process (d, ch, n, sr, p); });
      check (std::fabs (peakDb (y, 30000, y.size()) + 10.0) < 2.0, "Auto Gain on: -10 dBFS peak stays level-matched", peakDb (y, 30000, y.size()), "~ -10 dBFS");
      FetComp c2; FetComp::P p2; p2.autoGain = false; auto y2 = x;
      runMono (y2, [&] (float** d, int ch, int n) { c2.process (d, ch, n, sr, p2); });
      check (peakDb (y2, 30000, y2.size()) < -11.0 && peakDb (y2, 30000, y2.size()) > -17.0, "Auto Gain off: hardware-like level (drive - GR)", peakDb (y2, 30000, y2.size()), "-17 .. -11 dBFS"); }
    { FetComp c; FetComp::P p; p.attackMs = 0.02f; auto x = sine (220, -10.f, 0.05, sr); auto y = x; std::vector<float> g;
      for (size_t i = 0; i < y.size(); i += 64) { float* ch[1] = { y.data() + i }; g.push_back (c.process (ch, 1, (int) std::min<size_t> (64, y.size() - i), sr, p)); }
      FetComp c2; FetComp::P p2; p2.attackMs = 0.8f; auto y2 = x; std::vector<float> g2;
      for (size_t i = 0; i < y2.size(); i += 64) { float* ch[1] = { y2.data() + i }; g2.push_back (c2.process (ch, 1, (int) std::min<size_t> (64, y2.size() - i), sr, p2)); }
      const size_t k = (size_t) (0.001 * sr / 64);
      check (g[k] > 0.9f * g.back(), "attack 0.02 ms: gain reduction reached after ~1 ms", g[k] / g.back() * 100, "> 90 % of final");
      check (g2[k] < 0.8f * g2.back(), "attack 0.8 ms: slower than the fast setting", g2[k] / g2.back() * 100, "< 80 % of final"); }
    { FetComp c; FetComp::P p; p.releaseMs = 250.f; auto x = sine (220, -10.f, 1.0, sr); auto q = sine (220, -60.f, 1.5, sr); x.insert (x.end(), q.begin(), q.end());
      std::vector<float> g; for (size_t i = 0; i < x.size(); i += 64) { float* ch[1] = { x.data() + i }; g.push_back (c.process (ch, 1, (int) std::min<size_t> (64, x.size() - i), sr, p)); }
      const size_t k0 = (size_t) (1.0 * sr / 64); const float g0 = g[k0 - 1]; size_t k = k0; while (k < g.size() && g[k] > g0 * 0.5f) ++k;
      check ((k - k0) * 64 / sr * 1000 > 50 && (k - k0) * 64 / sr * 1000 < 160, "release 250 ms: time to 50% (ms)", (k - k0) * 64 / sr * 1000, "50-160 ms (two-stage)"); }
    { FetComp c; FetComp::P p; p.scHpf = 300.f;   // sidechain HPF: a 60 Hz tone should compress far less
      auto x = sine (60, -10.f, 1.0, sr); auto y = x; float gr = 0; runMono (y, [&] (float** d, int ch, int n) { gr = c.process (d, ch, n, sr, p); });
      FetComp c2; FetComp::P p2; auto z = x; float gr2 = 0; runMono (z, [&] (float** d, int ch, int n) { gr2 = c2.process (d, ch, n, sr, p2); });
      check (gr < gr2 - 6, "sidechain HPF 300 Hz: less gain reduction on a 60 Hz tone", gr2 - gr, "> 6 dB less"); }

    std::printf ("Opto compressor (LA-2A-style)\n");
    { OptoComp c; OptoComp::P p; auto x = sine (220, -10.f, 2.0, sr); auto y = x; float gr = 0;
      runMono (y, [&] (float** d, int ch, int n) { gr = c.process (d, ch, n, sr, p); });
      check (gr > 5.0 && gr < 8.0, "Peak Reduction 40, -10 dBFS tone: gain reduction (dB)", gr, "~6.5");
      OptoComp c2; OptoComp::P p2; p2.limit = true; auto z = x; float gl = 0; runMono (z, [&] (float** d, int ch, int n) { gl = c2.process (d, ch, n, sr, p2); });
      check (gl > gr + 1.5, "Limit mode reduces more than Compress", gl - gr, "> 1.5 dB more");
      OptoComp c3; OptoComp::P p3; p3.peak = 80; auto w = x; float g80 = 0; runMono (w, [&] (float** d, int ch, int n) { g80 = c3.process (d, ch, n, sr, p3); });
      check (g80 > gr + 5, "higher Peak Reduction compresses more", g80 - gr, "> 5 dB more"); }
    for (double dur : { 0.3, 3.0 })
    { OptoComp c; OptoComp::P p; auto x = sine (220, -10.f, dur, sr); auto q = sine (220, -70.f, 10.0, sr); x.insert (x.end(), q.begin(), q.end());
      std::vector<float> g; for (size_t i = 0; i < x.size(); i += 64) { float* ch[1] = { x.data() + i }; g.push_back (c.process (ch, 1, (int) std::min<size_t> (64, x.size() - i), sr, p)); }
      const size_t k0 = (size_t) (dur * sr / 64); const float g0 = g[k0 - 1];
      size_t kh = k0; while (kh < g.size() && g[kh] > g0 * 0.5f) ++kh; size_t kt = k0; while (kt < g.size() && g[kt] > g0 * 0.1f) ++kt;
      double th = (kh - k0) * 64 / sr * 1000, tt = (kt - k0) * 64 / sr; char b[96];
      std::snprintf (b, sizeof b, "after %.1f s: time to 50%% release (ms)", dur); check (th > 40 && th < 100, b, th, "~60-70 ms (40-100)");
      std::snprintf (b, sizeof b, "after %.1f s: time to 90%% release (s)", dur); check (tt > 0.5 && tt < 6.0, b, tt, "0.5-5 s tail"); }

    std::printf ("Bus compressor (G-style)\n");
    { BusComp c; BusComp::P p; p.threshDb = -12.f; p.ratio = 1; p.attack = 2; p.release = 1;
      auto x = sine (220, -6.f, 2.0, sr); auto y = x; float gr = 0;
      runMono (y, [&] (float** d, int ch, int n) { gr = c.process (d, ch, n, sr, p); });
      check (gr > 5.3 && gr < 6.7, "4:1, thr -12, -6 dBFS tone: gain reduction (dB)", gr, "~6 (threshold shifts -2 dB at 4:1)");
      BusComp c2; BusComp::P p2 = p; p2.ratio = 0; auto z = x; float g2 = 0;
      runMono (z, [&] (float** d, int ch, int n) { g2 = c2.process (d, ch, n, sr, p2); });
      check (g2 > 4.3 && g2 < 5.7, "2:1, same settings: gain reduction (dB)", g2, "~5 (threshold shifts -4 dB at 2:1)");
      BusComp c3; BusComp::P p3 = p; p3.ratio = 2; auto w = x; float g3 = 0;
      runMono (w, [&] (float** d, int ch, int n) { g3 = c3.process (d, ch, n, sr, p3); });
      check (g3 > 4.8 && g3 < 6.2, "10:1, same settings: gain reduction (dB)", g3, "~5.4 (no shift at 10:1)"); }
    for (int at : { 0, 5 })
    { BusComp c; BusComp::P p; p.threshDb = -20.f; p.attack = at; p.release = 1; auto x = sine (220, -6.f, 0.3, sr); std::vector<float> g;
      for (size_t i = 0; i < x.size(); i += 16) { float* ch[1] = { x.data() + i }; g.push_back (c.process (ch, 1, (int) std::min<size_t> (16, x.size() - i), sr, p)); }
      const double frac = g[(size_t) (0.005 * sr / 16)] / g.back() * 100; char b[80];
      std::snprintf (b, sizeof b, "attack %s: gain reduction reached after 5 ms (%% of final)", at == 0 ? "0.1 ms" : "30 ms");
      check (at == 0 ? frac > 95 : frac < 45, b, frac, at == 0 ? "> 95 %" : "< 45 %"); }
    { BusComp c; BusComp::P p; p.threshDb = -20.f; p.attack = 0; p.release = 1;        // 0.3 s = 10-90% recovery time
      auto x = sine (220, -6.f, 1.5, sr); auto q = sine (220, -90.f, 2.0, sr); x.insert (x.end(), q.begin(), q.end());
      std::vector<float> g; for (size_t i = 0; i < x.size(); i += 16) { float* ch[1] = { x.data() + i }; g.push_back (c.process (ch, 1, (int) std::min<size_t> (16, x.size() - i), sr, p)); }
      const size_t k0 = (size_t) (1.5 * sr / 16); const float g0 = g[k0 - 1]; size_t a = k0, b = k0;
      while (a < g.size() && g[a] > g0 * 0.9f) ++a;
      b = a;
      while (b < g.size() && g[b] > g0 * 0.1f) ++b;
      const double t = (double) (b - a) * 16 / sr;
      check (t > 0.24 && t < 0.36, "release 0.3 s: time from 90% to 10% of gain reduction (s)", t, "~0.30 s"); }
    for (double dur : { 0.05, 3.0 })
    { BusComp c; BusComp::P p; p.threshDb = -20.f; p.attack = 1; p.release = 4;
      auto x = sine (220, -6.f, dur, sr); auto q = sine (220, -90.f, 5.0, sr); x.insert (x.end(), q.begin(), q.end());
      std::vector<float> g; for (size_t i = 0; i < x.size(); i += 16) { float* ch[1] = { x.data() + i }; g.push_back (c.process (ch, 1, (int) std::min<size_t> (16, x.size() - i), sr, p)); }
      const size_t k0 = (size_t) (dur * sr / 16); const float g0 = g[k0 - 1]; size_t k = k0; while (k < g.size() && g[k] > g0 * 0.5f) ++k;
      const double t = (double) (k - k0) * 16 / sr; char b[80]; std::snprintf (b, sizeof b, "Auto release after %.2f s of compression: time to 50%% (s)", dur);
      check (dur < 1 ? t < 0.12 : t > 0.25, b, t, dur < 1 ? "< 0.12 s (short peak, fast)" : "> 0.25 s (sustained, slow)"); }
    { BusComp c; BusComp::P p; p.threshDb = 0.f; p.makeupDb = 10.f; auto x = sine (220, -40.f, 1.0, sr); auto y = x;
      runMono (y, [&] (float** d, int ch, int n) { c.process (d, ch, n, sr, p); });
      check (std::fabs (peakDb (y, 20000, y.size()) - peakDb (x, 20000, x.size()) - 10.0) < 0.5, "makeup +10 dB below threshold: level change (dB)", peakDb (y, 20000, y.size()) - peakDb (x, 20000, x.size()), "+10 dB");
      BusComp c2; BusComp::P p2; p2.mix = 0.f; p2.threshDb = -30.f; auto z = sine (220, -6.f, 1.0, sr); auto z0 = z;
      runMono (z, [&] (float** d, int ch, int n) { c2.process (d, ch, n, sr, p2); });
      check (std::fabs (rms (z, 20000, z.size()) - rms (z0, 20000, z0.size())) < 1.0e-3, "mix 0 returns the dry signal (rms difference)", std::fabs (rms (z, 20000, z.size()) - rms (z0, 20000, z0.size())), "~ 0");
      BusComp c3; BusComp::P p3; p3.scHpf = 300.f; p3.threshDb = -20.f; auto s60 = sine (60, -6.f, 1.0, sr); float gh = 0;
      runMono (s60, [&] (float** d, int ch, int n) { gh = c3.process (d, ch, n, sr, p3); });
      BusComp c4; BusComp::P p4; p4.threshDb = -20.f; auto s60b = sine (60, -6.f, 1.0, sr); float gn = 0;
      runMono (s60b, [&] (float** d, int ch, int n) { gn = c4.process (d, ch, n, sr, p4); });
      check (gh < gn - 3, "sidechain HPF 300 Hz: less gain reduction on a 60 Hz tone", gn - gh, "> 3 dB less"); }

    std::printf ("Tube saturation and anti-aliasing\n");
    { Sat s; Sat::P p; p.driveDb = 12.f; auto x = sine (1000, -12.f, 1.0, sr); auto y = x;
      runMono (y, [&] (float** d, int c, int n) { s.process (d, c, n, sr, p); });
      const size_t a = 20000, b = a + 16384; double f1 = goertzel (y, 1000, sr, a, b);
      check (g2dB ((float) (goertzel (y, 2000, sr, a, b) / f1)) > -40 && g2dB ((float) (goertzel (y, 2000, sr, a, b) / f1)) < -20, "12 dB drive: 2nd harmonic vs fundamental (dB)", g2dB ((float) (goertzel (y, 2000, sr, a, b) / f1)), "-40 .. -20 (bias adds even harmonics)"); }
    { Sat::P p; p.driveDb = 24.f; p.bias = 0.f; const double f0 = 9700.0;
      auto x = sine (f0, -6.f, 1.0, sr); auto ya = x, yn = x; Sat sa, sn;
      runMono (ya, [&] (float** d, int c, int n) { sa.process (d, c, n, sr, p, true); });
      runMono (yn, [&] (float** d, int c, int n) { sn.process (d, c, n, sr, p, false); });
      // aliased images of harmonics 3,5,7,9 (odd only because bias = 0)
      double ea = 0, en = 0;
      for (int k : { 3, 5, 7, 9, 11 }) { double f = k * f0; double al = std::fmod (f, sr); if (al > sr / 2) al = sr - al;
          if (std::fabs (al - f0) < 100) continue;
          ea += std::pow (goertzel (ya, al, sr, 20000, 36384), 2);
          en += std::pow (goertzel (yn, al, sr, 20000, 36384), 2); }
      check (10 * std::log10 (en / ea) > 6.0, "ADAA vs plain tanh: alias energy reduction (dB)", 10 * std::log10 (en / ea), "> 6 dB"); }

    std::printf ("Width\n");
    { Width w; w.prepare (sr); std::mt19937 rng (1); std::normal_distribution<float> nd (0.f, 0.1f);
      for (float wd : { 0.f, 1.f, 2.f }) { w.reset(); std::vector<float> L (8192), R (8192); for (size_t i = 0; i < L.size(); ++i) { L[i] = nd (rng); R[i] = nd (rng); }
        for (size_t i = 0; i < L.size(); i += 256) { float* ch[2] = { L.data() + i, R.data() + i }; w.process (ch, 2, 256, sr, wd, 0.f); }
        double sd = 0; for (size_t i = 2048; i < L.size(); ++i) sd += (L[i] - R[i]) * (L[i] - R[i]); sd = std::sqrt (sd / 6144);
        char b[64]; std::snprintf (b, sizeof b, "width %.0f: side signal level", wd); check (wd == 0.f ? sd < 1e-4 : sd > 0.05 * wd, b, sd, wd == 0.f ? "~0 (mono)" : "grows with width"); } }

    std::printf ("Robustness: random settings at several sample rates (no NaN / blow-ups)\n");
    { std::mt19937 rng (7); std::uniform_real_distribution<float> u (0.f, 1.f); bool ok = true; double worst = -99;
      for (double s : { 22050.0, 44100.0, 96000.0, 192000.0 })
      { for (int t = 0; t < 12; ++t)
        { Gate g; FetComp f; OptoComp o; BusComp bc; ProcessEq a; SolidEq q; PulseEq pt; Sat st; Width wd; wd.prepare (s);
          Gate::P gp { -80 + 80 * u (rng), 80 * u (rng), 10 + 490 * u (rng) };
          FetComp::P fp; fp.inDb = 30 * u (rng); fp.ratio = (int) (4.99 * u (rng)); fp.attackMs = 0.02f + 0.78f * u (rng); fp.releaseMs = 50 + 1050 * u (rng); fp.mix = u (rng); fp.outDb = -12 + 24 * u (rng); fp.scHpf = 20 + 280 * u (rng); fp.autoGain = u (rng) > 0.5f;
          OptoComp::P op { 100 * u (rng), 40 * u (rng), u (rng) > 0.5f, 20 + 280 * u (rng) };
          BusComp::P bp { -40 + 40 * u (rng), (int) (2.99 * u (rng)), (int) (5.99 * u (rng)), (int) (4.99 * u (rng)), 15 * u (rng), u (rng), 20 + 280 * u (rng) };
          ProcessEq::P ap { 20 + 280 * u (rng), (int) (6.99 * u (rng)), (int) (10.99 * u (rng)), (int) (6.99 * u (rng)), (int) (10.99 * u (rng)), (int) (6.99 * u (rng)), (int) (10.99 * u (rng)), (int) (6.99 * u (rng)), (int) (10.99 * u (rng)), u (rng) > .5f, u (rng) > .5f };
          SolidEq::P sp { 16 + 334 * u (rng), 3000 + 19000 * u (rng), 30 + 420 * u (rng), -15 + 30 * u (rng), 200 + 2300 * u (rng), -15 + 30 * u (rng), 0.5f + 3.5f * u (rng), 600 + 6400 * u (rng), -15 + 30 * u (rng), 0.5f + 3.5f * u (rng), 1500 + 14500 * u (rng), -15 + 30 * u (rng), u (rng) > .5f, u (rng) > .5f };
          PulseEq::P pp { (int) (3.99 * u (rng)), 10 * u (rng), 10 * u (rng), (int) (6.99 * u (rng)), 10 * u (rng), 10 * u (rng), (int) (2.99 * u (rng)), 10 * u (rng), u (rng) > .5f };
          Sat::P stp { 24 * u (rng), 0.3f * u (rng), u (rng), -12 + 24 * u (rng) };
          std::vector<float> L ((size_t) s), R ((size_t) s);
          for (size_t i = 0; i < L.size(); ++i) { L[i] = (u (rng) - .5f) * 1.2f; R[i] = (u (rng) - .5f) * 1.2f; }
          for (size_t i = 0; i < L.size(); i += 512)
          { int n = (int) std::min<size_t> (512, L.size() - i); float* ch[2] = { L.data() + i, R.data() + i };
            g.process (ch, 2, n, s, gp); f.process (ch, 2, n, s, fp); o.process (ch, 2, n, s, op); bc.process (ch, 2, n, s, bp); a.process (ch, 2, n, s, ap); q.process (ch, 2, n, s, sp);
            pt.process (ch, 2, n, s, pp); st.process (ch, 2, n, s, stp); wd.process (ch, 2, n, s, 2.f * u (rng), 30 * u (rng)); }
          for (size_t i = 0; i < L.size(); ++i) { if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) ok = false; worst = std::max (worst, (double) g2dB (std::max (std::fabs (L[i]), std::fabs (R[i])))); } } }
      check (ok, "all outputs finite (48 random full-chain runs)", ok ? 1 : 0, "1");
      check (worst < 40.0, "output never explodes (peak dBFS)", worst, "< +40 dBFS"); }

    std::printf ("\n%d checks, %d failed\n", checks, fails);
    return fails == 0 ? 0 : 1;
}

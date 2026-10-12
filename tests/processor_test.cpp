// End-to-end test of the real PluginProcessor.cpp logic (parameter mapping, module order, bypass, presets, meters)
// running on audio, using a functional JUCE stand-in (tests/stub_fn). Not a test of JUCE itself.
//   g++ -std=c++17 -O2 -DSC_TEST_NO_EDITOR -Itests/stub_fn -ISource tests/processor_test.cpp Source/PluginProcessor.cpp -o processor_test && ./processor_test
#include "PluginProcessor.h"
#include <cstdio>
#include <random>
using namespace juce;

static int fails = 0, checks = 0;
static void check (bool ok, const char* what, double value, const char* expect)
{
    ++checks; if (! ok) ++fails;
    std::printf ("  [%s] %-62s %9.3f  (%s)\n", ok ? "PASS" : "FAIL", what, value, expect);
}
struct Sig { std::vector<float> L, R; };
static Sig guitarish (double sr, double sec, float rmsDb, unsigned seed)
{
    std::mt19937 rng (seed); std::normal_distribution<float> nd (0.f, 1.f);
    Sig s; const size_t n = (size_t) (sec * sr); s.L.resize (n); s.R.resize (n);
    float lpL = 0, lpR = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const double t = (double) i / sr, env = std::exp (-std::fmod (t, 0.5) * 3.0);     // plucked-note envelopes
        float tone = 0; for (int h = 1; h <= 8; ++h)
            tone += (float) std::sin (2 * 3.14159265 * 110.0 * h * t) / (float) h;
        lpL += 0.2f * (nd (rng) - lpL); lpR += 0.2f * (nd (rng) - lpR);
        s.L[i] = (float) env * (tone + 0.6f * lpL); s.R[i] = (float) env * (0.8f * tone + 0.6f * lpR);
    }
    double e = 0; for (float v : s.L) e += (double) v * v; e = std::sqrt (e / (double) n);
    const float g = std::pow (10.f, rmsDb / 20.f) / (float) e; for (auto& v : s.L) v *= g; for (auto& v : s.R) v *= g;
    return s;
}
static double rms (const std::vector<float>& a, size_t from, size_t to) { double s = 0; for (size_t i = from; i < to; ++i) s += (double) a[i] * a[i]; return std::sqrt (s / (double) (to - from)); }
static double rmsDiff (const std::vector<float>& a, const std::vector<float>& b, size_t from)
{ double s = 0; for (size_t i = from; i < a.size(); ++i) { double d = (double) a[i] - b[i]; s += d * d; } return std::sqrt (s / (double) (a.size() - from)); }
static double maxDiff (const std::vector<float>& a, const std::vector<float>& b, size_t from) { double m = 0; for (size_t i = from; i < a.size(); ++i) m = std::max (m, (double) std::fabs (a[i] - b[i])); return m; }
static bool finiteBounded (const Sig& s, double lim) { for (auto* v : { &s.L, &s.R }) for (float x : *v) if (! std::isfinite (x) || std::fabs (x) > lim) return false; return true; }

struct Rig
{
    ChannelProcessor p; double sr;
    explicit Rig (double rate = 44100.0) : sr (rate) { p.prepareToPlay (rate, 512); }
    void set (const char* id, float raw) { auto* r = p.apvts.getParameter (id); if (! r) { std::printf ("  !! unknown parameter %s\n", id); ++fails; return; } r->setValueNotifyingHost (r->convertTo0to1 (raw)); }
    float get (const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
    void slots (std::initializer_list<int> mods) { int i = 1; for (int m : mods) set (("slot" + std::to_string (i++)).c_str(), (float) m); while (i <= 7) set (("slot" + std::to_string (i++)).c_str(), 7.f); }
    Sig run (const Sig& in)
    {
        Sig out = in; AudioBuffer<float> b; MidiBuffer m;
        for (size_t i = 0; i < in.L.size(); i += 512)
        {
            const int n = (int) std::min<size_t> (512, in.L.size() - i); b.setSize (2, n);
            for (int k = 0; k < n; ++k) { b.getWritePointer (0)[k] = in.L[i + (size_t) k]; b.getWritePointer (1)[k] = in.R[i + (size_t) k]; }
            p.processBlock (b, m);
            for (int k = 0; k < n; ++k) { out.L[i + (size_t) k] = b.getWritePointer (0)[k]; out.R[i + (size_t) k] = b.getWritePointer (1)[k]; }
        }
        return out;
    }
};

int main()
{
    const double sr = 44100.0;
    const Sig in = guitarish (sr, 3.0, -20.f, 1), hot = guitarish (sr, 3.0, -9.f, 2);
    const size_t half = in.L.size() / 2;

    std::printf ("Setup\n");
    { Rig r; int n = 0; bool defaultsOk = true, brandFree = true;
      for (auto* ap : r.p.getParameters())
      { auto* rp = dynamic_cast<RangedAudioParameter*> (ap); ++n;
        const float d = rp->getDefaultValue(); if (! (d >= 0.f && d <= 1.f)) defaultsOk = false;
        const std::string nm = rp->getName (64).s;
        for (const char* bad : { "SSL", "API", "Pultec", "1176", "LA-2A", "Neve", "UREI" }) if (nm.find (bad) != std::string::npos) brandFree = false; }
      check (n >= 80, "parameters registered", n, ">= 80");
      check (defaultsOk, "every default lies inside its range", defaultsOk ? 1 : 0, "1");
      check (brandFree, "no brand names in any parameter name", brandFree ? 1 : 0, "1");
      bool presetsFree = true; for (int i = 0; i < r.p.getNumPrograms(); ++i) { const std::string nm = r.p.getProgramName (i).s; for (const char* bad : { "SSL", "API", "Pultec", "Green", "American", "Idiot", "Boulevard", "Dookie" }) if (nm.find (bad) != std::string::npos) presetsFree = false; }
      check (presetsFree, "no brand/song/album names in preset names", presetsFree ? 1 : 0, "1");
      check (r.p.getNumPrograms() == 13, "factory presets", r.p.getNumPrograms(), "13 (Init + 12)");
      check (r.p.getName().s == "Suburban Channel", "plugin name", 0, "Suburban Channel"); }

    std::printf ("Whole plugin\n");
    { Rig r; Sig out = r.run (in);
      check (finiteBounded (out, 10.0), "default chain output is finite and sane", 1, "1");
      check (rmsDiff (out.L, in.L, half) / rms (in.L, half, in.L.size()) > 0.05, "default chain changes the audio (relative difference)", rmsDiff (out.L, in.L, half) / rms (in.L, half, in.L.size()), "> 0.05"); }
    { Rig r; r.set ("bypass", 1.f); Sig out = r.run (in);
      check (maxDiff (out.L, in.L, half) < 1e-3 && maxDiff (out.R, in.R, half) < 1e-3, "bypass returns the input (max difference)", maxDiff (out.L, in.L, half), "< 0.001"); }
    { Rig r; r.set ("mix", 0.f); Sig out = r.run (in);
      check (maxDiff (out.L, in.L, half) < 1e-3, "mix 0 returns the input (max difference)", maxDiff (out.L, in.L, half), "< 0.001"); }
    { Rig r; r.slots ({}); Sig out = r.run (in);
      check (maxDiff (out.L, in.L, half) < 1e-4, "all slots Off = clean pass-through", maxDiff (out.L, in.L, half), "< 0.0001"); }
    { Rig r; r.slots ({}); r.set ("in_gain", 6.f); r.set ("out_gain", -6.f); Sig out = r.run (in);
      check (maxDiff (out.L, in.L, half) < 1e-3, "input +6 dB then output -6 dB cancels", maxDiff (out.L, in.L, half), "< 0.001"); }

    std::printf ("Each module does something (the right parameters reach the right module)\n");
    { Rig r; r.slots ({ 0 }); Sig q = guitarish (sr, 3.0, -78.f, 3); Sig out = r.run (q);
      check (rms (out.L, half, out.L.size()) < 0.3 * rms (q.L, half, q.L.size()), "Gate attenuates a quiet signal (output/input rms)", rms (out.L, half, out.L.size()) / rms (q.L, half, q.L.size()), "< 0.3"); }
    { Rig r; r.slots ({ 1 }); Sig out = r.run (hot); check (rmsDiff (out.L, hot.L, half) > 0.01, "Compressor (FET) changes a hot signal", rmsDiff (out.L, hot.L, half), "> 0.01"); }
    { Rig r; r.slots ({ 2 }); Sig flat = r.run (in); Rig r2; r2.slots ({ 2 }); r2.set ("eq_lg", 10.f); Sig lf = r2.run (in);
      check (rmsDiff (lf.L, flat.L, half) > 0.01, "Process EQ: LF gain control changes the sound", rmsDiff (lf.L, flat.L, half), "> 0.01");
      check (rmsDiff (flat.L, in.L, half) < 1e-3, "Process EQ: flat settings leave the sound alone", rmsDiff (flat.L, in.L, half), "< 0.001"); }
    { Rig r; r.slots ({ 3 }); Sig flat = r.run (in); Rig r2; r2.slots ({ 3 }); r2.set ("ssl_hg", 12.f); Sig hf = r2.run (in);
      check (rmsDiff (hf.L, flat.L, half) > 0.005, "Solid EQ: HF gain control changes the sound", rmsDiff (hf.L, flat.L, half), "> 0.005");
      Rig r3; r3.slots ({ 3 }); r3.set ("ssl_hpf", 400.f); r3.set ("ssl_lpf", 4000.f); Sig bl = r3.run (in);
      check (rms (bl.L, half, bl.L.size()) < 0.8 * rms (in.L, half, in.L.size()), "Solid EQ: 400 Hz / 4 kHz band-limit removes energy", rms (bl.L, half, bl.L.size()) / rms (in.L, half, in.L.size()), "< 0.8"); }
    { Rig r; r.slots ({ 4 }); Sig flat = r.run (in); Rig r2; r2.slots ({ 4 }); r2.set ("pt_lb", 10.f); Sig lb = r2.run (in);
      check (rmsDiff (lb.L, flat.L, half) > 0.01, "Pulse EQ: low boost control changes the sound", rmsDiff (lb.L, flat.L, half), "> 0.01"); }
    { Rig r; r.slots ({ 5 }); Sig out = r.run (in); check (rmsDiff (out.L, in.L, half) > 0.005, "Tube Sat changes the sound", rmsDiff (out.L, in.L, half), "> 0.005"); }
    { Rig r; r.slots ({ 6 }); r.set ("wd_width", 0.f); Sig out = r.run (in);
      check (maxDiff (out.L, out.R, half) < 1e-3, "Width 0 makes the two channels identical", maxDiff (out.L, out.R, half), "< 0.001");
      Rig r2; r2.slots ({ 6 }); r2.set ("wd_width", 2.f); Sig wide = r2.run (in);
      check (rmsDiff (wide.L, wide.R, half) > rmsDiff (in.L, in.R, half), "Width 2 widens the stereo image", rmsDiff (wide.L, wide.R, half), "> input difference"); }

    std::printf ("Compressor types and meters\n");
    { Sig o[3]; for (int t = 0; t < 3; ++t) { Rig r; r.slots ({ 1 }); r.set ("comp_type", (float) t); o[t] = r.run (hot);
        char b[80]; std::snprintf (b, sizeof b, "comp type %d: gain-reduction meter reads > 1 dB", t);
        check (r.p.meterGrDb.load() > 1.0f, b, r.p.meterGrDb.load(), "> 1 dB"); }
      check (rmsDiff (o[0].L, o[1].L, half) > 0.005 && rmsDiff (o[1].L, o[2].L, half) > 0.005 && rmsDiff (o[0].L, o[2].L, half) > 0.005, "FET, Opto and Bus sound different from each other", rmsDiff (o[0].L, o[2].L, half), "> 0.005"); }
    { Rig r; r.slots ({}); Sig s = in; for (size_t i = 0; i < s.L.size(); ++i) { s.L[i] = 0.1259f * (float) std::sin (2 * 3.14159265 * 220.0 * (double) i / sr); s.R[i] = s.L[i]; }
      r.run (s); check (std::fabs (r.p.meterOutDb.load() + 21.0) < 1.0, "VU meter: -18 dBFS peak sine reads about -21 dBFS RMS", r.p.meterOutDb.load(), "-21 +- 1");
      check (r.p.meterGrDb.load() == 0.0f, "gain-reduction meter reads 0 with no compressor", r.p.meterGrDb.load(), "0"); }

    std::printf ("Module order and slots\n");
    { Rig a; a.slots ({ 3, 5 }); a.set ("ssl_hg", 15.f); a.set ("ssl_hf", 3000.f); a.set ("sat_drive", 24.f); Sig x = a.run (hot);
      Rig b; b.slots ({ 5, 3 }); b.set ("ssl_hg", 15.f); b.set ("ssl_hf", 3000.f); b.set ("sat_drive", 24.f); Sig y = b.run (hot);
      check (rmsDiff (x.L, y.L, half) > 0.01, "EQ->Sat sounds different from Sat->EQ", rmsDiff (x.L, y.L, half), "> 0.01"); }
    { Rig a; a.slots ({ 5 }); Sig x = a.run (in); Rig b; b.slots ({ 5, 5 }); Sig y = b.run (in);
      check (maxDiff (x.L, y.L, 0) < 1e-6, "a module placed in two slots runs only once", maxDiff (x.L, y.L, 0), "identical"); }

    std::printf ("Presets\n");
    for (int i = 0; i < 13; ++i)
    { Rig r; r.slots ({ 6, 5, 4, 3, 2, 1, 0 }); r.set ("in_gain", 3.f); r.set ("mix", 0.9f);
      r.p.setCurrentProgram (i); Sig out = r.run (in);
      bool kept = r.get ("slot1") == 6.f && r.get ("slot7") == 0.f && r.get ("in_gain") == 3.f && std::fabs (r.get ("mix") - 0.9f) < 0.01f;
      char b[96]; std::snprintf (b, sizeof b, "preset %2d \"%s\": loads, keeps order/globals, output sane", i, r.p.getProgramName (i).s.c_str());
      check (kept && finiteBounded (out, 20.0) && rms (out.L, half, out.L.size()) > 1e-3, b, i, "ok"); }
    { Rig r; r.p.setCurrentProgram (1); bool changed = r.get ("comp_type") != 0.f || r.get ("ssl_hg") != 0.f; r.p.setCurrentProgram (0);
      check (changed && r.get ("ssl_hg") == 0.f && r.get ("comp_type") == 0.f, "Init resets what another preset changed", 1, "1"); }
    { Rig r; r.p.setCurrentProgram (12);
      check (r.get ("ssl_hpf") == 400.f && r.get ("ssl_lpf") == 4000.f && r.get ("wd_width") == 0.f, "Lo-Fi Radio Intro sets 400 Hz / 4 kHz and mono", r.get ("ssl_hpf"), "400 / 4000 / 0"); }

    std::printf ("Random settings (no NaN, no blow-ups)\n");
    { std::mt19937 rng (5); std::uniform_real_distribution<float> u (0.f, 1.f); bool ok = true; double worst = 0;
      for (double rate : { 44100.0, 96000.0 }) for (int t = 0; t < 12; ++t)
      { Rig r (rate); for (auto* ap : r.p.getParameters()) { auto* rp = dynamic_cast<RangedAudioParameter*> (ap); rp->setValueNotifyingHost (u (rng)); }
        Sig q = guitarish (rate, 1.0, -14.f, (unsigned) (t + 10)); Sig out = r.run (q);
        if (! finiteBounded (out, 100.0)) ok = false;
        for (float v : out.L) worst = std::max (worst, (double) std::fabs (v)); }
      check (ok, "24 random full-plugin runs stay finite and bounded", worst, "peak < 100"); }

    std::printf ("\n%d checks, %d failed\n", checks, fails);
    return fails == 0 ? 0 : 1;
}

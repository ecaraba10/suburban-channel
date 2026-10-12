#include "PluginProcessor.h"
#ifndef SC_TEST_NO_EDITOR
 #include "PluginEditor.h"
#endif

using bgtr::dB2g;

//==============================================================================
static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto F = [&] (const String& id, const String& name, float lo, float hi, float def,
                  const String& unit = {}, float skew = 1.f)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.01f, skew), def,
            AudioParameterFloatAttributes().withLabel (unit)));
    };
    auto C = [&] (const String& id, const String& name, StringArray items, int def)
    {
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };
    auto B = [&] (const String& id, const String& name, bool def)
    {
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    // Global
    B ("bypass", "Bypass", false);
    F ("in_gain",  "Input",  -24.f, 24.f, 0.f, " dB");
    F ("out_gain", "Output", -24.f, 24.f, 0.f, " dB");
    F ("mix",      "Mix",     0.f,  1.f,  1.f);
    C ("meter_mode", "Meter", { "Output", "Gain Reduction" }, 0);

    // Signal order
    const StringArray mods { "Gate", "Compressor", "Process EQ", "Solid EQ", "Pulse EQ", "Tube Sat", "Width", "Off" };
    for (int i = 0; i < 7; ++i)
        C ("slot" + String (i + 1), "Slot " + String (i + 1), mods, i);

    // Gate
    B ("gate_on", "Gate On", true);
    F ("gate_thresh",  "Gate Thresh",  -80.f, 0.f, -50.f, " dB");
    F ("gate_range",   "Gate Range",     0.f, 80.f, 30.f, " dB");
    F ("gate_release", "Gate Release",  10.f, 500.f, 80.f, " ms", 0.5f);

    // Compressor: FET, Opto or Bus
    B ("comp_on", "Compressor On", true);
    C ("comp_type", "Comp Type", { "FET", "Opto", "Bus" }, 0);
    F ("comp_in",      "Comp Input",   0.f, 30.f, 12.f, " dB");
    C ("comp_ratio",   "Comp Ratio", { "4:1", "8:1", "12:1", "20:1", "All" }, 1);
    F ("comp_attack",  "Comp Attack", 0.02f, 0.8f, 0.2f, " ms");
    F ("comp_release", "Comp Release", 50.f, 1100.f, 250.f, " ms", 0.5f);
    F ("comp_mix",     "Comp Mix",     0.f, 1.f, 1.f);
    F ("comp_out",     "Comp Output", -12.f, 12.f, 0.f, " dB");
    B ("comp_auto",    "Comp Auto Gain", true);
    F ("opto_peak", "Opto Peak Reduction", 0.f, 100.f, 40.f);
    F ("opto_gain", "Opto Gain", 0.f, 40.f, 0.f, " dB");
    C ("opto_mode", "Opto Mode", { "Compress", "Limit" }, 0);
    F ("bus_thresh", "Bus Threshold", -40.f, 0.f, -12.f, " dB");
    C ("bus_ratio", "Bus Ratio", { "2:1", "4:1", "10:1" }, 1);
    C ("bus_attack", "Bus Attack", { "0.1 ms", "0.3 ms", "1 ms", "3 ms", "10 ms", "30 ms" }, 3);
    C ("bus_release", "Bus Release", { "0.1 s", "0.3 s", "0.6 s", "1.2 s", "Auto" }, 4);
    F ("bus_makeup", "Bus Makeup", 0.f, 15.f, 0.f, " dB");
    F ("bus_mix", "Bus Mix", 0.f, 1.f, 1.f);
    F ("comp_schpf", "Comp Sidechain HPF", 20.f, 300.f, 20.f, " Hz", 0.5f);

    // Process EQ (stepped, discrete-style)
    const StringArray gains { "-12", "-9", "-6", "-4", "-2", "0", "+2", "+4", "+6", "+9", "+12" };
    B ("eq_on", "Process EQ On", true);
    F ("eq_hpf", "Process HPF (extra)", 20.f, 300.f, 20.f, " Hz", 0.4f);
    C ("eq_lf",  "Process LF",  { "30", "40", "50", "100", "200", "300", "400" }, 3);
    C ("eq_lg",  "Process LF Gain", gains, 5);
    B ("eq_lshelf", "Process LF Shelf", true);
    C ("eq_lmf", "Process LMF", { "75", "150", "180", "240", "500", "700", "1k" }, 4);
    C ("eq_lmg", "Process LMF Gain", gains, 5);
    C ("eq_hmf", "Process HMF", { "800", "1.5k", "3k", "5k", "8k", "10k", "12.5k" }, 2);
    C ("eq_hmg", "Process HMF Gain", gains, 5);
    C ("eq_hf",  "Process HF",  { "2.5k", "5k", "7.5k", "10k", "12.5k", "15k", "20k" }, 3);
    C ("eq_hg",  "Process HF Gain", gains, 5);
    B ("eq_hshelf", "Process HF Shelf", true);

    // Solid EQ (console-style)
    B ("ssl_on", "Solid EQ On", true);
    F ("ssl_hpf", "Solid HPF", 16.f, 600.f, 16.f, " Hz", 0.5f);
    F ("ssl_lpf", "Solid LPF", 3000.f, 22000.f, 22000.f, " Hz", 0.5f);
    F ("ssl_lf",  "Solid LF", 30.f, 450.f, 100.f, " Hz", 0.5f);
    F ("ssl_lg",  "Solid LF Gain", -15.f, 15.f, 0.f, " dB");
    B ("ssl_lbell", "Solid LF Bell", false);
    F ("ssl_lmf", "Solid LMF", 200.f, 2500.f, 600.f, " Hz", 0.5f);
    F ("ssl_lmg", "Solid LMF Gain", -15.f, 15.f, 0.f, " dB");
    F ("ssl_lmq", "Solid LMF Q", 0.5f, 4.f, 1.f, "", 0.6f);
    F ("ssl_hmf", "Solid HMF", 600.f, 7000.f, 3000.f, " Hz", 0.5f);
    F ("ssl_hmg", "Solid HMF Gain", -15.f, 15.f, 0.f, " dB");
    F ("ssl_hmq", "Solid HMF Q", 0.5f, 4.f, 1.f, "", 0.6f);
    F ("ssl_hf",  "Solid HF", 1500.f, 16000.f, 8000.f, " Hz", 0.5f);
    F ("ssl_hg",  "Solid HF Gain", -15.f, 15.f, 0.f, " dB");
    B ("ssl_hbell", "Solid HF Bell", false);

    // Pulse EQ (passive program-style)
    B ("pt_on", "Pulse EQ On", true);
    C ("pt_lf",  "Pulse Low Freq", { "20", "30", "60", "100" }, 2);
    F ("pt_lb",  "Pulse Low Boost", 0.f, 10.f, 0.f);
    F ("pt_la",  "Pulse Low Atten", 0.f, 10.f, 0.f);
    C ("pt_hf",  "Pulse High Freq", { "3k", "4k", "5k", "8k", "10k", "12k", "16k" }, 4);
    F ("pt_hb",  "Pulse High Boost", 0.f, 10.f, 0.f);
    F ("pt_bw",  "Pulse Bandwidth", 0.f, 10.f, 5.f);
    C ("pt_haf", "Pulse Atten Freq", { "5k", "10k", "20k" }, 1);
    F ("pt_ha",  "Pulse High Atten", 0.f, 10.f, 0.f);
    B ("pt_amp", "Pulse Tube Amp", true);

    // Saturation
    B ("sat_on", "Sat On", true);
    F ("sat_drive", "Sat Drive", 0.f, 24.f, 6.f, " dB");
    F ("sat_bias",  "Sat Bias",  0.f, 0.3f, 0.1f);
    F ("sat_mix",   "Sat Mix",   0.f, 1.f, 1.f);
    F ("sat_out",   "Sat Output", -12.f, 12.f, 0.f, " dB");

    // Width
    B ("wd_on", "Width On", true);
    F ("wd_width", "Width", 0.f, 2.f, 1.f);
    F ("wd_haas",  "Haas Delay", 0.f, 30.f, 0.f, " ms");

    return { std::make_move_iterator (p.begin()), std::make_move_iterator (p.end()) };
}

//==============================================================================
ChannelProcessor::ChannelProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
}

bool ChannelProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in  = l.getMainInputChannelSet();
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && in == out;
}

void ChannelProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    gate.reset(); fet.reset(); opto.reset(); bus.reset(); api.reset(); ssl.reset(); pultec.reset(); sat.reset();
    width.prepare (sampleRate);
    sIn.reset(); sOut.reset(); sMix.reset();
    dry.setSize (2, juce::jmax (1, samplesPerBlock));
    vuPower = 0.f;
    meterOutDb.store (-100.f);
    meterGrDb.store (0.f);
    setLatencySamples (0);
}

//==============================================================================
void ChannelProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n   = buffer.getNumSamples();
    const int nCh = juce::jmin (buffer.getNumChannels(), 2);

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);
    if (nCh == 0 || n == 0) return;

    if (dry.getNumSamples() < n || dry.getNumChannels() < nCh)
        dry.setSize (2, n, false, false, true);
    for (int c = 0; c < nCh; ++c)
        dry.copyFrom (c, 0, buffer, c, 0, n);

    float* ch[2] = { buffer.getWritePointer (0), nCh > 1 ? buffer.getWritePointer (1) : nullptr };

    // Input gain (smoothed)
    {
        const float t = dB2g (v ("in_gain")), a = bgtr::coefMs (sr, 10.f);
        for (int i = 0; i < n; ++i)
        {
            const float g = sIn.tick (t, a);
            for (int c = 0; c < nCh; ++c) ch[c][i] *= g;
        }
    }

    float grNow = 0.f;
    unsigned used = 0;
    for (int s = 1; s <= 7; ++s)
    {
        const int m = (int) v ("slot" + juce::String (s));
        if (m < 0 || m > 6 || (used & (1u << m))) continue;   // "Off" or module already used
        used |= (1u << m);

        switch (m)
        {
            case 0:
                if (on ("gate_on"))
                {
                    bgtr::Gate::P p;
                    p.threshDb = v ("gate_thresh"); p.rangeDb = v ("gate_range"); p.releaseMs = v ("gate_release");
                    gate.process (ch, nCh, n, sr, p);
                }
                break;
            case 1:
                if (on ("comp_on"))
                {
                    const int type = (int) v ("comp_type");
                    if (type == 2)
                    {
                        bgtr::BusComp::P p;
                        p.threshDb = v ("bus_thresh"); p.ratio = (int) v ("bus_ratio"); p.attack = (int) v ("bus_attack");
                        p.release = (int) v ("bus_release"); p.makeupDb = v ("bus_makeup"); p.mix = v ("bus_mix"); p.scHpf = v ("comp_schpf");
                        grNow = bus.process (ch, nCh, n, sr, p);
                    }
                    else if (type == 1)
                    {
                        bgtr::OptoComp::P p;
                        p.peak = v ("opto_peak"); p.gainDb = v ("opto_gain"); p.limit = (int) v ("opto_mode") == 1; p.scHpf = v ("comp_schpf");
                        grNow = opto.process (ch, nCh, n, sr, p);
                    }
                    else
                    {
                        bgtr::FetComp::P p;
                        p.inDb = v ("comp_in"); p.ratio = (int) v ("comp_ratio"); p.attackMs = v ("comp_attack");
                        p.releaseMs = v ("comp_release"); p.mix = v ("comp_mix"); p.outDb = v ("comp_out");
                        p.scHpf = v ("comp_schpf"); p.autoGain = on ("comp_auto");
                        grNow = fet.process (ch, nCh, n, sr, p);
                    }
                }
                break;
            case 2:
                if (on ("eq_on"))
                {
                    bgtr::ProcessEq::P p;
                    p.hpf = v ("eq_hpf");
                    p.lf = (int) v ("eq_lf");   p.lg = (int) v ("eq_lg");
                    p.lmf = (int) v ("eq_lmf"); p.lmg = (int) v ("eq_lmg");
                    p.hmf = (int) v ("eq_hmf"); p.hmg = (int) v ("eq_hmg");
                    p.hf = (int) v ("eq_hf");   p.hg = (int) v ("eq_hg");
                    p.lShelf = on ("eq_lshelf"); p.hShelf = on ("eq_hshelf");
                    api.process (ch, nCh, n, sr, p);
                }
                break;
            case 3:
                if (on ("ssl_on"))
                {
                    bgtr::SolidEq::P p;
                    p.hpf = v ("ssl_hpf"); p.lpf = v ("ssl_lpf");
                    p.lf = v ("ssl_lf");   p.lg = v ("ssl_lg");   p.lBell = on ("ssl_lbell");
                    p.lmf = v ("ssl_lmf"); p.lmg = v ("ssl_lmg"); p.lmq = v ("ssl_lmq");
                    p.hmf = v ("ssl_hmf"); p.hmg = v ("ssl_hmg"); p.hmq = v ("ssl_hmq");
                    p.hf = v ("ssl_hf");   p.hg = v ("ssl_hg");   p.hBell = on ("ssl_hbell");
                    ssl.process (ch, nCh, n, sr, p);
                }
                break;
            case 4:
                if (on ("pt_on"))
                {
                    bgtr::PulseEq::P p;
                    p.lf = (int) v ("pt_lf"); p.lb = v ("pt_lb"); p.la = v ("pt_la");
                    p.hf = (int) v ("pt_hf"); p.hb = v ("pt_hb"); p.bw = v ("pt_bw");
                    p.haf = (int) v ("pt_haf"); p.ha = v ("pt_ha"); p.amp = on ("pt_amp");
                    pultec.process (ch, nCh, n, sr, p);
                }
                break;
            case 5:
                if (on ("sat_on"))
                {
                    bgtr::Sat::P p;
                    p.driveDb = v ("sat_drive"); p.bias = v ("sat_bias"); p.mix = v ("sat_mix"); p.outDb = v ("sat_out");
                    sat.process (ch, nCh, n, sr, p);
                }
                break;
            case 6:
                if (on ("wd_on")) width.process (ch, nCh, n, sr, v ("wd_width"), v ("wd_haas"));
                break;
        }
    }

    // Output gain, dry/wet mix and bypass (all smoothed, so toggling bypass never clicks)
    {
        const float tOut = dB2g (v ("out_gain")), tMix = on ("bypass") ? 0.f : v ("mix"), a = bgtr::coefMs (sr, 10.f);
        for (int i = 0; i < n; ++i)
        {
            const float og = sOut.tick (tOut, a), m = sMix.tick (tMix, a);
            for (int c = 0; c < nCh; ++c)
                ch[c][i] = dry.getSample (c, i) * (1.f - m) + ch[c][i] * og * m;
        }
    }

    // Metering: VU-style (300 ms) RMS of the final output, and the active compressor's gain reduction
    double sum = 0.0;
    for (int c = 0; c < nCh; ++c)
        for (int i = 0; i < n; ++i) sum += (double) ch[c][i] * ch[c][i];
    const float pw = (float) (sum / (double) (n * nCh));
    vuPower += (pw - vuPower) * (1.f - std::exp (-(float) n / (0.3f * (float) sr)));
    meterOutDb.store (10.f * std::log10 (vuPower + 1.0e-10f));
    meterGrDb.store (grNow);
}

//==============================================================================
// Factory presets for already-recorded guitar tracks. Choice parameters take their INDEX.
// Every preset starts from default values, then overrides only what is listed. Presets never touch the global
// controls (bypass, input, output, mix, meter) or the signal-chain order.
namespace
{
    struct Preset { const char* name; std::vector<std::pair<const char*, float>> values; };

    const std::vector<Preset>& presets()
    {
        // Choice indices: comp_ratio 0=4:1 1=8:1 2=12:1 3=20:1 4=All, comp_type 0=FET 1=Opto 2=Bus,
        // bus_ratio 0=2:1 1=4:1 2=10:1, bus_attack 0=0.1 1=0.3 2=1 3=3 4=10 5=30 ms, bus_release 0=0.1 1=0.3 2=0.6 3=1.2 s 4=Auto,
        // pt_hf 0=3k 1=4k 2=5k 3=8k 4=10k 5=12k 6=16k, pt_lf 0=20 1=30 2=60 3=100 Hz.
        // Starting points built from published CLA habits: bright guitars (about +6 dB near 8 kHz), pushed low end,
        // light-to-moderate opto compression (2-5 dB), no mid scooping, hard-panned doubled pairs, little or no reverb.
        static const std::vector<Preset> p = {
            { "Init", {} },

            { "Anthem Wall", {
                { "comp_type", 1.f }, { "opto_peak", 40.f }, { "opto_gain", 4.f },
                { "ssl_lf", 100.f }, { "ssl_lg", 3.f }, { "ssl_hf", 8000.f }, { "ssl_hg", 6.f },
                { "pt_lf", 2.f }, { "pt_lb", 2.f }, { "pt_la", 1.5f },
                { "sat_drive", 4.f }, { "wd_width", 1.1f } } },

            { "Garage Punk Chug", {
                { "gate_thresh", -45.f }, { "gate_range", 40.f }, { "gate_release", 50.f },
                { "comp_in", 15.f }, { "comp_ratio", 1.f }, { "comp_attack", 0.1f }, { "comp_release", 150.f }, { "comp_schpf", 80.f },
                { "ssl_hpf", 90.f }, { "ssl_lmf", 2500.f }, { "ssl_lmg", 3.f }, { "ssl_lmq", 1.2f },
                { "ssl_hf", 8000.f }, { "ssl_hg", 3.f },
                { "sat_drive", 7.f }, { "sat_bias", 0.15f } } },

            { "Rock Opera Layers", {
                { "comp_in", 10.f }, { "comp_ratio", 0.f }, { "comp_release", 300.f }, { "comp_mix", 0.65f },
                { "ssl_lf", 120.f }, { "ssl_lg", 3.f }, { "ssl_hf", 8000.f }, { "ssl_hg", 5.f },
                { "pt_lb", 2.5f }, { "pt_la", 2.f },
                { "wd_width", 1.3f } } },

            { "Ballad Acoustic Bed", {
                { "gate_on", 0.f },
                { "comp_type", 1.f }, { "opto_peak", 35.f }, { "opto_gain", 3.f },
                { "ssl_hpf", 90.f }, { "ssl_hf", 10000.f }, { "ssl_hg", 3.f }, { "ssl_hbell", 1.f },
                { "pt_hf", 5.f }, { "pt_hb", 2.f }, { "pt_bw", 7.f },
                { "sat_drive", 2.f }, { "sat_mix", 0.5f }, { "wd_width", 1.25f } } },

            { "Dirty Intro Riff", {
                { "comp_in", 18.f }, { "comp_ratio", 2.f }, { "comp_attack", 0.15f }, { "comp_release", 180.f }, { "comp_mix", 0.85f },
                { "ssl_lmf", 800.f }, { "ssl_lmg", 3.f }, { "ssl_lmq", 1.5f }, { "ssl_hf", 8000.f }, { "ssl_hg", 4.f },
                { "sat_drive", 14.f }, { "sat_bias", 0.2f }, { "sat_mix", 0.9f }, { "sat_out", -3.f } } },

            { "Chorus Lift Doubles", {
                { "comp_type", 1.f }, { "opto_peak", 45.f }, { "opto_gain", 5.f },
                { "ssl_lf", 100.f }, { "ssl_lg", 2.f }, { "ssl_hf", 8000.f }, { "ssl_hg", 6.f },
                { "sat_drive", 5.f }, { "wd_width", 1.5f }, { "wd_haas", 14.f } } },

            { "Palm Mute Verse", {
                { "gate_thresh", -42.f }, { "gate_range", 45.f }, { "gate_release", 40.f },
                { "comp_in", 16.f }, { "comp_ratio", 2.f }, { "comp_attack", 0.1f }, { "comp_release", 120.f }, { "comp_schpf", 100.f },
                { "eq_hpf", 100.f }, { "eq_lmf", 3.f }, { "eq_lmg", 3.f },        // Process EQ LMF 240 Hz, -4 dB
                { "ssl_hf", 8000.f }, { "ssl_hg", 3.f }, { "sat_drive", 4.f } } },

            { "Lead Sting", {
                { "gate_on", 0.f },
                { "comp_type", 1.f }, { "opto_peak", 50.f }, { "opto_mode", 1.f }, { "opto_gain", 4.f },
                { "ssl_hpf", 120.f }, { "ssl_hmf", 2800.f }, { "ssl_hmg", 4.f }, { "ssl_hmq", 1.4f },
                { "ssl_hf", 8000.f }, { "ssl_hg", 3.f },
                { "sat_drive", 8.f }, { "sat_bias", 0.15f }, { "wd_width", 1.15f }, { "wd_haas", 12.f } } },

            { "Radio Gloss", {
                { "comp_in", 9.f }, { "comp_ratio", 0.f }, { "comp_release", 400.f }, { "comp_mix", 0.8f },
                { "ssl_lf", 100.f }, { "ssl_lg", 2.f }, { "ssl_hf", 8000.f }, { "ssl_hg", 6.f },
                { "pt_hf", 4.f }, { "pt_hb", 2.5f }, { "pt_bw", 5.f },
                { "sat_drive", 3.f }, { "wd_width", 1.2f } } },

            { "Arena Clean Shimmer", {
                { "gate_on", 0.f },
                { "comp_type", 1.f }, { "opto_peak", 30.f }, { "opto_gain", 2.f },
                { "ssl_hpf", 100.f }, { "ssl_hf", 10000.f }, { "ssl_hg", 3.f }, { "ssl_hbell", 1.f },
                { "pt_hf", 5.f }, { "pt_hb", 3.f }, { "pt_bw", 7.f },
                { "sat_drive", 2.f }, { "sat_mix", 0.3f }, { "wd_width", 1.35f }, { "wd_haas", 8.f } } },

            // Band-limited "through a small radio" tone for intros and breaks: forums and articles put the
            // telephone/radio window at a 300-600 Hz high-pass and a 3-7 kHz low-pass with a bump near 1 kHz.
            // The chain is mono and squashed, with grit from the saturation.
            // Stereo-linked bus compressor with a slow-ish attack and Auto release: gentle cohesion on a guitar group.
            { "Bus Glue", {
                { "comp_type", 2.f }, { "bus_thresh", -14.f }, { "bus_ratio", 1.f }, { "bus_attack", 4.f },
                { "bus_release", 4.f }, { "bus_makeup", 2.f }, { "comp_schpf", 60.f },
                { "ssl_hf", 8000.f }, { "ssl_hg", 3.f }, { "sat_drive", 3.f } } },

            { "Lo-Fi Radio Intro", {
                { "gate_on", 0.f },
                { "comp_in", 16.f }, { "comp_ratio", 2.f }, { "comp_attack", 0.1f }, { "comp_release", 150.f },
                { "ssl_hpf", 400.f }, { "ssl_lpf", 4000.f },
                { "ssl_hmf", 1300.f }, { "ssl_hmg", 3.f }, { "ssl_hmq", 1.0f },
                { "sat_drive", 9.f }, { "sat_bias", 0.2f }, { "sat_mix", 0.7f },
                { "wd_width", 0.f } } },
        };
        return p;
    }

    bool isGlobalParam (const juce::String& id)
    {
        return id.startsWith ("slot") || id == "bypass" || id == "in_gain" || id == "out_gain" || id == "mix" || id == "meter_mode";
    }
}

int ChannelProcessor::getNumPrograms() { return (int) presets().size(); }

const juce::String ChannelProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, getNumPrograms()) ? presets()[(size_t) i].name : "";
}

void ChannelProcessor::setCurrentProgram (int i)
{
    if (! juce::isPositiveAndBelow (i, getNumPrograms())) return;
    currentPreset = i;

    for (auto* param : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (param))
            if (! isGlobalParam (r->paramID))
                r->setValueNotifyingHost (r->getDefaultValue());

    for (auto& kv : presets()[(size_t) i].values)
        if (auto* r = apvts.getParameter (kv.first))
            r->setValueNotifyingHost (r->convertTo0to1 (kv.second));
}

//==============================================================================
juce::AudioProcessorEditor* ChannelProcessor::createEditor()
{
#ifdef SC_TEST_NO_EDITOR
    return nullptr;       // the processor tests build without the GUI
#else
    return new ChannelEditor (*this);
#endif
}

void ChannelProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void ChannelProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChannelProcessor(); }

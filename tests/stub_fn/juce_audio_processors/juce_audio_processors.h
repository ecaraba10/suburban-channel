// Functional stand-in for the parts of JUCE 8 that PluginProcessor.cpp uses, so the real processor logic
// (parameter mapping, module order, presets, bypass, metering) can run in a plain C++ test. Not JUCE itself.
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#define JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(x)
#define JUCE_CALLTYPE
#define jassert(x) ((void) 0)
namespace juce {
template <class T> T jmax (T a, T b) { return a > b ? a : b; }
template <class T> T jmin (T a, T b) { return a < b ? a : b; }
template <class T> T jlimit (T lo, T hi, T v) { return v < lo ? lo : (v > hi ? hi : v); }
template <class T> bool isPositiveAndBelow (T v, T hi) { return v >= T() && v < hi; }

struct String {
    std::string s;
    String() {} String (const char* c) : s (c) {} String (const std::string& c) : s (c) {} String (int v) : s (std::to_string (v)) {}
    bool operator== (const String& o) const { return s == o.s; } bool operator!= (const String& o) const { return s != o.s; }
    bool startsWith (const String& p) const { return s.rfind (p.s, 0) == 0; }
    String operator+ (const String& o) const { return String (s + o.s); }
    friend String operator+ (const char* a, const String& b) { return String (std::string (a) + b.s); }
};
struct Identifier { std::string s; Identifier (const char* c) : s (c) {} operator String() const { return String (s); } };
struct StringArray { std::vector<String> items; StringArray() {} StringArray (std::initializer_list<const char*> l) { for (auto* c : l) items.push_back (String (c)); } int size() const { return (int) items.size(); } };
struct MemoryBlock {}; struct MidiBuffer {}; struct UndoManager {}; struct ScopedNoDenormals { ScopedNoDenormals() {} ~ScopedNoDenormals() {} };
struct XmlElement { bool hasTagName (const String&) const { return true; } };
struct ValueTree { std::unique_ptr<XmlElement> createXml() const { return nullptr; } static ValueTree fromXml (const XmlElement&) { return {}; } Identifier getType() const { return "x"; } };

template <class T> struct NormalisableRange {
    T start {}, end { 1 }, interval { 0 }, skew { 1 };
    NormalisableRange() {} NormalisableRange (T s, T e, T i, T k) : start (s), end (e), interval (i), skew (k) {}
    T convertTo0to1 (T v) const { T p = jlimit<T> (0, 1, (v - start) / (end - start)); return skew == 1 ? p : std::pow (p, skew); }
    T convertFrom0to1 (T p) const { p = jlimit<T> (0, 1, p); if (skew != 1 && p > 0) p = std::exp (std::log (p) / skew); return start + (end - start) * p; }
};
struct ParameterID { std::string id; template <class S> ParameterID (S&& s, int = 0) : id (String (s).s) {} };
struct AudioProcessorParameter { virtual ~AudioProcessorParameter() = default; };
struct RangedAudioParameter : AudioProcessorParameter {
    String paramID, name; std::atomic<float> raw { 0.f };
    virtual float convertTo0to1 (float) const = 0; virtual float convertFrom0to1 (float) const = 0; virtual float defaultRaw() const = 0;
    String getName (int) const { return name; } float getDefaultValue() const { return convertTo0to1 (defaultRaw()); }
    void setValueNotifyingHost (float norm) { raw.store (convertFrom0to1 (norm)); }
    void init (const ParameterID& id, const String& n) { paramID = String (id.id); name = n; raw.store (defaultRaw()); }
};
struct AudioParameterFloatAttributes { AudioParameterFloatAttributes withLabel (String) const { return *this; } };
struct AudioParameterFloat : RangedAudioParameter {
    NormalisableRange<float> range; float def;
    AudioParameterFloat (const ParameterID& id, const String& n, NormalisableRange<float> r, float d, const AudioParameterFloatAttributes& = {}) : range (r), def (d) { init (id, n); }
    float convertTo0to1 (float v) const override { return range.convertTo0to1 (v); } float convertFrom0to1 (float p) const override { return range.convertFrom0to1 (p); } float defaultRaw() const override { return def; }
};
struct AudioParameterChoice : RangedAudioParameter {
    StringArray choices; int defIdx;
    AudioParameterChoice (const ParameterID& id, const String& n, const StringArray& c, int d) : choices (c), defIdx (d) { init (id, n); }
    float convertTo0to1 (float v) const override { return choices.size() > 1 ? v / (float) (choices.size() - 1) : 0.f; }
    float convertFrom0to1 (float p) const override { return std::round (jlimit (0.f, 1.f, p) * (float) (choices.size() - 1)); } float defaultRaw() const override { return (float) defIdx; }
};
struct AudioParameterBool : RangedAudioParameter {
    bool def;
    AudioParameterBool (const ParameterID& id, const String& n, bool d) : def (d) { init (id, n); }
    float convertTo0to1 (float v) const override { return v > 0.5f ? 1.f : 0.f; } float convertFrom0to1 (float p) const override { return p > 0.5f ? 1.f : 0.f; } float defaultRaw() const override { return def ? 1.f : 0.f; }
};

struct AudioChannelSet { int n = 2; static AudioChannelSet stereo() { return { 2 }; } static AudioChannelSet mono() { return { 1 }; } bool operator== (const AudioChannelSet& o) const { return n == o.n; } };
template <class T> struct AudioBuffer {
    std::vector<std::vector<T>> ch; int ns = 0;
    void setSize (int c, int n, bool = false, bool = false, bool = false) { ch.assign ((size_t) c, std::vector<T> ((size_t) n, T())); ns = n; }
    int getNumSamples() const { return ns; } int getNumChannels() const { return (int) ch.size(); }
    void clear (int c, int s, int n) { for (int i = 0; i < n; ++i) ch[(size_t) c][(size_t) (s + i)] = T(); }
    void copyFrom (int dc, int ds, const AudioBuffer& src, int sc, int ss, int n) { for (int i = 0; i < n; ++i) ch[(size_t) dc][(size_t) (ds + i)] = src.ch[(size_t) sc][(size_t) (ss + i)]; }
    T* getWritePointer (int c) { return ch[(size_t) c].data(); } const T* getReadPointer (int c) const { return ch[(size_t) c].data(); }
    T getSample (int c, int i) const { return ch[(size_t) c][(size_t) i]; }
};
struct AudioProcessor;
struct AudioProcessorValueTreeState {
    struct ParameterLayout {
        std::vector<std::unique_ptr<RangedAudioParameter>> params;
        template <class It> ParameterLayout (It b, It e) { for (; b != e; ++b) params.push_back (std::move (*b)); }
    };
    std::vector<std::unique_ptr<RangedAudioParameter>> owned; std::map<std::string, RangedAudioParameter*> byId; ValueTree state;
    AudioProcessorValueTreeState (AudioProcessor&, UndoManager*, const Identifier&, ParameterLayout l);
    RangedAudioParameter* getParameter (const String& id) const { auto it = byId.find (id.s); return it == byId.end() ? nullptr : it->second; }
    std::atomic<float>* getRawParameterValue (const String& id) const { auto* p = getParameter (id); return p ? &p->raw : nullptr; }
    ValueTree copyState() { return {}; } void replaceState (const ValueTree&) {}
};
struct AudioProcessor {
    struct BusesLayout { AudioChannelSet getMainInputChannelSet() const { return {}; } AudioChannelSet getMainOutputChannelSet() const { return {}; } };
    struct BusesProperties { BusesProperties withInput (const String&, const AudioChannelSet&, bool) const { return *this; } BusesProperties withOutput (const String&, const AudioChannelSet&, bool) const { return *this; } };
    std::vector<AudioProcessorParameter*> paramList;
    AudioProcessor (const BusesProperties&) {} virtual ~AudioProcessor() = default;
    virtual void prepareToPlay (double, int) = 0; virtual void releaseResources() = 0;
    virtual bool isBusesLayoutSupported (const BusesLayout&) const { return true; }
    virtual void processBlock (AudioBuffer<float>&, MidiBuffer&) = 0;
    virtual struct AudioProcessorEditor* createEditor() = 0; virtual bool hasEditor() const = 0;
    virtual const String getName() const = 0; virtual bool acceptsMidi() const = 0; virtual bool producesMidi() const = 0; virtual double getTailLengthSeconds() const = 0;
    virtual int getNumPrograms() = 0; virtual int getCurrentProgram() = 0; virtual void setCurrentProgram (int) = 0;
    virtual const String getProgramName (int) = 0; virtual void changeProgramName (int, const String&) = 0;
    virtual void getStateInformation (MemoryBlock&) = 0; virtual void setStateInformation (const void*, int) = 0;
    virtual AudioProcessorParameter* getBypassParameter() const { return nullptr; }
    int getTotalNumInputChannels() const { return 2; } int getTotalNumOutputChannels() const { return 2; } void setLatencySamples (int) {}
    const std::vector<AudioProcessorParameter*>& getParameters() const { return paramList; }
    static void copyXmlToBinary (const XmlElement&, MemoryBlock&) {} static std::unique_ptr<XmlElement> getXmlFromBinary (const void*, int) { return nullptr; }
};
inline AudioProcessorValueTreeState::AudioProcessorValueTreeState (AudioProcessor& p, UndoManager*, const Identifier&, ParameterLayout l)
{ for (auto& up : l.params) { byId[up->paramID.s] = up.get(); p.paramList.push_back (up.get()); owned.push_back (std::move (up)); } }
}

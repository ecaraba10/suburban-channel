// Minimal stand-in for JUCE 8 declarations used by Suburban Channel (syntax/type check only, never linked).
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#define JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(x)
#define JUCE_CALLTYPE
#define jassert(x) ((void) 0)
namespace juce {
using uint32 = std::uint32_t; using uint8 = std::uint8_t;
enum NotificationType { dontSendNotification, sendNotification, sendNotificationSync, sendNotificationAsync };
template <class T> T jmax (T a, T b) { return a > b ? a : b; }
template <class T> T jmin (T a, T b) { return a < b ? a : b; }
template <class T> T jlimit (T lo, T hi, T v) { return v < lo ? lo : (v > hi ? hi : v); }
template <class T> bool isPositiveAndBelow (T v, T hi) { return v >= T() && v < hi; }
template <class T> T degreesToRadians (T d) { return d * (T) 0.017453292519943295; }

struct String;
struct Identifier { Identifier (const char*) {} operator String() const; };
struct String {
    String() {} String (const char*) {} String (int) {} String (float) {}
    bool operator== (const String&) const { return true; } bool operator!= (const String&) const { return false; }
    bool startsWith (const String&) const { return false; }
    String operator+ (const String&) const { return {}; }
    friend String operator+ (const char*, const String&) { return {}; }
};
inline Identifier::operator String() const { return {}; }
inline String& operator<< (String& s, int) { return s; } inline String& operator<< (String& s, const char*) { return s; } inline String& operator<< (String& s, const String&) { return s; }
struct StringArray { StringArray() {} StringArray (std::initializer_list<const char*>) {} };
struct MemoryBlock {}; struct MidiBuffer {}; struct UndoManager {}; struct ScopedNoDenormals { ScopedNoDenormals() {} ~ScopedNoDenormals() {} };
struct XmlElement { bool hasTagName (const String&) const { return true; } };
struct StringRef { StringRef (const char*) {} StringRef (const String&) {} };
template <class T> struct Point { T x {}, y {}; Point() {} Point (T a, T b) : x (a), y (b) {} };
template <class T> struct Line { Line (Point<T>, Point<T>) {} };
template <class T> struct Rectangle {
    Rectangle() {} Rectangle (T, T) {} Rectangle (T, T, T, T) {}
    T getX() const { return {}; } T getY() const { return {}; } T getWidth() const { return {}; } T getHeight() const { return {}; }
    T getRight() const { return {}; } T getBottom() const { return {}; } T getCentreX() const { return {}; } T getCentreY() const { return {}; }
    Point<T> getCentre() const { return {}; }
    Rectangle reduced (T) const { return *this; } Rectangle reduced (T, T) const { return *this; } Rectangle expanded (T) const { return *this; }
    Rectangle withSizeKeepingCentre (T, T) const { return *this; } Rectangle withCentre (Point<T>) const { return *this; }
    Rectangle<float> toFloat() const { return {}; } Rectangle<int> toNearestInt() const { return {}; }
};
struct Colour {
    Colour() {} explicit Colour (uint32) {}
    Colour brighter (float = 0.4f) const { return *this; } Colour darker (float = 0.4f) const { return *this; } Colour withAlpha (float) const { return *this; }
    uint8 getAlpha() const { return 0; }
};
struct Colours { static const Colour white, black, transparentBlack; };
struct ColourGradient { ColourGradient (Colour, float, float, Colour, float, float, bool) {} };
struct AffineTransform { static AffineTransform rotation (float) { return {}; } template <class T> AffineTransform translated (Point<T>) const { return *this; } };
struct PathStrokeType { PathStrokeType (float) {} };
struct Path { void addRoundedRectangle (float, float, float, float, float) {} void addCentredArc (float, float, float, float, float, float, float, bool) {} };
struct FontOptions { FontOptions() {} FontOptions (float) {} FontOptions (float, int) {} };
struct Font { enum FontStyleFlags { plain = 0, bold = 1, italic = 2 }; Font (FontOptions) {} Font (float, int = plain) {} };
struct Justification { enum Flags { centred, centredLeft, bottomLeft }; Justification (Flags) {} };
struct Graphics {
    void fillAll (Colour) {} void setColour (Colour) {} void setFont (float) {} void setFont (const Font&) {}
    void fillRoundedRectangle (Rectangle<float>, float) {} void fillRoundedRectangle (float, float, float, float, float) {}
    void drawRoundedRectangle (Rectangle<float>, float, float) {}
    void drawText (const String&, int, int, int, int, Justification) {}
    void drawText (const String&, Rectangle<int>, Justification, bool = true) {}
    void drawText (const String&, Rectangle<float>, Justification, bool = true) {}
    void setGradientFill (const ColourGradient&) {} void fillEllipse (Rectangle<float>) {} void fillEllipse (float, float, float, float) {}
    void drawEllipse (Rectangle<float>, float) {} void drawLine (Line<float>, float = 1.f) {} void drawLine (float, float, float, float, float) {}
    void strokePath (const Path&, const PathStrokeType&) {} void fillPath (const Path&, const AffineTransform&) {}
    void saveState() {} void restoreState() {} bool reduceClipRegion (Rectangle<int>) { return true; }
};
struct SettableTooltipClient { virtual ~SettableTooltipClient() = default; void setTooltip (const String&) {} };
struct LookAndFeel { virtual ~LookAndFeel() = default; void setColour (int, Colour) {} };
struct Component {
    virtual ~Component() = default;
    virtual void paint (Graphics&) {} virtual void resized() {}
    void setBounds (int, int, int, int) {} void setBounds (Rectangle<int>) {} void setVisible (bool) {} void addAndMakeVisible (Component&) {}
    Rectangle<int> getLocalBounds() const { return {}; } int getWidth() const { return 0; } int getHeight() const { return 0; } int getX() const { return 0; }
    void setSize (int, int) {} void setLookAndFeel (LookAndFeel*) {} void repaint() {} void setInterceptsMouseClicks (bool, bool) {}
    void setColour (int, Colour) {} Colour findColour (int) const { return {}; }
};
struct Label : Component { enum ColourIds { textColourId = 1 }; void setText (const String&, NotificationType) {} void setJustificationType (Justification) {} void setFont (const Font&) {} };
struct Button : Component, SettableTooltipClient { bool getToggleState() const { return false; } String getButtonText() const { return {}; } std::function<void()> onClick; };
struct ToggleButton : Button { ToggleButton() {} ToggleButton (const String&) {} };
struct TextButton : Button { enum ColourIds { buttonColourId = 1, textColourOffId }; TextButton() {} TextButton (const String&) {} };
struct ComboBox : Component, SettableTooltipClient {
    enum ColourIds { backgroundColourId = 1, textColourId, outlineColourId, arrowColourId };
    void addItemList (const StringArray&, int) {} void addItem (const String&, int) {} void setSelectedId (int, NotificationType = sendNotification) {} int getSelectedId() const { return 0; }
    std::function<void()> onChange;
};
struct Slider : Component, SettableTooltipClient {
    enum SliderStyle { RotaryVerticalDrag }; enum TextEntryBoxPosition { TextBoxBelow };
    enum ColourIds { thumbColourId = 1, trackColourId, rotarySliderFillColourId, textBoxTextColourId, textBoxBackgroundColourId, textBoxOutlineColourId };
    Slider() {} Slider (SliderStyle, TextEntryBoxPosition) {}
    void setTextBoxStyle (TextEntryBoxPosition, bool, int, int) {}
};
struct PopupMenu { enum ColourIds { backgroundColourId = 1, textColourId, highlightedBackgroundColourId }; };
struct LookAndFeel_V4 : LookAndFeel {
    virtual void drawRotarySlider (Graphics&, int, int, int, int, float, float, float, Slider&) {}
    virtual void drawToggleButton (Graphics&, ToggleButton&, bool, bool) {}
};
struct TooltipWindow : Component { TooltipWindow (Component* = nullptr, int = 700) {} };
struct Timer { virtual ~Timer() = default; void startTimerHz (int) {} virtual void timerCallback() = 0; };
template <class T> struct NormalisableRange { NormalisableRange() {} NormalisableRange (T, T, T, T) {} };
struct ParameterID { template <class S> ParameterID (S&&, int = 0) {} };
struct AudioProcessorParameter { virtual ~AudioProcessorParameter() = default; String getName (int) const { return {}; } void setValueNotifyingHost (float) {} float getDefaultValue() const { return 0; } };
struct RangedAudioParameter : AudioProcessorParameter { const String paramID; float convertTo0to1 (float) const { return 0; } float convertFrom0to1 (float) const { return 0; } };
struct AudioParameterFloatAttributes { AudioParameterFloatAttributes withLabel (String) const { return *this; } };
struct AudioParameterFloat : RangedAudioParameter { AudioParameterFloat (const ParameterID&, const String&, NormalisableRange<float>, float, const AudioParameterFloatAttributes& = {}) {} };
struct AudioParameterChoice : RangedAudioParameter { AudioParameterChoice (const ParameterID&, const String&, const StringArray&, int) {} const StringArray choices; };
struct AudioParameterBool : RangedAudioParameter { AudioParameterBool (const ParameterID&, const String&, bool) {} };
struct SliderParameterAttachment { SliderParameterAttachment (RangedAudioParameter&, Slider&, UndoManager* = nullptr) {} };
struct ComboBoxParameterAttachment { ComboBoxParameterAttachment (RangedAudioParameter&, ComboBox&, UndoManager* = nullptr) {} };
struct ButtonParameterAttachment { ButtonParameterAttachment (RangedAudioParameter&, Button&, UndoManager* = nullptr) {} };
struct AudioChannelSet { static AudioChannelSet stereo() { return {}; } static AudioChannelSet mono() { return {}; } bool operator== (const AudioChannelSet&) const { return true; } };
template <class T> struct AudioBuffer {
    void setSize (int, int, bool = false, bool = false, bool = false) {} int getNumSamples() const { return 0; } int getNumChannels() const { return 0; }
    void clear (int, int, int) {} void copyFrom (int, int, const AudioBuffer&, int, int, int) {}
    T* getWritePointer (int) { return nullptr; } const T* getReadPointer (int) const { return nullptr; } T getSample (int, int) const { return {}; }
};
struct ValueTree { std::unique_ptr<XmlElement> createXml() const { return nullptr; } static ValueTree fromXml (const XmlElement&) { return {}; } Identifier getType() const { return "x"; } };
struct AudioProcessorValueTreeState {
    struct ParameterLayout { template <class It> ParameterLayout (It, It) {} };
    AudioProcessorValueTreeState (struct AudioProcessor&, UndoManager*, const Identifier&, ParameterLayout) {}
    RangedAudioParameter* getParameter (const String&) const { return nullptr; } std::atomic<float>* getRawParameterValue (const String&) const { return nullptr; }
    ValueTree copyState() { return {}; } void replaceState (const ValueTree&) {} ValueTree state;
};
struct AudioProcessor {
    struct BusesLayout { AudioChannelSet getMainInputChannelSet() const { return {}; } AudioChannelSet getMainOutputChannelSet() const { return {}; } };
    struct BusesProperties { BusesProperties withInput (const String&, const AudioChannelSet&, bool) const { return *this; } BusesProperties withOutput (const String&, const AudioChannelSet&, bool) const { return *this; } };
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
    const std::vector<AudioProcessorParameter*>& getParameters() const { static std::vector<AudioProcessorParameter*> v; return v; }
    static void copyXmlToBinary (const XmlElement&, MemoryBlock&) {} static std::unique_ptr<XmlElement> getXmlFromBinary (const void*, int) { return nullptr; }
};
struct AudioProcessorEditor : Component { AudioProcessorEditor (AudioProcessor&) {} AudioProcessorEditor (AudioProcessor*) {} };
}
inline const juce::Colour juce::Colours::white, juce::Colours::black, juce::Colours::transparentBlack;

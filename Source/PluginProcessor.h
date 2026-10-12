#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp.h"

// Seven modules, any of which can sit in any of seven slots (slot order = signal order).
// 0 Gate | 1 Compressor (FET, Opto or Bus) | 2 API EQ | 3 SSL EQ | 4 Pultec | 5 Tube Sat | 6 Width | 7 Off
class ChannelProcessor : public juce::AudioProcessor
{
public:
    ChannelProcessor();
    ~ChannelProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Suburban Channel"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;

    // Meter values for the editor (written on the audio thread, read on the UI thread)
    std::atomic<float> meterOutDb { -100.f };   // VU-style RMS of the output, dBFS
    std::atomic<float> meterGrDb  { 0.f };      // gain reduction of the active compressor, dB

private:
    float v (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    bool  on (const juce::String& id) const { return v (id) > 0.5f; }

    double sr = 44100.0;
    bgtr::Gate gate;
    bgtr::FetComp fet;
    bgtr::OptoComp opto;
    bgtr::BusComp bus;
    bgtr::ProcessEq api;
    bgtr::SolidEq ssl;
    bgtr::PulseEq pultec;
    bgtr::Sat sat;
    bgtr::Width width;
    bgtr::Smooth sIn, sOut, sMix;

    juce::AudioBuffer<float> dry;
    float vuPower = 0.f;
    int currentPreset = 0;
    juce::AudioParameterBool* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelProcessor)
};

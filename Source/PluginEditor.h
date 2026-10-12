#pragma once
#include "PluginProcessor.h"

// Black UI. Knob colours: blue = API EQ, grey = SSL EQ, cream = everything else.
class ChannelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ChannelLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

// Analog-style VU meter. Shows output level (0 VU = -18 dBFS) or gain reduction of the active compressor.
class VuMeter : public juce::Component, private juce::Timer
{
public:
    explicit VuMeter (ChannelProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    ChannelProcessor& proc;
    float needle = 0.f;
    int mode = 0;
};

struct CtlSpec
{
    const char* id;
    const char* label;
    char kind;   // 'k' knob (stepped for choice params), 't' toggle, 'c' combo box
    int only;    // -1 = always shown, 0 = only for FET compressor, 1 = only for Opto compressor
    juce::uint32 body = 0, cap = 0, ptr = 0;   // knob body / centre cap / pointer colours (0 = panel default / none)
};

class ChannelEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ChannelEditor (ChannelProcessor&);
    ~ChannelEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Cell
    {
        std::unique_ptr<juce::Component> comp;   // declared before attachments so they are destroyed first
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::SliderParameterAttachment>   sAtt;
        std::unique_ptr<juce::ComboBoxParameterAttachment> cAtt;
        std::unique_ptr<juce::ButtonParameterAttachment>   bAtt;
        int only = -1, visIndex = -1;
    };

    struct Panel
    {
        juce::String title;
        juce::Colour accent;
        int cols = 4, layoutCols = 1;
        std::unique_ptr<Cell> on, type;
        std::vector<std::unique_ptr<Cell>> cells;
        juce::Rectangle<int> bounds;
        bool visible = false;
    };

    std::unique_ptr<Cell> makeCell (const CtlSpec&, juce::Colour knob, juce::Colour pointer);
    void addCellToEditor (Cell&);
    void relayout();
    void timerCallback() override;

    ChannelProcessor& proc;
    ChannelLookAndFeel laf;
    VuMeter vu;
    juce::ComboBox meterBox, presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    juce::Label meterLabel, presetLabel;
    std::unique_ptr<Cell> bypassCell;
    juce::TooltipWindow tooltipWindow { this, 600 };
    std::unique_ptr<juce::ComboBoxParameterAttachment> meterAtt;
    std::vector<std::unique_ptr<Cell>> globalCells;
    juce::ComboBox slotBox[7];
    std::vector<std::unique_ptr<juce::ComboBoxParameterAttachment>> slotAtt;
    std::vector<Panel> panels;
    juce::String lastSignature;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelEditor)
};

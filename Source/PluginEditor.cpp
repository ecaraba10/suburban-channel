#include "PluginEditor.h"

namespace
{
    constexpr int W = 980, pad = 10, gap = 8, ppad = 12, headerH = 30, cellW = 64, cellH = 88, topH = 198;

    struct PanelSpec
    {
        const char* title;
        juce::uint32 accent, knob, pointer;
        int cols;
        const char* onId;
        const char* typeId;
        std::vector<CtlSpec> ctls;
    };

    // Panels are indexed by module number: 0 Gate, 1 Compressor, 2 Process EQ, 3 Solid EQ, 4 Pulse EQ, 5 Tube Sat, 6 Width
    // Knob colours: FET = black with silver tops, Opto = black with white pointers, Bus = grey with light caps, Process EQ = blue frequency
    // knobs and silver gain knobs, Solid EQ = grey knobs with colour-coded band caps, Pulse EQ = black with white pointers.
    constexpr juce::uint32 kBlack = 0xff161616, kSilver = 0xffc6c9ce, kWhite = 0xfff2f2f2, kInk = 0xff111111,
                           kGrey = 0xff868a92, kBlue = 0xff2c6bd8, kBrown = 0xff8a5a3a, kGreen = 0xff2fa35a,
                           kPurple = 0xff8e5ad6, kLight = 0xffe9e4d6;

    const std::vector<PanelSpec>& specs()
    {
        static const std::vector<PanelSpec> s = {
            { "GATE", kWhite, kSilver, kInk, 3, "gate_on", nullptr, {
                { "gate_thresh", "Thresh", 'k', -1 }, { "gate_range", "Range", 'k', -1 }, { "gate_release", "Release", 'k', -1 } } },

            { "COMPRESSOR", kWhite, kBlack, kWhite, 8, "comp_on", "comp_type", {
                { "comp_in", "Input", 'k', 0, kBlack, kSilver, kWhite }, { "comp_ratio", "Ratio", 'k', 0, kBlack, kSilver, kWhite },
                { "comp_attack", "Attack", 'k', 0, kBlack, kSilver, kWhite }, { "comp_release", "Release", 'k', 0, kBlack, kSilver, kWhite },
                { "comp_mix", "Mix", 'k', 0, kBlack, kSilver, kWhite }, { "comp_out", "Output", 'k', 0, kBlack, kSilver, kWhite },
                { "comp_auto", "Auto Gain", 't', 0 },
                { "opto_peak", "Peak Red.", 'k', 1, kBlack, 0, kWhite }, { "opto_gain", "Gain", 'k', 1, kBlack, 0, kWhite },
                { "opto_mode", "Mode", 'c', 1 },
                { "bus_thresh", "Threshold", 'k', 2, kGrey, kLight, kInk }, { "bus_ratio", "Ratio", 'k', 2, kGrey, kLight, kInk },
                { "bus_attack", "Attack", 'k', 2, kGrey, kLight, kInk }, { "bus_release", "Release", 'k', 2, kGrey, kLight, kInk },
                { "bus_makeup", "Makeup", 'k', 2, kGrey, kLight, kInk }, { "bus_mix", "Mix", 'k', 2, kGrey, kLight, kInk },
                { "comp_schpf", "SC HPF", 'k', -1, kBlack, kSilver, kWhite } } },

            { "PROCESS EQ", 0xff4d9bff, kBlue, kWhite, 6, "eq_on", nullptr, {
                { "eq_hpf", "HPF", 'k', -1, kGrey, 0, kWhite },
                { "eq_lf", "LF", 'k', -1, kBlue, 0, kWhite }, { "eq_lg", "LF Gain", 'k', -1, kSilver, 0, kInk },
                { "eq_lshelf", "LF Shelf", 't', -1 },
                { "eq_lmf", "LMF", 'k', -1, kBlue, 0, kWhite }, { "eq_lmg", "LMF Gain", 'k', -1, kSilver, 0, kInk },
                { "eq_hmf", "HMF", 'k', -1, kBlue, 0, kWhite }, { "eq_hmg", "HMF Gain", 'k', -1, kSilver, 0, kInk },
                { "eq_hf", "HF", 'k', -1, kBlue, 0, kWhite }, { "eq_hg", "HF Gain", 'k', -1, kSilver, 0, kInk },
                { "eq_hshelf", "HF Shelf", 't', -1 } } },

            { "SOLID EQ", 0xffb4b8bf, kGrey, kWhite, 8, "ssl_on", nullptr, {
                { "ssl_hpf", "HPF", 'k', -1, kGrey, kLight, kInk }, { "ssl_lpf", "LPF", 'k', -1, kGrey, kLight, kInk },
                { "ssl_lf", "LF", 'k', -1, kGrey, kBrown, kWhite }, { "ssl_lg", "LF Gain", 'k', -1, kGrey, kBrown, kWhite },
                { "ssl_lbell", "LF Bell", 't', -1 },
                { "ssl_hf", "HF", 'k', -1, kGrey, kPurple, kWhite }, { "ssl_hg", "HF Gain", 'k', -1, kGrey, kPurple, kWhite },
                { "ssl_hbell", "HF Bell", 't', -1 },
                { "ssl_lmf", "LMF", 'k', -1, kGrey, kBlue, kWhite }, { "ssl_lmg", "LMF Gain", 'k', -1, kGrey, kBlue, kWhite },
                { "ssl_lmq", "LMF Q", 'k', -1, kGrey, kBlue, kWhite },
                { "ssl_hmf", "HMF", 'k', -1, kGrey, kGreen, kWhite }, { "ssl_hmg", "HMF Gain", 'k', -1, kGrey, kGreen, kWhite },
                { "ssl_hmq", "HMF Q", 'k', -1, kGrey, kGreen, kWhite } } },

            { "PULSE EQ", kWhite, kBlack, kWhite, 5, "pt_on", nullptr, {
                { "pt_lf", "Low Freq", 'k', -1 }, { "pt_lb", "Low Boost", 'k', -1 }, { "pt_la", "Low Atten", 'k', -1 },
                { "pt_hf", "High Freq", 'k', -1 }, { "pt_hb", "High Boost", 'k', -1 }, { "pt_bw", "Bandwidth", 'k', -1 },
                { "pt_haf", "Atten Freq", 'k', -1 }, { "pt_ha", "High Atten", 'k', -1 }, { "pt_amp", "Tube Amp", 't', -1 } } },

            { "TUBE SAT", kWhite, kSilver, kInk, 4, "sat_on", nullptr, {
                { "sat_drive", "Drive", 'k', -1 }, { "sat_bias", "Bias", 'k', -1 },
                { "sat_mix", "Mix", 'k', -1 }, { "sat_out", "Output", 'k', -1 } } },

            { "WIDTH", kWhite, kSilver, kInk, 2, "wd_on", nullptr, {
                { "wd_width", "Width", 'k', -1 }, { "wd_haas", "Haas", 'k', -1 } } },
        };
        return s;
    }

    void placeCell (juce::Component* comp, juce::Label* label, int x, int y)
    {
        if (label != nullptr) label->setBounds (x, y, cellW, 13);
        if (dynamic_cast<juce::Slider*> (comp) != nullptr)           comp->setBounds (x, y + 13, cellW, 72);
        else if (dynamic_cast<juce::ComboBox*> (comp) != nullptr)    comp->setBounds (x + 2, y + 32, cellW - 4, 24);
        else                                                         comp->setBounds (x + 2, y + 34, cellW - 4, 22);
    }
}

//==============================================================================
ChannelLookAndFeel::ChannelLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff15171a));
    setColour (juce::ComboBox::textColourId,       juce::Colour (0xffe6e8ec));
    setColour (juce::ComboBox::outlineColourId,    juce::Colour (0xff34373d));
    setColour (juce::ComboBox::arrowColourId,      juce::Colour (0xff9aa0ab));
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff101113));
    setColour (juce::PopupMenu::textColourId,       juce::Colour (0xffe6e8ec));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff2a2d33));
    setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff15171a));
    setColour (juce::TextButton::textColourOffId,  juce::Colour (0xffe6e8ec));
    setColour (juce::Slider::textBoxTextColourId,    juce::Colour (0xffc9ccd3));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
}

void ChannelLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                             float startAngle, float endAngle, juce::Slider& s)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.f);
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre (d, d);

    const auto body = s.findColour (juce::Slider::rotarySliderFillColourId);
    g.setColour (juce::Colour (0xff000000));
    g.fillEllipse (r.expanded (2.f));
    g.setGradientFill (juce::ColourGradient (body.brighter (0.25f), r.getCentreX(), r.getY(),
                                             body.darker (0.55f), r.getCentreX(), r.getBottom(), false));
    g.fillEllipse (r);
    g.setColour (body.brighter (0.8f).withAlpha (0.55f));
    g.drawEllipse (r, 1.2f);

    const auto cap = s.findColour (juce::Slider::trackColourId);
    if (cap.getAlpha() > 0)
    {
        auto c = r.reduced (d * 0.2f);
        g.setGradientFill (juce::ColourGradient (cap.brighter (0.3f), c.getCentreX(), c.getY(),
                                                 cap.darker (0.4f), c.getCentreX(), c.getBottom(), false));
        g.fillEllipse (c);
    }

    const float ang = startAngle + pos * (endAngle - startAngle);
    juce::Path p;
    p.addRoundedRectangle (-1.6f, -d * 0.5f + 3.f, 3.2f, d * 0.32f, 1.6f);
    g.setColour (s.findColour (juce::Slider::thumbColourId));
    g.fillPath (p, juce::AffineTransform::rotation (ang).translated (r.getCentre()));
}

void ChannelLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    const bool on = b.getToggleState();
    const float s = 14.f;
    auto led = juce::Rectangle<float> (s, s).withCentre ({ 2.f + s * 0.5f, (float) b.getHeight() * 0.5f });
    g.setColour (on ? juce::Colour (0xffff8a1f) : juce::Colour (0xff23252a));
    g.fillRoundedRectangle (led, 3.f);
    g.setColour (juce::Colour (0xff4a4d54));
    g.drawRoundedRectangle (led, 3.f, 1.f);
    g.setColour (juce::Colours::white.withAlpha (on ? 1.f : 0.6f));
    g.setFont (11.f);
    const int tx = (int) led.getRight() + 5;
    g.drawText (b.getButtonText(), tx, 0, b.getWidth() - tx, b.getHeight(), juce::Justification::centredLeft);
}

//==============================================================================
VuMeter::VuMeter (ChannelProcessor& p) : proc (p) { startTimerHz (30); }

void VuMeter::timerCallback()
{
    mode = (int) proc.apvts.getRawParameterValue ("meter_mode")->load();
    float target;
    if (mode == 0)
    {
        const float vu = proc.meterOutDb.load() + 18.f;   // 0 VU = -18 dBFS RMS
        target = vu < -40.f ? 0.f : juce::jlimit (0.f, 1.f, std::pow (10.f, vu / 20.f) / std::pow (10.f, 3.f / 20.f));
    }
    else
        target = juce::jlimit (0.f, 1.f, proc.meterGrDb.load() / 20.f);

    needle += (target - needle) * 0.35f;
    repaint();
}

void VuMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff050506));
    g.fillRoundedRectangle (r, 8.f);
    g.setColour (juce::Colour (0xff2a2a2e));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.f, 1.f);

    auto face = r.reduced (7.f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff3e4b4), face.getCentreX(), face.getY(),
                                             juce::Colour (0xffd9c183), face.getCentreX(), face.getBottom(), false));
    g.fillRoundedRectangle (face, 4.f);

    g.saveState();
    g.reduceClipRegion (face.toNearestInt());

    const bool gr = (mode == 1);
    const float px = face.getCentreX(), py = face.getBottom() + 6.f;
    const float R = face.getHeight() * 0.92f;
    const float a0 = -55.f, a1 = 55.f;
    auto ang = [&] (float n) { return juce::degreesToRadians (a0 + (a1 - a0) * n); };
    auto pt  = [&] (float n, float rad) { return juce::Point<float> (px + rad * std::sin (ang (n)), py - rad * std::cos (ang (n))); };
    auto vuNorm = [] (float vu) { return juce::jlimit (0.f, 1.f, std::pow (10.f, vu / 20.f) / std::pow (10.f, 3.f / 20.f)); };

    struct Tick { float norm; const char* text; };
    std::vector<Tick> ticks;
    if (! gr)
    {
        static const float vals[] = { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 };
        static const char* txt[]  = { "20", "10", "7", "5", "3", "2", "1", "0", "1", "2", "3" };
        for (int i = 0; i < 11; ++i) ticks.push_back ({ vuNorm (vals[i]), txt[i] });

        juce::Path red;
        red.addCentredArc (px, py, R - 2.f, R - 2.f, 0.f, ang (vuNorm (0.f)), ang (1.f), true);
        g.setColour (juce::Colour (0xffc0392b));
        g.strokePath (red, juce::PathStrokeType (3.f));
    }
    else
    {
        static const float vals[] = { 0, 2, 4, 6, 8, 10, 15, 20 };
        static const char* txt[]  = { "0", "2", "4", "6", "8", "10", "15", "20" };
        for (int i = 0; i < 8; ++i) ticks.push_back ({ vals[i] / 20.f, txt[i] });
    }

    juce::Path arc;
    arc.addCentredArc (px, py, R - 2.f, R - 2.f, 0.f, ang (0.f), ang (gr ? 1.f : vuNorm (0.f)), true);
    g.setColour (juce::Colour (0xff2b2418));
    g.strokePath (arc, juce::PathStrokeType (1.2f));

    g.setFont (10.f);
    for (auto& t : ticks)
    {
        g.setColour (juce::Colour (0xff2b2418));
        g.drawLine ({ pt (t.norm, R - 7.f), pt (t.norm, R) }, 1.2f);
        g.drawText (t.text, juce::Rectangle<float> (14.f, 11.f).withCentre (pt (t.norm, R - 16.f)).toNearestInt(),
                    juce::Justification::centred);
    }

    g.setFont (juce::FontOptions (15.f, juce::Font::bold));
    g.setColour (juce::Colour (0xff2b2418));
    g.drawText (gr ? "GR dB" : "VU", juce::Rectangle<float> (60.f, 18.f).withCentre ({ px, face.getBottom() - 30.f }).toNearestInt(),
                juce::Justification::centred);
    g.setFont (8.5f);
    g.drawText (gr ? "GAIN REDUCTION" : "OUTPUT LEVEL", face.toNearestInt().reduced (6, 3), juce::Justification::bottomLeft);

    const auto tip = pt (needle, R + 4.f);
    g.setColour (juce::Colour (0xff151515));
    g.drawLine (px, py, tip.x, tip.y, 1.6f);
    g.fillEllipse (px - 13.f, py - 13.f, 26.f, 26.f);
    g.restoreState();
}

//==============================================================================
std::unique_ptr<ChannelEditor::Cell> ChannelEditor::makeCell (const CtlSpec& c, juce::Colour knob, juce::Colour pointer)
{
    auto cell = std::make_unique<Cell>();
    cell->only = c.only;
    auto* param = proc.apvts.getParameter (c.id);
    jassert (param != nullptr);
    if (param == nullptr) return cell;
    const juce::String tip = param->getName (64);

    if (auto* boolP = dynamic_cast<juce::AudioParameterBool*> (param))
    {
        auto tb = std::make_unique<juce::ToggleButton> (c.label);
        cell->bAtt = std::make_unique<juce::ButtonParameterAttachment> (*boolP, *tb);
        tb->setTooltip (tip);
        cell->comp = std::move (tb);
        return cell;
    }

    cell->label = std::make_unique<juce::Label>();
    cell->label->setText (c.label, juce::dontSendNotification);
    cell->label->setJustificationType (juce::Justification::centred);
    cell->label->setFont (juce::FontOptions (11.f));
    cell->label->setColour (juce::Label::textColourId, juce::Colour (0xff9aa0ab));
    cell->label->setInterceptsMouseClicks (false, false);

    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param);
    if (choice != nullptr && c.kind == 'c')
    {
        auto cb = std::make_unique<juce::ComboBox>();
        cb->addItemList (choice->choices, 1);
        cell->cAtt = std::make_unique<juce::ComboBoxParameterAttachment> (*choice, *cb);
        cb->setTooltip (tip);
        cell->comp = std::move (cb);
    }
    else if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
    {
        auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow);
        s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 13);
        s->setColour (juce::Slider::rotarySliderFillColourId, c.body != 0 ? juce::Colour (c.body) : knob);
        s->setColour (juce::Slider::thumbColourId, c.ptr != 0 ? juce::Colour (c.ptr) : pointer);
        s->setColour (juce::Slider::trackColourId, c.cap != 0 ? juce::Colour (c.cap) : juce::Colours::transparentBlack);
        cell->sAtt = std::make_unique<juce::SliderParameterAttachment> (*ranged, *s);
        s->setTooltip (tip);
        cell->comp = std::move (s);
    }
    return cell;
}

void ChannelEditor::addCellToEditor (Cell& c)
{
    if (c.comp)  addAndMakeVisible (*c.comp);
    if (c.label) addAndMakeVisible (*c.label);
}

ChannelEditor::ChannelEditor (ChannelProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), vu (p)
{
    setLookAndFeel (&laf);

    addAndMakeVisible (vu);

    meterLabel.setText ("METER", juce::dontSendNotification);
    meterLabel.setFont (juce::FontOptions (10.f, juce::Font::bold));
    meterLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0ab));
    addAndMakeVisible (meterLabel);
    if (auto* mm = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("meter_mode")))
    {
        meterBox.addItemList (mm->choices, 1);
        meterAtt = std::make_unique<juce::ComboBoxParameterAttachment> (*mm, meterBox);
    }
    addAndMakeVisible (meterBox);

    bypassCell = makeCell ({ "bypass", "Bypass", 't', -1 }, juce::Colours::white, juce::Colours::black);
    addCellToEditor (*bypassCell);

    presetLabel.setText ("PRESET", juce::dontSendNotification);
    presetLabel.setFont (juce::FontOptions (10.f, juce::Font::bold));
    presetLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa0ab));
    addAndMakeVisible (presetLabel);
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);
    auto step = [this] (int dir)
    {
        const int cnt = proc.getNumPrograms();
        presetBox.setSelectedId ((presetBox.getSelectedId() - 1 + dir + cnt) % cnt + 1, juce::sendNotificationSync);
    };
    prevBtn.onClick = [step] { step (-1); };
    nextBtn.onClick = [step] { step (1); };
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);

    const juce::Colour cream (0xffe9e4d6), ink (0xff111111);
    static const CtlSpec globals[] = { { "in_gain", "Input", 'k', -1 }, { "out_gain", "Output", 'k', -1 },
                                       { "mix", "Mix", 'k', -1 } };
    for (auto& ctl : globals)
    {
        globalCells.push_back (makeCell (ctl, cream, ink));
        addCellToEditor (*globalCells.back());
    }

    for (int i = 0; i < 7; ++i)
    {
        if (auto* sp = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("slot" + juce::String (i + 1))))
        {
            slotBox[i].addItemList (sp->choices, 1);
            slotAtt.push_back (std::make_unique<juce::ComboBoxParameterAttachment> (*sp, slotBox[i]));
        }
        addAndMakeVisible (slotBox[i]);
    }

    panels.reserve (specs().size());
    for (auto& sp : specs())
    {
        Panel pn;
        pn.title  = sp.title;
        pn.accent = juce::Colour (sp.accent);
        pn.cols   = sp.cols;
        pn.on = makeCell ({ sp.onId, "On", 't', -1 }, juce::Colour (sp.knob), juce::Colour (sp.pointer));
        if (sp.typeId != nullptr)
            pn.type = makeCell ({ sp.typeId, "Type", 'c', -1 }, juce::Colour (sp.knob), juce::Colour (sp.pointer));
        for (auto& c : sp.ctls)
            pn.cells.push_back (makeCell (c, juce::Colour (sp.knob), juce::Colour (sp.pointer)));

        addCellToEditor (*pn.on);
        if (pn.type) addCellToEditor (*pn.type);
        for (auto& c : pn.cells) addCellToEditor (*c);
        panels.push_back (std::move (pn));
    }

    setSize (W, 400);
    relayout();
    startTimerHz (10);
}

ChannelEditor::~ChannelEditor() { setLookAndFeel (nullptr); }

void ChannelEditor::timerCallback()
{
    juce::String sig;
    for (int s = 1; s <= 7; ++s) sig << (int) proc.apvts.getRawParameterValue ("slot" + juce::String (s))->load() << ",";
    sig << (int) proc.apvts.getRawParameterValue ("comp_type")->load();
    if (sig != lastSignature) relayout();
}

// Panels are shown in signal-chain order. A module that is not in any slot is hidden, and the
// compressor shows only the controls of the selected type (FET or Opto). The window height follows.
void ChannelEditor::relayout()
{
    juce::String sig;
    std::vector<int> order;
    unsigned used = 0;
    for (int s = 1; s <= 7; ++s)
    {
        const int m = (int) proc.apvts.getRawParameterValue ("slot" + juce::String (s))->load();
        sig << m << ",";
        if (m < 0 || m > 6 || (used & (1u << m))) continue;
        used |= (1u << m);
        order.push_back (m);
    }
    const int type = (int) proc.apvts.getRawParameterValue ("comp_type")->load();
    sig << type;
    lastSignature = sig;

    for (auto& p : panels) p.visible = false;

    int x = pad, y = topH, rowH = 0;
    for (int m : order)
    {
        auto& p = panels[(size_t) m];
        int n = 0;
        for (auto& c : p.cells)
        {
            const bool show = (c->only < 0 || c->only == type);
            c->visIndex = show ? n++ : -1;
        }
        p.layoutCols = juce::jmax (1, juce::jmin (p.cols, n));
        const int rows = (n + p.layoutCols - 1) / p.layoutCols;
        const int w = juce::jmax (p.layoutCols * cellW + 2 * ppad, p.type ? 312 : 150);   // header needs room for title, type menu and On
        const int h = headerH + rows * cellH + ppad;

        if (x + w > W - pad && x > pad) { x = pad; y += rowH + gap; rowH = 0; }
        p.bounds = { x, y, w, h };
        p.visible = true;
        x += w + gap;
        rowH = juce::jmax (rowH, h);
    }

    for (auto& p : panels)
    {
        p.on->comp->setVisible (p.visible);
        if (p.type) p.type->comp->setVisible (p.visible);
        for (auto& c : p.cells)
        {
            const bool show = p.visible && c->visIndex >= 0;
            c->comp->setVisible (show);
            if (c->label) c->label->setVisible (show);
        }
    }

    const int totalH = (order.empty() ? topH : y + rowH) + pad;
    if (totalH != getHeight()) setSize (W, totalH);
    else resized();
}

void ChannelEditor::resized()
{
    vu.setBounds (240, 10, 250, 120);
    meterLabel.setBounds (508, 24, 130, 14);
    meterBox.setBounds (508, 40, 140, 24);
    bypassCell->comp->setBounds (12, 98, 90, 22);
    presetLabel.setBounds (508, 72, 140, 14);
    prevBtn.setBounds (508, 88, 24, 24);
    presetBox.setBounds (536, 88, 112, 24);
    nextBtn.setBounds (652, 88, 24, 24);

    int gx = W - pad - 3 * cellW;
    for (auto& g : globalCells)
    {
        placeCell (g->comp.get(), g->label.get(), gx, 18);
        gx += cellW;
    }

    for (int i = 0; i < 7; ++i)
        slotBox[i].setBounds (84 + i * 126, 160, 120, 26);

    for (auto& p : panels)
    {
        if (! p.visible) continue;
        const auto& b = p.bounds;
        p.on->comp->setBounds (b.getRight() - ppad - 52, b.getY() + 5, 52, 20);
        if (p.type) p.type->comp->setBounds (b.getX() + ppad + 104, b.getY() + 4, 124, 22);

        for (auto& c : p.cells)
        {
            if (c->visIndex < 0) continue;
            const int col = c->visIndex % p.layoutCols, row = c->visIndex / p.layoutCols;
            placeCell (c->comp.get(), c->label.get(), b.getX() + ppad + col * cellW, b.getY() + headerH + row * cellH);
        }
    }
}

void ChannelEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (24.f, juce::Font::bold));
    g.drawText ("SUBURBAN", 16, 22, 220, 30, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff4d9bff));
    g.drawText ("CHANNEL", 16, 50, 220, 30, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff6c727d));
    g.setFont (11.f);
    g.drawText ("channel strip", 16, 82, 220, 16, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff9aa0ab));
    g.setFont (juce::FontOptions (10.f, juce::Font::bold));
    g.drawText ("SIGNAL CHAIN", pad, 160, 74, 26, juce::Justification::centredLeft);
    g.setFont (10.f);
    for (int i = 0; i < 7; ++i)
        g.drawText (juce::String (i + 1), slotBox[i].getX(), 146, slotBox[i].getWidth(), 14, juce::Justification::centred);

    for (auto& p : panels)
    {
        if (! p.visible) continue;
        auto b = p.bounds.toFloat();
        g.setColour (juce::Colour (0xff121214));
        g.fillRoundedRectangle (b, 6.f);
        g.setColour (juce::Colour (0xff2a2c31));
        g.drawRoundedRectangle (b.reduced (0.5f), 6.f, 1.f);
        juce::Colour stripe = juce::Colours::transparentBlack;
        if (&p == &panels[1])      stripe = (int) proc.apvts.getRawParameterValue ("comp_type")->load() == 0 ? juce::Colour (0xff2f6fd6)
                                                                                                           : juce::Colour (0xff8f9298);
        else if (&p == &panels[4]) stripe = juce::Colour (0xff5f7f8f);
        if (stripe.getAlpha() > 0)
        {
            g.setColour (stripe);
            g.fillRoundedRectangle (b.getX() + 1.f, b.getY() + 1.f, b.getWidth() - 2.f, 3.f, 1.5f);
        }
        g.setColour (p.accent);
        g.setFont (juce::FontOptions (13.f, juce::Font::bold));
        g.drawText (p.title, p.bounds.getX() + ppad, p.bounds.getY() + 4, 100, 22, juce::Justification::centredLeft);
    }
}

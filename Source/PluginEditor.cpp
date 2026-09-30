#include "PluginEditor.h"

namespace
{
const juce::Colour bg (0xff151518);
const juce::Colour panel (0xff212127);
const juce::Colour panelEdge (0xff2e2e36);
const juce::Colour textDim (0xff9a9aa6);
const juce::Colour cDrive (0xffff6b3d), cCrush (0xffffc53d), cFilter (0xff3dd6ff),
                   cTape (0xffb57bff), cDelay (0xff4dff9a), cMaster (0xffe8e8ee);
} // namespace

//==============================================================================
SmadderLookAndFeel::SmadderLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2b2b33));
    setColour (juce::ComboBox::outlineColourId, panelEdge);
    setColour (juce::ComboBox::textColourId, juce::Colours::white);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff24242b));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff3a3a46));
    setColour (juce::Label::textColourId, textDim);
}

void SmadderLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider& s)
{
    const auto colour = s.findColour (juce::Slider::rotarySliderFillColourId);
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float track = juce::jmax (3.0f, radius * 0.12f);

    // Background track
    juce::Path bgArc;
    bgArc.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff34343d));
    g.strokePath (bgArc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc
    juce::Path valArc;
    valArc.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f, startAngle, angle, true);
    g.setColour (colour);
    g.strokePath (valArc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Knob body
    const float bodyR = radius - track * 2.4f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b3b45), centre.x, centre.y - bodyR,
                                             juce::Colour (0xff1c1c21), centre.x, centre.y + bodyR, false));
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);

    // Pointer
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -bodyR + 3.0f, 3.0f, bodyR * 0.45f, 1.5f);
    g.setColour (colour.brighter (0.3f));
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void SmadderLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const auto colour = b.findColour (juce::ToggleButton::tickColourId);
    const bool on = b.getToggleState();
    g.setColour (on ? colour.withAlpha (0.25f) : juce::Colour (0xff2b2b33).brighter (highlighted ? 0.08f : 0.0f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (on ? colour : panelEdge);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    g.setColour (on ? juce::Colours::white : textDim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

//==============================================================================
SmadderEditor::Knob* SmadderEditor::addKnob (const juce::String& paramId, const juce::String& text, juce::Colour colour)
{
    auto k = std::make_unique<Knob>();
    k->colour = colour;
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    k->slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k->slider.setColour (juce::Slider::textBoxHighlightColourId, colour.withAlpha (0.4f));
    k->slider.setPopupDisplayEnabled (false, false, nullptr);
    k->label.setText (text, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k->slider);
    knobs.push_back (std::move (k));
    return knobs.back().get();
}

SmadderEditor::SmadderEditor (SmadderProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    Section drive { "DRIVE", cDrive, { addKnob ("drive", "Drive", cDrive) }, {} };
    Section crush { "CRUSH", cCrush, { addKnob ("bits", "Bits", cCrush), addKnob ("downsample", "Downsample", cCrush) }, {} };
    Section filter { "FILTER", cFilter, { addKnob ("cutoff", "Cutoff", cFilter), addKnob ("reso", "Resonance", cFilter),
                                         addKnob ("lfoRate", "LFO Rate", cFilter), addKnob ("lfoDepth", "LFO Depth", cFilter) }, {} };
    Section tape { "TAPE", cTape, { addKnob ("wow", "Wow/Flutter", cTape), addKnob ("noise", "Noise", cTape) }, {} };

    delayMsKnob = addKnob ("delayMs", "Time", cDelay);
    delayDivKnob = addKnob ("delayDiv", "Time", cDelay);
    Section delay { "DELAY", cDelay, { delayMsKnob, addKnob ("feedback", "Feedback", cDelay),
                                       addKnob ("delayTone", "Tone", cDelay), addKnob ("delayMix", "Mix", cDelay) }, {} };
    Section master { "MASTER", cMaster, { addKnob ("mix", "Dry/Wet", cMaster), addKnob ("output", "Output", cMaster) }, {} };

    sections = { drive, crush, filter, tape, delay, master };

    filterType.addItemList (proc.apvts.getParameter ("filterType")->getAllValueStrings(), 1);
    addAndMakeVisible (filterType);
    filterTypeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "filterType", filterType);

    for (auto* b : { &syncButton, &pingPongButton })
    {
        b->setColour (juce::ToggleButton::tickColourId, cDelay);
        addAndMakeVisible (*b);
    }
    syncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "delaySync", syncButton);
    pingPongAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "pingPong", pingPongButton);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        if (presetBox.getSelectedId() > 0)
            proc.loadPreset (presetBox.getSelectedId() - 1);
    };
    addAndMakeVisible (presetBox);

    setSize (880, 540);
    timerCallback();
    startTimerHz (15);
}

SmadderEditor::~SmadderEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SmadderEditor::timerCallback()
{
    const bool sync = proc.apvts.getRawParameterValue ("delaySync")->load() > 0.5f;
    delayDivKnob->slider.setVisible (sync);
    delayDivKnob->label.setVisible (sync);
    delayMsKnob->slider.setVisible (! sync);
    delayMsKnob->label.setVisible (! sync);

    if (! presetBox.isPopupActive() && presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

void SmadderEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    // Header
    auto header = getLocalBounds().removeFromTop (60).reduced (20, 0);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("SMADDER", header.removeFromLeft (200), juce::Justification::centredLeft);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("multi-fx  /  drive  crush  filter  tape  delay", header.removeFromLeft (320), juce::Justification::centredLeft);
    g.drawText ("Preset", presetBox.getBounds().translated (-60, 0).withWidth (54), juce::Justification::centredRight);

    // Accent strip
    const juce::Colour strip[] = { cDrive, cCrush, cFilter, cTape, cDelay };
    const float sw = (float) getWidth() / 5.0f;
    for (int i = 0; i < 5; ++i)
    {
        g.setColour (strip[i]);
        g.fillRect ((float) i * sw, 0.0f, sw, 3.0f);
    }

    for (auto& s : sections)
    {
        auto r = s.bounds.toFloat();
        g.setColour (panel);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
        g.setColour (s.colour);
        g.fillRoundedRectangle (r.getX() + 14.0f, r.getY() + 15.0f, 8.0f, 8.0f, 2.0f);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (s.title, (int) r.getX() + 30, (int) r.getY() + 8, 120, 22, juce::Justification::centredLeft);
    }
}

void SmadderEditor::layoutSection (Section& s, juce::Rectangle<int> r)
{
    s.bounds = r;
    auto area = r.reduced (10, 8);
    area.removeFromTop (32);
    const int n = (int) s.knobs.size();
    const int w = area.getWidth() / n;
    for (int i = 0; i < n; ++i)
    {
        auto cell = area.removeFromLeft (w);
        auto labelArea = cell.removeFromTop (18);
        s.knobs[(size_t) i]->label.setBounds (labelArea);
        s.knobs[(size_t) i]->slider.setBounds (cell.reduced (4, 0));
    }
}

void SmadderEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (60).reduced (20, 14);
    presetBox.setBounds (header.removeFromRight (190));

    r.reduce (14, 0);
    r.removeFromBottom (14);
    const int gap = 10;
    auto row1 = r.removeFromTop ((r.getHeight() - gap) / 2);
    r.removeFromTop (gap);
    auto row2 = r;

    auto layoutRow = [this, gap] (juce::Rectangle<int> row, std::initializer_list<int> idx)
    {
        int total = 0;
        for (int i : idx) total += (int) sections[(size_t) i].knobs.size();
        const int avail = row.getWidth() - gap * ((int) idx.size() - 1);
        int count = 0;
        for (int i : idx)
        {
            const int w = avail * (int) sections[(size_t) i].knobs.size() / total;
            layoutSection (sections[(size_t) i], row.removeFromLeft (w));
            if (++count < (int) idx.size()) row.removeFromLeft (gap);
        }
    };

    layoutRow (row1, { 0, 1, 2 });
    layoutRow (row2, { 3, 4, 5 });

    // Delay time: the division knob shares the ms knob's spot
    delayDivKnob->slider.setBounds (delayMsKnob->slider.getBounds());
    delayDivKnob->label.setBounds (delayMsKnob->label.getBounds());

    // Extra controls in section title rows
    auto filterTitle = sections[2].bounds.reduced (10, 8).removeFromTop (26);
    filterType.setBounds (filterTitle.removeFromRight (120));

    auto delayTitle = sections[4].bounds.reduced (10, 8).removeFromTop (26);
    pingPongButton.setBounds (delayTitle.removeFromRight (86));
    delayTitle.removeFromRight (6);
    syncButton.setBounds (delayTitle.removeFromRight (60));
}

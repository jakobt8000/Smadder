#pragma once

#include "PluginProcessor.h"

class SmadderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SmadderLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

class SmadderEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SmadderEditor (SmadderProcessor&);
    ~SmadderEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        juce::Colour colour;
    };

    struct Section
    {
        juce::String title;
        juce::Colour colour;
        std::vector<Knob*> knobs;
        juce::Rectangle<int> bounds;
    };

    Knob* addKnob (const juce::String& paramId, const juce::String& text, juce::Colour colour);
    void timerCallback() override;
    void layoutSection (Section&, juce::Rectangle<int>);

    SmadderProcessor& proc;
    SmadderLookAndFeel lnf;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<Section> sections;

    Knob* delayMsKnob = nullptr;
    Knob* delayDivKnob = nullptr;

    juce::ComboBox filterType, presetBox;
    juce::ToggleButton syncButton { "Sync" }, pingPongButton { "Ping-Pong" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterTypeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAtt, pingPongAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmadderEditor)
};

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace dm7ui
{
const juce::Colour bg        { 0xff141518 };
const juce::Colour panel     { 0xff1e2024 };
const juce::Colour panelEdge { 0xff2c2f35 };
const juce::Colour text      { 0xffe9e6df };
const juce::Colour dim       { 0xff8b8d93 };
const juce::Colour accent    { 0xffe8a33d };
const juce::Colour accentDim { 0xff5a4426 };

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

// Slide switch modelled on the mic's rear-panel switches (icon shows the curve)
class CurveSwitch : public juce::ToggleButton
{
public:
    enum class Kind { bassRolloff, presence };
    CurveSwitch (const juce::String& name, Kind k) : juce::ToggleButton (name), kind (k) {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    Kind kind;
    void drawIcon (juce::Graphics&, juce::Rectangle<float>, bool engaged);
};

class ResponseGraph : public juce::Component, private juce::Timer
{
public:
    explicit ResponseGraph (DM7VoiceProcessor& p);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void rebuild();
    DM7VoiceProcessor& proc;
    std::vector<float> curveDb;
    juce::String lastKey;
};
} // namespace dm7ui

class DM7VoiceEditor : public juce::AudioProcessorEditor
{
public:
    explicit DM7VoiceEditor (DM7VoiceProcessor&);
    ~DM7VoiceEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const char* paramId, const juce::String& title);

    DM7VoiceProcessor& proc;
    dm7ui::LookAndFeel lnf;
    dm7ui::ResponseGraph graph;

    Knob input, proximity, character, mix, output;
    dm7ui::CurveSwitch bassSwitch { "BASS ROLL-OFF", dm7ui::CurveSwitch::Kind::bassRolloff };
    dm7ui::CurveSwitch presenceSwitch { "PRESENCE BOOST", dm7ui::CurveSwitch::Kind::presence };
    std::unique_ptr<ButtonAttachment> bassAtt, presenceAtt;
    juce::ComboBox presetBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DM7VoiceEditor)
};

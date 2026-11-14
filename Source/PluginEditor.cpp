#include "PluginEditor.h"

using namespace juce;

namespace dm7ui
{
// ---------------------------------------------------------------- LookAndFeel
LookAndFeel::LookAndFeel()
{
    setColour (Slider::textBoxTextColourId, text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Label::textColourId, text);
    setColour (ComboBox::backgroundColourId, panel);
    setColour (ComboBox::textColourId, text);
    setColour (ComboBox::outlineColourId, panelEdge);
    setColour (ComboBox::arrowColourId, accent);
    setColour (PopupMenu::backgroundColourId, panel);
    setColour (PopupMenu::textColourId, text);
    setColour (PopupMenu::highlightedBackgroundColourId, accentDim);
    setColour (PopupMenu::highlightedTextColourId, text);
}

void LookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                    float start, float end, Slider& s)
{
    auto bounds = Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
    const float r = jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto c = bounds.getCentre();
    const float angle = start + pos * (end - start);
    const float track = 4.0f;

    // outer track
    Path bgArc;
    bgArc.addCentredArc (c.x, c.y, r - track, r - track, 0, start, end, true);
    g.setColour (panelEdge);
    g.strokePath (bgArc, PathStrokeType (track, PathStrokeType::curved, PathStrokeType::rounded));

    // value arc (bipolar knobs draw from 12 o'clock)
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (start + end) / 2.0f : start;
    Path valArc;
    valArc.addCentredArc (c.x, c.y, r - track, r - track, 0, jmin (from, angle), jmax (from, angle), true);
    g.setColour (s.isEnabled() ? accent : dim);
    g.strokePath (valArc, PathStrokeType (track, PathStrokeType::curved, PathStrokeType::rounded));

    // knob body - knurled black like a mic grille cap
    const float kr = r - track * 2.6f;
    ColourGradient grad (Colour (0xff34373d), c.x, c.y - kr, Colour (0xff111214), c.x, c.y + kr, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
    g.setColour (Colour (0xff3d4047));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.0f);

    // pointer
    Path p;
    p.addRoundedRectangle (-1.5f, -kr + 4.0f, 3.0f, kr * 0.45f, 1.5f);
    g.setColour (text);
    g.fillPath (p, AffineTransform::rotation (angle).translated (c.x, c.y));
}

void LookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (box.findColour (ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (box.findColour (ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    Path arrow;
    const float ax = (float) w - 18.0f, ay = (float) h / 2.0f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (accent);
    g.fillPath (arrow);
}

Label* LookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (FontOptions (13.0f));
    l->setColour (Label::textColourId, text);
    l->setColour (Label::outlineWhenEditingColourId, accent);
    return l;
}

// ---------------------------------------------------------------- CurveSwitch
void CurveSwitch::drawIcon (Graphics& g, Rectangle<float> r, bool engaged)
{
    Path p;
    const float x0 = r.getX(), x1 = r.getRight(), mid = r.getCentreY();
    p.startNewSubPath (x0, mid);
    if (kind == Kind::bassRolloff)
    {
        // flat line that bends downward at the low end (drawn left-to-right)
        if (engaged)
        {
            p.clear();
            p.startNewSubPath (x0, r.getBottom());
            p.quadraticTo (x0 + r.getWidth() * 0.3f, mid, x0 + r.getWidth() * 0.5f, mid);
        }
        p.lineTo (x1, mid);
    }
    else
    {
        // flat line with a bump in the upper mids
        const float bx = x0 + r.getWidth() * 0.62f;
        if (engaged)
        {
            p.lineTo (bx - r.getWidth() * 0.25f, mid);
            p.quadraticTo (bx, r.getY() - 3.0f, bx + r.getWidth() * 0.22f, mid);
        }
        p.lineTo (x1, mid);
    }
    g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
}

void CurveSwitch::paintButton (Graphics& g, bool highlighted, bool)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();

    g.setColour (panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (on ? accent.withAlpha (0.8f) : (highlighted ? dim : panelEdge));
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    // label
    auto content = r.reduced (12.0f, 8.0f);
    auto labelArea = content.removeFromTop (16.0f);
    g.setColour (on ? dm7ui::text : dim);
    g.setFont (FontOptions (12.0f, Font::bold));
    g.drawText (getButtonText(), labelArea, Justification::centredLeft);

    // slide switch: two curve icons (off / on) with the slider under the active one
    content.removeFromTop (6.0f);
    auto sw = content.removeFromLeft (92.0f).withSizeKeepingCentre (92.0f, 28.0f);
    g.setColour (Colour (0xff0d0e10));
    g.fillRoundedRectangle (sw, 5.0f);
    auto leftHalf = sw.withWidth (sw.getWidth() / 2.0f).reduced (3.0f);
    auto rightHalf = leftHalf.translated (sw.getWidth() / 2.0f, 0.0f);
    auto thumb = on ? rightHalf : leftHalf;
    g.setColour (on ? accent : Colour (0xff4a4d55));
    g.fillRoundedRectangle (thumb, 4.0f);

    g.setColour (on ? dim : dm7ui::text);
    drawIcon (g, leftHalf.reduced (8.0f, 7.0f), false);
    g.setColour (on ? Colour (0xff141518) : dim);
    drawIcon (g, rightHalf.reduced (8.0f, 7.0f), true);

    g.setColour (on ? accent : dim);
    g.setFont (FontOptions (11.0f));
    g.drawText (on ? (kind == Kind::bassRolloff ? "Low end tamed" : "Upper mids lifted")
                   : "Flat",
                content.withTrimmedLeft (12.0f), Justification::centredLeft);
}

// ---------------------------------------------------------------- ResponseGraph
ResponseGraph::ResponseGraph (DM7VoiceProcessor& p) : proc (p)
{
    rebuild();
    startTimerHz (30);
}

void ResponseGraph::timerCallback()
{
    auto& s = proc.apvts;
    String key;
    for (auto* id : { ParamID::proximity, ParamID::presence, ParamID::bassCut, ParamID::mix })
        key << s.getRawParameterValue (id)->load() << ";";
    if (key != lastKey)
    {
        lastKey = key;
        rebuild();
        repaint();
    }
}

void ResponseGraph::rebuild()
{
    double fs = 48000.0, mix = 1.0;
    const auto chain = proc.getDisplayChain (fs, mix);
    constexpr int points = 240;
    curveDb.resize (points);
    for (int i = 0; i < points; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, i / double (points - 1));
        curveDb[(size_t) i] = f < fs * 0.5 ? (float) dm7::chainMagnitudeDb (chain, f, fs, mix) : -60.0f;
    }
}

static float freqToX (float f, Rectangle<float> r)
{
    return r.getX() + r.getWidth() * (float) (std::log10 (f / 20.0f) / 3.0f);
}

void ResponseGraph::paint (Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (area, 10.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (area.reduced (0.5f), 10.0f, 1.0f);

    auto plot = area.reduced (36.0f, 14.0f).withTrimmedBottom (8.0f);
    const float range = 15.0f;
    auto dbToY = [&] (float db) { return jmap (db, -range, range, plot.getBottom(), plot.getY()); };

    g.setFont (FontOptions (10.0f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
    {
        const float x = freqToX (f, plot);
        g.setColour (panelEdge);
        g.drawVerticalLine (roundToInt (x), plot.getY(), plot.getBottom());
        g.setColour (dim);
        const String label = f >= 1000.0f ? String (f / 1000.0f, 0) + "k" : String ((int) f);
        g.drawText (label, Rectangle<float> (x - 20, plot.getBottom() + 2, 40, 12), Justification::centred);
    }
    for (float db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
    {
        const float y = dbToY (db);
        g.setColour (std::abs (db) < 0.01f ? dim.withAlpha (0.5f) : panelEdge);
        g.drawHorizontalLine (roundToInt (y), plot.getX(), plot.getRight());
        g.setColour (dim);
        g.drawText ((db > 0 ? "+" : "") + String ((int) db), Rectangle<float> (area.getX() + 4, y - 6, 28, 12),
                    Justification::centredRight);
    }

    if (curveDb.empty())
        return;

    Path curve, fill;
    for (size_t i = 0; i < curveDb.size(); ++i)
    {
        const float x = plot.getX() + plot.getWidth() * (float) i / (float) (curveDb.size() - 1);
        const float y = dbToY (jlimit (-range - 3.0f, range, curveDb[i]));
        if (i == 0) { curve.startNewSubPath (x, y); fill.startNewSubPath (x, dbToY (0)); }
        curve.lineTo (x, y);
        fill.lineTo (x, y);
    }
    fill.lineTo (plot.getRight(), dbToY (0));
    fill.closeSubPath();

    g.saveState();
    g.reduceClipRegion (plot.toNearestInt());
    g.setGradientFill (ColourGradient (accent.withAlpha (0.28f), 0, plot.getY(),
                                       accent.withAlpha (0.02f), 0, plot.getBottom(), false));
    g.fillPath (fill);
    g.setColour (accent);
    g.strokePath (curve, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));
    g.restoreState();

    g.setColour (dim);
    g.setFont (FontOptions (10.0f, Font::bold));
    g.drawText ("TONE CURVE", plot.reduced (6.0f, 4.0f), Justification::topLeft);
}
} // namespace dm7ui

// ---------------------------------------------------------------- Editor
DM7VoiceEditor::DM7VoiceEditor (DM7VoiceProcessor& p)
    : AudioProcessorEditor (&p), proc (p), graph (p)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (graph);
    setupKnob (input, ParamID::input, "INPUT");
    setupKnob (proximity, ParamID::proximity, "PROXIMITY");
    setupKnob (character, ParamID::character, "CHARACTER");
    setupKnob (mix, ParamID::mix, "MIX");
    setupKnob (output, ParamID::output, "OUTPUT");

    proximity.slider.setTooltip ("How close you are to the grille: more bass and warmth as you get closer");
    character.slider.setTooltip ("Preamp drive: thickness and gentle saturation");

    addAndMakeVisible (bassSwitch);
    addAndMakeVisible (presenceSwitch);
    bassAtt = std::make_unique<ButtonAttachment> (proc.apvts, ParamID::bassCut, bassSwitch);
    presenceAtt = std::make_unique<ButtonAttachment> (proc.apvts, ParamID::presence, presenceSwitch);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    setSize (800, 480);
}

DM7VoiceEditor::~DM7VoiceEditor() { setLookAndFeel (nullptr); }

void DM7VoiceEditor::setupKnob (Knob& k, const char* paramId, const String& title)
{
    k.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 80, 18);
    k.slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    k.slider.setDoubleClickReturnValue (true, proc.apvts.getParameter (paramId)->convertFrom0to1 (
                                                  proc.apvts.getParameter (paramId)->getDefaultValue()));
    k.slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    k.slider.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    k.label.setText (title, dontSendNotification);
    k.label.setJustificationType (Justification::centred);
    k.label.setFont (FontOptions (12.0f, Font::bold));
    k.label.setColour (Label::textColourId, dm7ui::dim);
    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
    k.attachment = std::make_unique<SliderAttachment> (proc.apvts, paramId, k.slider);
}

void DM7VoiceEditor::paint (Graphics& g)
{
    g.fillAll (dm7ui::bg);

    auto header = getLocalBounds().removeFromTop (64).reduced (24, 0);
    g.setColour (dm7ui::text);
    g.setFont (FontOptions (26.0f, Font::bold));
    g.drawText ("DM7 VOICE", header.removeFromTop (40).withTrimmedTop (12), Justification::bottomLeft);
    g.setColour (dm7ui::dim);
    g.setFont (FontOptions (11.0f));
    g.drawText (String (CharPointer_UTF8 ("DYNAMIC BROADCAST MIC TONE  \xc2\xb7  SM7B STYLE")), header, Justification::topLeft);

    g.setColour (dm7ui::accent);
    g.fillRect (24, 63, 36, 2);

    g.setColour (dm7ui::dim);
    g.setFont (FontOptions (10.0f));
    g.drawText ("MAYWORKS", getLocalBounds().removeFromBottom (22).reduced (24, 0), Justification::centredRight);
}

void DM7VoiceEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (72);
    presetBox.setBounds (header.removeFromRight (240).reduced (24, 20).withTrimmedTop (4));

    r.removeFromBottom (22);
    r.reduce (20, 0);
    graph.setBounds (r.removeFromTop (190));
    r.removeFromTop (16);

    auto switches = r.removeFromRight (230);
    auto sw1 = switches.removeFromTop (switches.getHeight() / 2).reduced (0, 4);
    bassSwitch.setBounds (sw1);
    presenceSwitch.setBounds (switches.reduced (0, 4));

    r.removeFromRight (12);
    const int kw = r.getWidth() / 5;
    for (auto* k : { &input, &proximity, &character, &mix, &output })
    {
        auto col = r.removeFromLeft (kw);
        auto unit = col.withSizeKeepingCentre (col.getWidth(), jmin (col.getHeight(), col.getWidth() + 34));
        k->label.setBounds (unit.removeFromTop (18));
        k->slider.setBounds (unit.reduced (4, 0));
    }
}

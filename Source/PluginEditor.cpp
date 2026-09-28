#include "PluginEditor.h"

using namespace juce;

static Font uiFont (float size, bool bold = false)
{
    return Font (FontOptions (size, bold ? Font::bold : Font::plain));
}

//==============================================================================
// Look & feel
VaultLookAndFeel::VaultLookAndFeel()
{
    setColour (ComboBox::backgroundColourId, Pal::track);
    setColour (ComboBox::outlineColourId, Pal::panelEdge);
    setColour (ComboBox::textColourId, Pal::cream);
    setColour (ComboBox::arrowColourId, Pal::amber);
    setColour (PopupMenu::backgroundColourId, Pal::panel);
    setColour (PopupMenu::textColourId, Pal::cream);
    setColour (PopupMenu::highlightedBackgroundColourId, Pal::amberDim);
    setColour (PopupMenu::highlightedTextColourId, Pal::cream);
    setColour (TextButton::buttonColourId, Pal::track);
    setColour (TextButton::buttonOnColourId, Pal::amberDim);
    setColour (TextButton::textColourOffId, Pal::amber);
    setColour (TextButton::textColourOnId, Pal::cream);
    setColour (Slider::textBoxTextColourId, Pal::cream);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId, Pal::amberDim);
    setColour (Label::textWhenEditingColourId, Pal::cream);
    setColour (TextEditor::backgroundColourId, Pal::track);
    setColour (TextEditor::highlightColourId, Pal::amberDim);
    setColour (CaretComponent::caretColourId, Pal::amber);
}

void VaultLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, Slider& s)
{
    const auto bounds = Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float arcR = radius - 3.0f;
    const bool hot = s.isMouseOverOrDragging();

    // Track + value arc
    Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Pal::track);
    g.strokePath (track, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));

    Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (hot ? Pal::amber.brighter (0.15f) : Pal::amber);
    g.strokePath (value, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // Knob body
    const float bodyR = arcR - 8.0f;
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR + 2.5f, bodyR * 2, bodyR * 2);   // shadow

    ColourGradient grad (Pal::knobBody.brighter (0.35f), centre.x, centre.y - bodyR,
                         Pal::knobBody.darker (0.45f), centre.x, centre.y + bodyR, false);
    g.setGradientFill (grad);
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2, bodyR * 2);
    g.setColour (Pal::panelEdge.brighter (0.2f));
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2, bodyR * 2, 1.0f);

    // Pointer
    Path pointer;
    pointer.addRoundedRectangle (-1.5f, -bodyR + 3.0f, 3.0f, bodyR * 0.5f, 1.5f);
    g.setColour (Pal::cream);
    g.fillPath (pointer, AffineTransform::rotation (angle).translated (centre));
}

Label* VaultLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (uiFont (12.5f));
    l->setJustificationType (Justification::centred);
    l->setColour (Label::textColourId, Pal::cream);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    return l;
}

Font VaultLookAndFeel::getComboBoxFont (ComboBox&) { return uiFont (14.0f); }
Font VaultLookAndFeel::getPopupMenuFont() { return uiFont (14.0f); }

//==============================================================================
// Knob
Knob::Knob (AudioProcessorValueTreeState& state, const char* paramId, const String& t)
    : title (t.toUpperCase())
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 90, 18);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (220);
    slider.setVelocityBasedMode (false);
    addAndMakeVisible (slider);

    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);

    if (auto* p = state.getParameter (paramId))
    {
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        slider.setTooltip (p->getName (64) + "  (double-click to reset)");
    }
}

void Knob::resized()
{
    slider.setBounds (getLocalBounds().withTrimmedTop (18));
}

void Knob::paint (Graphics& g)
{
    g.setColour (Pal::dim);
    g.setFont (uiFont (11.5f, true).withExtraKerningFactor (0.08f));
    g.drawText (title, getLocalBounds().removeFromTop (16), Justification::centred);
}

//==============================================================================
// Room type selector (2 x 2)
static const char* kTypeNames[] = { "CHAMBER", "PLATE", "ROOM", "HALL" };
static const char* kTypeDesc[]  = { "Warm, dense echo chamber", "Bright, smooth studio plate",
                                    "Natural, tight live room", "Big, lush concert hall" };

TypeSelector::TypeSelector (RangedAudioParameter& p)
    : param (p),
      attachment (p, [this] (float v) { selected = roundToInt (v); repaint(); }, nullptr)
{
    attachment.sendInitialUpdate();
    setMouseCursor (MouseCursor::PointingHandCursor);
}

Rectangle<float> TypeSelector::cell (int i) const
{
    const auto b = getLocalBounds().toFloat();
    const float gap = 8.0f;
    const float cw = (b.getWidth() - gap) * 0.5f, ch = (b.getHeight() - gap) * 0.5f;
    return { b.getX() + (float) (i % 2) * (cw + gap), b.getY() + (float) (i / 2) * (ch + gap), cw, ch };
}

int TypeSelector::hitTest (Point<float> p) const
{
    for (int i = 0; i < 4; ++i)
        if (cell (i).contains (p)) return i;
    return -1;
}

void TypeSelector::paint (Graphics& g)
{
    for (int i = 0; i < 4; ++i)
    {
        const auto r = cell (i);
        const bool on = i == selected;
        g.setColour (on ? Pal::amber.withAlpha (0.16f) : (i == hover ? Pal::track.brighter (0.08f) : Pal::track));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (on ? Pal::amber : Pal::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, on ? 1.6f : 1.0f);

        // indicator lamp
        const auto lamp = Rectangle<float> (7.0f, 7.0f).withCentre ({ r.getX() + 14.0f, r.getY() + 16.0f });
        g.setColour (on ? Pal::amber : Pal::panelEdge.brighter (0.2f));
        g.fillEllipse (lamp);
        if (on) { g.setColour (Pal::amber.withAlpha (0.25f)); g.fillEllipse (lamp.expanded (4.0f)); }

        g.setColour (on ? Pal::cream : Pal::dim);
        g.setFont (uiFont (15.0f, true).withExtraKerningFactor (0.1f));
        g.drawText (kTypeNames[i], r.withTrimmedLeft (26.0f).withHeight (32.0f), Justification::centredLeft);
        g.setColour (Pal::dim.withAlpha (on ? 1.0f : 0.7f));
        g.setFont (uiFont (11.0f));
        g.drawFittedText (kTypeDesc[i], r.withTrimmedLeft (10.0f).withTrimmedRight (6.0f).withTrimmedTop (30.0f).toNearestInt(),
                          Justification::topLeft, 2);
    }
}

void TypeSelector::mouseDown (const MouseEvent& e)
{
    const int i = hitTest (e.position);
    if (i >= 0 && i != selected)
        attachment.setValueAsCompleteGesture ((float) i);
}

void TypeSelector::mouseMove (const MouseEvent& e)
{
    const int h = hitTest (e.position);
    if (h != hover) { hover = h; repaint(); }
}

void TypeSelector::mouseExit (const MouseEvent&) { hover = -1; repaint(); }

//==============================================================================
// Decay display: shows pre-delay gap, early reflections and late decay (LF + HF)
DecayDisplay::DecayDisplay (AudioProcessorValueTreeState& s) : state (s)
{
    startTimerHz (30);
}

void DecayDisplay::timerCallback()
{
    const char* ids[] = { IDs::type, IDs::predelay, IDs::decay, IDs::size, IDs::damping, IDs::early, IDs::diffusion, IDs::mix };
    bool changed = false;
    for (size_t i = 0; i < last.size(); ++i)
    {
        const float v = state.getRawParameterValue (ids[i])->load();
        if (v != last[i]) { last[i] = v; changed = true; }
    }
    if (changed) repaint();
}

void DecayDisplay::paint (Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    g.setColour (Pal::track);
    g.fillRoundedRectangle (full, 6.0f);

    const int type = jlimit (0, 3, roundToInt (last[0]));
    const float pre = last[1] * 0.001f, decay = last[2], size = last[3] * 0.01f;
    const float damp = last[4] * 0.01f, early = last[5] * 0.01f, diff = last[6] * 0.01f;
    const auto& v = ev::getVoice (type);

    const auto area = full.reduced (12.0f, 10.0f).withTrimmedTop (18.0f).withTrimmedBottom (14.0f);
    const float total = jmax (0.4f, pre + decay * 1.08f + 0.05f);
    auto xOf = [&] (float t)  { return area.getX() + area.getWidth() * jlimit (0.0f, 1.0f, t / total); };
    auto yOf = [&] (float db) { return area.getY() + area.getHeight() * jlimit (0.0f, 1.0f, -db / 60.0f); };

    // grid + time labels
    const float steps[] = { 0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f };
    float step = 5.0f;
    for (float s : steps) if (total / s <= 7.0f) { step = s; break; }
    g.setFont (uiFont (10.0f));
    for (float t = step; t < total; t += step)
    {
        const float x = xOf (t);
        if (x > area.getRight() - 24.0f) break;
        g.setColour (Pal::panelEdge.withAlpha (0.5f));
        g.drawVerticalLine (roundToInt (x), area.getY(), area.getBottom());
        g.setColour (Pal::dim.withAlpha (0.7f));
        const String label = step < 1.0f ? String (roundToInt (t * 1000)) + " ms" : String (t, 0) + " s";
        g.drawText (label, Rectangle<float> (x - 30, area.getBottom() + 1, 60, 12), Justification::centred);
    }
    for (float db : { -20.0f, -40.0f })
    {
        g.setColour (Pal::panelEdge.withAlpha (0.35f));
        g.drawHorizontalLine (roundToInt (yOf (db)), area.getX(), area.getRight());
    }

    // late decay (LF curve, filled)
    const float peakDb = -8.0f;
    const float build = 0.012f + 0.05f * diff * (0.5f + size);
    const float tStart = pre;
    Path late;
    late.startNewSubPath (xOf (tStart), area.getBottom());
    late.lineTo (xOf (tStart), yOf (-30.0f));
    late.quadraticTo (xOf (tStart + build * 0.4f), yOf (peakDb), xOf (tStart + build), yOf (peakDb));
    late.lineTo (xOf (tStart + build + decay), yOf (peakDb - 60.0f));
    late.lineTo (xOf (tStart + build + decay), area.getBottom());
    late.closeSubPath();

    g.setGradientFill (ColourGradient (Pal::amber.withAlpha (0.45f), area.getX(), area.getY(),
                                       Pal::amber.withAlpha (0.05f), area.getX(), area.getBottom(), false));
    g.fillPath (late);
    g.setColour (Pal::amber);
    g.strokePath (late, PathStrokeType (1.5f));

    // HF decay (shorter, depends on damping)
    const float alpha = v.alphaBright + (v.alphaDark - v.alphaBright) * damp;
    Path hf;
    hf.startNewSubPath (xOf (tStart + build), yOf (peakDb));
    hf.lineTo (xOf (tStart + build + decay * alpha), yOf (peakDb - 60.0f));
    g.setColour (Pal::cream.withAlpha (0.55f));
    const float dashes[] = { 4.0f, 3.0f };
    PathStrokeType (1.2f).createDashedStroke (hf, hf, dashes, 2);
    g.fillPath (hf);

    // early reflections
    if (v.erTaps > 0 && early > 0.001f)
    {
        const float scale = 0.55f + 0.9f * size;
        for (int t = 0; t < v.erTaps; ++t)
        {
            const float shaped = std::pow (((float) t + 0.5f) / (float) v.erTaps, 1.3f);
            const float time = pre + (v.erStartMs + (v.erEndMs - v.erStartMs) * shaped) * scale * 0.001f;
            const float lvl = early * v.erGain * std::exp (-2.5f * shaped) * 1.6f;
            const float db = jmax (-60.0f, Decibels::gainToDecibels (lvl, -60.0f) - 4.0f);
            g.setColour (Pal::cream.withAlpha (0.8f));
            g.drawLine (xOf (time), area.getBottom(), xOf (time), yOf (db), 1.4f);
        }
    }

    // pre-delay marker
    if (pre > 0.0005f)
    {
        g.setColour (Pal::dim.withAlpha (0.5f));
        const float x = xOf (pre);
        g.drawLine (area.getX(), area.getBottom() - 4, x, area.getBottom() - 4, 1.0f);
    }

    // header text
    g.setFont (uiFont (12.0f, true).withExtraKerningFactor (0.08f));
    g.setColour (Pal::cream);
    g.drawText (String (kTypeNames[type]), full.reduced (12.0f, 6.0f).withHeight (16.0f), Justification::centredLeft);
    g.setFont (uiFont (11.5f));
    g.setColour (Pal::dim);
    g.drawText ("RT60 " + String (decay, 2) + " s   PRE " + String (roundToInt (pre * 1000.0f)) + " ms   HF "
                    + String (decay * alpha, 2) + " s",
                full.reduced (12.0f, 6.0f).withHeight (16.0f), Justification::centredRight);
}

//==============================================================================
// Editor content
EchoVaultEditor::Content::Content (EchoVaultProcessor& p)
    : proc (p),
      typeSelector (*p.apvts.getParameter (IDs::type)),
      display (p.apvts)
{
    addAndMakeVisible (typeSelector);
    addAndMakeVisible (display);

    for (int i = 0; i < p.getNumPrograms(); ++i)
        presetBox.addItem (p.getProgramName (i), i + 1);
    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0) { proc.setCurrentProgram (idx); shownProgram = idx; }
    };
    addAndMakeVisible (presetBox);

    auto step = [this] (int dir)
    {
        const int n = proc.getNumPrograms();
        const int idx = (proc.getCurrentProgram() + dir + n) % n;
        proc.setCurrentProgram (idx);
        refreshPresetBox();
    };
    prevButton.onClick = [step] { step (-1); };
    nextButton.onClick = [step] { step (1); };
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    auto& s = p.apvts;
    auto add = [&] (std::vector<std::unique_ptr<Knob>>& list, const char* id, const char* name)
    {
        list.push_back (std::make_unique<Knob> (s, id, name));
        addAndMakeVisible (*list.back());
    };
    add (spaceKnobs, IDs::predelay,  "Pre-Delay");
    add (spaceKnobs, IDs::decay,     "Decay");
    add (spaceKnobs, IDs::size,      "Size");
    add (spaceKnobs, IDs::diffusion, "Diffusion");
    add (spaceKnobs, IDs::early,     "Early");
    add (spaceKnobs, IDs::mod,       "Modulation");

    add (toneKnobs, IDs::damping, "Damping");
    add (toneKnobs, IDs::lowcut,  "Low Cut");
    add (toneKnobs, IDs::highcut, "High Cut");
    add (toneKnobs, IDs::warmth,  "Warmth");
    add (toneKnobs, IDs::width,   "Width");
    add (toneKnobs, IDs::mix,     "Mix");
    add (toneKnobs, IDs::output,  "Output");

    refreshPresetBox();
    startTimerHz (4);
}

void EchoVaultEditor::Content::refreshPresetBox()
{
    shownProgram = proc.getCurrentProgram();
    presetBox.setSelectedId (shownProgram + 1, dontSendNotification);
}

void EchoVaultEditor::Content::timerCallback()
{
    if (proc.getCurrentProgram() != shownProgram)
        refreshPresetBox();
}

void EchoVaultEditor::Content::paint (Graphics& g)
{
    g.setGradientFill (ColourGradient (Pal::bgTop, 0, 0, Pal::bg, 0, (float) getHeight(), false));
    g.fillAll();

    // header
    g.setColour (Pal::amber);
    g.fillEllipse (26.0f, 25.0f, 10.0f, 10.0f);
    g.setColour (Pal::amber.withAlpha (0.2f));
    g.fillEllipse (21.0f, 20.0f, 20.0f, 20.0f);

    g.setColour (Pal::cream);
    g.setFont (uiFont (26.0f, true).withExtraKerningFactor (0.18f));
    g.drawText ("ECHO VAULT", 50, 12, 260, 34, Justification::centredLeft);
    g.setColour (Pal::dim);
    g.setFont (uiFont (11.0f).withExtraKerningFactor (0.2f));
    g.drawText ("VINTAGE CHAMBER REVERB", 52, 42, 300, 14, Justification::centredLeft);

    g.setColour (Pal::panelEdge);
    g.drawHorizontalLine (66, 24.0f, (float) getWidth() - 24.0f);

    auto drawPanel = [&] (Rectangle<int> r, const String& title)
    {
        g.setColour (Pal::panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (Pal::panelEdge);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (Pal::amber.withAlpha (0.85f));
        g.setFont (uiFont (11.0f, true).withExtraKerningFactor (0.2f));
        g.drawText (title, r.getX() + 14, r.getY() + 6, 200, 16, Justification::centredLeft);
    };
    drawPanel (roomPanel, "ROOM");
    drawPanel (displayPanel, "DECAY");
    drawPanel (spacePanel, "SPACE");
    drawPanel (tonePanel, "TONE & OUTPUT");
}

void EchoVaultEditor::Content::resized()
{
    nextButton.setBounds (864, 20, 32, 28);
    presetBox.setBounds (572, 20, 286, 28);
    prevButton.setBounds (534, 20, 32, 28);

    roomPanel    = { 24, 80, 300, 172 };
    displayPanel = { 336, 80, 560, 172 };
    spacePanel   = { 24, 264, 872, 138 };
    tonePanel    = { 24, 412, 872, 138 };

    typeSelector.setBounds (roomPanel.reduced (12).withTrimmedTop (16));
    display.setBounds (displayPanel.reduced (12).withTrimmedTop (16));

    auto layoutRow = [] (std::vector<std::unique_ptr<Knob>>& knobs, Rectangle<int> r)
    {
        r = r.reduced (10).withTrimmedTop (14);
        const int w = r.getWidth() / (int) knobs.size();
        for (auto& k : knobs)
            k->setBounds (r.removeFromLeft (w).reduced (4, 0));
    };
    layoutRow (spaceKnobs, spacePanel);
    layoutRow (toneKnobs, tonePanel);
}

//==============================================================================
EchoVaultEditor::EchoVaultEditor (EchoVaultProcessor& p)
    : AudioProcessorEditor (p), content (p)
{
    setLookAndFeel (&lnf);
    content.setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.setBounds (0, 0, baseW, baseH);

    setResizable (true, true);
    setResizeLimits (baseW * 3 / 4, baseH * 3 / 4, baseW * 2, baseH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize (baseW, baseH);
}

EchoVaultEditor::~EchoVaultEditor()
{
    content.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void EchoVaultEditor::resized()
{
    const float scale = (float) getWidth() / (float) baseW;
    content.setTransform (AffineTransform::scale (scale));
}

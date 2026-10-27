#include "PluginEditor.h"
#include "Presets.h"

static juce::String sep() { return juce::String::fromUTF8 ("   \xc2\xb7   "); }

namespace Pal
{
    const juce::Colour bg       { 0xfff3f6fb };
    const juce::Colour panel    { 0xffffffff };
    const juce::Colour border   { 0xffdfe5ef };
    const juce::Colour track    { 0xffe4e9f2 };
    const juce::Colour accent   { 0xff2f6bff };
    const juce::Colour deep     { 0xff1a3d9c };
    const juce::Colour light    { 0xffd6e3ff };
    const juce::Colour text     { 0xff1b2a4a };
    const juce::Colour dim      { 0xff8a96ad };
    const juce::Colour screenTop    { 0xff13254d };
    const juce::Colour screenBottom { 0xff0a1531 };
    const juce::Colour screenLine   { 0xff8fb4ff };
    const juce::Colour screenText   { 0xffb9cbef };
    const juce::Colour toneLine     { 0xffffc46b };
}

//==============================================================================
VoxLookAndFeel::VoxLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Pal::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, Pal::light);
    setColour (juce::TextEditor::textColourId, Pal::text);
    setColour (juce::TextEditor::highlightedTextColourId, Pal::text);
    setColour (juce::CaretComponent::caretColourId, Pal::accent);
    setColour (juce::Label::textColourId, Pal::dim);
    setColour (juce::Label::textWhenEditingColourId, Pal::text);
    setColour (juce::ComboBox::textColourId, Pal::text);
    setColour (juce::ComboBox::arrowColourId, Pal::accent);
    setColour (juce::PopupMenu::backgroundColourId, Pal::panel);
    setColour (juce::PopupMenu::textColourId, Pal::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Pal::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

void VoxLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, juce::Slider& s)
{
    using namespace juce;
    const bool big = s.getProperties().getWithDefault ("big", false);
    auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (big ? 8.0f : 4.0f);
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float arcW = big ? 7.0f : 3.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const Colour acc = big ? Pal::deep : Pal::accent;

    Path track;
    track.addCentredArc (c.x, c.y, radius - arcW, radius - arcW, 0.0f, startAngle, endAngle, true);
    g.setColour (Pal::track);
    g.strokePath (track, PathStrokeType (arcW, PathStrokeType::curved, PathStrokeType::rounded));

    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? startAngle + 0.5f * (endAngle - startAngle) : startAngle;
    Path arc;
    arc.addCentredArc (c.x, c.y, radius - arcW, radius - arcW, 0.0f, jmin (from, angle), jmax (from, angle), true);
    g.setColour (acc);
    g.strokePath (arc, PathStrokeType (arcW, PathStrokeType::curved, PathStrokeType::rounded));

    const float bodyR = radius - arcW * 2.4f;
    g.setColour (Colour (0x141b2a4a));
    g.fillEllipse (c.x - bodyR, c.y - bodyR + 2.0f, bodyR * 2, bodyR * 2);
    g.setGradientFill (ColourGradient (Colours::white, c.x, c.y - bodyR, Colour (0xffeef2f8), c.x, c.y + bodyR, false));
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2);
    g.setColour (Pal::border);
    g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2, 1.0f);

    if (big)
    {
        g.setColour (Pal::deep);
        g.setFont (FontOptions (bodyR * 0.32f, Font::bold));
        g.drawText (String (roundToInt (s.getValue())), Rectangle<float> (c.x - bodyR, c.y - bodyR * 0.25f, bodyR * 2, bodyR * 0.5f), Justification::centred);
    }

    Path ptr;
    const float pl = bodyR * 0.82f;
    const float pw = big ? 4.0f : 2.6f;
    ptr.addRoundedRectangle (-pw * 0.5f, -pl, pw, pl * (big ? 0.25f : 0.42f), pw * 0.5f);
    g.setColour (acc);
    g.fillPath (ptr, AffineTransform::rotation (angle).translated (c.x, c.y));
}

void VoxLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState() && b.isEnabled();
    if (! b.isEnabled()) highlighted = false;
    g.setColour (on ? Pal::accent : Pal::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on || highlighted ? Pal::accent : Pal::border);
    g.drawRoundedRectangle (r, 6.0f, 1.2f);
    g.setColour (on ? juce::Colours::white : Pal::dim.withAlpha (b.isEnabled() ? 1.0f : 0.45f));
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

void VoxLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (1.0f);
    g.setColour (Pal::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (box.isMouseOver (true) ? Pal::accent : Pal::border);
    g.drawRoundedRectangle (r, 6.0f, 1.2f);
    juce::Path arrow;
    const float ax = r.getRight() - 16.0f, ay = r.getCentreY();
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (Pal::accent);
    g.fillPath (arrow);
}

//==============================================================================
void LevelMeter::setLevel (float v)
{
    const float norm = gr ? juce::jlimit (0.0f, 1.0f, -v / 24.0f)
                          : juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (v, -60.0f) + 60.0f) / 60.0f);
    value = norm > value ? norm : value * 0.92f + norm * 0.08f;   // tuned for 60 Hz
    if (value >= peakHold) { peakHold = value; holdFrames = 90; }
    else if (--holdFrames < 0) peakHold = juce::jmax (value, peakHold - 0.005f);
    // only repaint when the bar visibly moved
    if (std::abs (value - drawnValue) * (float) getHeight() > 0.5f || std::abs (peakHold - drawnPeak) * (float) getHeight() > 0.5f)
        repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    drawnValue = value;
    drawnPeak = peakHold;
    auto r = getLocalBounds().toFloat();
    g.setColour (Pal::track);
    g.fillRoundedRectangle (r, 3.0f);
    if (gr)
    {
        g.setColour (Pal::deep);
        g.fillRoundedRectangle (r.withHeight (r.getHeight() * value), 3.0f);
    }
    else
    {
        g.setGradientFill (juce::ColourGradient (Pal::deep, 0, r.getY(), Pal::accent.withAlpha (0.55f), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r.withTop (r.getBottom() - r.getHeight() * value), 3.0f);
        g.setColour (Pal::text);
        g.fillRect (r.getX(), r.getBottom() - r.getHeight() * peakHold, r.getWidth(), 2.0f);
    }
}

//==============================================================================
void CleanDisplay::refresh()
{
    proc.denoiser.copyDisplay (spectrum, noise, reduction, tone);
    const int clicks = proc.declicker.clickCount.load();
    if (clicks != lastClicks) { clickFlash = 1.0f; lastClicks = clicks; }
    else clickFlash = juce::jmax (0.0f, clickFlash - 0.02f);
    repaint();
}

void CleanDisplay::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto full = getLocalBounds().toFloat();
    const auto a = full.reduced (1.0f).withTrimmedBottom (16.0f);
    constexpr int N = SpectralDenoiser::numDisplayPoints;
    auto xAt = [&] (int i) { return a.getX() + a.getWidth() * (float) i / (N - 1); };
    auto freqToX = [&] (float f) { return a.getX() + a.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); };
    auto yAt = [&] (float db) { return jmap (jlimit (-110.0f, -10.0f, db), -110.0f, -10.0f, a.getBottom(), a.getY() + 8.0f); };

    g.fillAll (Pal::panel);
    g.setGradientFill (ColourGradient (Pal::screenTop, 0, full.getY(), Pal::screenBottom, 0, full.getBottom(), false));
    g.fillRoundedRectangle (full, 8.0f);

    g.setFont (FontOptions (10.0f));
    for (int f : { 50, 100, 200, 500, 1000, 2000, 5000, 10000 })
    {
        const float x = freqToX ((float) f);
        g.setColour (Colours::white.withAlpha (f == 100 || f == 1000 || f == 10000 ? 0.12f : 0.06f));
        g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
        g.setColour (Pal::screenText.withAlpha (0.7f));
        g.drawText (f >= 1000 ? String (f / 1000) + "k" : String (f), (int) x - 20, (int) a.getBottom() + 2, 40, 12, Justification::centred);
    }

    // voice spectrum
    Path spec;
    spec.startNewSubPath (a.getX(), a.getBottom());
    for (int i = 0; i < N; ++i) spec.lineTo (xAt (i), yAt (spectrum[(size_t) i]));
    spec.lineTo (a.getRight(), a.getBottom());
    spec.closeSubPath();
    g.setGradientFill (ColourGradient (Pal::screenLine.withAlpha (0.32f), 0, a.getY(), Pal::screenLine.withAlpha (0.04f), 0, a.getBottom(), false));
    g.fillPath (spec);
    g.setColour (Pal::screenLine.withAlpha (0.6f));
    g.strokePath (spec, PathStrokeType (1.0f));

    // detected noise floor
    Path nz, nzLine;
    nz.startNewSubPath (a.getX(), a.getBottom());
    for (int i = 0; i < N; ++i)
    {
        nz.lineTo (xAt (i), yAt (noise[(size_t) i]));
        if (i == 0) nzLine.startNewSubPath (xAt (i), yAt (noise[(size_t) i])); else nzLine.lineTo (xAt (i), yAt (noise[(size_t) i]));
    }
    nz.lineTo (a.getRight(), a.getBottom());
    nz.closeSubPath();
    g.setColour (Colour (0xffff8a7a).withAlpha (0.18f));
    g.fillPath (nz);
    g.setColour (Colour (0xffff9d8f));
    g.strokePath (nzLine, PathStrokeType (1.3f));

    // removed (noise + harsh resonances), hanging from the top (0..30 dB)
    Path red;
    red.startNewSubPath (a.getX(), a.getY());
    for (int i = 0; i < N; ++i)
        red.lineTo (xAt (i), a.getY() + jlimit (0.0f, 1.0f, reduction[(size_t) i] / 30.0f) * a.getHeight() * 0.4f);
    red.lineTo (a.getRight(), a.getY());
    red.closeSubPath();
    g.setColour (Colour (0xffe6f0ff).withAlpha (0.35f));
    g.fillPath (red);
    g.setColour (Colour (0xffe6f0ff));
    g.strokePath (red, PathStrokeType (1.3f));

    // microphone tone curve (+-12 dB around the middle)
    const float midY = a.getY() + a.getHeight() * 0.62f, dbPx = a.getHeight() * 0.022f;
    g.setColour (Pal::toneLine.withAlpha (0.18f));
    g.drawHorizontalLine ((int) midY, a.getX(), a.getRight());
    Path tc;
    for (int i = 0; i < N; ++i)
    {
        const float y = midY - jlimit (-12.0f, 12.0f, tone[(size_t) i]) * dbPx;
        if (i == 0) tc.startNewSubPath (xAt (i), y); else tc.lineTo (xAt (i), y);
    }
    g.setColour (Pal::toneLine);
    g.strokePath (tc, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // legend + click indicator
    g.setFont (FontOptions (10.0f, Font::bold));
    auto legend = a.reduced (10.0f, 8.0f).removeFromBottom (14.0f);
    g.setColour (Pal::screenLine);
    g.drawText ("STIMME", legend.removeFromLeft (56.0f), Justification::centredLeft);
    g.setColour (Colour (0xffff9d8f));
    g.drawText ("RAUSCHEN", legend.removeFromLeft (72.0f), Justification::centredLeft);
    g.setColour (Colour (0xffe6f0ff));
    g.drawText ("ENTFERNT", legend.removeFromLeft (70.0f), Justification::centredLeft);
    g.setColour (Pal::toneLine);
    g.drawText ("MIKRO-KURVE", legend.removeFromLeft (90.0f), Justification::centredLeft);

    auto pill = Rectangle<float> (a.getRight() - 150.0f, a.getY() + 8.0f, 140.0f, 20.0f);
    g.setColour (Pal::accent.withAlpha (0.25f + 0.65f * clickFlash));
    g.fillRoundedRectangle (pill, 10.0f);
    g.setColour (Colours::white);
    g.drawText (String ("KLICKS ENTFERNT: ") + String (lastClicks), pill, Justification::centred);

    String overlay;
    if (proc.denoiser.isLearning()) overlay = String::fromUTF8 ("LERNE RAUSCHEN \xe2\x80\x93 bitte 2 s nicht singen");
    else if (proc.denoiser.isMatching())
        overlay = String::fromUTF8 ("MATCH: singe normal weiter \xe2\x80\x93 ") + String (roundToInt (100.0f * jmin (1.0f, proc.denoiser.getMatchProgress()))) + " %";
    if (overlay.isNotEmpty())
    {
        auto box = Rectangle<float> (a.getCentreX() - 160.0f, a.getCentreY() - 18.0f, 320.0f, 36.0f);
        g.setColour (Pal::screenBottom.withAlpha (0.92f));
        g.fillRoundedRectangle (box, 6.0f);
        g.setColour (Pal::toneLine);
        g.drawRoundedRectangle (box, 6.0f, 1.0f);
        g.drawText (overlay, box, Justification::centred);
    }

    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (full.reduced (0.5f), 8.0f, 1.0f);
}

//==============================================================================
MainView::MainView (VoxClearProcessor& p) : proc (p), display (p)
{
    addAndMakeVisible (display);

    setupKnob (lowCut, "lowcut", "LOW CUT");
    setupKnob (declick, "declick", "DE-CLICK");
    setupKnob (noise, "noise", "NOISE");
    setupKnob (gate, "gate", "GATE");

    setupKnob (character, "mic_amount", "CHARACTER");
    setupKnob (smooth, "smooth", "SMOOTH");
    setupKnob (tube, "tube", "TUBE");

    setupKnob (push, "push", "PUSH");
    push.slider.getProperties().set ("big", true);
    setupKnob (demud, "demud", "DE-MUD");
    setupKnob (debox, "debox", "DE-BOX");
    setupKnob (presence, "presence", "PRESENCE");
    setupKnob (air, "air", "AIR");
    setupKnob (level, "level", "LEVEL");
    setupKnob (ride, "ride", "BEAT RIDE");
    setupKnob (output, "output", "OUTPUT");
    setupKnob (mix, "mix", "MIX");

    // presets
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && (idx != proc.getCurrentProgram() || presetModified()))
            proc.setCurrentProgram (idx);
        refreshPresetBox();
    };
    auto step = [this] (int d)
    {
        const int count = proc.getNumPrograms();
        proc.setCurrentProgram ((proc.getCurrentProgram() + d + count) % count);
        refreshPresetBox();
    };
    prevPreset.onClick = [step] { step (-1); };
    nextPreset.onClick = [step] { step (1); };
    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &prevPreset, &nextPreset }) addAndMakeVisible (c);

    // microphone
    sourceBox.addItemList (Mic::sourceNames(), 1);
    targetBox.addItemList (Mic::targetNames(), 1);
    addAndMakeVisible (sourceBox);
    addAndMakeVisible (targetBox);
    sourceAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "mic_source", sourceBox);
    targetAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "mic_target", targetBox);

    for (auto* b : { &bypass, &listen, &learn, &matchLearn, &matchOn, &matchReset })
        addAndMakeVisible (b);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "bypass", bypass);
    listenAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "listen", listen);
    matchOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "match_on", matchOn);
    learn.setClickingTogglesState (false);
    learn.onClick = [this]
    {
        if (proc.denoiser.hasProfile()) proc.denoiser.clearProfile();
        else proc.denoiser.requestLearn();
    };
    matchLearn.setClickingTogglesState (false);
    matchLearn.onClick = [this] { proc.denoiser.requestMatch(); };
    matchReset.setClickingTogglesState (false);
    matchReset.onClick = [this] { proc.clearMatch(); };

    addAndMakeVisible (defaultButton);
    defaultButton.setClickingTogglesState (false);
    defaultButton.onClick = [this]
    {
        juce::PopupMenu menu;
        menu.addSectionHeader (juce::String::fromUTF8 ("Startwerte für neue Instanzen"));
        menu.addItem (1, juce::String::fromUTF8 ("Aktuelle Einstellungen als Standard speichern"));
        menu.addItem (2, juce::String::fromUTF8 ("Standard löschen (Werkseinstellung)"), proc.hasUserDefault());
        menu.addSeparator();
        menu.addItem (3, juce::String::fromUTF8 ("Dein Mikro und MATCH werden automatisch gemerkt"), false);
        juce::Component::SafePointer<MainView> safe (this);   // the window may be closed while the menu is open
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&defaultButton), [safe] (int result)
        {
            if (safe == nullptr) return;
            if (result == 1) { safe->proc.saveAsDefault(); safe->defaultSavedTicks = 120; }
            if (result == 2) safe->proc.clearDefault();
        });
    };

    for (auto* m : { &inMeter, &outMeter, &nrMeter, &gateMeter, &compMeter })
        addAndMakeVisible (m);

    setSize (designWidth, designHeight);
    refreshPresetBox();
}

void MainView::setupKnob (Knob& k, const juce::String& id, const juce::String& name)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxTextColourId, Pal::text);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    k.slider.setVelocityBasedMode (false);
    k.slider.setMouseDragSensitivity (220);
    addAndMakeVisible (k.slider);
    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    k.label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (k.label);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
}

void MainView::placeKnob (Knob& k, juce::Rectangle<int> area)
{
    k.label.setBounds (area.removeFromTop (16));
    k.slider.setBounds (area);
}

bool MainView::presetModified() const
{
    const auto& all = Presets::all();
    const int idx = proc.getCurrentProgram();
    if (! juce::isPositiveAndBelow (idx, (int) all.size())) return false;
    for (auto* param : proc.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
        if (ranged == nullptr) continue;
        const auto id = ranged->getParameterID();
        if (id == "mic_source" || id == "match_on" || id == "bypass" || id == "listen") continue;
        float expected = ranged->getDefaultValue();
        for (const auto& [pid, value] : all[(size_t) idx].values)
            if (id == pid) expected = ranged->convertTo0to1 (value);
        if (std::abs (ranged->getValue() - expected) > 0.002f) return true;
    }
    return false;
}

void MainView::refreshPresetBox()
{
    const int idx = proc.getCurrentProgram();
    const bool modified = presetModified();
    if (idx == shownPreset && modified == shownModified) return;
    shownPreset = idx;
    shownModified = modified;
    presetBox.setSelectedId (idx + 1, juce::dontSendNotification);
    if (modified) presetBox.setText (proc.getProgramName (idx) + "  *", juce::dontSendNotification);
}

void MainView::paint (juce::Graphics& g)
{
    using namespace juce;
    g.fillAll (Pal::bg);

    g.setColour (Pal::text);
    g.setFont (FontOptions (24.0f, Font::bold));
    g.drawText ("VOX", 24, 12, 60, 32, Justification::centredLeft);
    g.setColour (Pal::accent);
    g.setFont (FontOptions (24.0f));
    g.drawText ("CLEAR", 78, 12, 100, 32, Justification::centredLeft);
    g.setColour (Pal::dim);
    g.setFont (FontOptions (10.5f, Font::bold));
    g.drawText ("PRESET", presetBox.getX() - 96, 12, 50, 32, Justification::centredRight);

    auto panelAt = [&g] (Rectangle<int> r, const String& title)
    {
        g.setColour (Colour (0x0c1b2a4a));
        g.fillRoundedRectangle (r.toFloat().translated (0.0f, 2.0f), 10.0f);
        g.setColour (Pal::panel);
        g.fillRoundedRectangle (r.toFloat(), 10.0f);
        g.setColour (Pal::border);
        g.drawRoundedRectangle (r.toFloat(), 10.0f, 1.0f);
        if (title.isNotEmpty())
        {
            g.setColour (Pal::accent);
            g.setFont (FontOptions (11.0f, Font::bold));
            g.drawText (title, r.getX() + 16, r.getY() + 8, 500, 16, Justification::centredLeft);
        }
    };
    panelAt (cleanPanel, "CLEAN" + sep() + "SCHMATZER, RAUSCHEN, PAUSEN");
    panelAt (micPanel, "MIKROFON" + sep() + "JEDES MIKRO AUF STUDIO-NIVEAU");
    panelAt (pushPanel, "VOCAL PUSH" + sep() + "NACH VORNE, KLAR, DIREKT");
    panelAt (meterPanel, {});

    g.setFont (FontOptions (10.5f, Font::bold));
    g.setColour (Pal::dim);
    g.drawText ("DEIN MIKRO", sourceBox.getX(), sourceBox.getY() - 16, 200, 14, Justification::centredLeft);
    g.drawText ("ZIEL-SOUND", targetBox.getX(), targetBox.getY() - 16, 200, 14, Justification::centredLeft);

    g.setFont (FontOptions (11.0f));
    g.drawFittedText (targetText, targetBox.getX(), targetBox.getBottom() + 6, targetBox.getWidth(), 30, Justification::topLeft, 2);
    g.drawText (statusText, cleanPanel.getX() + 16, cleanPanel.getBottom() - 24, display.getWidth(), 16, Justification::centredLeft);
    g.drawText (micText, character.slider.getX(), micPanel.getBottom() - 22, micPanel.getRight() - 16 - character.slider.getX(), 16, Justification::centredLeft);
    g.drawText (pushText, pushPanel.getX() + 16, pushPanel.getBottom() - 24, pushPanel.getWidth() - 32, 16, Justification::centredLeft);
    g.drawText (rideText, pushPanel.getX() + 16, pushPanel.getBottom() - 24, pushPanel.getWidth() - 32, 16, Justification::centredRight);
    g.drawFittedText (matchText, matchLearn.getX(), matchOn.getBottom() + 6, matchLearn.getWidth(), 30, Justification::centredTop, 2);

    g.setFont (FontOptions (10.0f, Font::bold));
    for (auto* m : { &inMeter, &outMeter, &nrMeter, &gateMeter, &compMeter })
    {
        const char* t = m == &inMeter ? "IN" : m == &outMeter ? "OUT" : m == &nrMeter ? "NR" : m == &gateMeter ? "GATE" : "LVL";
        g.drawText (t, m->getX() - 8, m->getBottom() + 4, m->getWidth() + 16, 14, Justification::centred);
    }
}

void MainView::resized()
{
    const int W = designWidth;
    bypass.setBounds (W - 110, 14, 90, 28);
    listen.setBounds (W - 206, 14, 90, 28);
    prevPreset.setBounds (336, 18, 20, 20);
    presetBox.setBounds (362, 14, 300, 28);
    nextPreset.setBounds (668, 18, 20, 20);
    defaultButton.setBounds (710, 14, 112, 28);

    meterPanel = { W - 170, 64, 150, designHeight - 84 };
    {
        const int top = meterPanel.getY() + 16, h = meterPanel.getHeight() - 236;
        int x = meterPanel.getX() + 14;
        for (auto* m : { &inMeter, &outMeter, &nrMeter, &gateMeter, &compMeter })
        {
            m->setBounds (x, top, 16, h);
            x += 26;
        }
        auto knobs = juce::Rectangle<int> (meterPanel.getX() + 6, meterPanel.getBottom() - 196, meterPanel.getWidth() - 12, 190);
        placeKnob (output, knobs.removeFromTop (95).withSizeKeepingCentre (120, 95));
        placeKnob (mix, knobs.withSizeKeepingCentre (120, 95));
    }

    const int cx = 20, cw = meterPanel.getX() - 12 - cx;

    cleanPanel = { cx, 64, cw, 250 };
    display.setBounds (cleanPanel.getX() + 14, cleanPanel.getY() + 30, cw - 28 - 320, 190);
    {
        auto area = juce::Rectangle<int> (display.getRight() + 12, cleanPanel.getY() + 28, 308, 192);
        auto top = area.removeFromTop (96), bottom = area;
        placeKnob (lowCut, top.removeFromLeft (154));
        placeKnob (declick, top);
        placeKnob (noise, bottom.removeFromLeft (154));
        placeKnob (gate, bottom);
        learn.setBounds (display.getRight() + 66, cleanPanel.getBottom() - 30, 200, 24);
    }

    micPanel = { cx, 326, cw, 170 };
    {
        sourceBox.setBounds (micPanel.getX() + 16, micPanel.getY() + 46, 230, 26);
        targetBox.setBounds (micPanel.getX() + 16, micPanel.getY() + 96, 230, 26);
        auto knobs = juce::Rectangle<int> (micPanel.getX() + 262, micPanel.getY() + 26, 330, 116);
        placeKnob (character, knobs.removeFromLeft (110));
        placeKnob (smooth, knobs.removeFromLeft (110));
        placeKnob (tube, knobs);
        const int mx = micPanel.getRight() - 16 - 190;
        matchLearn.setBounds (mx, micPanel.getY() + 32, 190, 28);
        matchOn.setBounds (mx, micPanel.getY() + 66, 120, 24);
        matchReset.setBounds (mx + 126, micPanel.getY() + 66, 64, 24);
    }

    pushPanel = { cx, 508, cw, designHeight - 508 - 20 };
    {
        auto row = pushPanel.reduced (14, 0).withTrimmedTop (26).withTrimmedBottom (30);
        placeKnob (push, row.removeFromLeft (160));
        const int w = row.getWidth() / 6;
        for (auto* k : { &demud, &debox, &presence, &air, &level, &ride })
            placeKnob (*k, row.removeFromLeft (w).reduced (4, 8));
    }
}

void MainView::tick (int frame)
{
    inMeter.setLevel (proc.inPeak.exchange (0.0f));
    outMeter.setLevel (proc.outPeak.exchange (0.0f));
    nrMeter.setLevel (-proc.denoiser.getReductionDb());
    gateMeter.setLevel (proc.gateGainDb.load());
    compMeter.setLevel (proc.compGainDb.load());
    display.refresh();

    if (frame % 6 != 0) return;   // texts and buttons: 10 Hz is plenty

    defaultSavedTicks = juce::jmax (0, defaultSavedTicks - 6);
    defaultButton.setButtonText (defaultSavedTicks > 0 ? juce::String::fromUTF8 ("GESPEICHERT â") : juce::String ("STANDARD"));
    defaultButton.setToggleState (defaultSavedTicks > 0, juce::dontSendNotification);

    using juce::String;
    const bool profile = proc.denoiser.hasProfile();
    learn.setButtonText (proc.denoiser.isLearning() ? "LERNT ..." : (profile ? "PROFIL AKTIV (RESET)" : "LEARN NOISE"));
    learn.setToggleState (profile || proc.denoiser.isLearning(), juce::dontSendNotification);

    const bool matching = proc.denoiser.isMatching();
    matchLearn.setButtonText (matching ? "LERNT ..." : (proc.hasMatch.load() ? "NEU LERNEN" : "MATCH LERNEN"));
    matchLearn.setToggleState (matching, juce::dontSendNotification);
    matchOn.setEnabled (proc.hasMatch.load());
    matchReset.setEnabled (proc.hasMatch.load());

    const auto& targets = Mic::targets();
    const int t = juce::jlimit (0, (int) targets.size() - 1, targetBox.getSelectedId() - 1);

    const String newStatus = "Rauschminderung " + String (proc.denoiser.getReductionDb(), 1) + " dB" + sep()
                           + "Profil: " + (profile ? "gelernt" : "automatisch") + sep()
                           + "Grundrauschen " + String (juce::roundToInt (proc.denoiser.getNoiseFloorDb())) + " dBFS";
    const String newMic = "Live: harte Resonanzen -" + String (proc.denoiser.getSmoothDb(), 1) + " dB" + sep()
                        + "Nahbesprechung -" + String (proc.denoiser.getProximityDb(), 1) + " dB";
    const String newPush = "Live: Matsch -" + String (proc.mudCutDb.load(), 1) + " dB" + sep()
                         + "Boxiness -" + String (proc.boxCutDb.load(), 1) + " dB" + sep()
                         + "S-Laute -" + String (proc.essCutDb.load(), 1) + " dB" + sep()
                         + "Leveler " + String (proc.compGainDb.load(), 1) + " dB";
    const String newRide = proc.sidechainActive.load() ? "Beat erkannt" + sep() + "Ride " + String (proc.rideDb.load(), 1) + " dB"
                                                       : String ("Beat Ride: Beat als Sidechain routen");
    const String newMatch = matching ? String::fromUTF8 ("Singe normal \xe2\x80\x93 ") + String (juce::roundToInt (100.0f * proc.denoiser.getMatchProgress())) + " %"
                          : proc.hasMatch.load() ? String::fromUTF8 ("Dein Mikro ist eingemessen") : String::fromUTF8 ("Lernt dein Mikro beim Singen (6 s)");
    const String newTarget = String::fromUTF8 (targets[(size_t) t].description);

    if (newStatus != statusText || newMic != micText || newPush != pushText || newRide != rideText || newMatch != matchText || newTarget != targetText)
    {
        statusText = newStatus; micText = newMic; pushText = newPush; rideText = newRide; matchText = newMatch; targetText = newTarget;
        repaint (cleanPanel.getX(), cleanPanel.getBottom() - 26, display.getWidth() + 16, 20);
        repaint (micPanel);
        repaint (pushPanel.getX(), pushPanel.getBottom() - 26, pushPanel.getWidth(), 20);
    }
    refreshPresetBox();
}

//==============================================================================
VoxClearEditor::VoxClearEditor (VoxClearProcessor& p)
    : AudioProcessorEditor (&p), view (p)
{
    const int savedWidth = p.uiWidth.load();   // read before JUCE applies any intermediate sizes
    setLookAndFeel (&lnf);
    addAndMakeVisible (view);
    setResizable (true, true);
    setResizeLimits (MainView::designWidth * 6 / 10, MainView::designHeight * 6 / 10,
                     MainView::designWidth * 16 / 10, MainView::designHeight * 16 / 10);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) MainView::designWidth / (double) MainView::designHeight);
    // reopen at the size the user left it (stored with the project)
    const int w = savedWidth > 0 ? juce::jlimit (MainView::designWidth * 6 / 10, MainView::designWidth * 16 / 10, savedWidth)
                                 : MainView::designWidth * 9 / 10;
    setSize (w, w * MainView::designHeight / MainView::designWidth);
    sizeRestored = true;
    startTimerHz (60);
}

VoxClearEditor::~VoxClearEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VoxClearEditor::resized()
{
    const float scale = (float) getWidth() / (float) MainView::designWidth;
    view.setTransform (juce::AffineTransform::scale (scale));
    if (sizeRestored)
        if (auto* p = dynamic_cast<VoxClearProcessor*> (getAudioProcessor()))
            p->uiWidth.store (getWidth());
}

void VoxClearEditor::timerCallback()
{
    view.tick (frame++);
}

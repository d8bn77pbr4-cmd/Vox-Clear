#include "PluginEditor.h"

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
    const bool on = b.getToggleState();
    g.setColour (on ? Pal::accent : Pal::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on || highlighted ? Pal::accent : Pal::border);
    g.drawRoundedRectangle (r, 6.0f, 1.2f);
    g.setColour (on ? juce::Colours::white : Pal::dim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

//==============================================================================
void LevelMeter::setLevel (float v)
{
    const float norm = gr ? juce::jlimit (0.0f, 1.0f, -v / 24.0f)
                          : juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (v, -60.0f) + 60.0f) / 60.0f);
    value = norm > value ? norm : value * 0.85f + norm * 0.15f;
    if (value >= peakHold) { peakHold = value; holdFrames = 45; }
    else if (--holdFrames < 0) peakHold = juce::jmax (value, peakHold - 0.01f);
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
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
    proc.denoiser.copyDisplay (spectrum, noise, reduction);
    const int clicks = proc.declicker.clickCount.load();
    if (clicks != lastClicks) { clickFlash = 1.0f; lastClicks = clicks; }
    else clickFlash = juce::jmax (0.0f, clickFlash - 0.04f);
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
    Path nz;
    nz.startNewSubPath (a.getX(), a.getBottom());
    for (int i = 0; i < N; ++i) nz.lineTo (xAt (i), yAt (noise[(size_t) i]));
    nz.lineTo (a.getRight(), a.getBottom());
    nz.closeSubPath();
    g.setColour (Colour (0xffff8a7a).withAlpha (0.18f));
    g.fillPath (nz);
    Path nzLine;
    for (int i = 0; i < N; ++i)
        if (i == 0) nzLine.startNewSubPath (xAt (i), yAt (noise[(size_t) i])); else nzLine.lineTo (xAt (i), yAt (noise[(size_t) i]));
    g.setColour (Colour (0xffff9d8f));
    g.strokePath (nzLine, PathStrokeType (1.3f));

    // removed amount, hanging from the top (0..30 dB)
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

    // legend + click indicator
    g.setFont (FontOptions (10.0f, Font::bold));
    auto legend = a.reduced (10.0f, 8.0f).removeFromBottom (14.0f);
    g.setColour (Pal::screenLine);
    g.drawText ("STIMME", legend.removeFromLeft (60.0f), Justification::centredLeft);
    g.setColour (Colour (0xffff9d8f));
    g.drawText ("RAUSCHEN", legend.removeFromLeft (80.0f), Justification::centredLeft);
    g.setColour (Colour (0xffe6f0ff));
    g.drawText ("ENTFERNT", legend.removeFromLeft (80.0f), Justification::centredLeft);

    auto pill = Rectangle<float> (a.getRight() - 150.0f, a.getY() + 8.0f, 140.0f, 20.0f);
    g.setColour (Pal::accent.withAlpha (0.25f + 0.65f * clickFlash));
    g.fillRoundedRectangle (pill, 10.0f);
    g.setColour (Colours::white);
    g.drawText (String ("KLICKS ENTFERNT: ") + String (lastClicks), pill, Justification::centred);

    if (proc.denoiser.isLearning())
    {
        auto box = Rectangle<float> (a.getCentreX() - 150.0f, a.getCentreY() - 18.0f, 300.0f, 36.0f);
        g.setColour (Pal::screenBottom.withAlpha (0.92f));
        g.fillRoundedRectangle (box, 6.0f);
        g.setColour (Colour (0xffff9d8f));
        g.drawRoundedRectangle (box, 6.0f, 1.0f);
        g.drawText (String::fromUTF8 ("LERNE RAUSCHEN \xe2\x80\x93 bitte 2 s nicht singen"), box, Justification::centred);
    }

    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (full.reduced (0.5f), 8.0f, 1.0f);
}

//==============================================================================
VoxClearEditor::VoxClearEditor (VoxClearProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (display);

    setupKnob (lowCut, "lowcut", "LOW CUT");
    setupKnob (declick, "declick", "DE-CLICK");
    setupKnob (noise, "noise", "NOISE");
    setupKnob (gate, "gate", "GATE");

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

    addAndMakeVisible (bypass);
    addAndMakeVisible (listen);
    addAndMakeVisible (learn);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "bypass", bypass);
    listenAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "listen", listen);
    learn.setClickingTogglesState (false);
    learn.onClick = [this]
    {
        if (proc.denoiser.hasProfile()) proc.denoiser.clearProfile();
        else proc.denoiser.requestLearn();
    };

    for (auto* m : { &inMeter, &outMeter, &nrMeter, &gateMeter, &compMeter })
        addAndMakeVisible (m);

    setSize (1000, 720);
    startTimerHz (30);
}

VoxClearEditor::~VoxClearEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VoxClearEditor::setupKnob (Knob& k, const juce::String& id, const juce::String& name)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxTextColourId, Pal::text);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (k.slider);
    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    addAndMakeVisible (k.label);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
}

void VoxClearEditor::placeKnob (Knob& k, juce::Rectangle<int> area)
{
    k.label.setBounds (area.removeFromTop (16));
    k.slider.setBounds (area);
}

void VoxClearEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    g.fillAll (Pal::bg);

    g.setColour (Pal::text);
    g.setFont (FontOptions (24.0f, Font::bold));
    g.drawText ("VOX", 28, 14, 60, 32, Justification::centredLeft);
    g.setColour (Pal::accent);
    g.setFont (FontOptions (24.0f));
    g.drawText ("CLEAR", 82, 14, 100, 32, Justification::centredLeft);
    g.setColour (Pal::dim);
    g.setFont (FontOptions (11.0f));
    g.drawText ("VOCAL CLEANUP  /  PUSH", 180, 14, 220, 32, Justification::centredLeft);

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
            g.drawText (title, r.getX() + 16, r.getY() + 8, 300, 16, Justification::centredLeft);
        }
    };
    panelAt (cleanPanel, "CLEAN" + sep() + "SCHMATZER, RAUSCHEN, PAUSEN");
    panelAt (pushPanel, "VOCAL PUSH" + sep() + "NACH VORNE, KLAR, DIREKT");
    panelAt (outPanel, {});
    panelAt (meterPanel, {});

    g.setFont (FontOptions (11.0f));
    g.setColour (Pal::dim);
    g.drawText (statusText, cleanPanel.getX() + 16, cleanPanel.getBottom() - 26, cleanPanel.getWidth() - 32, 16, Justification::centredLeft);
    g.drawText (pushText, pushPanel.getX() + 16, pushPanel.getBottom() - 26, pushPanel.getWidth() - 32, 16, Justification::centredLeft);
    g.drawText (rideText, outPanel.getX() + 16, outPanel.getY() + 8, outPanel.getWidth() - 32, 16, Justification::centredRight);

    // meter captions
    g.setFont (FontOptions (10.0f, Font::bold));
    for (auto* m : { &inMeter, &outMeter, &nrMeter, &gateMeter, &compMeter })
    {
        const char* t = m == &inMeter ? "IN" : m == &outMeter ? "OUT" : m == &nrMeter ? "NR" : m == &gateMeter ? "GATE" : "LVL";
        g.drawText (t, m->getX() - 10, m->getBottom() + 4, m->getWidth() + 20, 14, Justification::centred);
    }
}

void VoxClearEditor::resized()
{
    bypass.setBounds (getWidth() - 116, 16, 96, 28);
    listen.setBounds (getWidth() - 222, 16, 96, 28);

    meterPanel = { getWidth() - 196, 64, 176, getHeight() - 84 };
    {
        const int top = meterPanel.getY() + 16, h = meterPanel.getHeight() - 48;
        int x = meterPanel.getX() + 16;
        for (auto* m : { &inMeter, &outMeter, &nrMeter, &gateMeter, &compMeter })
        {
            m->setBounds (x, top, 18, h);
            x += 32;
        }
    }

    const int cx = 20, cw = getWidth() - 196 - 32;
    cleanPanel = { cx, 64, cw, 300 };
    display.setBounds (cleanPanel.getX() + 14, cleanPanel.getY() + 30, cw - 28 - 330, 236);
    {
        auto area = juce::Rectangle<int> (display.getRight() + 12, cleanPanel.getY() + 30, 318, 236);
        auto top = area.removeFromTop (110);
        auto bottom = area.removeFromTop (110);
        placeKnob (lowCut, top.removeFromLeft (159));
        placeKnob (declick, top);
        placeKnob (noise, bottom.removeFromLeft (159));
        placeKnob (gate, bottom);
    }
    learn.setBounds (cleanPanel.getRight() - 14 - 318 + 60, cleanPanel.getBottom() - 32, 200, 24);

    pushPanel = { cx, 376, cw, 196 };
    {
        auto row = pushPanel.reduced (14, 0).withTrimmedTop (26).withTrimmedBottom (30);
        placeKnob (push, row.removeFromLeft (170));
        const int w = row.getWidth() / 6;
        for (auto* k : { &demud, &debox, &presence, &air, &level, &ride })
            placeKnob (*k, row.removeFromLeft (w).reduced (4, 10));
    }

    outPanel = { cx, 584, cw, getHeight() - 584 - 20 };
    {
        auto row = outPanel.reduced (14, 8);
        placeKnob (output, row.removeFromLeft (120));
        placeKnob (mix, row.removeFromLeft (120));
    }
}

void VoxClearEditor::timerCallback()
{
    inMeter.setLevel (proc.inPeak.exchange (0.0f));
    outMeter.setLevel (proc.outPeak.exchange (0.0f));
    nrMeter.setLevel (-proc.denoiser.getReductionDb());
    gateMeter.setLevel (proc.gateGainDb.load());
    compMeter.setLevel (proc.compGainDb.load());
    display.refresh();

    const bool profile = proc.denoiser.hasProfile();
    learn.setButtonText (proc.denoiser.isLearning() ? "LERNT ..." : (profile ? "PROFIL AKTIV (RESET)" : "LEARN NOISE"));
    learn.setToggleState (profile || proc.denoiser.isLearning(), juce::dontSendNotification);

    using juce::String;
    statusText = "Rauschminderung " + String (proc.denoiser.getReductionDb(), 1) + " dB" + sep()
               + "Rauschprofil: " + (profile ? "gelernt" : "automatisch") + sep()
               + "Grundrauschen " + String (juce::roundToInt (proc.denoiser.getNoiseFloorDb())) + " dBFS";
    pushText = "Live: Matsch -" + String (proc.mudCutDb.load(), 1) + " dB" + sep()
             + "Boxiness -" + String (proc.boxCutDb.load(), 1) + " dB" + sep()
             + "S-Laute -" + String (proc.essCutDb.load(), 1) + " dB" + sep()
             + "Leveler " + String (proc.compGainDb.load(), 1) + " dB";
    rideText = proc.sidechainActive.load()
                 ? "Beat-Sidechain erkannt" + sep() + "Ride " + String (proc.rideDb.load(), 1) + " dB"
                 : String ("BEAT RIDE: Beat als Sidechain auf diese Spur routen (optional)");
    repaint (cleanPanel.getX(), cleanPanel.getBottom() - 28, cleanPanel.getWidth(), 20);
    repaint (pushPanel.getX(), pushPanel.getBottom() - 28, pushPanel.getWidth(), 20);
    repaint (outPanel);
}

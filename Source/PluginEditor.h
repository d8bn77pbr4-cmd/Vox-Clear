#pragma once

#include "PluginProcessor.h"
#include <functional>

class VoxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VoxLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (13.0f, juce::Font::bold); }
    juce::Font getPopupMenuFont() override { return juce::FontOptions (14.0f); }
};

class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (bool isGainReduction = false) : gr (isGainReduction) { setOpaque (false); }
    void setLevel (float newValue); // linear peak, or dB (<= 0) for reduction meters
    void paint (juce::Graphics&) override;

private:
    bool gr;
    float value = 0.0f, peakHold = 0.0f, drawnValue = -1.0f, drawnPeak = -1.0f;
    int holdFrames = 0;
};

// Live view: spectrum, detected noise floor, what is removed, and the microphone tone curve
class CleanDisplay : public juce::Component
{
public:
    explicit CleanDisplay (VoxClearProcessor& p) : proc (p) { setOpaque (true); spectrum.fill (-120.0f); noise.fill (-120.0f); }
    void refresh();
    void paint (juce::Graphics&) override;

private:
    VoxClearProcessor& proc;
    std::array<float, SpectralDenoiser::numDisplayPoints> spectrum {}, noise {}, reduction {}, tone {};
    int lastClicks = 0;
    float clickFlash = 0.0f;
};

// All controls at a fixed design size; the editor scales this view, so the window can be resized freely
class MainView : public juce::Component
{
public:
    static constexpr int designWidth = 1040, designHeight = 728;

    explicit MainView (VoxClearProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick (int frame);   // called at 60 Hz

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const juce::String& paramId, const juce::String& name);
    void placeKnob (Knob&, juce::Rectangle<int> area);
    void setupAutoDial (Knob&, const juce::String& name, double lo, double hi, std::function<juce::String (double)> text);
    void refreshPresetBox();
    void updateAutoDials();
    bool presetModified() const;

    VoxClearProcessor& proc;

    CleanDisplay display;
    Knob lowCut, declick, noise, gate;
    Knob autoLowCut, autoDeclick, autoNoise, autoGate;   // read-only: what AUTO currently uses
    juce::ToggleButton autoButton { "AUTO" };
    juce::ComboBox strengthBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> strengthAtt;
    Knob character, smooth, tube;
    Knob push, demud, debox, presence, air, level, ride, output, mix;

    juce::ComboBox presetBox, sourceBox, targetBox;
    juce::ArrowButton prevPreset { "prev", 0.5f, juce::Colour (0xff2f6bff) }, nextPreset { "next", 0.0f, juce::Colour (0xff2f6bff) };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sourceAtt, targetAtt;

    juce::ToggleButton bypass { "BYPASS" }, listen { "LISTEN" }, learn { "LEARN NOISE" };
    juce::ToggleButton matchLearn { "MATCH LERNEN" }, matchOn { "MATCH AN" }, matchReset { "RESET" };
    juce::ToggleButton defaultButton { "STANDARD" };
    int defaultSavedTicks = 0;   // shows a short confirmation after saving
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt, listenAtt, matchOnAtt;

    LevelMeter inMeter, outMeter, nrMeter { true }, gateMeter { true }, compMeter { true };
    juce::Rectangle<int> cleanPanel, micPanel, pushPanel, meterPanel;
    juce::String statusText, micText, pushText, rideText, matchText, targetText;
    int shownPreset = -1;
    bool shownModified = false;
};

class VoxClearEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VoxClearEditor (VoxClearProcessor&);
    ~VoxClearEditor() override;
    void resized() override;

private:
    void timerCallback() override;

    VoxLookAndFeel lnf;
    MainView view;
    int frame = 0;
    bool sizeRestored = false;   // only remember sizes the user chose, not the ones set while opening

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxClearEditor)
};

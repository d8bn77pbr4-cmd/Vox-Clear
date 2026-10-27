#pragma once

#include "PluginProcessor.h"

class VoxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VoxLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (bool isGainReduction = false) : gr (isGainReduction) {}
    void setLevel (float newValue); // linear peak, or dB (<= 0) for reduction meters
    void paint (juce::Graphics&) override;

private:
    bool gr;
    float value = 0.0f, peakHold = 0.0f;
    int holdFrames = 0;
};

// Live view of the cleaning: spectrum, detected noise floor, and how much is removed per frequency
class CleanDisplay : public juce::Component
{
public:
    explicit CleanDisplay (VoxClearProcessor& p) : proc (p) { spectrum.fill (-120.0f); noise.fill (-120.0f); }
    void refresh();
    void paint (juce::Graphics&) override;

private:
    VoxClearProcessor& proc;
    std::array<float, SpectralDenoiser::numDisplayPoints> spectrum {}, noise {}, reduction {};
    int lastClicks = 0;
    float clickFlash = 0.0f;
};

class VoxClearEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VoxClearEditor (VoxClearProcessor&);
    ~VoxClearEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const juce::String& paramId, const juce::String& name);
    void placeKnob (Knob&, juce::Rectangle<int> area);

    VoxClearProcessor& proc;
    VoxLookAndFeel lnf;

    CleanDisplay display;
    Knob lowCut, declick, noise, gate;
    Knob push, demud, debox, presence, air, level, ride, output, mix;

    juce::ToggleButton bypass { "BYPASS" }, listen { "LISTEN" }, learn { "LEARN NOISE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt, listenAtt;

    LevelMeter inMeter, outMeter, nrMeter { true }, gateMeter { true }, compMeter { true };
    juce::Rectangle<int> cleanPanel, pushPanel, outPanel, meterPanel;
    juce::String statusText, pushText, rideText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxClearEditor)
};

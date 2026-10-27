#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DspUtils.h"
#include "DeClicker.h"
#include "SpectralDenoiser.h"
#include "HumRemover.h"
#include "PopRemover.h"
#include "AutoTools.h"
#include "MicProfiles.h"

class VoxClearProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    VoxClearProcessor();
    ~VoxClearProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // factory presets are exposed as host programs too (FL's preset menu)
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    DeClicker declicker;
    SpectralDenoiser denoiser;
    HumRemover hum;
    PopRemover pops;

    // AUTO CLEAN: what the plugin currently uses (shown on the auto dials)
    struct AutoValues
    {
        std::atomic<float> lowCut { 80.0f }, declick { 0.5f }, noise { 0.3f }, gateDb { -80.0f }, snrDb { 0.0f };
        std::atomic<bool> ready { false };
    };
    AutoValues autoValues;

    // MATCH curve (learned from your microphone, stored with the project)
    std::array<std::atomic<float>, ToneSpec::matchBands> matchCurve {};
    std::atomic<bool> hasMatch { false };
    void clearMatch();

    // User defaults (stored in a small settings file next to the plugin's app data):
    // your mic + MATCH are remembered for every new instance, plus an optional full default setting.
    void saveAsDefault();
    void clearDefault();
    bool hasUserDefault() const;
    static juce::File& settingsFileOverride();   // tests only

    std::atomic<int> uiWidth { 0 };   // editor size, stored with the project

    // Metering (read by the editor)
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };
    std::atomic<float> gateGainDb { 0.0f }, compGainDb { 0.0f }, mudCutDb { 0.0f }, boxCutDb { 0.0f }, essCutDb { 0.0f }, rideDb { 0.0f };
    std::atomic<bool> sidechainActive { false };

private:
    void timerCallback() override;
    std::unique_ptr<juce::XmlElement> createStateXml();
    void restoreFromXml (const juce::XmlElement& xml);
    void applyUserDefaults();
    juce::String matchString() const;
    void setMatchFromString (const juce::String& text, bool has);

    std::unique_ptr<juce::PropertiesFile> settings;
    std::atomic<int> matchVersion { 0 }, lastSavedMic { -1 };
    int lastSavedMatchVersion = 0;

    void processChunk (juce::AudioBuffer<float>& main, const float* side, int chans);
    void updateAutoClean (int numSamples);
    juce::String autoStatsString() const;
    void restoreAutoStats (const juce::String& text);
    void updatePushFilters (float mudCut, float boxCut, float essCut);
    ToneSpec makeToneSpec (bool listening) const;

    double sr = 44100.0;
    int numCh = 2, blockCap = 512, currentProgram = 0;

    Biquad lowCut1, lowCut2;
    Biquad mudDetect, boxDetect, essDetect;   // detection band-passes (mono sum)
    Biquad mudEq, boxEq, essEq, presenceEq, airEq;
    SimpleDelay dryDelay;
    juce::AudioBuffer<float> dryBuffer, sideBuffer;

    // smoothed parameters (no zipper noise on knob moves or preset changes)
    float lowCutS = 80.0f, presDbS = 0.0f, airDbS = 0.0f, makeupS = 1.0f, tubeS = 0.0f;
    float tubeLp[2] = { 0.0f, 0.0f }, dcIn[2] = { 0.0f, 0.0f }, dcOut[2] = { 0.0f, 0.0f };
    float gateEnv = 0.0f, gateGain = 1.0f, gateHold = 0.0f;
    float fullEnv = 0.0f, mudEnv = 0.0f, boxEnv = 0.0f, essEnv = 0.0f;
    float mudCutS = 0.0f, boxCutS = 0.0f, essCutS = 0.0f;
    float compEnv = 0.0f, compGain = 1.0f;
    float vocEnv = 0.0f, beatEnv = 0.0f, rideAvgDiff = 0.0f, rideGainDb = 0.0f;
    float rideActiveSec = 0.0f;

    // AUTO CLEAN state
    QuantileTracker gatePauseDb, gateVoiceDb;   // post-noise-reduction level in pauses / while singing
    int gateTrackCount = 0;
    float autoNoiseS = 0.3f, autoDeclickS = 0.5f, autoGateS = -80.0f, autoLowCutS = 80.0f;

    juce::SmoothedValue<float> outGainS, mixS;

    std::atomic<float>* pLowCut;
    std::atomic<float>* pDeclick;
    std::atomic<float>* pNoise;
    std::atomic<float>* pGate;
    std::atomic<float>* pListen;
    std::atomic<float>* pPush;
    std::atomic<float>* pMud;
    std::atomic<float>* pBox;
    std::atomic<float>* pPresence;
    std::atomic<float>* pAir;
    std::atomic<float>* pLevel;
    std::atomic<float>* pRide;
    std::atomic<float>* pOutput;
    std::atomic<float>* pMix;
    std::atomic<float>* pBypass;
    std::atomic<float>* pMicSource;
    std::atomic<float>* pMicTarget;
    std::atomic<float>* pMicAmount;
    std::atomic<float>* pSmooth;
    std::atomic<float>* pTube;
    std::atomic<float>* pMatchOn;
    std::atomic<float>* pAutoClean;
    std::atomic<float>* pAutoStrength;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxClearProcessor)
};

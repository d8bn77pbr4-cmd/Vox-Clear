#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DspUtils.h"
#include "DeClicker.h"
#include "SpectralDenoiser.h"
#include "MicProfiles.h"

class VoxClearProcessor : public juce::AudioProcessor
{
public:
    VoxClearProcessor();
    ~VoxClearProcessor() override = default;

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

    // MATCH curve (learned from your microphone, stored with the project)
    std::array<std::atomic<float>, ToneSpec::matchBands> matchCurve {};
    std::atomic<bool> hasMatch { false };
    void clearMatch();

    // Metering (read by the editor)
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };
    std::atomic<float> gateGainDb { 0.0f }, compGainDb { 0.0f }, mudCutDb { 0.0f }, boxCutDb { 0.0f }, essCutDb { 0.0f }, rideDb { 0.0f };
    std::atomic<bool> sidechainActive { false };

private:
    void processChunk (juce::AudioBuffer<float>& main, const float* side, int chans);
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxClearProcessor)
};

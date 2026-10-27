#pragma once

#include "DspUtils.h"
#include "AutoTools.h"
#include <atomic>

// Plosive pops ("p", "b" blowing into the mic): a sudden, bass-dominated burst far above the voice's
// usual loud bass level (90th percentile while the voice is present, pauses and pops excluded). Only the band below ~120 Hz is turned down, and only for short bursts - a sung low
// note lasts longer than 80 ms and is left alone (the reference then adapts to it).
class PopRemover
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();
    void setSensitivity (float ratio) { k = ratio; }   // burst must exceed k x usual bass level (lower = more sensitive)
    void process (juce::AudioBuffer<float>& buffer, int numChannels);

    float getPopRate() const { return popRate.load(); }   // pops per minute (last ~30 s)
    float getLastCutDb() const { return lastCutDb.load(); }
    int getPopCount() const { return popCount.load(); }

private:
    double sr = 44100.0;
    int numCh = 2;
    Biquad lowpass;
    float k = 3.0f;
    float lowEnv = 0.0f, fullEnv = 0.0f, fullPeak = 0.0f, gain = 1.0f;
    float attC = 0.0f, relC = 0.0f, peakDecay = 0.0f, gainAtt = 0.0f, gainRel = 0.0f;
    QuantileTracker lowLoudDb;   // typical loud bass level of this voice
    int trackCount = 0, trackEvery = 48;
    int burst = 0, maxBurst = 1, calm = 0, calmNeeded = 1;
    double warmup = 0.0;
    bool ducking = false;
    float popAcc = 0.0f;
    std::atomic<float> popRate { 0.0f }, lastCutDb { 0.0f };
    std::atomic<int> popCount { 0 };
};

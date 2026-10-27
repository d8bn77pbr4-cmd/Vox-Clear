#pragma once

#include "DspUtils.h"
#include <vector>
#include <atomic>

// Mouth-click remover. Clicks are very short (< 5 ms) bursts in the high band that stand far
// above the recent high-band peak level (a peak reference, so the voice's own pitch pulses don't count). Consonants ("t", "k", "s") last longer and are left alone:
// a burst is only treated as a click once it has ended within the maximum click length, which the
// lookahead lets us decide before the audio is output. Only the high band is turned down, so the
// body of the voice stays untouched.
class DeClicker
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();
    void setAmount (float a) { amount = juce::jlimit (0.0f, 1.0f, a); }
    int getLatency() const { return lookahead; }
    void process (juce::AudioBuffer<float>& buffer, int numChannels);

    std::atomic<int> clickCount { 0 };

private:
    void markRun (juce::int64 start, juce::int64 end, float peakRatio);

    double sr = 44100.0;
    int numCh = 2, lookahead = 1, ringSize = 2, maxClickLen = 1, margin = 1;
    float amount = 0.5f;
    Biquad hpDetect, hpSplit;   // detect clicks above 3 kHz, attenuate above 1.5 kHz
    std::vector<std::vector<float>> xRing, hRing;
    std::vector<float> gainRing;
    juce::int64 now = 0, runStart = -1, lastClickEnd = -1000000;
    juce::int64 refractoryUntil = 0;
    int lowCount = 0, endHold = 1;
    bool longRun = false;
    float runRef = 0.0f, runPeak = 0.0f, runPeakLevel = 0.0f, fastEnv = 0.0f, refEnv = 0.0f, fastRel = 0.0f, refRel = 0.0f, smoothGain = 1.0f, gainC = 0.0f;
};

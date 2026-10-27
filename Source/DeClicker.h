#pragma once

#include "DspUtils.h"
#include "AutoTools.h"
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
    // manual knob: one value sets detection and depth together
    void setAmount (float a) { a = juce::jlimit (0.0f, 1.0f, a); setDetection (a > 0.0f ? juce::jmap (a, 4.5f, 1.9f) : 0.0f); setDepth (a); }
    void setDetection (float ratio) { thr = ratio; }   // burst must exceed ratio x recent peak level (lower = more sensitive, 0 = off)
    void setDepth (float d) { depth = juce::jlimit (0.0f, 1.0f, d); }
    // Bursts as loud as the speaker's typical loud high-frequency sounds (consonant peaks) are kept:
    // mouth clicks are clearly quieter than speech, click consonants of languages like Xhosa are not.
    void setProtection (float factor) { protection = factor; }

    float getClickRate() const { return clickRate.load(); }       // per minute, last ~30 s
    float getCrackleRate() const { return crackleRate.load(); }   // very short clicks (< 1.5 ms) per minute
    int getProtectedCount() const { return protectedCount.load(); }
    int getLatency() const { return lookahead; }
    void process (juce::AudioBuffer<float>& buffer, int numChannels);

    std::atomic<int> clickCount { 0 };

private:
    void markRun (juce::int64 start, juce::int64 end, float peakRatio);

    double sr = 44100.0;
    int numCh = 2, lookahead = 1, ringSize = 2, maxClickLen = 1, margin = 1;
    float thr = 3.5f, depth = 0.5f, protection = 1.0f;
    QuantileTracker speechPeakDb, hfFloorDb;   // typical loud / quiet high-band level of this speaker
    int trackCount = 0, trackEvery = 48, crackleLen = 1;
    float clickAcc = 0.0f, crackleAcc = 0.0f;
    std::atomic<float> clickRate { 0.0f }, crackleRate { 0.0f };
    std::atomic<int> protectedCount { 0 };
    Biquad hpDetect, hpSplit;   // detect clicks above 3 kHz, attenuate above 1.5 kHz
    std::vector<std::vector<float>> xRing, hRing;
    std::vector<float> gainRing;
    juce::int64 now = 0, runStart = -1, lastClickEnd = -1000000;
    juce::int64 refractoryUntil = 0;
    int lowCount = 0, endHold = 1;
    bool longRun = false;
    float runRef = 0.0f, runPeak = 0.0f, runPeakLevel = 0.0f, fastEnv = 0.0f, refEnv = 0.0f, fastRel = 0.0f, refRel = 0.0f, smoothGain = 1.0f, gainC = 0.0f;
};

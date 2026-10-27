#pragma once

#include "DspUtils.h"
#include <array>
#include <atomic>

// Mains hum (50 Hz Europe / 60 Hz Americas, ground loops) is found automatically: during pauses the
// level at the hum harmonics is compared with the level right next to them (Goertzel analysis on a
// decimated signal, 0.5 Hz resolution). Once confirmed, narrow notches remove the harmonics.
class HumRemover
{
public:
    static constexpr int numHarmonics = 8;

    void prepare (double sampleRate, int numChannels);
    void reset();
    void setDepth (float db) { depthDb = db; }   // e.g. -24 dB when hum is confirmed
    void process (juce::AudioBuffer<float>& buffer, int numChannels);

    int getBase() const { return base.load(); }          // 0 = none, 50 or 60
    bool isActive() const { return activeDb.load() < -1.0f; }
    float getConfidence() const { return confidence; }
    void seed (int b, float conf) { if (b == 50 || b == 60) { base.store (b); confidence = conf; } }

private:
    void analyseWindow();
    void updateNotches (float db);

    static constexpr int numBases = 2, analysisHarmonics = 6;
    double sr = 44100.0, dsr = 6000.0;
    int numCh = 2, decim = 8, decimCount = 0, winLen = 12000, winCount = 0;
    Biquad antiAlias;
    struct Goertzel { float coeff = 0.0f, s1 = 0.0f, s2 = 0.0f; };
    std::array<std::array<std::array<Goertzel, 3>, analysisHarmonics>, numBases> bank {};   // [base][k][target, below, above]
    float voicePeak = 0.0f, voiceDecay = 0.9999f, levelEnv = 0.0f, envRel = 0.999f;
    int pauseSamples = 0;
    float confidence = 0.0f, depthDb = -24.0f, currentDb = 0.0f;
    std::atomic<int> base { 0 };
    std::atomic<float> activeDb { 0.0f };
    std::array<Biquad, numHarmonics> notches;
    int numNotches = 0, notchBase = 0;
};

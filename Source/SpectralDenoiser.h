#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>
#include "MicProfiles.h"
#include "AutoTools.h"

// STFT stage. Its main job is noise reduction (hiss, room tone, fan noise): the noise floor per
// frequency is tracked automatically or taken from a learned profile, and gains are smoothed over
// time and frequency to avoid "musical noise".
// Because the FFT is computed anyway, the same frames also carry the microphone tone curve,
// resonance smoothing (harsh capsule peaks), proximity control and MATCH learning at almost no cost.
class SpectralDenoiser
{
public:
    static constexpr int numDisplayPoints = 300;

    void prepare (double sampleRate, int numChannels);
    void reset();
    void setAmount (float a) { amount = juce::jlimit (0.0f, 1.0f, a); }
    void setTone (const ToneSpec& spec) { tone = spec; }
    int getLatency() const { return fftSize; }
    void process (juce::AudioBuffer<float>& buffer, int numChannels);

    // LEARN: capture the noise profile from a few seconds of noise only (e.g. a pause)
    void requestLearn() { learnRequested.store (true); }
    void clearProfile() { clearRequested.store (true); }
    bool isLearning() const { return learning.load() || learnRequested.load(); }
    bool hasProfile() const { return profileActive.load(); }

    // noise profile persistence (message thread): "sampleRate;fftSize;dB,dB,..." - empty = no profile
    juce::String getProfileString() const;
    void setProfileString (const juce::String& text);

    // MATCH: learn the microphone's long-term balance while singing (6 s of voice)
    void requestMatch() { matchRequested.store (true); }
    bool isMatching() const { return matching.load() || matchRequested.load(); }
    float getMatchProgress() const { return matchProgress.load(); }
    bool consumeMatchResult (std::array<float, ToneSpec::matchBands>& out);

    float getReductionDb() const { return reductionDb.load(); }   // noise reduction, energy-weighted dB
    float getNoiseFloorDb() const { return noiseFloorDb.load(); } // broadband estimate, dBFS
    float getSmoothDb() const { return smoothDb.load(); }         // strongest resonance cut in the last frame
    float getProximityDb() const { return proximityDb.load(); }   // current bass build-up cut

    // AUTO analysis of the voice (same frames, no extra FFT)
    float getVoiceLevelDb() const { return voiceLevelDb.load(); }  // typical loud voice level (same scale as the noise floor)
    float getF0Low() const { return f0Low.load(); }                // lowest regularly used fundamental, Hz
    float getVoicedSeconds() const { return voicedSeconds.load(); }
    float getRumbleDb() const { return rumbleDb.load(); }          // noise below 80 Hz relative to 150-400 Hz
    void seedVoiceStats (float levelDb, float f0, float seconds);

    void copyDisplay (std::array<float, numDisplayPoints>& spectrum, std::array<float, numDisplayPoints>& noise,
                      std::array<float, numDisplayPoints>& reduction, std::array<float, numDisplayPoints>& toneCurve);
    static float displayPointToFreq (float i) { return 20.0f * std::pow (1000.0f, i / (numDisplayPoints - 1)); }

private:
    void processFrame();
    void updateToneTarget();
    void finishMatch();
    static void boxSmooth (const std::vector<float>& in, std::vector<float>& out, std::vector<double>& prefix, float halfWidthFactor);

    double sr = 44100.0;
    int fftOrder = 11, fftSize = 2048, hop = 512, numCh = 2, pos = 0, hopCounter = 0;
    float olaNorm = 1.0f, powNorm = 1.0f, amount = 0.5f;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window;
    std::vector<std::vector<float>> inRing, outRing, fftData;
    std::vector<float> power, smoothPow, minPow, winMinCur, winMinPrev, learned, learnAcc, gain, gainSm, gainTmp;
    bool firstFrame = true;
    int frameCount = 0, winFrames = 0, winLength = 1;
    int learnFrames = 0, learnTarget = 1;

    // tone / smoothing / proximity
    ToneSpec tone, cachedTone;
    bool toneValid = false;
    std::vector<float> binFreq, smoothWeight, proxShape, toneTarget, toneDb, total, ltPow, powDb, ref1, ref2, resRed, resTmp;
    std::vector<double> prefix;
    int proxLo = 1, proxMid = 2, proxHi = 3, proxBins = 1;
    float proxAvg = 0.0f, proxTime = 0.0f, proxCut = 0.0f;

    // AUTO voice statistics
    QuantileTracker voiceQ, f0Q;
    int f0Lo = 1, f0Hi = 2, f0Search = 3, rumbleLo = 1, rumbleMid = 2, rumbleHi = 3;
    std::atomic<float> voiceLevelDb { -200.0f }, f0Low { 0.0f }, voicedSeconds { 0.0f }, rumbleDb { 0.0f };

    // MATCH learning
    std::vector<double> matchAcc;
    int matchFrames = 0, matchTarget = 1;
    std::array<float, ToneSpec::matchBands> matchResult {};

    std::atomic<bool> learnRequested { false }, clearRequested { false }, learning { false }, profileActive { false };
    std::atomic<bool> matchRequested { false }, matching { false }, matchFinished { false };
    std::atomic<bool> profilePending { false };
    std::vector<float> pendingProfileDb;   // written by the message thread only while profilePending is false
    double pendingSr = 44100.0;
    int pendingFft = 2048;
    std::atomic<float> reductionDb { 0.0f }, noiseFloorDb { -120.0f }, smoothDb { 0.0f }, proximityDb { 0.0f }, matchProgress { 0.0f };

    juce::SpinLock displayLock;
    std::array<float, numDisplayPoints> dispSpec {}, dispNoise {}, dispRed {}, dispTone {};
    std::array<int, numDisplayPoints> dispLo {}, dispHi {};
};

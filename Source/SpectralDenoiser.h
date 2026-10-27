#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

// STFT noise reduction for hiss, room tone and fan noise.
// The noise floor per frequency is tracked automatically (rising minimum of the smoothed power),
// or taken from a learned profile. Gains are smoothed over time and frequency to avoid "musical noise".
class SpectralDenoiser
{
public:
    static constexpr int numDisplayPoints = 300;

    void prepare (double sampleRate, int numChannels);
    void reset();
    void setAmount (float a) { amount = juce::jlimit (0.0f, 1.0f, a); }
    int getLatency() const { return fftSize; }
    void process (juce::AudioBuffer<float>& buffer, int numChannels);

    // LEARN: capture the noise profile from a few seconds of noise only (e.g. a pause)
    void requestLearn() { learnRequested.store (true); }
    void clearProfile() { clearRequested.store (true); }
    bool isLearning() const { return learning.load() || learnRequested.load(); }
    bool hasProfile() const { return profileActive.load(); }

    float getReductionDb() const { return reductionDb.load(); }   // energy-weighted, positive dB
    float getNoiseFloorDb() const { return noiseFloorDb.load(); } // broadband estimate, dBFS

    void copyDisplay (std::array<float, numDisplayPoints>& spectrum, std::array<float, numDisplayPoints>& noise,
                      std::array<float, numDisplayPoints>& reduction);
    static float displayPointToFreq (float i) { return 20.0f * std::pow (1000.0f, i / (numDisplayPoints - 1)); }

private:
    void processFrame();

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

    std::atomic<bool> learnRequested { false }, clearRequested { false }, learning { false }, profileActive { false };
    std::atomic<float> reductionDb { 0.0f }, noiseFloorDb { -120.0f };

    juce::SpinLock displayLock;
    std::array<float, numDisplayPoints> dispSpec {}, dispNoise {}, dispRed {};
    std::array<int, numDisplayPoints> dispLo {}, dispHi {};
};

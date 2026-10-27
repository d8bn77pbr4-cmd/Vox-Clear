#include "PopRemover.h"
#include "AutoTools.h"

void PopRemover::prepare (double sampleRate, int numChannels)
{
    sr = sampleRate;
    numCh = juce::jlimit (1, Biquad::maxChannels, numChannels);
    auto c = [this] (double s) { return (float) std::exp (-1.0 / (s * sr)); };
    attC = c (0.001); relC = c (0.03);
    peakDecay = c (5.0);
    trackEvery = juce::jmax (1, (int) std::round (0.001 * sr));
    gainAtt = c (0.0015); gainRel = c (0.05);
    maxBurst = (int) (0.08 * sr);
    calmNeeded = (int) (0.03 * sr);
    lowpass.setHighPass (sr, 120.0, 0.707);   // used as complement: low = x - highpass(x); below most voices' fundamental
    reset();
}

void PopRemover::reset()
{
    lowpass.reset();
    lowEnv = fullEnv = fullPeak = 0.0f;
    lowLoudDb = {};
    trackCount = 0;
    gain = 1.0f;
    burst = 0;
    warmup = 0.0;
    ducking = false;
    popAcc = 0.0f;
}

void PopRemover::process (juce::AudioBuffer<float>& buffer, int channels)
{
    channels = juce::jmin (channels, numCh);
    const int n = buffer.getNumSamples();
    float minGain = 1.0f;
    float low[Biquad::maxChannels] = {};

    for (int i = 0; i < n; ++i)
    {
        float lowLvl = 0.0f, fullLvl = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
        {
            const float x = buffer.getSample (ch, i);
            low[ch] = x - lowpass.process (x, ch);   // exact complement of the high-pass: low band below 120 Hz
            lowLvl = juce::jmax (lowLvl, std::abs (low[ch]));
            fullLvl = juce::jmax (fullLvl, std::abs (x));
        }
        lowEnv = lowLvl > lowEnv ? lowLvl + (lowEnv - lowLvl) * attC : lowLvl + (lowEnv - lowLvl) * relC;
        fullEnv = fullLvl > fullEnv ? fullLvl + (fullEnv - fullLvl) * attC : fullLvl + (fullEnv - fullLvl) * relC;

        fullPeak = juce::jmax (fullEnv, fullPeak * peakDecay);
        const bool voicePresent = fullEnv > 0.1f * fullPeak && fullEnv > 1.0e-3f;
        if (voicePresent) warmup += 1.0 / sr;
        const float ref = lowLoudDb.valid ? std::pow (10.0f, lowLoudDb.value / 20.0f) : 1.0f;

        // a pop: bass far above the voice's loud bass level and dominating the signal
        const bool exceed = warmup > 1.0 && lowEnv > k * ref && lowEnv > 0.003f && lowEnv > 0.5f * fullEnv;
        burst = exceed ? burst + 1 : 0;
        const bool isPop = exceed && burst < maxBurst;

        // learn the loud bass level while singing (not in pauses, not from pops; a long low note counts)
        if (++trackCount >= trackEvery)
        {
            trackCount = 0;
            const bool popLike = lowEnv > 0.7f * k * ref && burst < maxBurst;   // near-misses must not inflate the reference
            if (voicePresent && ! popLike)
                lowLoudDb.add (gainToDb (lowEnv), 0.9f, burst >= maxBurst ? 0.3f : 0.05f);
        }

        const float target = isPop ? juce::jlimit (0.05f, 1.0f, 1.5f * ref / lowEnv) : 1.0f;   // down to just above the voice's own bass
        gain = target < gain ? target + (gain - target) * gainAtt : target + (gain - target) * gainRel;
        minGain = juce::jmin (minGain, gain);
        if (isPop && ! ducking && target < 0.7f) { popAcc += 1.0f; ++popCount; ducking = true; }
        calm = exceed ? 0 : calm + 1;
        if (calm > calmNeeded) ducking = false;   // one pop = one count, even if the voice on top makes it flicker

        if (gain < 0.9999f)
            for (int ch = 0; ch < channels; ++ch)
                buffer.setSample (ch, i, buffer.getSample (ch, i) - (1.0f - gain) * low[ch]);
    }
    popAcc *= (float) std::exp (-(double) n / (30.0 * sr));
    popRate.store (popAcc * 2.0f);
    lastCutDb.store (gainToDb (minGain));
}

#include "HumRemover.h"
#include "AutoTools.h"

void HumRemover::prepare (double sampleRate, int numChannels)
{
    sr = sampleRate;
    numCh = juce::jlimit (1, Biquad::maxChannels, numChannels);
    decim = juce::jmax (1, (int) std::round (sr / 6000.0));
    dsr = sr / decim;
    winLen = (int) (2.0 * dsr);   // 2 s window -> 0.5 Hz resolution
    antiAlias.setBandPass (sr, 300.0, 0.4);   // keeps 50..800 Hz, removes what would alias after decimation
    const double baseHz[numBases] = { 50.0, 60.0 };
    for (int b = 0; b < numBases; ++b)
        for (int k = 0; k < analysisHarmonics; ++k)
        {
            const double f = baseHz[b] * (k + 1);
            const double freqs[3] = { f, f - 25.0, f + 25.0 };
            for (int j = 0; j < 3; ++j)
                bank[(size_t) b][(size_t) k][(size_t) j].coeff = (float) (2.0 * std::cos (juce::MathConstants<double>::twoPi * freqs[j] / dsr));
        }
    voiceDecay = (float) std::exp (-1.0 / (10.0 * sr));
    envRel = (float) std::exp (-1.0 / (0.02 * sr));
    reset();
}

void HumRemover::reset()
{
    for (auto& b : bank) for (auto& k : b) for (auto& g : k) g.s1 = g.s2 = 0.0f;
    antiAlias.reset();
    for (auto& n : notches) n.reset();
    decimCount = winCount = pauseSamples = 0;
    voicePeak = levelEnv = 0.0f;
    currentDb = 0.0f;
    activeDb.store (0.0f);
    notchBase = 0;
}

void HumRemover::analyseWindow()
{
    // only the pause moments were analysed (voice parts were fed as zeros); need enough of them
    const bool pause = pauseSamples > winLen / 4;
    float score[numBases] = { 0.0f, 0.0f };
    for (int b = 0; b < numBases; ++b)
    {
        for (int k = 0; k < analysisHarmonics; ++k)
        {
            float p[3];
            for (int j = 0; j < 3; ++j)
            {
                auto& g = bank[(size_t) b][(size_t) k][(size_t) j];
                p[j] = g.s1 * g.s1 + g.s2 * g.s2 - g.coeff * g.s1 * g.s2;
                g.s1 = g.s2 = 0.0f;
            }
            const float prominence = 10.0f * std::log10 ((p[0] + 1.0e-20f) / (0.5f * (p[1] + p[2]) + 1.0e-20f));
            score[b] += juce::jlimit (-10.0f, 40.0f, prominence) / analysisHarmonics;
        }
    }
    if (! pause) return;

    const int best = score[1] > score[0] ? 1 : 0;
    const bool found = score[best] > 10.0f && score[best] - score[1 - best] > 4.0f;
    if (found)
    {
        const int b = best == 0 ? 50 : 60;
        if (base.load() != b) confidence = 0.0f;
        base.store (b);
        confidence = juce::jmin (5.0f, confidence + 1.0f);
    }
    else
        confidence = juce::jmax (0.0f, confidence - 0.5f);   // hum went away (e.g. other room / cable)
}

void HumRemover::updateNotches (float db)
{
    const int b = base.load();
    if (b != notchBase)
    {
        for (auto& n : notches) n.reset();
        notchBase = b;
    }
    numNotches = 0;
    for (int k = 1; k <= numHarmonics && b > 0; ++k)
    {
        const double f = (double) b * k;
        if (f > sr * 0.45) break;
        notches[(size_t) numNotches++].setPeak (sr, f, 30.0, db);   // Q 30: a few Hz wide, the voice keeps its harmonics
    }
}

void HumRemover::process (juce::AudioBuffer<float>& buffer, int channels)
{
    channels = juce::jmin (channels, numCh);
    const int n = buffer.getNumSamples();

    // ---- analysis (mono, decimated) ----
    for (int i = 0; i < n; ++i)
    {
        float m = 0.0f;
        for (int ch = 0; ch < channels; ++ch) m += buffer.getSample (ch, i);
        m /= (float) channels;
        voicePeak = juce::jmax (std::abs (m), voicePeak * voiceDecay);
        levelEnv = juce::jmax (std::abs (m), levelEnv * envRel);
        // a pause moment: clearly below the voice; hum is then not masked by singing
        const bool pauseNow = voicePeak > 1.0e-4f && levelEnv > 1.0e-7f && levelEnv < voicePeak * 0.056f;   // -25 dB
        const float filtered = antiAlias.process (m, 0);
        if (++decimCount >= decim)
        {
            decimCount = 0;
            const float in = pauseNow ? filtered : 0.0f;
            if (pauseNow) ++pauseSamples;
            for (auto& b : bank)
                for (auto& k : b)
                    for (auto& g : k)
                    {
                        const float s = in + g.coeff * g.s1 - g.s2;
                        g.s2 = g.s1;
                        g.s1 = s;
                    }
            if (++winCount >= winLen)
            {
                winCount = 0;
                analyseWindow();
                pauseSamples = 0;
            }
        }
    }

    // ---- removal: glide the notch depth in/out ----
    const float target = confidence >= 2.0f ? depthDb : 0.0f;
    if (std::abs (currentDb - target) > 0.05f || base.load() != notchBase)
    {
        currentDb += (target - currentDb) * juce::jmin (1.0f, (float) n / (float) (0.3 * sr));
        updateNotches (currentDb);
    }
    activeDb.store (currentDb);
    if (currentDb > -0.1f || numNotches == 0) return;

    for (int ch = 0; ch < channels; ++ch)
    {
        float* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            float x = d[i];
            for (int k = 0; k < numNotches; ++k) x = notches[(size_t) k].process (x, ch);
            d[i] = x;
        }
    }
}

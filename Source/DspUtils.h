#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

// RBJ biquad with per-channel state; coefficients can be changed at audio rate without allocation
struct Biquad
{
    static constexpr int maxChannels = 2;

    void reset() { for (auto& s : state) s = { 0.0f, 0.0f }; }

    void setPeak (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = juce::MathConstants<double>::twoPi * f / fs;
        const double alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
        set (1 + alpha * A, -2 * c, 1 - alpha * A, 1 + alpha / A, -2 * c, 1 - alpha / A);
    }

    void setHighShelf (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = juce::MathConstants<double>::twoPi * f / fs;
        const double alpha = std::sin (w) / (2.0 * q), c = std::cos (w), sA = 2.0 * std::sqrt (A) * alpha;
        set (A * ((A + 1) + (A - 1) * c + sA), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sA),
             (A + 1) - (A - 1) * c + sA, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sA);
    }

    void setHighPass (double fs, double f, double q)
    {
        const double w = juce::MathConstants<double>::twoPi * f / fs, alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
        set ((1 + c) * 0.5, -(1 + c), (1 + c) * 0.5, 1 + alpha, -2 * c, 1 - alpha);
    }

    void setBandPass (double fs, double f, double q)
    {
        const double w = juce::MathConstants<double>::twoPi * f / fs, alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
        set (alpha, 0.0, -alpha, 1 + alpha, -2 * c, 1 - alpha);
    }

    float process (float x, int ch)
    {
        auto& s = state[(size_t) ch];
        const float y = b0 * x + s[0];
        s[0] = b1 * x - a1 * y + s[1];
        s[1] = b2 * x - a2 * y;
        return y;
    }

private:
    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    {
        b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
        a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
    }

    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    std::array<std::array<float, 2>, maxChannels> state {};
};

// Integer-delay circular buffer (latency compensation for dry / listen paths)
class SimpleDelay
{
public:
    void prepare (int numChannels, int maxDelay)
    {
        buffer.setSize (numChannels, maxDelay + 1);
        buffer.clear();
        writePos = 0;
    }
    void setDelay (int d) { delay = juce::jlimit (0, buffer.getNumSamples() - 1, d); }

    void process (juce::AudioBuffer<float>& io, int numChannels)
    {
        const int size = buffer.getNumSamples();
        const int chans = juce::jmin (numChannels, buffer.getNumChannels());
        int wp = writePos;
        for (int i = 0; i < io.getNumSamples(); ++i)
        {
            int rp = wp - delay;
            if (rp < 0) rp += size;
            for (int ch = 0; ch < chans; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                d[wp] = io.getSample (ch, i);
                io.setSample (ch, i, d[rp]);
            }
            if (++wp >= size) wp = 0;
        }
        writePos = wp;
    }

private:
    juce::AudioBuffer<float> buffer;
    int writePos = 0, delay = 0;
};

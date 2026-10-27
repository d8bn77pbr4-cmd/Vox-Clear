#include "DeClicker.h"

void DeClicker::prepare (double sampleRate, int numChannels)
{
    sr = sampleRate;
    numCh = juce::jlimit (1, Biquad::maxChannels, numChannels);
    lookahead = (int) std::round (0.008 * sr);     // decide within 8 ms
    maxClickLen = (int) std::round (0.005 * sr);   // anything longer is a consonant, not a click
    margin = juce::jmax (1, (int) std::round (0.0007 * sr));
    ringSize = 2 * lookahead;
    xRing.assign ((size_t) numCh, std::vector<float> ((size_t) ringSize, 0.0f));
    hRing.assign ((size_t) numCh, std::vector<float> ((size_t) ringSize, 0.0f));
    gainRing.assign ((size_t) ringSize, 1.0f);
    hpDetect.setHighPass (sr, 3000.0, 0.707);
    hpSplit.setHighPass (sr, 1500.0, 0.707);
    endHold = juce::jmax (1, (int) std::round (0.0005 * sr));
    fastRel = (float) std::exp (-1.0 / (0.0003 * sr));
    // peak hold reference: jumps to every peak and
    refRel = (float) std::exp (-1.0 / (0.050 * sr));     // falls slowly, so the voice's pitch pulses keep it up
    gainC = (float) std::exp (-1.0 / (0.0003 * sr));
    reset();
}

void DeClicker::reset()
{
    for (auto& r : xRing) std::fill (r.begin(), r.end(), 0.0f);
    for (auto& r : hRing) std::fill (r.begin(), r.end(), 0.0f);
    std::fill (gainRing.begin(), gainRing.end(), 1.0f);
    hpDetect.reset();
    hpSplit.reset();
    now = 0; runStart = -1; longRun = false; lastClickEnd = -1000000; lowCount = 0; refractoryUntil = 0;
    fastEnv = refEnv = runPeak = runRef = 0.0f;
    smoothGain = 1.0f;
}

void DeClicker::markRun (juce::int64 start, juce::int64 end, float peakRatio)
{
    // bring the click down towards the surrounding level; amount controls how far
    const float toRef = juce::jlimit (0.02f, 1.0f, 1.5f / peakRatio);
    const float g = std::pow (toRef, juce::jmin (1.0f, 0.4f + amount));
    for (juce::int64 t = juce::jmax ((juce::int64) 0, start - margin); t <= end + margin; ++t)
    {
        float ramp = 1.0f;
        if (t < start) ramp = (float) (t - (start - margin)) / (float) margin;
        else if (t > end) ramp = (float) ((end + margin) - t) / (float) margin;
        auto& slot = gainRing[(size_t) (t % ringSize)];
        slot = juce::jmin (slot, 1.0f - ramp * (1.0f - g));
    }
    if (start - lastClickEnd > (juce::int64) (0.003 * sr))   // bursts within 3 ms belong to the same click
        ++clickCount;
    lastClickEnd = end;
}

void DeClicker::process (juce::AudioBuffer<float>& buffer, int channels)
{
    channels = juce::jmin (channels, numCh);
    const float thr = juce::jmap (amount, 5.0f, 2.0f);   // how far above the recent peak level
    const bool active = amount > 0.0f;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto w = (size_t) (now % ringSize);
        float peak = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
        {
            const float x = buffer.getSample (ch, i);
            xRing[(size_t) ch][w] = x;
            hRing[(size_t) ch][w] = hpSplit.process (x, ch);
            peak = juce::jmax (peak, std::abs (hpDetect.process (x, ch)));
        }

        fastEnv = peak > fastEnv ? peak : fastEnv * fastRel;
        const float slow = refEnv + 1.0e-5f;

        if (runStart < 0 && ! longRun)
        {
            if (active && now >= refractoryUntil && fastEnv > thr * slow && fastEnv > 3.0e-4f)
            {
                runStart = now;
                runRef = slow;           // frozen pre-onset level
                runPeak = fastEnv / slow;
                runPeakLevel = fastEnv;
                lowCount = 0;
            }
        }
        else
        {
            // a burst has ended once it is back near the old level or 26 dB below its own peak
            const bool stillHigh = fastEnv > juce::jmax (0.7f * thr * runRef, 0.05f * runPeakLevel);
            if (runStart >= 0)
            {
                runPeak = juce::jmax (runPeak, fastEnv / runRef);
                runPeakLevel = juce::jmax (runPeakLevel, fastEnv);
                lowCount = stillHigh ? 0 : lowCount + 1;
                if (lowCount >= endHold)   // quiet for 0.5 ms: the burst is over
                {
                    markRun (runStart, now - lowCount, runPeak);
                    runStart = -1;
                }
                else if (now - runStart > maxClickLen)
                {
                    runStart = -1;       // too long: consonant, leave it alone
                    longRun = true;
                }
            }
            else if (! stillHigh)
            {
                longRun = false;
                refractoryUntil = now + (juce::int64) (0.02 * sr);   // ignore the consonant's tail
            }
        }
        if (runStart < 0)
            refEnv = juce::jmax (peak, refEnv * refRel);

        // output the sample from 'lookahead' ago with its (smoothed) high-band gain
        const juce::int64 outT = now - lookahead;
        if (outT >= 0)
        {
            const auto r = (size_t) (outT % ringSize);
            const float g = gainRing[r];
            gainRing[r] = 1.0f;
            smoothGain = g + (smoothGain - g) * gainC;
            for (int ch = 0; ch < channels; ++ch)
                buffer.setSample (ch, i, xRing[(size_t) ch][r] - (1.0f - smoothGain) * hRing[(size_t) ch][r]);
        }
        else
        {
            for (int ch = 0; ch < channels; ++ch)
                buffer.setSample (ch, i, 0.0f);
        }
        ++now;
    }
}

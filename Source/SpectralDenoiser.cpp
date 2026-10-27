#include "SpectralDenoiser.h"

void SpectralDenoiser::prepare (double sampleRate, int numChannels)
{
    sr = sampleRate;
    numCh = juce::jmax (1, numChannels);
    fftOrder = sampleRate <= 50000.0 ? 11 : (sampleRate <= 100000.0 ? 12 : 13);
    fftSize = 1 << fftOrder;
    hop = fftSize / 4;
    fft = std::make_unique<juce::dsp::FFT> (fftOrder);

    window.resize ((size_t) fftSize);
    double sumW = 0.0, sumW2 = 0.0;
    for (int i = 0; i < fftSize; ++i)
    {
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) fftSize);
        sumW += window[(size_t) i];
        sumW2 += window[(size_t) i] * window[(size_t) i];
    }
    olaNorm = (float) (hop / sumW2);
    powNorm = (float) ((2.0 / sumW) * (2.0 / sumW));   // full-scale sine -> 0 dB

    inRing.assign ((size_t) numCh, std::vector<float> ((size_t) fftSize, 0.0f));
    outRing.assign ((size_t) numCh, std::vector<float> ((size_t) fftSize, 0.0f));
    fftData.assign ((size_t) numCh, std::vector<float> ((size_t) fftSize * 2, 0.0f));

    const size_t bins = (size_t) fftSize / 2 + 1;
    for (auto* v : { &power, &smoothPow, &minPow, &winMinCur, &winMinPrev, &learned, &learnAcc, &gain, &gainSm, &gainTmp })
        v->assign (bins, 0.0f);
    std::fill (gainSm.begin(), gainSm.end(), 1.0f);

    for (int i = 0; i < numDisplayPoints; ++i)
    {
        const double lo = displayPointToFreq ((float) i - 0.5f) * fftSize / sr;
        const double hi = displayPointToFreq ((float) i + 0.5f) * fftSize / sr;
        dispLo[(size_t) i] = juce::jlimit (1, (int) bins - 1, (int) std::floor (lo));
        dispHi[(size_t) i] = juce::jlimit (dispLo[(size_t) i], (int) bins - 1, (int) std::floor (hi));
    }

    learnTarget = juce::jmax (1, (int) (2.0 * sr / hop));   // 2 seconds of noise
    winLength = juce::jmax (1, (int) (3.0 * sr / hop));     // 3 s sub-windows for the windowed minimum
    reset();
}

void SpectralDenoiser::reset()
{
    for (auto& v : inRing) std::fill (v.begin(), v.end(), 0.0f);
    for (auto& v : outRing) std::fill (v.begin(), v.end(), 0.0f);
    std::fill (gainSm.begin(), gainSm.end(), 1.0f);
    pos = hopCounter = 0;
    firstFrame = true;
    frameCount = winFrames = 0;
    dispSpec.fill (-120.0f);
    dispNoise.fill (-120.0f);
    dispRed.fill (0.0f);
}

void SpectralDenoiser::process (juce::AudioBuffer<float>& buffer, int channels)
{
    channels = juce::jmin (channels, numCh);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        for (int ch = 0; ch < channels; ++ch)
        {
            auto& in = inRing[(size_t) ch];
            auto& out = outRing[(size_t) ch];
            const float y = out[(size_t) pos];
            out[(size_t) pos] = 0.0f;
            in[(size_t) pos] = buffer.getSample (ch, i);
            buffer.setSample (ch, i, y);
        }
        if (++pos >= fftSize) pos = 0;
        if (++hopCounter >= hop)
        {
            hopCounter = 0;
            processFrame();
        }
    }
}

void SpectralDenoiser::processFrame()
{
    const int bins = fftSize / 2 + 1;
    const int mask = fftSize - 1;
    const float frameSec = (float) hop / (float) sr;

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& d = fftData[(size_t) ch];
        const auto& in = inRing[(size_t) ch];
        for (int k = 0; k < fftSize; ++k)
            d[(size_t) k] = in[(size_t) ((pos + k) & mask)] * window[(size_t) k];
        std::fill (d.begin() + fftSize, d.end(), 0.0f);
        fft->performRealOnlyForwardTransform (d.data(), true);
    }

    for (int b = 0; b < bins; ++b)
    {
        float p = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            const auto& d = fftData[(size_t) ch];
            p += d[(size_t) (2 * b)] * d[(size_t) (2 * b)] + d[(size_t) (2 * b + 1)] * d[(size_t) (2 * b + 1)];
        }
        power[(size_t) b] = p / (float) numCh * powNorm + 1.0e-14f;
    }

    // automatic noise floor per bin: average only while the bin looks like noise (not voice);
    // quieter frames pull it down quickly, voice bins let it rise only slowly (3 dB/s)
    // the windowed minimum (3-6 s) of the smoothed power is a lower bound, so the estimate
    // recovers quickly after start-up, digital silence or a louder noise source
    const float rise = std::pow (10.0f, 3.0f * frameSec / 10.0f);
    const bool ready = ++frameCount > fftSize / hop;   // analysis buffer filled with real audio
    const bool newWindow = ready && ++winFrames >= winLength;
    if (newWindow) winFrames = 0;
    for (int b = 0; ready && b < bins; ++b)
    {
        const float p = power[(size_t) b];
        auto& s = smoothPow[(size_t) b];
        auto& m = minPow[(size_t) b];
        auto& cur = winMinCur[(size_t) b];
        auto& prev = winMinPrev[(size_t) b];
        if (firstFrame) { s = m = cur = prev = p; continue; }
        s = 0.7f * s + 0.3f * p;
        cur = juce::jmin (cur, s);
        if (newWindow) { prev = cur; cur = s; }
        if (s < 0.5f * m)      m = 0.85f * m + 0.15f * s;   // noise clearly got quieter
        else if (s < 3.0f * m) m = 0.95f * m + 0.05f * s;   // noise-like: plain average
        else                   m *= rise;                   // voice in this bin: barely move
        m = juce::jmax (m, juce::jmin (cur, prev));
    }
    if (ready) firstFrame = false;

    // learned profile
    if (clearRequested.exchange (false))
        profileActive.store (false);
    if (learnRequested.exchange (false))
    {
        std::fill (learnAcc.begin(), learnAcc.end(), 0.0f);
        learnFrames = 0;
        learning.store (true);
    }
    if (learning.load())
    {
        for (int b = 0; b < bins; ++b) learnAcc[(size_t) b] += power[(size_t) b];
        if (++learnFrames >= learnTarget)
        {
            for (int b = 0; b < bins; ++b) learned[(size_t) b] = learnAcc[(size_t) b] / (float) learnFrames;
            learning.store (false);
            profileActive.store (true);
        }
    }
    const bool useProfile = profileActive.load();

    // spectral subtraction gain with over-subtraction and a floor
    const float alpha = 1.0f + 3.0f * amount;
    const float gMin = juce::Decibels::decibelsToGain (-(6.0f + 30.0f * amount));
    const float relC = std::exp (-frameSec / 0.03f);
    double noiseSum = 0.0, inSum = 0.0, outSum = 0.0;
    for (int b = 0; b < bins; ++b)
    {
        const float n = useProfile ? learned[(size_t) b] : 1.1f * minPow[(size_t) b];   // estimate sits slightly below the mean
        noiseSum += n;
        float g = 1.0f;
        if (amount > 0.0f && ! firstFrame)
        {
            // decide on the lightly smoothed power: single random noise peaks no longer open a bin
            const float pDecide = juce::jmax (smoothPow[(size_t) b], 0.5f * power[(size_t) b]);
            g = std::sqrt (juce::jmax (gMin * gMin, 1.0f - alpha * n / pDecide));
        }
        auto& gs = gainSm[(size_t) b];
        gs = g > gs ? gs + (g - gs) * 0.6f : g + (gs - g) * relC;   // open quickly, close smoothly
    }
    // frequency smoothing (2 passes); gainSm keeps the temporal state
    gain = gainSm;
    for (int pass = 0; pass < 2; ++pass)
    {
        gainTmp = gain;
        for (int b = 1; b < bins - 1; ++b)
            gain[(size_t) b] = 0.25f * gainTmp[(size_t) b - 1] + 0.5f * gainTmp[(size_t) b] + 0.25f * gainTmp[(size_t) b + 1];
    }
    for (int b = 0; b < bins; ++b)
    {
        inSum += power[(size_t) b];
        outSum += power[(size_t) b] * gain[(size_t) b] * gain[(size_t) b];
    }
    reductionDb.store ((float) (10.0 * std::log10 ((inSum + 1.0e-20) / (outSum + 1.0e-20))));
    noiseFloorDb.store ((float) (10.0 * std::log10 (noiseSum + 1.0e-20)));

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& d = fftData[(size_t) ch];
        for (int b = 0; b < bins; ++b)
        {
            d[(size_t) (2 * b)] *= gain[(size_t) b];
            d[(size_t) (2 * b + 1)] *= gain[(size_t) b];
        }
        fft->performRealOnlyInverseTransform (d.data());
        auto& out = outRing[(size_t) ch];
        for (int k = 0; k < fftSize; ++k)
            out[(size_t) ((pos + k) & mask)] += d[(size_t) k] * window[(size_t) k] * olaNorm;
    }

    const juce::SpinLock::ScopedTryLockType lock (displayLock);
    if (lock.isLocked())
    {
        const float fall = std::exp (-frameSec / 0.2f);
        for (int i = 0; i < numDisplayPoints; ++i)
        {
            float p = 0.0f, n = 0.0f, g = 1.0f;
            for (int b = dispLo[(size_t) i]; b <= dispHi[(size_t) i]; ++b)
            {
                p = juce::jmax (p, power[(size_t) b]);
                n = juce::jmax (n, useProfile ? learned[(size_t) b] : 1.1f * minPow[(size_t) b]);
                g = juce::jmin (g, gain[(size_t) b]);
            }
            const float tilt = 3.0f * std::log2 (displayPointToFreq ((float) i) / 1000.0f);   // easier to read
            const float pDb = 10.0f * std::log10 (p) + tilt;
            auto& s = dispSpec[(size_t) i];
            s = pDb > s ? pDb : pDb + (s - pDb) * fall;
            dispNoise[(size_t) i] = 10.0f * std::log10 (n) + tilt;
            dispRed[(size_t) i] = -20.0f * std::log10 (juce::jmax (1.0e-4f, g));
        }
    }
}

void SpectralDenoiser::copyDisplay (std::array<float, numDisplayPoints>& spectrum, std::array<float, numDisplayPoints>& noise,
                                    std::array<float, numDisplayPoints>& reduction)
{
    const juce::SpinLock::ScopedLockType lock (displayLock);
    spectrum = dispSpec;
    noise = dispNoise;
    reduction = dispRed;
}

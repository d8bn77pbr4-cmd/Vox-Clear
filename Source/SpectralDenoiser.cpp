#include "SpectralDenoiser.h"

namespace
{
    inline float dbToGain (float db) { return std::exp (db * 0.11512925f); }   // 10^(dB/20)
}

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
    for (auto* v : { &power, &smoothPow, &minPow, &winMinCur, &winMinPrev, &learned, &learnAcc, &gain, &gainSm, &gainTmp,
                     &binFreq, &smoothWeight, &proxShape, &toneTarget, &toneDb, &total, &ltPow, &powDb, &ref1, &ref2, &resRed, &resTmp })
        v->assign (bins, 0.0f);
    prefix.assign (bins + 1, 0.0);
    matchAcc.assign (bins, 0.0);

    for (size_t b = 0; b < bins; ++b)
    {
        const float f = juce::jmax (1.0f, (float) ((double) b * sr / fftSize));
        binFreq[b] = f;
        // resonance smoothing works where harsh capsule peaks live: 1.2 .. 12 kHz (soft edges)
        const float lo = juce::jlimit (0.0f, 1.0f, std::log2 (f / 600.0f));
        const float hi = juce::jlimit (0.0f, 1.0f, std::log2 (24000.0f / f));
        smoothWeight[b] = lo * hi;
        proxShape[b] = 1.0f / (1.0f + (f / 180.0f) * (f / 180.0f));   // low shelf around 180 Hz
    }
    auto binOf = [this] (float f) { return juce::jlimit (1, fftSize / 2, (int) std::round (f * fftSize / sr)); };
    proxLo = binOf (80.0f); proxMid = binOf (250.0f); proxHi = binOf (3000.0f);
    proxBins = binOf (1500.0f);

    for (int i = 0; i < numDisplayPoints; ++i)
    {
        const double lo = displayPointToFreq ((float) i - 0.5f) * fftSize / sr;
        const double hi = displayPointToFreq ((float) i + 0.5f) * fftSize / sr;
        dispLo[(size_t) i] = juce::jlimit (1, (int) bins - 1, (int) std::floor (lo));
        dispHi[(size_t) i] = juce::jlimit (dispLo[(size_t) i], (int) bins - 1, (int) std::floor (hi));
    }

    learnTarget = juce::jmax (1, (int) (2.0 * sr / hop));   // 2 seconds of noise
    winLength = juce::jmax (1, (int) (3.0 * sr / hop));     // 3 s sub-windows for the windowed minimum
    matchTarget = juce::jmax (1, (int) (6.0 * sr / hop));   // 6 seconds of singing
    toneValid = false;
    reset();
}

void SpectralDenoiser::reset()
{
    for (auto& v : inRing) std::fill (v.begin(), v.end(), 0.0f);
    for (auto& v : outRing) std::fill (v.begin(), v.end(), 0.0f);
    std::fill (gainSm.begin(), gainSm.end(), 1.0f);
    std::fill (resRed.begin(), resRed.end(), 0.0f);
    std::fill (ltPow.begin(), ltPow.end(), 0.0f);
    pos = hopCounter = 0;
    firstFrame = true;
    frameCount = winFrames = 0;
    proxAvg = proxTime = proxCut = 0.0f;
    toneValid = false;
    dispSpec.fill (-120.0f);
    dispNoise.fill (-120.0f);
    dispRed.fill (0.0f);
    dispTone.fill (0.0f);
}

void SpectralDenoiser::process (juce::AudioBuffer<float>& buffer, int channels)
{
    channels = juce::jmin (channels, numCh);
    const int n = buffer.getNumSamples();
    int i = 0;
    while (i < n)
    {
        // copy straight up to the next frame boundary (or the ring end), then run a frame
        const int len = juce::jmin (n - i, hop - hopCounter, fftSize - pos);
        for (int ch = 0; ch < channels; ++ch)
        {
            float* io = buffer.getWritePointer (ch, i);
            float* in = inRing[(size_t) ch].data() + pos;
            float* out = outRing[(size_t) ch].data() + pos;
            for (int k = 0; k < len; ++k)
            {
                in[k] = io[k];
                io[k] = out[k];
                out[k] = 0.0f;
            }
        }
        i += len;
        pos += len;
        if (pos >= fftSize) pos = 0;
        hopCounter += len;
        if (hopCounter >= hop)
        {
            hopCounter = 0;
            processFrame();
        }
    }
}

void SpectralDenoiser::boxSmooth (const std::vector<float>& in, std::vector<float>& out, std::vector<double>& pre, float halfWidthFactor)
{
    const int bins = (int) in.size();
    pre[0] = 0.0;
    for (int b = 0; b < bins; ++b)
        pre[(size_t) b + 1] = pre[(size_t) b] + in[(size_t) b];
    for (int b = 1; b < bins; ++b)
    {
        const int h = juce::jmax (1, (int) std::round ((float) b * halfWidthFactor));
        const int lo = juce::jmax (1, b - h), hi = juce::jmin (bins - 1, b + h);
        out[(size_t) b] = (float) ((pre[(size_t) hi + 1] - pre[(size_t) lo]) / (hi - lo + 1));
    }
    out[0] = out[1];
}

void SpectralDenoiser::updateToneTarget()
{
    const int bins = fftSize / 2 + 1;
    for (int b = 1; b < bins; ++b)
    {
        const float f = binFreq[(size_t) b];
        float db = 0.0f;
        for (const auto& band : tone.bands) db += toneBandDb (band, f);
        // MATCH curve: interpolate the 1/3-octave values
        const float idx = juce::jlimit (0.0f, (float) (ToneSpec::matchBands - 1), 3.0f * std::log2 (f / 20.0f));
        const int i0 = juce::jmin ((int) idx, ToneSpec::matchBands - 2);
        db += tone.matchDb[(size_t) i0] + (idx - (float) i0) * (tone.matchDb[(size_t) i0 + 1] - tone.matchDb[(size_t) i0]);
        toneTarget[(size_t) b] = juce::jlimit (-15.0f, 15.0f, db);
    }
    toneTarget[0] = toneTarget[1];
    cachedTone = tone;
    toneValid = true;
}

void SpectralDenoiser::finishMatch()
{
    // 1/3-octave levels of the learned voice spectrum vs. a reference lead vocal; only the broad,
    // partial difference is kept, so the singer's own character stays
    std::array<float, ToneSpec::matchBands> diff {};
    std::array<bool, ToneSpec::matchBands> valid {};
    const int bins = fftSize / 2 + 1;
    for (int i = 0; i < ToneSpec::matchBands; ++i)
    {
        const float fc = ToneSpec::matchBandFreq (i);
        const int lo = juce::jlimit (1, bins - 1, (int) std::floor (fc * std::pow (2.0f, -1.0f / 6.0f) * fftSize / sr));
        const int hi = juce::jlimit (lo, bins - 1, (int) std::ceil (fc * std::pow (2.0f, 1.0f / 6.0f) * fftSize / sr));
        double s = 0.0;
        for (int b = lo; b <= hi; ++b) s += matchAcc[(size_t) b];
        const float levelDb = (float) (10.0 * std::log10 (s / (hi - lo + 1) / matchFrames + 1.0e-20));
        diff[(size_t) i] = Mic::referenceVocalDb (fc) - levelDb;
        valid[(size_t) i] = fc >= 100.0f && fc <= 14000.0f && fc < sr * 0.45;
    }
    std::array<float, ToneSpec::matchBands> core {};   // fixed size: no allocation on the audio thread
    int numCore = 0;
    for (int i = 0; i < ToneSpec::matchBands; ++i)
        if (ToneSpec::matchBandFreq (i) >= 200.0f && ToneSpec::matchBandFreq (i) <= 5000.0f) core[(size_t) numCore++] = diff[(size_t) i];
    std::sort (core.begin(), core.begin() + numCore);
    const float offset = numCore > 0 ? core[(size_t) numCore / 2] : 0.0f;

    std::array<float, ToneSpec::matchBands> c {};
    for (int i = 0; i < ToneSpec::matchBands; ++i) c[(size_t) i] = valid[(size_t) i] ? diff[(size_t) i] - offset : 0.0f;
    for (int pass = 0; pass < 2; ++pass)   // ~2/3 octave smoothing
    {
        auto t = c;
        for (int i = 1; i < ToneSpec::matchBands - 1; ++i) c[(size_t) i] = 0.25f * t[(size_t) i - 1] + 0.5f * t[(size_t) i] + 0.25f * t[(size_t) i + 1];
    }
    for (int i = 0; i < ToneSpec::matchBands; ++i)
    {
        const float fc = ToneSpec::matchBandFreq (i);
        float taper = 1.0f;
        if (fc < 100.0f) taper = juce::jlimit (0.0f, 1.0f, std::log2 (fc / 50.0f));
        if (fc > 12000.0f) taper = juce::jlimit (0.0f, 1.0f, std::log2 (16000.0f / fc) / std::log2 (16000.0f / 12000.0f));
        // below 400 Hz only gentle cuts: the voice's natural warmth must not count as "too much bass"
        matchResult[(size_t) i] = juce::jlimit (fc < 400.0f ? -2.0f : -4.0f, 4.0f, 0.6f * c[(size_t) i]) * taper;
    }
}

bool SpectralDenoiser::consumeMatchResult (std::array<float, ToneSpec::matchBands>& out)
{
    if (! matchFinished.exchange (false)) return false;
    out = matchResult;
    return true;
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
    double noiseSum = 0.0, inSum = 0.0, outSum = 0.0, framePow = 0.0;
    for (int b = 0; b < bins; ++b)
    {
        const float n = useProfile ? learned[(size_t) b] : 1.1f * minPow[(size_t) b];   // estimate sits slightly below the mean
        noiseSum += n;
        framePow += power[(size_t) b];
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
    const bool voiced = ! firstFrame && framePow > 8.0 * noiseSum && framePow > 1.0e-6;

    // ---- microphone tone curve (smoothed per frame, so preset changes glide instead of jumping) ----
    if (! toneValid || ! tone.sameCurveAs (cachedTone))
        updateToneTarget();
    for (int b = 0; b < bins; ++b)
        toneDb[(size_t) b] += 0.2f * (toneTarget[(size_t) b] - toneDb[(size_t) b]);

    // ---- resonance smoothing: fixed narrow peaks of the capsule/room are turned down ----
    // Detected in the long-term spectrum (~1.5 s, voiced frames only): the voice's own harmonics move
    // with the pitch and smear out there, while a capsule resonance always sits on the same frequency.
    float maxRes = 0.0f;
    if (voiced)
    {
        const float c = std::exp (-frameSec / 1.5f);
        for (int b = 0; b < bins; ++b)
            ltPow[(size_t) b] = ltPow[(size_t) b] <= 0.0f ? power[(size_t) b] : c * ltPow[(size_t) b] + (1.0f - c) * power[(size_t) b];
    }
    if (tone.smooth > 0.0f || smoothDb.load() > 0.01f)   // keep running while a previous cut fades out
    {
        if (voiced)
        {
            for (int b = 0; b < bins; ++b) powDb[(size_t) b] = 10.0f * std::log10 (ltPow[(size_t) b] + 1.0e-14f);
            const float hw = std::pow (2.0f, 1.0f / 4.0f) - 1.0f;   // +-1/4 octave, two passes: wide enough for the whole resonance
            boxSmooth (powDb, ref1, prefix, hw);
            boxSmooth (ref1, ref2, prefix, hw);
        }
        for (int b = 1; b < bins; ++b)
        {
            const float target = tone.smooth > 0.0f
                ? juce::jmin (9.0f, juce::jmax (0.0f, powDb[(size_t) b] - ref2[(size_t) b] - 1.0f) * tone.smooth * smoothWeight[(size_t) b])
                : 0.0f;
            auto& r = resRed[(size_t) b];
            r += (target > r ? 0.08f : 0.03f) * (target - r);   // slow: this is a static correction
        }
        for (int b = 1; b < bins - 1; ++b)
        {
            resTmp[(size_t) b] = 0.25f * resRed[(size_t) b - 1] + 0.5f * resRed[(size_t) b] + 0.25f * resRed[(size_t) b + 1];
            maxRes = juce::jmax (maxRes, resTmp[(size_t) b]);
        }
        resTmp[0] = 0.0f;
        resTmp[(size_t) bins - 1] = 0.0f;
    }
    else
        std::fill (resTmp.begin(), resTmp.end(), 0.0f);
    smoothDb.store (maxRes);

    // ---- proximity control: keep the bass/mid balance steady when the singer moves closer ----
    if (tone.proximity > 0.0f && voiced)
    {
        double lowE = 0.0, midE = 0.0;
        for (int b = proxLo; b < proxMid; ++b) lowE += power[(size_t) b];
        for (int b = proxMid; b < proxHi; ++b) midE += power[(size_t) b];
        const float ratioDb = (float) (10.0 * std::log10 ((lowE + 1.0e-20) / (midE + 1.0e-20)));
        proxTime += frameSec;
        const float avgC = std::exp (-frameSec / juce::jlimit (0.3f, 4.0f, proxTime));
        proxAvg = proxTime <= frameSec ? ratioDb : ratioDb + (proxAvg - ratioDb) * avgC;
        const float target = proxTime > 1.0f ? tone.proximity * juce::jlimit (0.0f, 6.0f, 0.8f * (ratioDb - proxAvg - 1.5f)) : 0.0f;
        proxCut = target > proxCut ? proxCut + (target - proxCut) * 0.3f : proxCut * 0.95f + target * 0.05f;
    }
    else
        proxCut *= 0.95f;
    proximityDb.store (proxCut);

    // ---- MATCH learning (voiced frames only) ----
    if (matchRequested.exchange (false))
    {
        std::fill (matchAcc.begin(), matchAcc.end(), 0.0);
        matchFrames = 0;
        matchProgress.store (0.0f);
        matching.store (true);
    }
    if (matching.load() && voiced)
    {
        for (int b = 0; b < bins; ++b) matchAcc[(size_t) b] += power[(size_t) b];
        matchProgress.store ((float) ++matchFrames / (float) matchTarget);
        if (matchFrames >= matchTarget)
        {
            finishMatch();
            matching.store (false);
            matchFinished.store (true);
        }
    }

    // ---- combine: noise gain x tone x resonance x proximity ----
    for (int b = 0; b < bins; ++b)
    {
        float db = toneDb[(size_t) b] - resTmp[(size_t) b];
        if (b < proxBins) db -= proxCut * proxShape[(size_t) b];
        total[(size_t) b] = gain[(size_t) b] * dbToGain (db);
    }

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& d = fftData[(size_t) ch];
        for (int b = 0; b < bins; ++b)
        {
            d[(size_t) (2 * b)] *= total[(size_t) b];
            d[(size_t) (2 * b + 1)] *= total[(size_t) b];
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
            float p = 0.0f, n = 0.0f, g = 1.0f, t = 0.0f;
            for (int b = dispLo[(size_t) i]; b <= dispHi[(size_t) i]; ++b)
            {
                p = juce::jmax (p, power[(size_t) b]);
                n = juce::jmax (n, useProfile ? learned[(size_t) b] : 1.1f * minPow[(size_t) b]);
                g = juce::jmin (g, gain[(size_t) b] * dbToGain (-resTmp[(size_t) b]));
                t += toneDb[(size_t) b] - (b < proxBins ? proxCut * proxShape[(size_t) b] : 0.0f);
            }
            const float tilt = 3.0f * std::log2 (displayPointToFreq ((float) i) / 1000.0f);   // easier to read
            const float pDb = 10.0f * std::log10 (p) + tilt;
            auto& s = dispSpec[(size_t) i];
            s = pDb > s ? pDb : pDb + (s - pDb) * fall;
            dispNoise[(size_t) i] = 10.0f * std::log10 (n) + tilt;
            dispRed[(size_t) i] = -20.0f * std::log10 (juce::jmax (1.0e-4f, g));
            dispTone[(size_t) i] = t / (float) (dispHi[(size_t) i] - dispLo[(size_t) i] + 1);
        }
    }
}

void SpectralDenoiser::copyDisplay (std::array<float, numDisplayPoints>& spectrum, std::array<float, numDisplayPoints>& noise,
                                    std::array<float, numDisplayPoints>& reduction, std::array<float, numDisplayPoints>& toneCurve)
{
    const juce::SpinLock::ScopedLockType lock (displayLock);
    spectrum = dispSpec;
    noise = dispNoise;
    reduction = dispRed;
    toneCurve = dispTone;
}

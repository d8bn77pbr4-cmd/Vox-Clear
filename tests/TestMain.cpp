#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>

namespace
{
    constexpr double fs = 48000.0;
    constexpr int block = 512;

    void setP (VoxClearProcessor& p, const char* id, float v)
    {
        auto* pr = p.apvts.getParameter (id);
        pr->setValueNotifyingHost (pr->convertTo0to1 (v));
    }

    void allOff (VoxClearProcessor& p)
    {
        setP (p, "lowcut", 20.0f); setP (p, "declick", 0.0f); setP (p, "noise", 0.0f); setP (p, "gate", -80.0f);
        setP (p, "push", 0.0f); setP (p, "ride", 0.0f);
        setP (p, "mic_amount", 0.0f); setP (p, "smooth", 0.0f); setP (p, "tube", 0.0f);
    }

    // run a mono signal (duplicated to stereo) through the plugin, optionally with a beat sidechain
    std::vector<float> run (VoxClearProcessor& p, const std::vector<float>& in, const std::vector<float>* beat = nullptr, int blk = block)
    {
        const int chans = beat ? 4 : 2;
        const int block = blk;
        juce::AudioBuffer<float> buf (chans, block);
        juce::MidiBuffer m;
        std::vector<float> out (in.size(), 0.0f);
        for (size_t s = 0; s + block <= in.size(); s += block)
        {
            for (int i = 0; i < block; ++i)
            {
                buf.setSample (0, i, in[s + (size_t) i]);
                buf.setSample (1, i, in[s + (size_t) i]);
                if (beat) { buf.setSample (2, i, (*beat)[s + (size_t) i]); buf.setSample (3, i, (*beat)[s + (size_t) i]); }
            }
            p.processBlock (buf, m);
            for (int i = 0; i < block; ++i) out[s + (size_t) i] = buf.getSample (0, i);
        }
        return out;
    }

    std::unique_ptr<VoxClearProcessor> makeProc (bool sidechain = false)
    {
        auto p = std::make_unique<VoxClearProcessor>();
        auto layout = p->getBusesLayout();
        if (sidechain) layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
        p->setBusesLayout (layout);
        p->setRateAndBufferSizeDetails (fs, block);
        p->prepareToPlay (fs, block);
        return p;
    }

    // simple "voice": harmonic tone at 180 Hz with phrases (on 0.6 s / off 0.4 s)
    float voice (int i, bool gaps = true)
    {
        const double t = i / fs;
        const bool on = ! gaps || std::fmod (t, 1.0) < 0.6;
        if (! on) return 0.0f;
        double v = 0.0;
        for (int h = 1; h <= 12; ++h) v += std::sin (juce::MathConstants<double>::twoPi * 180.0 * h * t) / h;
        return (float) (0.15 * v);
    }

    // level of x in a narrow band around f (band-pass + RMS), for frequency-response checks
    double bandDb (const std::vector<float>& x, float f, size_t a, size_t b, float q = 4.3f)
    {
        juce::dsp::IIR::Filter<float> bp;
        bp.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (fs, f, q);
        double s = 0.0;
        for (size_t i = 0; i < b; ++i)
        {
            const float y = bp.processSample (x[i]);
            if (i >= a) s += (double) y * y;
        }
        return 10.0 * std::log10 (s / (double) (b - a) + 1.0e-20);
    }

    // amplitude of a sine component at f (correlation)
    double toneDb (const std::vector<float>& x, double f, size_t a, size_t b)
    {
        double re = 0.0, im = 0.0;
        for (size_t i = a; i < b; ++i)
        {
            const double ph = juce::MathConstants<double>::twoPi * f * (double) i / fs;
            re += x[i] * std::cos (ph); im += x[i] * std::sin (ph);
        }
        return 20.0 * std::log10 (2.0 * std::sqrt (re * re + im * im) / (double) (b - a) + 1.0e-12);
    }

    double rmsDb (const std::vector<float>& x, size_t a, size_t b)
    {
        double s = 0.0;
        for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i];
        return 10.0 * std::log10 (s / (double) (b - a) + 1.0e-20);
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::Random rng (5);
    const int total = (int) (fs * 6);


    // 1) everything off -> output is the input, delayed by the reported latency
    {
        auto p = makeProc();
        allOff (*p);
        std::vector<float> in ((size_t) total);
        for (auto& v : in) v = rng.nextFloat() - 0.5f;
        auto out = run (*p, in);
        const int L = p->getLatencySamples();
        float err = 0.0f;
        // from 0.5 s on: parameters set after prepare glide to their new values first (that is the anti-zipper smoothing)
        for (int i = L + (int) (0.5 * fs); i < total - block; ++i) err = juce::jmax (err, std::abs (out[(size_t) i] - in[(size_t) (i - L)]));
        std::printf ("[identity] latency %d samples (%.1f ms), max error %.2e\n", L, 1000.0 * L / fs, err);
    }

    // 2) noise reduction: voice phrases + white noise at -50 dBFS
    {
        auto p = makeProc();
        allOff (*p);
        setP (*p, "noise", 50.0f);
        std::vector<float> in ((size_t) total), clean ((size_t) total);
        for (int i = 0; i < total; ++i)
        {
            clean[(size_t) i] = voice (i);
            in[(size_t) i] = clean[(size_t) i] + 0.0055f * (rng.nextFloat() * 2.0f - 1.0f);   // ~ -50 dBFS RMS
        }
        auto out = run (*p, in);
        const int L = p->getLatencySamples();
        // gap 4.65..4.95 s, voiced 4.1..4.5 s (output indices shifted by latency)
        auto at = [&] (double t) { return (size_t) (t * fs) + (size_t) L; };
        const double gapIn = rmsDb (in, (size_t) (4.65 * fs), (size_t) (4.95 * fs)), gapOut = rmsDb (out, at (4.65), at (4.95));
        const double vIn = rmsDb (clean, (size_t) (4.1 * fs), (size_t) (4.5 * fs)), vOut = rmsDb (out, at (4.1), at (4.5));
        std::printf ("[noise 50%%] noise in pauses: %.1f -> %.1f dBFS (%.1f dB less) | voice level: %.1f -> %.1f dB\n",
                     gapIn, gapOut, gapIn - gapOut, vIn, vOut);
        setP (*p, "noise", 100.0f);
        out = run (*p, in);
        std::printf ("[noise 100%%] noise in pauses: %.1f dBFS | voice level: %.1f dB\n", rmsDb (out, at (4.65), at (4.95)), rmsDb (out, at (4.1), at (4.5)));
        setP (*p, "noise", 50.0f);
        setP (*p, "gate", -45.0f);
        out = run (*p, in);
        std::printf ("[noise 50%% + gate -45] noise in pauses: %.1f dBFS (gate gain %.1f dB)\n", rmsDb (out, at (4.65), at (4.95)), p->gateGainDb.load());
    }

    // 3) mouth clicks vs. consonants
    {
        auto p = makeProc();
        allOff (*p);
        setP (*p, "declick", 50.0f);
        std::vector<float> in ((size_t) total);
        for (int i = 0; i < total; ++i) in[(size_t) i] = voice (i, false) * 0.6f;
        std::vector<int> clicks, consonants;
        for (int c = 0; c < 15; ++c) clicks.push_back ((int) ((0.5 + c * 0.35) * fs));
        for (int c = 0; c < 5; ++c) consonants.push_back ((int) ((0.67 + c * 1.05) * fs));
        juce::dsp::IIR::Filter<float> hpf;
        hpf.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (fs, 3000.0f);
        for (int c : clicks)              // ~1.5 ms decaying click
            for (int k = 0; k < 72; ++k) in[(size_t) (c + k)] += 0.5f * std::exp (-k / 15.0f) * (rng.nextFloat() * 2.0f - 1.0f);
        for (int c : consonants)          // 25 ms "t"-like burst
            for (int k = 0; k < 1200; ++k) in[(size_t) (c + k)] += 0.25f * std::exp (-k / 500.0f) * hpf.processSample (rng.nextFloat() * 2.0f - 1.0f);

        auto out = run (*p, in);
        const int L = p->getLatencySamples();
        // measure in the high band (> 3 kHz), where mouth clicks live
        auto highBand = [&] (const std::vector<float>& x)
        {
            juce::dsp::IIR::Filter<float> f;
            f.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (fs, 3000.0f);
            std::vector<float> y (x.size());
            for (size_t i = 0; i < x.size(); ++i) y[i] = f.processSample (x[i]);
            return y;
        };
        const auto inH = highBand (in), outH = highBand (out);
        double clickRed = 0.0, consRed = 0.0;
        for (int c : clicks)  clickRed += rmsDb (inH, (size_t) c, (size_t) c + 96) - rmsDb (outH, (size_t) (c + L), (size_t) (c + L + 96));
        for (int c : consonants) consRed += rmsDb (inH, (size_t) c, (size_t) c + 1200) - rmsDb (outH, (size_t) (c + L), (size_t) (c + L + 1200));
        std::printf ("[de-click 50%%] clicks (>3 kHz): %.1f dB quieter (detected %d of %d) | 't' consonants: %.1f dB change\n",
                     clickRed / clicks.size(), p->declicker.clickCount.load(), (int) clicks.size(), consRed / consonants.size());
    }

    // 3b) false positives: voice phrases + light noise, no clicks at all
    {
        auto p = makeProc();
        allOff (*p);
        setP (*p, "declick", 60.0f);
        std::vector<float> in ((size_t) total);
        for (int i = 0; i < total; ++i) in[(size_t) i] = voice (i) * 0.8f + 0.004f * (rng.nextFloat() * 2.0f - 1.0f);
        run (*p, in);
        const int withGaps = p->declicker.clickCount.load();
        auto p2 = makeProc();
        allOff (*p2);
        setP (*p2, "declick", 60.0f);
        for (int i = 0; i < total; ++i) in[(size_t) i] = 0.004f * (rng.nextFloat() * 2.0f - 1.0f);
        run (*p2, in);
        std::printf ("[de-click false positives, 6 s] voice+noise: %d | noise only: %d\n", withGaps, p2->declicker.clickCount.load());
    }

    // 4) vocal push on a muddy voice
    {
        auto p = makeProc();
        allOff (*p);
        setP (*p, "push", 50.0f);
        std::vector<float> in ((size_t) total);
        Biquad mud; mud.setPeak (fs, 280.0, 1.0, 9.0);
        for (int i = 0; i < total; ++i) in[(size_t) i] = mud.process (voice (i, false) * 0.5f, 0);
        run (*p, in);
        std::printf ("[push 50%%] muddy voice -> mud cut %.1f dB, box cut %.1f dB, leveler %.1f dB\n",
                     p->mudCutDb.load(), p->boxCutDb.load(), p->compGainDb.load());
    }

    // 5) beat ride: beat gets 6 dB louder after 4 s, vocal stays the same
    {
        auto p = makeProc (true);
        allOff (*p);
        setP (*p, "ride", 100.0f);
        const int len = (int) (fs * 8);
        std::vector<float> in ((size_t) len), beat ((size_t) len);
        for (int i = 0; i < len; ++i)
        {
            in[(size_t) i] = voice (i, false) * 0.3f;
            beat[(size_t) i] = (i < 4 * fs ? 0.1f : 0.2f) * (rng.nextFloat() * 2.0f - 1.0f);
        }
        std::vector<float> first (in.begin(), in.begin() + (long) (4 * fs)), firstBeat (beat.begin(), beat.begin() + (long) (4 * fs));
        run (*p, first, &firstBeat);
        const float before = p->rideDb.load();
        std::vector<float> second (in.begin() + (long) (4 * fs), in.end()), secondBeat (beat.begin() + (long) (4 * fs), beat.end());
        std::vector<float> s1 (second.begin(), second.begin() + (long) fs), b1 (secondBeat.begin(), secondBeat.begin() + (long) fs);
        run (*p, s1, &b1);
        std::printf ("[ride 100%%] sidechain seen=%d | ride before %.1f dB, 1 s after beat +6 dB: %+.1f dB\n",
                     (int) p->sidechainActive.load(), before, p->rideDb.load());
    }

    // 6) presets: every preset applies, "Your Mic" stays untouched
    {
        auto p = makeProc();
        setP (*p, "mic_source", 3.0f);
        bool ok = true;
        for (int i = 0; i < p->getNumPrograms(); ++i)
        {
            p->setCurrentProgram (i);
            ok = ok && p->getCurrentProgram() == i && (int) p->apvts.getRawParameterValue ("mic_source")->load() == 3;
        }
        p->setCurrentProgram (4);   // R&B / Soul - Warm
        std::printf ("[presets] %d presets, all applied=%d, mic kept=%d | R&B: target=%d tube=%.0f amount=%.0f\n", p->getNumPrograms(), (int) ok,
                     (int) p->apvts.getRawParameterValue ("mic_source")->load(), (int) p->apvts.getRawParameterValue ("mic_target")->load(),
                     p->apvts.getRawParameterValue ("tube")->load(), p->apvts.getRawParameterValue ("mic_amount")->load());
    }

    // 7) smoothness: switching presets while a sine plays must not click
    {
        auto p = makeProc();
        p->setCurrentProgram (11);   // Clean Only
        const int len = (int) (fs * 4);
        std::vector<float> in ((size_t) len), out ((size_t) len);
        for (int i = 0; i < len; ++i) in[(size_t) i] = 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * 330.0 * i / fs);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer m;
        for (int s0 = 0; s0 + block <= len; s0 += block)
        {
            if (s0 == (int) (2.0 * fs) / block * block) p->setCurrentProgram (9);   // -> Rock / Grit (tube, EQ, leveler)
            for (int i = 0; i < block; ++i) { buf.setSample (0, i, in[(size_t) (s0 + i)]); buf.setSample (1, i, in[(size_t) (s0 + i)]); }
            p->processBlock (buf, m);
            for (int i = 0; i < block; ++i) out[(size_t) (s0 + i)] = buf.getSample (0, i);
        }
        auto maxD2 = [&] (int a, int b) { float mx = 0; for (int i = a; i < b; ++i) mx = juce::jmax (mx, std::abs (out[(size_t) i + 1] - 2 * out[(size_t) i] + out[(size_t) i - 1])); return mx; };
        const int L = p->getLatencySamples(), sw = (int) (2.0 * fs) + L;
        const float steadyBefore = maxD2 (sw - 20000, sw - 2000), around = maxD2 (sw - 2000, sw + 20000), steadyAfter = maxD2 (sw + 40000, sw + 60000);
        std::printf ("[smooth switch] max 2nd difference: before %.4f | during switch %.4f | after %.4f  (click would be >> both)\n", steadyBefore, around, steadyAfter);
    }

    // 8) mic character curves: white noise through "Bright Pop" at 100 %, compared with the designed curve
    {
        auto p = makeProc();
        allOff (*p);
        setP (*p, "mic_target", 3.0f);
        setP (*p, "mic_amount", 100.0f);
        const int len = (int) (fs * 6);
        std::vector<float> in ((size_t) len);
        for (auto& v : in) v = 0.1f * (rng.nextFloat() * 2.0f - 1.0f);
        auto out = run (*p, in);
        const int L = p->getLatencySamples();
        std::vector<float> outAligned (out.begin() + L, out.end());
        std::printf ("[mic curve Bright Pop] freq: measured / designed dB\n");
        for (float f : { 120.0f, 350.0f, 1000.0f, 3500.0f, 9000.0f, 14000.0f })
        {
            float designed = 0.0f;
            for (const auto& b : Mic::targets()[3].bands) designed += toneBandDb (b, f);
            const double meas = bandDb (outAligned, f, (size_t) fs, (size_t) (len - L - 1000)) - bandDb (in, f, (size_t) fs, (size_t) (len - L - 1000));
            std::printf ("   %6.0f Hz: %+5.1f / %+5.1f\n", f, meas, designed);
        }
    }

    // 9) SMOOTH: a harsh, narrow capsule resonance at 6.5 kHz gets tamed, the rest stays
    {
        auto measure = [&] (float smoothAmt, float resFreq = 6500.0f, float resQ = 12.0f, float resDb = 12.0f)
        {
            auto p = makeProc();
            allOff (*p);
            setP (*p, "smooth", smoothAmt);
            const int len = (int) (fs * 10);
            std::vector<float> in ((size_t) len);
            juce::dsp::IIR::Filter<float> res, pink;
            res.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (fs, resFreq, resQ, juce::Decibels::decibelsToGain (resDb));
            pink.coefficients = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass (fs, 2000.0f);
            for (int i = 0; i < len; ++i)
            {
                const float w = rng.nextFloat() * 2.0f - 1.0f;
                in[(size_t) i] = res.processSample (0.3f * pink.processSample (w) + 0.05f * w) * (voice (i) != 0.0f ? 1.0f : 0.02f);
            }
            auto out = run (*p, in);
            const int L = p->getLatencySamples();
            std::vector<float> o (out.begin() + L, out.end());
            const size_t from = (size_t) (5 * fs), to = (size_t) (len - L - 1000);   // settled (long-term analysis needs a few seconds)
            return std::make_pair (bandDb (o, resFreq, from, to, 2.0f * resQ) - bandDb (in, resFreq, from, to, 2.0f * resQ),
                                   bandDb (o, 2000.0f, from, to) - bandDb (in, 2000.0f, from, to));
        };
        const auto off = measure (0.0f), on = measure (100.0f);
        std::printf ("[smooth 100%%] 6.5 kHz resonance: %+.1f dB (off: %+.1f) | 2 kHz neighbour: %+.1f dB\n", on.first, off.first, on.second);
        const auto broad = measure (100.0f, 3000.0f, 1.5f, 6.0f);
        std::printf ("[smooth 100%%] broad +6 dB formant-like bump at 3 kHz (should mostly stay): %+.1f dB\n", broad.first);
    }

    // 10) TUBE: harmonics of a 200 Hz sine at -6 dBFS
    {
        auto p = makeProc();
        allOff (*p);
        setP (*p, "tube", 100.0f);
        const int len = (int) (fs * 2);
        std::vector<float> in ((size_t) len);
        for (int i = 0; i < len; ++i) in[(size_t) i] = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 200.0 * i / fs);
        auto out = run (*p, in);
        const double f1 = toneDb (out, 200.0, (size_t) fs, (size_t) len - 600), h2 = toneDb (out, 400.0, (size_t) fs, (size_t) len - 600), h3 = toneDb (out, 600.0, (size_t) fs, (size_t) len - 600);
        std::printf ("[tube 100%%] 2nd harmonic %.1f dB, 3rd harmonic %.1f dB below the fundamental\n", f1 - h2, f1 - h3);
    }

    // 11) MATCH: a dull microphone (high frequencies rolled off) gets a high-frequency correction
    {
        auto p = makeProc();
        allOff (*p);
        setP (*p, "mic_amount", 100.0f);   // tone path active, neutral target
        setP (*p, "mic_target", 0.0f);
        p->denoiser.requestMatch();
        const int len = (int) (fs * 14);
        std::vector<float> in ((size_t) len);
        juce::dsp::IIR::Filter<float> dull;
        dull.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (fs, 2500.0f, 0.7f);
        for (int i = 0; i < len; ++i)
        {
            const double t = i / fs;
            float v = 0.0f;
            if (std::fmod (t, 1.0) < 0.7) v = voice (i, false) + 0.05f * (rng.nextFloat() * 2.0f - 1.0f);   // bright source with breath
            in[(size_t) i] = dull.processSample (v) + 0.0005f * (rng.nextFloat() * 2.0f - 1.0f);
        }
        run (*p, in);
        std::printf ("[match] learned=%d | correction at 250 Hz %+.1f, 1 kHz %+.1f, 4 kHz %+.1f, 8 kHz %+.1f dB\n", (int) p->hasMatch.load(),
                     p->matchCurve[11].load(), p->matchCurve[17].load(), p->matchCurve[23].load(), p->matchCurve[26].load());
    }

    // 12) host sends a block 16x larger than announced: same result, no allocation path
    {
        auto a = makeProc(), b = makeProc();
        std::vector<float> in ((size_t) (fs * 2));
        for (int i = 0; i < (int) in.size(); ++i) in[(size_t) i] = voice (i) * 0.5f;
        const auto outSmall = run (*a, in, nullptr, block), outBig = run (*b, in, nullptr, block * 16);
        float diff = 0.0f;
        const size_t full = in.size() / (size_t) (block * 16) * (size_t) (block * 16);   // samples both runs processed
        for (size_t i = 0; i < full; ++i) diff = juce::jmax (diff, std::abs (outBig[i] - outSmall[i]));
        std::printf ("[big blocks] 8192-sample blocks vs 512: max difference %.2e\n", diff);
    }

    // CPU: everything on, 48 kHz, 512 samples
    {
        auto p = makeProc (true);
        p->setCurrentProgram (1);   // Home Studio Rescue: heavy everything
        setP (*p, "ride", 50.0f); setP (*p, "mic_source", 1.0f);
        const int len = (int) (fs * 20);
        std::vector<float> in ((size_t) len), beat ((size_t) len);
        for (int i = 0; i < len; ++i) { in[(size_t) i] = voice (i) * 0.5f + 0.003f * (rng.nextFloat() - 0.5f); beat[(size_t) i] = 0.1f * (rng.nextFloat() - 0.5f); }
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        run (*p, in, &beat);
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        std::printf ("[cpu] %.1f %% of one core (all modules on)\n", 100.0 * ms / 20000.0);
    }

    // screenshot
    if (argc > 1)
    {
        auto p = makeProc();
        setP (*p, "declick", 60.0f);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
        CleanDisplay* disp = nullptr;
        for (auto* c : ed->getChildren()) if (auto* d = dynamic_cast<CleanDisplay*> (c)) disp = d;
        std::vector<float> in ((size_t) block);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer m;
        for (int b = 0; b < 400; ++b)
        {
            for (int i = 0; i < block; ++i)
            {
                const int t = b * block + i;
                float v = voice (t) * 0.8f + 0.004f * (rng.nextFloat() * 2.0f - 1.0f);
                if (t % 15000 < 40) v += 0.4f * (rng.nextFloat() * 2.0f - 1.0f);
                buf.setSample (0, i, v); buf.setSample (1, i, v);
            }
            p->processBlock (buf, m);
            if (disp && b % 4 == 0) disp->refresh();
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (argv[1]); f.deleteFile();
        juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        p->editorBeingDeleted (ed.get());
    }
    return 0;
}

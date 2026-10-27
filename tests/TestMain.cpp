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
    }

    // run a mono signal (duplicated to stereo) through the plugin, optionally with a beat sidechain
    std::vector<float> run (VoxClearProcessor& p, const std::vector<float>& in, const std::vector<float>* beat = nullptr)
    {
        const int chans = beat ? 4 : 2;
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
        for (int i = L + 1000; i < total - block; ++i) err = juce::jmax (err, std::abs (out[(size_t) i] - in[(size_t) (i - L)]));
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

    // CPU: everything on, 48 kHz, 512 samples
    {
        auto p = makeProc (true);
        setP (*p, "ride", 50.0f); setP (*p, "gate", -50.0f);
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

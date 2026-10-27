#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    inline float toDb (float power) { return 10.0f * std::log10 (power + 1.0e-12f); }
    inline float coeff (double sr, double seconds) { return (float) std::exp (-1.0 / (seconds * sr)); }
}

VoxClearProcessor::VoxClearProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Beat Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pLowCut   = apvts.getRawParameterValue ("lowcut");
    pDeclick  = apvts.getRawParameterValue ("declick");
    pNoise    = apvts.getRawParameterValue ("noise");
    pGate     = apvts.getRawParameterValue ("gate");
    pListen   = apvts.getRawParameterValue ("listen");
    pPush     = apvts.getRawParameterValue ("push");
    pMud      = apvts.getRawParameterValue ("demud");
    pBox      = apvts.getRawParameterValue ("debox");
    pPresence = apvts.getRawParameterValue ("presence");
    pAir      = apvts.getRawParameterValue ("air");
    pLevel    = apvts.getRawParameterValue ("level");
    pRide     = apvts.getRawParameterValue ("ride");
    pOutput   = apvts.getRawParameterValue ("output");
    pMix      = apvts.getRawParameterValue ("mix");
    pBypass   = apvts.getRawParameterValue ("bypass");
}

juce::AudioProcessorValueTreeState::ParameterLayout VoxClearProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; });
    auto db = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; });
    auto hz = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v <= 20.5f ? String ("Off") : String (roundToInt (v)) + " Hz"; });
    auto gate = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v <= -79.9f ? String ("Off") : String (roundToInt (v)) + " dB"; });
    auto pctRange = NormalisableRange<float> (0.0f, 100.0f, 0.1f);

    NormalisableRange<float> lowCutRange (20.0f, 300.0f, 1.0f);
    lowCutRange.setSkewForCentre (90.0f);

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "lowcut", 1 },   "Low Cut",   lowCutRange, 80.0f, hz));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "declick", 1 },  "De-Click",  pctRange, 50.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "noise", 1 },    "Noise Reduction", pctRange, 50.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "gate", 1 },     "Gate",      NormalisableRange<float> (-80.0f, -20.0f, 0.5f), -60.0f, gate));
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "listen", 1 },   "Listen Removed", false));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "push", 1 },     "Push",      pctRange, 50.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "demud", 1 },    "De-Mud",    pctRange, 50.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "debox", 1 },    "De-Box",    pctRange, 50.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "presence", 1 }, "Presence",  pctRange, 50.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "air", 1 },      "Air",       pctRange, 50.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "level", 1 },    "Level",     pctRange, 50.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "ride", 1 },     "Beat Ride", pctRange, 0.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 },   "Output",    NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, db));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 },      "Mix",       pctRange, 100.0f, pct));
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "bypass", 1 },   "Bypass",    false));

    return { p.begin(), p.end() };
}

bool VoxClearProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    if (out != layouts.getMainInputChannelSet())
        return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void VoxClearProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    numCh = juce::jlimit (1, 2, getMainBusNumOutputChannels());

    declicker.prepare (sampleRate, numCh);
    denoiser.prepare (sampleRate, numCh);

    const int latency = declicker.getLatency() + denoiser.getLatency();
    setLatencySamples (latency);
    dryDelay.prepare (numCh, latency + 8);
    dryDelay.setDelay (latency);
    dryBuffer.setSize (numCh, samplesPerBlock);
    sideBuffer.setSize (2, samplesPerBlock);

    for (auto* b : { &lowCut1, &lowCut2, &mudDetect, &boxDetect, &essDetect, &mudEq, &boxEq, &essEq, &presenceEq, &airEq })
        b->reset();
    mudDetect.setBandPass (sr, 280.0, 1.0);
    boxDetect.setBandPass (sr, 650.0, 1.2);
    essDetect.setBandPass (sr, juce::jmin (7000.0, sr * 0.4), 2.0);
    lastLowCut = lastPresence = lastAir = -1.0f;
    mudCutS = boxCutS = essCutS = 0.0f;
    updatePushFilters (0.0f, 0.0f, 0.0f);

    gateEnv = 0.0f; gateGain = 1.0f; gateHold = 0.0f;
    fullEnv = mudEnv = boxEnv = essEnv = 0.0f;
    compEnv = 0.0f; compGain = 1.0f;
    vocEnv = beatEnv = 0.0f; rideAvgDiff = 0.0f; rideGainDb = 0.0f; rideActiveSec = 0.0f;

    outGainS.reset (sampleRate, 0.03);
    mixS.reset (sampleRate, 0.03);
    outGainS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    mixS.setCurrentAndTargetValue (pMix->load() * 0.01f);
}

void VoxClearProcessor::updatePushFilters (float mudCut, float boxCut, float essCut)
{
    mudEq.setPeak (sr, 280.0, 1.0, -mudCut);
    boxEq.setPeak (sr, 650.0, 1.2, -boxCut);
    essEq.setPeak (sr, juce::jmin (7000.0, sr * 0.4), 2.0, -essCut);
}

void VoxClearProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto main = getBusBuffer (buffer, false, 0);
    const int n = main.getNumSamples();
    const int chans = juce::jmin (numCh, main.getNumChannels());
    if (n == 0 || chans == 0) return;

    // optional beat sidechain (mono sum)
    bool haveSide = false;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        auto side = getBusBuffer (buffer, true, 1);
        if (side.getNumChannels() > 0)
        {
            sideBuffer.setSize (1, n, false, false, true);
            sideBuffer.copyFrom (0, 0, side, 0, 0, n);
            for (int c = 1; c < side.getNumChannels(); ++c)
                sideBuffer.addFrom (0, 0, side, c, 0, n);
            sideBuffer.applyGain (1.0f / (float) side.getNumChannels());
            haveSide = true;
        }
    }

    float peak = 0.0f;
    for (int ch = 0; ch < chans; ++ch) peak = juce::jmax (peak, main.getMagnitude (ch, 0, n));
    inPeak = juce::jmax (inPeak.load(), peak);

    dryBuffer.setSize (chans, n, false, false, true);
    for (int ch = 0; ch < chans; ++ch) dryBuffer.copyFrom (ch, 0, main, ch, 0, n);
    dryDelay.process (dryBuffer, chans);

    if (pBypass->load() > 0.5f)
    {
        for (int ch = 0; ch < chans; ++ch) main.copyFrom (ch, 0, dryBuffer, ch, 0, n);
        outPeak = juce::jmax (outPeak.load(), peak);
        return;
    }

    // ---------------- CLEAN ----------------
    // 1) low cut (24 dB/oct Butterworth)
    const float lowCut = pLowCut->load();
    if (! juce::exactlyEqual (lowCut, lastLowCut))
    {
        lowCut1.setHighPass (sr, lowCut, 0.5412);
        lowCut2.setHighPass (sr, lowCut, 1.3066);
        lastLowCut = lowCut;
    }
    if (lowCut > 20.5f)
        for (int ch = 0; ch < chans; ++ch)
        {
            auto* d = main.getWritePointer (ch);
            for (int i = 0; i < n; ++i) d[i] = lowCut2.process (lowCut1.process (d[i], ch), ch);
        }

    // 2) mouth clicks, 3) noise
    declicker.setAmount (pDeclick->load() * 0.01f);
    declicker.process (main, chans);
    denoiser.setAmount (pNoise->load() * 0.01f);
    denoiser.process (main, chans);

    // 4) gate for the pauses (downward expander with hold)
    {
        const float thrDb = pGate->load();
        const bool gateOn = thrDb > -79.9f;
        const float envAtt = coeff (sr, 0.0005), envRel = coeff (sr, 0.01);
        const float openC = coeff (sr, 0.001), closeC = coeff (sr, 0.06);
        const float holdSamples = (float) (0.04 * sr);
        float minG = 1.0f;
        for (int i = 0; i < n; ++i)
        {
            float lvl = 0.0f;
            for (int ch = 0; ch < chans; ++ch) lvl = juce::jmax (lvl, std::abs (main.getSample (ch, i)));
            gateEnv = lvl > gateEnv ? lvl + (gateEnv - lvl) * envAtt : lvl + (gateEnv - lvl) * envRel;

            float target = 1.0f;
            if (gateOn)
            {
                const float envDb = juce::Decibels::gainToDecibels (gateEnv, -120.0f);
                if (envDb > thrDb) gateHold = holdSamples;
                else if (gateHold > 0.0f) gateHold -= 1.0f;
                else target = juce::Decibels::decibelsToGain (juce::jmax (-40.0f, (envDb - thrDb) * 4.0f));
            }
            gateGain = target > gateGain ? target + (gateGain - target) * openC : target + (gateGain - target) * closeC;
            minG = juce::jmin (minG, gateGain);
            for (int ch = 0; ch < chans; ++ch) main.getWritePointer (ch)[i] *= gateGain;
        }
        gateGainDb = juce::Decibels::gainToDecibels (minG, -60.0f);
    }

    // LISTEN: hear only what the cleaning removed (aligned dry minus cleaned)
    if (pListen->load() > 0.5f)
    {
        for (int ch = 0; ch < chans; ++ch)
        {
            auto* d = main.getWritePointer (ch);
            const auto* dry = dryBuffer.getReadPointer (ch);
            for (int i = 0; i < n; ++i) d[i] = dry[i] - d[i];
        }
        float op = 0.0f;
        for (int ch = 0; ch < chans; ++ch) op = juce::jmax (op, main.getMagnitude (ch, 0, n));
        outPeak = juce::jmax (outPeak.load(), op);
        return;
    }

    // ---------------- VOCAL PUSH ----------------
    const float push = pPush->load() / 50.0f;   // 0..2, scales every push amount
    const float mudA = juce::jlimit (0.0f, 1.5f, pMud->load() * 0.01f * push);
    const float boxA = juce::jlimit (0.0f, 1.5f, pBox->load() * 0.01f * push);
    const float presA = juce::jlimit (0.0f, 1.5f, pPresence->load() * 0.01f * push);
    const float airA = juce::jlimit (0.0f, 1.5f, pAir->load() * 0.01f * push);
    const float levelA = juce::jlimit (0.0f, 1.5f, pLevel->load() * 0.01f * push);

    if (! juce::exactlyEqual (presA, lastPresence))
    {
        presenceEq.setPeak (sr, 4000.0, 0.9, 5.0 * presA);
        lastPresence = presA;
    }
    if (! juce::exactlyEqual (airA, lastAir))
    {
        airEq.setHighShelf (sr, juce::jmin (11000.0, sr * 0.42), 0.7, 4.0 * airA);
        lastAir = airA;
    }

    // dynamic mud / box / sibilance control: cut harder when that band takes too much of the voice
    {
        const float envC = coeff (sr, 0.015);
        const float cutC = coeff (sr / 32.0, 0.04);
        constexpr int sub = 32;
        for (int start = 0; start < n; start += sub)
        {
            const int len = juce::jmin (sub, n - start);
            for (int i = start; i < start + len; ++i)
            {
                float m = 0.0f;
                for (int ch = 0; ch < chans; ++ch) m += main.getSample (ch, i);
                m /= (float) chans;
                const float bm = mudDetect.process (m, 0), bb = boxDetect.process (m, 0), be = essDetect.process (m, 0);
                fullEnv = m * m + (fullEnv - m * m) * envC;
                mudEnv = bm * bm + (mudEnv - bm * bm) * envC;
                boxEnv = bb * bb + (boxEnv - bb * bb) * envC;
                essEnv = be * be + (essEnv - be * be) * envC;
            }

            float mudT = 2.0f * mudA, boxT = 1.5f * boxA, essT = 0.0f;   // gentle static clean-up
            if (toDb (fullEnv) > -60.0f)
            {
                mudT += 1.5f * mudA * juce::jlimit (0.0f, 8.0f, toDb (mudEnv) - toDb (fullEnv) + 9.0f);
                boxT += 1.5f * boxA * juce::jlimit (0.0f, 8.0f, toDb (boxEnv) - toDb (fullEnv) + 12.0f);
                essT = presA * juce::jlimit (0.0f, 10.0f, toDb (essEnv) - toDb (fullEnv) + 14.0f);
            }
            mudCutS = mudT + (mudCutS - mudT) * cutC;
            boxCutS = boxT + (boxCutS - boxT) * cutC;
            essCutS = essT + (essCutS - essT) * cutC;
            updatePushFilters (mudCutS, boxCutS, essCutS);

            for (int ch = 0; ch < chans; ++ch)
            {
                auto* d = main.getWritePointer (ch);
                for (int i = start; i < start + len; ++i)
                    d[i] = airEq.process (presenceEq.process (essEq.process (boxEq.process (mudEq.process (d[i], ch), ch), ch), ch), ch);
            }
        }
        mudCutDb = mudCutS;
        boxCutDb = boxCutS;
        essCutDb = essCutS;
    }

    // leveler: smooth vocal compression with auto makeup
    if (levelA > 0.0f)
    {
        const float threshold = -14.0f - 14.0f * levelA;
        const float ratio = 1.5f + 2.5f * juce::jmin (1.0f, levelA);
        const float knee = 6.0f;
        const float makeup = juce::Decibels::decibelsToGain (-threshold * (1.0f - 1.0f / ratio) * 0.6f);
        const float detC = coeff (sr, 0.005), att = coeff (sr, 0.004), rel = coeff (sr, 0.09);
        float minG = 1.0f;
        for (int i = 0; i < n; ++i)
        {
            float sq = 0.0f;
            for (int ch = 0; ch < chans; ++ch) sq = juce::jmax (sq, main.getSample (ch, i) * main.getSample (ch, i));
            compEnv = sq + (compEnv - sq) * detC;
            const float over = toDb (compEnv) - threshold;
            float gr = 0.0f;
            if (2.0f * over > knee) gr = over * (1.0f / ratio - 1.0f);
            else if (2.0f * std::abs (over) <= knee) gr = (1.0f / ratio - 1.0f) * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
            const float target = juce::Decibels::decibelsToGain (gr);
            compGain = target < compGain ? target + (compGain - target) * att : target + (compGain - target) * rel;
            minG = juce::jmin (minG, compGain);
            for (int ch = 0; ch < chans; ++ch) main.getWritePointer (ch)[i] *= compGain * makeup;
        }
        compGainDb = juce::Decibels::gainToDecibels (minG);
    }
    else
        compGainDb = 0.0f;

    // beat ride: keep the vocal at a constant distance above the beat (needs the sidechain)
    {
        const float rideA = pRide->load() * 0.01f;
        const float envC = coeff (sr, 0.3), gC = coeff (sr, 0.2);
        const float dt = (float) (1.0 / sr);
        bool scActive = false;
        for (int i = 0; i < n; ++i)
        {
            float v = 0.0f;
            for (int ch = 0; ch < chans; ++ch) v = juce::jmax (v, main.getSample (ch, i) * main.getSample (ch, i));
            vocEnv = v + (vocEnv - v) * envC;
            float target = 0.0f;
            if (haveSide)
            {
                const float s = sideBuffer.getSample (0, i);
                beatEnv = s * s + (beatEnv - s * s) * envC;
                const float vDb = toDb (vocEnv), bDb = toDb (beatEnv);
                scActive = scActive || bDb > -60.0f;
                if (vDb > -45.0f && bDb > -60.0f)
                {
                    // learn the usual vocal-to-beat distance (memory grows to 8 s), then hold it
                    const float diff = vDb - bDb;
                    rideActiveSec += dt;
                    const float avgC = std::exp (-dt / juce::jlimit (0.3f, 8.0f, rideActiveSec));
                    rideAvgDiff = rideActiveSec <= dt ? diff : diff + (rideAvgDiff - diff) * avgC;
                    if (rideActiveSec > 1.0f)   // envelopes need ~1 s to settle
                        target = rideA * juce::jlimit (-6.0f, 6.0f, rideAvgDiff - diff);
                }
            }
            rideGainDb = target + (rideGainDb - target) * gC;
            if (std::abs (rideGainDb) > 0.01f)
            {
                const float g = juce::Decibels::decibelsToGain (rideGainDb);
                for (int ch = 0; ch < chans; ++ch) main.getWritePointer (ch)[i] *= g;
            }
        }
        sidechainActive = scActive;
        rideDb = rideGainDb;
    }

    // ---------------- OUTPUT ----------------
    outGainS.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    mixS.setTargetValue (pMix->load() * 0.01f);
    for (int i = 0; i < n; ++i)
    {
        const float g = outGainS.getNextValue(), mix = mixS.getNextValue();
        for (int ch = 0; ch < chans; ++ch)
        {
            auto* d = main.getWritePointer (ch);
            d[i] = (d[i] * mix + dryBuffer.getSample (ch, i) * (1.0f - mix)) * g;
        }
    }
    float op = 0.0f;
    for (int ch = 0; ch < chans; ++ch) op = juce::jmax (op, main.getMagnitude (ch, 0, n));
    outPeak = juce::jmax (outPeak.load(), op);
}

juce::AudioProcessorEditor* VoxClearProcessor::createEditor()
{
    return new VoxClearEditor (*this);
}

void VoxClearProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VoxClearProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoxClearProcessor();
}

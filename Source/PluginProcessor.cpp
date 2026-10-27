#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    inline float toDb (float power) { return 10.0f * std::log10 (power + 1.0e-12f); }
    inline float coeff (double sr, double seconds) { return (float) std::exp (-1.0 / (seconds * sr)); }

    // parameters a preset never changes: they describe the user's microphone / session state
    bool presetKeepsParam (const juce::String& id)
    {
        return id == "mic_source" || id == "match_on" || id == "bypass" || id == "listen";
    }
}

VoxClearProcessor::VoxClearProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Beat Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pLowCut    = apvts.getRawParameterValue ("lowcut");
    pDeclick   = apvts.getRawParameterValue ("declick");
    pNoise     = apvts.getRawParameterValue ("noise");
    pGate      = apvts.getRawParameterValue ("gate");
    pListen    = apvts.getRawParameterValue ("listen");
    pPush      = apvts.getRawParameterValue ("push");
    pMud       = apvts.getRawParameterValue ("demud");
    pBox       = apvts.getRawParameterValue ("debox");
    pPresence  = apvts.getRawParameterValue ("presence");
    pAir       = apvts.getRawParameterValue ("air");
    pLevel     = apvts.getRawParameterValue ("level");
    pRide      = apvts.getRawParameterValue ("ride");
    pOutput    = apvts.getRawParameterValue ("output");
    pMix       = apvts.getRawParameterValue ("mix");
    pBypass    = apvts.getRawParameterValue ("bypass");
    pMicSource = apvts.getRawParameterValue ("mic_source");
    pMicTarget = apvts.getRawParameterValue ("mic_target");
    pMicAmount = apvts.getRawParameterValue ("mic_amount");
    pSmooth    = apvts.getRawParameterValue ("smooth");
    pTube      = apvts.getRawParameterValue ("tube");
    pMatchOn   = apvts.getRawParameterValue ("match_on");
    for (auto& m : matchCurve) m.store (0.0f);
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

    // defaults = preset "Natural - Clean & Open"
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "lowcut", 1 },   "Low Cut",   lowCutRange, 80.0f, hz));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "declick", 1 },  "De-Click",  pctRange, 40.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "noise", 1 },    "Noise Reduction", pctRange, 30.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "gate", 1 },     "Gate",      NormalisableRange<float> (-80.0f, -20.0f, 0.5f), -70.0f, gate));
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "listen", 1 },   "Listen Removed", false));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "push", 1 },     "Push",      pctRange, 35.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "demud", 1 },    "De-Mud",    pctRange, 50.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "debox", 1 },    "De-Box",    pctRange, 50.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "presence", 1 }, "Presence",  pctRange, 40.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "air", 1 },      "Air",       pctRange, 40.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "level", 1 },    "Level",     pctRange, 40.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "ride", 1 },     "Beat Ride", pctRange, 0.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 },   "Output",    NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, db));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 },      "Mix",       pctRange, 100.0f, pct));
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "bypass", 1 },   "Bypass",    false));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mic_source", 1 }, "Your Mic",     Mic::sourceNames(), 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mic_target", 1 }, "Target Sound", Mic::targetNames(), 2));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "mic_amount", 1 }, "Character",    pctRange, 60.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "smooth", 1 },     "Smooth",       pctRange, 30.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "tube", 1 },       "Tube",         pctRange, 10.0f, pct));
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "match_on", 1 },   "Match",        true));

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

//==============================================================================
int VoxClearProcessor::getNumPrograms() { return (int) Presets::all().size(); }

const juce::String VoxClearProcessor::getProgramName (int index)
{
    const auto& all = Presets::all();
    return juce::isPositiveAndBelow (index, (int) all.size()) ? juce::String::fromUTF8 (all[(size_t) index].name) : juce::String();
}

void VoxClearProcessor::setCurrentProgram (int index)
{
    const auto& all = Presets::all();
    if (! juce::isPositiveAndBelow (index, (int) all.size())) return;
    const auto& preset = all[(size_t) index];

    for (auto* param : getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
        if (ranged == nullptr || presetKeepsParam (ranged->getParameterID())) continue;

        float norm = ranged->getDefaultValue();
        for (const auto& [id, value] : preset.values)
            if (ranged->getParameterID() == id) norm = ranged->convertTo0to1 (value);

        ranged->beginChangeGesture();
        ranged->setValueNotifyingHost (norm);
        ranged->endChangeGesture();
    }
    currentProgram = index;
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

void VoxClearProcessor::clearMatch()
{
    for (auto& m : matchCurve) m.store (0.0f);
    hasMatch.store (false);
}

//==============================================================================
void VoxClearProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    numCh = juce::jlimit (1, 2, getMainBusNumOutputChannels());
    blockCap = juce::jmax (64, samplesPerBlock);

    declicker.prepare (sampleRate, numCh);
    denoiser.prepare (sampleRate, numCh);

    const int latency = declicker.getLatency() + denoiser.getLatency();
    setLatencySamples (latency);
    dryDelay.prepare (numCh, latency + 8);
    dryDelay.setDelay (latency);
    dryBuffer.setSize (numCh, blockCap);
    sideBuffer.setSize (1, blockCap);

    for (auto* b : { &lowCut1, &lowCut2, &mudDetect, &boxDetect, &essDetect, &mudEq, &boxEq, &essEq, &presenceEq, &airEq })
        b->reset();
    mudDetect.setBandPass (sr, 280.0, 1.0);
    boxDetect.setBandPass (sr, 650.0, 1.2);
    essDetect.setBandPass (sr, juce::jmin (7000.0, sr * 0.4), 2.0);

    // start every smoothed value at its target so playback never starts with a glide
    lowCutS = pLowCut->load();
    lowCut1.setHighPass (sr, lowCutS, 0.5412);
    lowCut2.setHighPass (sr, lowCutS, 1.3066);
    const float push = pPush->load() / 50.0f;
    presDbS = 5.0f * juce::jlimit (0.0f, 1.5f, pPresence->load() * 0.01f * push);
    airDbS = 4.0f * juce::jlimit (0.0f, 1.5f, pAir->load() * 0.01f * push);
    presenceEq.setPeak (sr, 4000.0, 0.9, presDbS);
    airEq.setHighShelf (sr, juce::jmin (11000.0, sr * 0.42), 0.7, airDbS);
    makeupS = 1.0f;
    tubeS = pTube->load() * 0.01f;
    for (int c = 0; c < 2; ++c) tubeLp[c] = dcIn[c] = dcOut[c] = 0.0f;
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

ToneSpec VoxClearProcessor::makeToneSpec (bool listening) const
{
    ToneSpec spec;
    spec.smooth = pSmooth->load() * 0.01f;
    if (listening) return spec;   // LISTEN shows what is *removed*, so the tone curve stays neutral

    const float amount = pMicAmount->load() * 0.01f;
    const auto& target = Mic::targets()[(size_t) juce::jlimit (0, (int) Mic::targets().size() - 1, (int) pMicTarget->load())];
    const auto& source = Mic::sources()[(size_t) juce::jlimit (0, (int) Mic::sources().size() - 1, (int) pMicSource->load())];
    size_t k = 0;
    for (auto b : target.bands) { b.gainDb *= amount; spec.bands[k++] = b; }
    for (auto b : source.bands) { b.gainDb *= juce::jmin (1.0f, 2.0f * amount); spec.bands[k++] = b; }   // fix your mic early on the knob
    if (pMatchOn->load() > 0.5f && hasMatch.load())
        for (size_t i = 0; i < (size_t) ToneSpec::matchBands; ++i) spec.matchDb[i] = matchCurve[i].load();
    spec.proximity = juce::jmin (1.0f, 1.5f * amount);
    return spec;
}

void VoxClearProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto main = getBusBuffer (buffer, false, 0);
    const int n = main.getNumSamples();
    const int chans = juce::jmin (numCh, main.getNumChannels());
    if (n == 0 || chans == 0) return;

    // optional beat sidechain
    const float* const* sidePtrs = nullptr;
    int sideChans = 0;
    juce::AudioBuffer<float> side;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        side = getBusBuffer (buffer, true, 1);
        sideChans = side.getNumChannels();
        sidePtrs = side.getArrayOfReadPointers();
    }

    // MATCH finished? (copied into atomics so the project can save it)
    std::array<float, ToneSpec::matchBands> learnedMatch;
    if (denoiser.consumeMatchResult (learnedMatch))
    {
        for (size_t i = 0; i < learnedMatch.size(); ++i) matchCurve[i].store (learnedMatch[i]);
        hasMatch.store (true);
    }

    // hosts may send blocks larger than announced: process in chunks so nothing is ever allocated here
    for (int start = 0; start < n; start += blockCap)
    {
        const int len = juce::jmin (blockCap, n - start);
        juce::AudioBuffer<float> view (main.getArrayOfWritePointers(), chans, start, len);
        const float* sideMono = nullptr;
        if (sideChans > 0)
        {
            sideBuffer.copyFrom (0, 0, sidePtrs[0] + start, len);
            for (int c = 1; c < sideChans; ++c) sideBuffer.addFrom (0, 0, sidePtrs[c] + start, len);
            if (sideChans > 1) sideBuffer.applyGain (0, 0, len, 1.0f / (float) sideChans);
            sideMono = sideBuffer.getReadPointer (0);
        }
        processChunk (view, sideMono, chans);
    }
}

void VoxClearProcessor::processChunk (juce::AudioBuffer<float>& main, const float* side, int chans)
{
    const int n = main.getNumSamples();
    constexpr int sub = 32;                                  // control-rate step for filters
    const float subC = 1.0f - coeff (sr / sub, 0.02);        // ~20 ms parameter glide

    float peak = 0.0f;
    for (int ch = 0; ch < chans; ++ch) peak = juce::jmax (peak, main.getMagnitude (ch, 0, n));
    inPeak = juce::jmax (inPeak.load(), peak);

    for (int ch = 0; ch < chans; ++ch) dryBuffer.copyFrom (ch, 0, main, ch, 0, n);
    juce::AudioBuffer<float> dry (dryBuffer.getArrayOfWritePointers(), chans, 0, n);
    dryDelay.process (dry, chans);

    if (pBypass->load() > 0.5f)
    {
        for (int ch = 0; ch < chans; ++ch) main.copyFrom (ch, 0, dry, ch, 0, n);
        outPeak = juce::jmax (outPeak.load(), peak);
        return;
    }

    // ---------------- CLEAN ----------------
    // 1) low cut (24 dB/oct Butterworth), frequency glides in the log domain
    {
        const float target = pLowCut->load();
        for (int start = 0; start < n; start += sub)
        {
            const int len = juce::jmin (sub, n - start);
            if (std::abs (lowCutS - target) > 0.05f)
            {
                lowCutS = std::exp (std::log (lowCutS) + subC * (std::log (target) - std::log (lowCutS)));
                lowCut1.setHighPass (sr, lowCutS, 0.5412);
                lowCut2.setHighPass (sr, lowCutS, 1.3066);
            }
            if (lowCutS > 20.5f)
                for (int ch = 0; ch < chans; ++ch)
                {
                    auto* d = main.getWritePointer (ch, start);
                    for (int i = 0; i < len; ++i) d[i] = lowCut2.process (lowCut1.process (d[i], ch), ch);
                }
        }
    }

    // 2) mouth clicks, 3) noise + microphone tone (one FFT stage)
    const bool listening = pListen->load() > 0.5f;
    declicker.setAmount (pDeclick->load() * 0.01f);
    declicker.process (main, chans);
    denoiser.setAmount (pNoise->load() * 0.01f);
    denoiser.setTone (makeToneSpec (listening));
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
    if (listening)
    {
        for (int ch = 0; ch < chans; ++ch)
        {
            auto* d = main.getWritePointer (ch);
            const auto* dr = dry.getReadPointer (ch);
            for (int i = 0; i < n; ++i) d[i] = dr[i] - d[i];
        }
        float op = 0.0f;
        for (int ch = 0; ch < chans; ++ch) op = juce::jmax (op, main.getMagnitude (ch, 0, n));
        outPeak = juce::jmax (outPeak.load(), op);
        return;
    }

    // ---------------- MIC: tube harmonics ----------------
    // Only the band below ~6 kHz is shaped, so the added 2nd/3rd harmonics stay below Nyquist
    // without oversampling: warm density for a few multiplications per sample.
    {
        const float target = pTube->load() * 0.01f;
        const float lpC = 1.0f - coeff (sr, 1.0 / (juce::MathConstants<double>::twoPi * 6000.0));
        const float tC = 1.0f - coeff (sr, 0.02);
        const float dcR = coeff (sr, 1.0 / (juce::MathConstants<double>::twoPi * 8.0));
        if (target > 0.0f || tubeS > 0.0001f)
            for (int i = 0; i < n; ++i)
            {
                tubeS += tC * (target - tubeS);
                const float a2 = 0.35f * tubeS, a3 = 0.12f * tubeS;
                for (int ch = 0; ch < chans; ++ch)
                {
                    float* d = main.getWritePointer (ch);
                    const float x = d[i];
                    tubeLp[ch] += lpC * (x - tubeLp[ch]);
                    const float lo = tubeLp[ch];
                    const float shaped = a2 * lo * lo - a3 * lo * lo * lo;   // even (warmth) + odd (density)
                    // DC blocker on the added harmonics only
                    const float dcFree = shaped - dcIn[ch] + dcR * dcOut[ch];
                    dcIn[ch] = shaped;
                    dcOut[ch] = dcFree;
                    d[i] = x + dcFree;
                }
            }
    }

    // ---------------- VOCAL PUSH ----------------
    const float push = pPush->load() / 50.0f;   // 0..2, scales every push amount
    const float mudA = juce::jlimit (0.0f, 1.5f, pMud->load() * 0.01f * push);
    const float boxA = juce::jlimit (0.0f, 1.5f, pBox->load() * 0.01f * push);
    const float presA = juce::jlimit (0.0f, 1.5f, pPresence->load() * 0.01f * push);
    const float airA = juce::jlimit (0.0f, 1.5f, pAir->load() * 0.01f * push);
    const float levelA = juce::jlimit (0.0f, 1.5f, pLevel->load() * 0.01f * push);

    // dynamic mud / box / sibilance control + gliding presence and air
    {
        const float envC = coeff (sr, 0.015);
        const float cutC = coeff (sr / sub, 0.04);
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

            const float presT = 5.0f * presA, airT = 4.0f * airA;
            if (std::abs (presDbS - presT) > 0.001f)
            {
                presDbS += subC * (presT - presDbS);
                presenceEq.setPeak (sr, 4000.0, 0.9, presDbS);
            }
            if (std::abs (airDbS - airT) > 0.001f)
            {
                airDbS += subC * (airT - airDbS);
                airEq.setHighShelf (sr, juce::jmin (11000.0, sr * 0.42), 0.7, airDbS);
            }

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

    // leveler: smooth vocal compression with auto makeup (makeup glides, so preset changes don't jump)
    {
        const float threshold = -14.0f - 14.0f * levelA;
        const float ratio = 1.5f + 2.5f * juce::jmin (1.0f, levelA);
        const float knee = 6.0f;
        const float makeupT = levelA > 0.0f ? juce::Decibels::decibelsToGain (-threshold * (1.0f - 1.0f / ratio) * 0.6f) : 1.0f;
        const float detC = coeff (sr, 0.005), att = coeff (sr, 0.004), rel = coeff (sr, 0.09), mkC = 1.0f - coeff (sr, 0.05);
        float minG = 1.0f;
        if (levelA > 0.0f || std::abs (makeupS - 1.0f) > 0.0005f || compGain < 0.9995f)
            for (int i = 0; i < n; ++i)
            {
                float sq = 0.0f;
                for (int ch = 0; ch < chans; ++ch) sq = juce::jmax (sq, main.getSample (ch, i) * main.getSample (ch, i));
                compEnv = sq + (compEnv - sq) * detC;
                float gr = 0.0f;
                if (levelA > 0.0f)
                {
                    const float over = toDb (compEnv) - threshold;
                    if (2.0f * over > knee) gr = over * (1.0f / ratio - 1.0f);
                    else if (2.0f * std::abs (over) <= knee) gr = (1.0f / ratio - 1.0f) * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
                }
                const float target = juce::Decibels::decibelsToGain (gr);
                compGain = target < compGain ? target + (compGain - target) * att : target + (compGain - target) * rel;
                makeupS += mkC * (makeupT - makeupS);
                minG = juce::jmin (minG, compGain);
                for (int ch = 0; ch < chans; ++ch) main.getWritePointer (ch)[i] *= compGain * makeupS;
            }
        compGainDb = juce::Decibels::gainToDecibels (minG);
    }

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
            if (side != nullptr)
            {
                const float s = side[i];
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
            d[i] = (d[i] * mix + dry.getSample (ch, i) * (1.0f - mix)) * g;
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
    auto state = apvts.copyState();
    juce::StringArray values;
    for (auto& m : matchCurve) values.add (juce::String (m.load(), 3));
    state.setProperty ("match", values.joinIntoString (","), nullptr);
    state.setProperty ("hasMatch", hasMatch.load(), nullptr);
    state.setProperty ("preset", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VoxClearProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            const auto values = juce::StringArray::fromTokens (state.getProperty ("match").toString(), ",", {});
            for (size_t i = 0; i < matchCurve.size(); ++i)
                matchCurve[i].store ((int) i < values.size() ? values[(int) i].getFloatValue() : 0.0f);
            hasMatch.store ((bool) state.getProperty ("hasMatch", false));
            currentProgram = (int) state.getProperty ("preset", 0);
            apvts.replaceState (state);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoxClearProcessor();
}

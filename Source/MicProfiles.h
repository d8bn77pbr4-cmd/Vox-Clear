#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>

// Smooth tone shapes in the log-frequency domain. They are applied inside the spectral stage
// (the FFT that already runs for noise reduction), so the microphone curves cost almost no CPU.
struct ToneBand
{
    enum Type { none = 0, bell, lowShelf, highShelf };
    int type = none;
    float freq = 1000.0f, gainDb = 0.0f, widthOct = 1.0f;   // width: bell width / shelf transition in octaves
};

inline float toneBandDb (const ToneBand& b, float freq)
{
    if (b.type == ToneBand::none || juce::exactlyEqual (b.gainDb, 0.0f)) return 0.0f;
    const float o = std::log2 (freq / b.freq);
    switch (b.type)
    {
        case ToneBand::lowShelf:  return b.gainDb / (1.0f + std::exp (o / (0.25f * b.widthOct)));
        case ToneBand::highShelf: return b.gainDb / (1.0f + std::exp (-o / (0.25f * b.widthOct)));
        default:
        {
            const float d = o / (0.5f * b.widthOct);
            return b.gainDb * std::exp (-0.5f * d * d);
        }
    }
}

// What the spectral stage needs from the processor (recomputed per bin only when it changes)
struct ToneSpec
{
    static constexpr int maxBands = 12;
    static constexpr int matchBands = 31;            // 1/3 octave, 20 Hz .. 20 kHz
    std::array<ToneBand, maxBands> bands {};
    std::array<float, matchBands> matchDb {};        // learned MATCH correction
    float smooth = 0.0f;                             // resonance smoothing 0..1
    float proximity = 0.0f;                          // proximity (bass build-up) control 0..1

    bool sameCurveAs (const ToneSpec& o) const
    {
        for (size_t i = 0; i < (size_t) maxBands; ++i)
            if (bands[i].type != o.bands[i].type || ! juce::exactlyEqual (bands[i].freq, o.bands[i].freq)
                || ! juce::exactlyEqual (bands[i].gainDb, o.bands[i].gainDb) || ! juce::exactlyEqual (bands[i].widthOct, o.bands[i].widthOct))
                return false;
        for (size_t i = 0; i < (size_t) matchBands; ++i)
            if (! juce::exactlyEqual (matchDb[i], o.matchDb[i])) return false;
        return true;
    }
    static float matchBandFreq (int i) { return 20.0f * std::pow (2.0f, (float) i / 3.0f); }
};

namespace Mic
{
    using B = ToneBand;

    // Target character: what typically makes the classic studio microphone families sound the way they do.
    // These are informed approximations of typical traits, not measurements or copies of specific models.
    struct Target
    {
        const char* name;
        const char* description;
        std::array<ToneBand, 6> bands;
    };

    inline const std::array<Target, 6>& targets()
    {
        static const std::array<Target, 6> t {{
            { "Neutral", "Keine Charakter-Kurve", {} },
            { "Classic Tube", "Warmer Röhren-Großmembraner: voller Körper, seidige breite Präsenz, weiche Höhen",
              {{ { B::lowShelf, 120.0f, 1.5f, 1.0f }, { B::bell, 250.0f, 1.0f, 1.5f }, { B::bell, 900.0f, -1.0f, 1.2f },
                 { B::bell, 5000.0f, 2.0f, 2.0f }, { B::highShelf, 12000.0f, 1.5f, 1.0f }, { B::highShelf, 17000.0f, -2.0f, 0.6f } }} },
            { "Modern Studio", "Moderner Studio-Standard: aufgeräumte Tiefmitten, klare Präsenz, offene Höhen",
              {{ { B::lowShelf, 100.0f, 0.5f, 1.0f }, { B::bell, 400.0f, -1.5f, 1.5f }, { B::bell, 4500.0f, 2.5f, 1.5f },
                 { B::highShelf, 10000.0f, 2.0f, 1.0f } }} },
            { "Bright Pop", "Teurer, luftiger Pop-Großmembraner: straffer Bass, viel Glanz und Air",
              {{ { B::lowShelf, 120.0f, -1.0f, 1.0f }, { B::bell, 350.0f, -2.0f, 1.5f }, { B::bell, 3500.0f, 2.0f, 1.5f },
                 { B::highShelf, 9000.0f, 4.0f, 1.0f }, { B::bell, 14000.0f, 1.5f, 1.0f } }} },
            { "Broadcast Dynamic", "Großes Broadcast-Dynamikmikro: dicker Bass, klare Artikulation, keine Schärfe",
              {{ { B::lowShelf, 150.0f, 2.0f, 1.0f }, { B::bell, 2500.0f, 1.5f, 1.5f }, { B::bell, 7000.0f, -1.5f, 1.0f },
                 { B::highShelf, 12000.0f, -3.0f, 1.0f } }} },
            { "Velvet Ribbon", "Bändchen-Charakter: warm, dunkel, samtig, sehr weiche Höhen",
              {{ { B::lowShelf, 100.0f, 2.0f, 1.0f }, { B::bell, 300.0f, 1.0f, 2.0f }, { B::highShelf, 8000.0f, -4.0f, 1.2f } }} },
        }};
        return t;
    }

    // Source compensation: typical weaknesses of everyday microphones
    struct Source
    {
        const char* name;
        const char* description;
        std::array<ToneBand, 5> bands;
    };

    inline const std::array<Source, 6>& sources()
    {
        static const std::array<Source, 6> s {{
            { "Standard / unbekannt", "Keine Korrektur (oder MATCH verwenden)", {} },
            { "USB-Mikrofon", "Dünner Bass, nasale Mitten, harte Spitze um 7 kHz",
              {{ { B::lowShelf, 120.0f, 2.0f, 1.0f }, { B::bell, 1000.0f, -2.5f, 1.0f }, { B::bell, 7000.0f, -3.0f, 0.6f },
                 { B::highShelf, 12000.0f, 1.0f, 1.0f } }} },
            { "Budget-Kondensator", "Boxige Mitten, scharfe Präsenz-Resonanzen",
              {{ { B::bell, 500.0f, -1.5f, 1.0f }, { B::bell, 3000.0f, -2.0f, 0.8f }, { B::bell, 6500.0f, -3.0f, 0.5f } }} },
            { "Bühnen-Dynamisch", "Nahbesprechungs-Bass, Präsenzspitze, kaum Höhen über 10 kHz",
              {{ { B::bell, 200.0f, -2.0f, 1.0f }, { B::bell, 5000.0f, -2.0f, 0.8f }, { B::highShelf, 10000.0f, 4.0f, 1.0f } }} },
            { "Headset / Laptop", "Wenig Bass, nasale Mitten, gedämpfte Höhen",
              {{ { B::lowShelf, 150.0f, 3.0f, 1.0f }, { B::bell, 1500.0f, -3.0f, 1.0f }, { B::bell, 3000.0f, -2.0f, 1.0f },
                 { B::highShelf, 8000.0f, 3.0f, 1.0f } }} },
            { "Gutes Studio-Kondensator", "Kaum Korrektur nötig", {} },
        }};
        return s;
    }

    inline juce::StringArray targetNames()  { juce::StringArray a; for (auto& t : targets()) a.add (juce::String::fromUTF8 (t.name)); return a; }
    inline juce::StringArray sourceNames()  { juce::StringArray a; for (auto& s : sources()) a.add (juce::String::fromUTF8 (s.name)); return a; }

    // Long-term balance of a finished, professionally recorded lead vocal (dB relative to 1 kHz) used by MATCH.
    // Deliberately broad: MATCH only corrects the overall tilt of the microphone, not the singer's voice.
    inline float referenceVocalDb (float f)
    {
        const float o = std::log2 (f / 1000.0f);
        if (f < 1000.0f) return -1.5f * std::abs (o) + 1.0f / (1.0f + std::pow (f / 250.0f - 1.0f, 2.0f));
        return f < 10000.0f ? -3.5f * o : -3.5f * std::log2 (10.0f) - 7.0f * std::log2 (f / 10000.0f);
    }
}

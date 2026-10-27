#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <utility>

// Factory presets. They set the sound (cleaning, mic character, push) but never touch
// "mic_source" and the learned MATCH curve - those describe *your* microphone, so every
// preset works on top of whatever mic is plugged in. Parameters not listed fall back to default.
namespace Presets
{
    struct Preset
    {
        const char* name;
        const char* description;
        std::vector<std::pair<const char*, float>> values;
    };

    // mic_target index: 0 Neutral, 1 Classic Tube, 2 Modern Studio, 3 Bright Pop, 4 Broadcast Dynamic, 5 Velvet Ribbon
    inline const std::vector<Preset>& all()
    {
        static const std::vector<Preset> list {
            { "Natural - Clean & Open", "Dezente Reinigung, moderner Studio-Klang, natürlich",
              { { "lowcut", 80 }, { "declick", 40 }, { "noise", 30 }, { "gate", -70 }, { "push", 35 }, { "demud", 50 }, { "debox", 50 },
                { "presence", 40 }, { "air", 40 }, { "level", 40 }, { "mic_target", 2 }, { "mic_amount", 60 }, { "smooth", 30 }, { "tube", 10 } } },
            { "Home Studio Rescue", "Starke Reinigung für unbehandelte Räume und laute Umgebung",
              { { "lowcut", 100 }, { "declick", 70 }, { "noise", 70 }, { "gate", -50 }, { "push", 50 }, { "demud", 70 }, { "debox", 70 },
                { "presence", 50 }, { "air", 50 }, { "level", 50 }, { "mic_target", 2 }, { "mic_amount", 80 }, { "smooth", 50 }, { "tube", 15 } } },
            { "Rap - Up Front", "Dicht, direkt und laut vorne, klare Artikulation",
              { { "lowcut", 100 }, { "declick", 60 }, { "noise", 50 }, { "gate", -55 }, { "push", 70 }, { "demud", 60 }, { "debox", 60 },
                { "presence", 65 }, { "air", 45 }, { "level", 75 }, { "mic_target", 4 }, { "mic_amount", 70 }, { "smooth", 40 }, { "tube", 25 } } },
            { "Pop Lead - Airy", "Glänzende, luftige Pop-Stimme mit weichen S-Lauten",
              { { "lowcut", 110 }, { "declick", 60 }, { "noise", 50 }, { "gate", -60 }, { "push", 60 }, { "demud", 55 }, { "debox", 55 },
                { "presence", 55 }, { "air", 75 }, { "level", 60 }, { "mic_target", 3 }, { "mic_amount", 80 }, { "smooth", 55 }, { "tube", 15 } } },
            { "R&B / Soul - Warm", "Warmer Röhren-Charakter, voller Körper, seidige Höhen",
              { { "lowcut", 80 }, { "declick", 50 }, { "noise", 40 }, { "gate", -65 }, { "push", 50 }, { "demud", 40 }, { "debox", 50 },
                { "presence", 45 }, { "air", 50 }, { "level", 55 }, { "mic_target", 1 }, { "mic_amount", 85 }, { "smooth", 40 }, { "tube", 45 } } },
            { "Trap Melodic - Bright & Tight", "Straff, hell und präsent für melodische Trap-Vocals",
              { { "lowcut", 130 }, { "declick", 60 }, { "noise", 50 }, { "gate", -55 }, { "push", 65 }, { "demud", 65 }, { "debox", 60 },
                { "presence", 60 }, { "air", 70 }, { "level", 70 }, { "mic_target", 3 }, { "mic_amount", 75 }, { "smooth", 50 }, { "tube", 20 } } },
            { "Singer-Songwriter - Intimate", "Nah, warm und samtig, wenig Bearbeitung",
              { { "lowcut", 70 }, { "declick", 50 }, { "noise", 40 }, { "gate", -70 }, { "push", 35 }, { "demud", 40 }, { "debox", 45 },
                { "presence", 35 }, { "air", 45 }, { "level", 35 }, { "mic_target", 5 }, { "mic_amount", 70 }, { "smooth", 30 }, { "tube", 30 } } },
            { "Podcast / Voice-Over", "Gleichmäßig, satt und sprachverständlich",
              { { "lowcut", 90 }, { "declick", 70 }, { "noise", 60 }, { "gate", -50 }, { "push", 55 }, { "demud", 60 }, { "debox", 60 },
                { "presence", 50 }, { "air", 30 }, { "level", 85 }, { "mic_target", 4 }, { "mic_amount", 80 }, { "smooth", 40 }, { "tube", 15 } } },
            { "Ad-Libs & Doubles", "Schlanker und heller, damit sie die Lead-Stimme nicht verdecken",
              { { "lowcut", 160 }, { "declick", 60 }, { "noise", 50 }, { "gate", -50 }, { "push", 60 }, { "demud", 75 }, { "debox", 65 },
                { "presence", 65 }, { "air", 60 }, { "level", 70 }, { "mic_target", 3 }, { "mic_amount", 60 }, { "smooth", 50 }, { "tube", 10 } } },
            { "Rock / Grit", "Mehr Biss und Röhren-Sättigung, durchsetzungsstark",
              { { "lowcut", 100 }, { "declick", 40 }, { "noise", 40 }, { "gate", -60 }, { "push", 60 }, { "demud", 55 }, { "debox", 55 },
                { "presence", 70 }, { "air", 40 }, { "level", 70 }, { "mic_target", 1 }, { "mic_amount", 70 }, { "smooth", 35 }, { "tube", 60 } } },
            { "Laptop / Headset Fix", "Maximale Reinigung und Klangaufwertung für einfache Mikros",
              { { "lowcut", 120 }, { "declick", 70 }, { "noise", 80 }, { "gate", -48 }, { "push", 55 }, { "demud", 60 }, { "debox", 70 },
                { "presence", 50 }, { "air", 55 }, { "level", 60 }, { "mic_target", 2 }, { "mic_amount", 90 }, { "smooth", 60 }, { "tube", 20 } } },
            { "Clean Only", "Nur reinigen: kein Push, kein Charakter",
              { { "lowcut", 80 }, { "declick", 60 }, { "noise", 50 }, { "gate", -60 }, { "push", 0 }, { "level", 0 }, { "mic_target", 0 },
                { "mic_amount", 0 }, { "smooth", 0 }, { "tube", 0 } } },
        };
        return list;
    }
}

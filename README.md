# Vox Clear – Vocal Cleanup & Push (VST3 / AU)

Für Home-Recording: gehört auf den **Insert der Haupt-Vocal-Spur** im Mixer.
FL-Studio-Sends werden *nach* den Insert-Effekten abgegriffen – alles, was an Hall, Delay oder Sidechain-Busse geschickt wird, ist also bereits gereinigt.

## CLEAN – Schmatzer, Rauschen, Pausen

| Regler | Funktion |
|---|---|
| LOW CUT | Hochpass 24 dB/Okt (Trittschall, Brummen, Plosive). Ganz links = aus |
| DE-CLICK | Entfernt Mundgeräusche/Schmatzer. Erkennt nur Bursts < 5 ms, die deutlich über dem letzten Spitzenpegel liegen – Konsonanten (t, k, s) und die Stimme selbst bleiben unberührt. Senkt nur die Höhen (> 1,5 kHz) für ~2 ms ab |
| NOISE | Spektrale Rauschentfernung (Rauschen, Raumton, Lüfter). Rauschprofil wird **automatisch** gelernt, ohne Klick |
| LEARN NOISE | Optional: 2 s Pause abspielen → festes Profil (für konstante Geräusche). Erneut klicken = zurück auf automatisch |
| GATE | Schließt in den Pausen (ganz links = aus) |
| LISTEN | Nur das hören, was entfernt wird – zum Einstellen |

## VOCAL PUSH – nach vorne, klar, direkt

| Regler | Funktion |
|---|---|
| PUSH | Makro: skaliert alle Push-Regler (0 = aus, 50 = normal, 100 = doppelt) |
| DE-MUD | Dynamische Absenkung um 280 Hz – stärker, je matschiger die Stelle |
| DE-BOX | Dynamische Absenkung um 650 Hz (Karton-/Pappklang) |
| PRESENCE | Präsenz um 4 kHz + automatischer De-Esser (zähmt S-Laute dynamisch) |
| AIR | Höhen-Shelf ab 11 kHz |
| LEVEL | Leveler/Kompressor mit Auto-Makeup – gleichmäßige Lautstärke |
| BEAT RIDE | Hält die Stimme konstant über dem Beat (benötigt Sidechain, s. u.) |

### Beat Ride einrichten (optional)
1. Im Mixer die **Beat-Spur** auswählen.
2. Rechtsklick auf den Send-Pfeil zur Vocal-Spur → **„Sidechain to this track“**.
3. In Vox Clear oben links im Plugin-Fenster (Zahnrad/Wrapper-Einstellungen) den **Sidechain-Eingang** dieser Spur zuweisen.
4. BEAT RIDE aufdrehen. Die Anzeige unten zeigt „Beat-Sidechain erkannt“.

## Technik
- Latenz ca. 51 ms (48 kHz) – FL gleicht das automatisch aus. Beim **Einsingen** das Plugin deaktivieren.
- CPU: ca. 2,5 % eines Kerns (gemessen, alle Module an).

## Bauen
GitHub → *Actions* → Artefakte **VoxClear-VST3-Windows** / **VoxClear-macOS**.
Lokal: `cmake -B build && cmake --build build --config Release --target VoxClear_VST3`

## Installation (Mac)
```
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components && cp -R ~/Downloads/dist/"Vox Clear.vst3" ~/Library/Audio/Plug-Ins/VST3/ && cp -R ~/Downloads/dist/"Vox Clear.component" ~/Library/Audio/Plug-Ins/Components/ && xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Vox Clear.vst3" ~/Library/Audio/Plug-Ins/Components/"Vox Clear.component" && echo "Installiert!"
```
Danach FL Studio → *Options → Manage plugins → Find installed plugins* → Stern bei *Vox Clear*.

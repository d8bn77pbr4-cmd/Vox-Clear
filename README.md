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

## PRESETS

Oben in der Preset-Leiste (oder im FL-Preset-Menü). Ein Preset stellt den **Sound** ein – Reinigung, Mikro-Charakter, Push –,
lässt aber **„Dein Mikro“ und das gelernte MATCH** unverändert. So funktioniert jedes Preset mit jedem Mikrofon.
Ein `*` hinter dem Namen zeigt, dass du danach etwas verändert hast. Preset-Wechsel gleiten weich über (kein Knacken).

| Preset | Wofür |
|---|---|
| Natural – Clean & Open | Dezent, natürlich, moderner Studio-Klang |
| Home Studio Rescue | Starke Reinigung für unbehandelte Räume |
| Rap – Up Front | Dicht, direkt, laut vorne |
| Pop Lead – Airy | Glänzend, luftig, weiche S-Laute |
| R&B / Soul – Warm | Röhren-Wärme, voller Körper |
| Trap Melodic – Bright & Tight | Straff, hell, präsent |
| Singer-Songwriter – Intimate | Nah, warm, samtig |
| Podcast / Voice-Over | Gleichmäßig, satt, verständlich |
| Ad-Libs & Doubles | Schlanker, damit die Lead-Stimme vorne bleibt |
| Rock / Grit | Mehr Biss und Sättigung |
| Laptop / Headset Fix | Maximale Aufwertung einfacher Mikros |
| Clean Only | Nur reinigen, kein Charakter |

## MIKROFON – jedes Mikro auf Studio-Niveau

Was Spitzen-Studiomikrofone ausmacht und was davon hier per DSP umgesetzt ist:

| Eigenschaft hochwertiger Mikros | Umsetzung |
|---|---|
| Ausgewogener, glatter Frequenzgang | **DEIN MIKRO** gleicht typische Schwächen aus (USB, Budget-Kondensator, Bühnen-Dynamisch, Headset) · **MATCH** misst dein Mikro beim Singen ein |
| Charakter (warm, modern, luftig, dunkel) | **ZIEL-SOUND**: Classic Tube, Modern Studio, Bright Pop, Broadcast Dynamic, Velvet Ribbon · Intensität mit **CHARACTER** |
| Keine spitzen, harten Kapsel-Resonanzen | **SMOOTH**: erkennt feste schmale Resonanzen im Langzeit-Spektrum und senkt sie ab – Obertöne und Formanten der Stimme bleiben unberührt |
| Kontrollierter Bass bei Nahbesprechung | automatische **Nahbesprechungs-Kontrolle** (an CHARACTER gekoppelt) |
| Röhren-/Übertrager-Dichte | **TUBE**: 2./3. Oberton, nur im Band unter 6 kHz – kein Aliasing, ohne Oversampling |
| Geringes Eigenrauschen | NOISE (Bereich CLEAN) |

**MATCH:** „MATCH LERNEN“ klicken und 6 s normal singen. Die gelernte Korrektur (max. ±4 dB, im Bass höchstens −2 dB) wird mit dem Projekt gespeichert. „MATCH AN“ schaltet sie ein/aus, „RESET“ löscht sie.

Ehrlich gesagt: Raumklang und Richtcharakteristik lassen sich nachträglich nicht ändern, und die Profile sind fundierte Annäherungen an typische Eigenschaften, keine vermessenen Kopien bestimmter Mikrofone.
Die gesamte Mikro-Bearbeitung läuft in der FFT, die für die Rauschentfernung ohnehin gerechnet wird – praktisch ohne zusätzliche CPU-Last.

### Behringer C1
Unter **DEIN MIKRO → Behringer C1** auswählen. Das Profil zähmt die zischelige Spitze um 5–9 kHz, gibt etwas Körper (150 Hz),
räumt leicht boxige Mitten auf und öffnet die Höhen über 14 kHz minimal. Es basiert auf dem veröffentlichten Frequenzgang
und typischen Erfahrungen, nicht auf einer Messung deines Exemplars – zusätzlich einmal **MATCH LERNEN**, dann ist es auf dein Mikro eingemessen.
Gut dazu: SMOOTH 40–60 %, Ziel-Sound *Modern Studio* oder *Classic Tube*.

## Einstellungen bleiben erhalten

- **Fenster schließen ist kein Problem:** Vox Clear arbeitet weiter, alle Einstellungen bleiben. Das Fenster muss nicht offen sein.
- **Mit dem FL-Projekt gespeichert:** alle Regler, Preset, gelerntes Rauschprofil (LEARN NOISE), MATCH und Fenstergröße.
- **Neue Instanzen:** *Dein Mikro* und *MATCH* merkt sich Vox Clear automatisch – beim nächsten Einfügen ist dein C1 schon gewählt.
- **STANDARD** (oben): „Aktuelle Einstellungen als Standard speichern“ – jede neue Instanz startet dann mit genau diesen Einstellungen.
  „Standard löschen“ stellt die Werkseinstellung wieder her.
- Gespeichert wird in einer kleinen Datei: Mac `~/Library/Application Support/VoxClear/Vox Clear.settings`,
  Windows `%APPDATA%\VoxClear\Vox Clear.settings`.

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
- CPU: ca. 3 % eines Kerns (gemessen, alle Module inkl. Mikrofon an).
- Fenster frei skalierbar (an der Ecke unten rechts ziehen).

## Bauen
GitHub → *Actions* → Artefakte **VoxClear-VST3-Windows** / **VoxClear-macOS**.
Lokal: `cmake -B build && cmake --build build --config Release --target VoxClear_VST3`

## Installation (Mac)
```
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components && cp -R ~/Downloads/dist/"Vox Clear.vst3" ~/Library/Audio/Plug-Ins/VST3/ && cp -R ~/Downloads/dist/"Vox Clear.component" ~/Library/Audio/Plug-Ins/Components/ && xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Vox Clear.vst3" ~/Library/Audio/Plug-Ins/Components/"Vox Clear.component" && echo "Installiert!"
```
Danach FL Studio → *Options → Manage plugins → Find installed plugins* → Stern bei *Vox Clear*.

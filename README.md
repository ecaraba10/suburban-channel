# Suburban Channel

A JUCE channel strip for recorded guitars (amped, reamped or bussed). Seven modules in any order: Gate, Compressor (FET, Opto or Bus), Process EQ, Solid EQ, Pulse EQ, Tube Sat and Width. VST3 (plus AU on macOS) and Standalone.

Not affiliated with or endorsed by any engineer, band, or hardware maker. The plugin itself uses generic labels only (Process EQ, Solid EQ, Pulse EQ, FET, Opto). Hardware names in this README only describe what each module is modeled on, and can be removed if you prefer. Do a trademark search before releasing it under any name.

## Build

Requires CMake 3.22+ and a C++17 compiler (Xcode, Visual Studio 2022, or GCC/Clang).

    cmake -B build
    cmake --build build --config Release

JUCE 8 is downloaded automatically. Plugins land under `build/SuburbanChannel_artefacts/Release/`.

## Tests (no JUCE needed)

    g++ -std=c++17 -O2 tests/dsp_test.cpp -o dsp_test && ./dsp_test
    g++ -std=c++17 -O2 -DSC_TEST_NO_EDITOR -Itests/stub_fn -ISource tests/processor_test.cpp Source/PluginProcessor.cpp -o processor_test && ./processor_test

- `tests/dsp_test.cpp` (58 checks) tests the DSP core (`Source/Dsp.h`): EQ curves, Pulse EQ specs and the low-end trick, compressor gain reduction/attack/release (FET, Opto and Bus), Auto Gain, sidechain filter, saturation anti-aliasing, width, and random-settings robustness at 22.05 to 192 kHz.
- `tests/processor_test.cpp` (46 checks) runs the real `PluginProcessor.cpp` on audio against a small functional stand-in for JUCE (`tests/stub_fn`): every module responds to the right parameters, module order and bypass/mix work, all 13 presets load and keep your chain order and global controls, the meters read correctly, and 24 random full-plugin runs stay finite.
- Both suites also pass under AddressSanitizer and UndefinedBehaviorSanitizer. `tests/stub_syntax` is a declaration-only JUCE stand-in used to compile-check the editor (`g++ -std=c++17 -fsyntax-only -Itests/stub_syntax -ISource Source/PluginEditor.cpp`).
- `tests/python/` holds the Python checks used to design and cross-check the DSP.

The stand-ins are written from JUCE 8's documented signatures. They are not JUCE, so the first build against the real JUCE is still the final check.

## Modules and what they follow

| Module | Based on | From published specs | Approximated |
|---|---|---|---|
| Gate | generic | n/a | everything |
| Compressor: FET | UREI 1176 | fixed threshold with Input driving it, ratios 4/8/12/20/All, 20-800 us attack, 50-1100 ms release, FET distortion that grows with gain reduction, All-buttons ratio between 12:1 and 20:1 | exact timing curves, program dependence (fast + slow release), feed-forward detector instead of post-FET sensing |
| Compressor: Bus | SSL G-series bus compressor | stereo-linked, ratios 2/4/10:1, attack 0.1/0.3/1/3/10/30 ms, release 0.1/0.3/0.6/1.2 s or Auto, makeup 0-15 dB, effective threshold drops as the ratio drops; stepped times follow the 10-90% convention (time constant = time/2.2, which also falls out of the RC values on the clone schematic) | feed-forward detector (the hardware is mostly feed-back), soft knee, the -4/-2/0 dB ratio threshold offsets, the Auto release model (fast + slow stage weighted by how long compression has lasted) |
| Compressor: Opto | Teletronix LA-2A | Peak Reduction / Gain (up to +40 dB) / Compress-Limit (3:1 / 100:1), ~10 ms attack, release ~60 ms to the first half then 0.5-5 s | soft knee, detector, the two-stage release model, input-side sensing |
| Process EQ | API 550B | 4 bands, 7 stepped frequencies each, 2/4/6/9/12 dB steps, reciprocal boost/cut, shelf/peak switch, proportional Q | Q values and filter shapes; the HPF is an extra |
| Solid EQ | SSL 4000 E | LF 30-450 Hz, LMF 0.2-2.5 kHz, HMF 0.6-7 kHz, HF 1.5-16 kHz, +/-15 dB, bell on LF/HF, mid Q, 18 dB/oct HPF 16-350 Hz (extended to 600 Hz here for lo-fi radio sounds), 12 dB/oct LPF 3-22 kHz | filter shapes, Q range, no Brown/Black switch |
| Pulse EQ | Pultec EQP-1A | low 20/30/60/100 Hz (+13.5 / -17.5 dB), high boost 3-16 kHz up to +18 dB with bandwidth, high atten 5/10/20 kHz up to -16 dB, clean tube make-up amp (THD 0.15% at +10 dBm) | the passive network (two fitted shelves reproduce the documented low-end trick: boost near 80 Hz, dip near 200 Hz at the 30 Hz setting) |
| Tube Sat | generic | n/a | biased tanh with anti-aliasing |
| Width | generic | n/a | mid/side plus Haas delay |

Compressor extras (all types): a sidechain high-pass (SC HPF) so low end doesn't pump the compression, and Auto Gain on the FET (level-matches a -10 dBFS peak; off = real unit behavior where more input is also louder).

## Using it

- **Signal Chain:** seven dropdowns set the module order. A module set to Off in every slot is hidden and the window shrinks. Presets never change the chain order or the global controls.
- **Compressor:** pick FET, Opto or Bus in its header; only that type's knobs show.
- **Meter:** a VU meter showing output level (0 VU = -18 dBFS RMS) or the selected compressor's gain reduction.
- **Bypass:** a clickless crossfade, also wired to your DAW's bypass button.
- **Presets:** ten factory presets in the window and in your DAW's preset menu. Hover any control for its name.

## Presets

Descriptive names on purpose; none use song, album or artist titles. They lean on habits engineers describe in published interviews and articles about big modern rock guitar mixes: stereo pairs panned hard left and right, a little extra top end around 8 kHz (about +6 dB), pushing the low end instead of scooping mids, light-to-moderate opto-style compression (roughly 2-5 dB), fast FET compression for tight chug, and little or no reverb. They are starting points, not recreations of any recording.

Anthem Wall, Garage Punk Chug, Rock Opera Layers, Ballad Acoustic Bed, Dirty Intro Riff, Chorus Lift Doubles, Palm Mute Verse, Lead Sting, Radio Gloss, Arena Clean Shimmer, Bus Glue, Lo-Fi Radio Intro.

Lo-Fi Radio Intro is a mono, squashed, band-limited tone (400 Hz high-pass, 4 kHz low-pass, a bump near 1.3 kHz, some grit). Forums and articles put the radio/telephone window at a 300-600 Hz high-pass and a 3-7 kHz low-pass: a lower high-pass sounds fuller, a higher one thinner; a lower low-pass sounds darker, a higher one brighter.

## Knob colours

Knobs follow each modeled unit's panel: 1176 = black knobs with silver tops (blue stripe); LA-2A = black with white pointers; API 550 = blue frequency knobs and silver gain knobs; SSL 4000 E = grey knobs with colour-coded band caps (brown LF, blue LMF, green HMF, purple HF, white filters, as in Waves' SSL 4000 manual); Pultec = black with white pointers on a blue-grey stripe.

## Quality notes

- Zero latency. Saturation and the compressors' output stages use first-order antiderivative anti-aliasing instead of oversampling.
- Gains, mix, bypass and EQ settings are smoothed, so moving knobs doesn't click.
- Filters are real-time safe (no allocation in the audio thread).

## Known limits

- Not a component-level circuit emulation. Original schematics were not used for the passive networks, only published specs and open-source references (read for behavior only; no code copied, the open-source Pultec model is GPL-3.0).
- The JUCE plugin shell has not been built against the real JUCE (no internet in the environment where it was written). It compiles against a stand-in built from JUCE 8's documented signatures, and its logic runs in `tests/processor_test.cpp`.

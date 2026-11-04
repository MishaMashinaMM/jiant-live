# JIANT factory sample bank 1

Twelve new synthetic mono recordings, 5406 bytes including the 256-byte header after IMA ADPCM encoding. The bank fits SAMPLE's 6144-byte bank without truncating any slot. The JSON lives on the website; nothing is embedded in the firmware image. Audio recordings are dedicated to the public domain under CC0-1.0. The generator source is GPL-3.0-only, like JIANT.

| Slot / key (KEYS SLOTS, SLOT 1) | Sound | Rate | Duration | Source |
|---|---|---:|---:|---|
| 1 / C4 | KICK | 8000 | 110 ms | JIANT DRUM-X |
| 2 / C#4 | SNARE | 8000 | 90 ms | JIANT DRUM-X |
| 3 / D4 | HAT | 8000 | 55 ms | JIANT DRUM-X |
| 4 / D#4 | CLAP | 8000 | 100 ms | JIANT DRUM-X |
| 5 / E4 | TOM | 8000 | 90 ms | JIANT DRUM-X |
| 6 / F4 | AAH | 4000 | 240 ms | JIANT VOICE, LANG OFF |
| 7 / F#4 | BIRD | 4000 | 220 ms | JIANT ANIMAL / CMOS BIRD |
| 8 / G4 | PIANO | 4000 | 250 ms | New damped additive synthesis |
| 9 / G#4 | FLUTE | 4000 | 300 ms | New additive synthesis |
| 10 / A4 | VINYL | 2000 | 350 ms | New deterministic filtered noise / crackle |
| 11 / A#4 | BASS | 4000 | 250 ms | JIANT ANALOG |
| 12 / B4 | CHORD | 4000 | 250 ms | JIANT WHEEL, C major |

Piano and flute are new syntheses, not recordings copied from an unverified CC0 library. Every sound is normalized and faded at its edges. The generator is `tools/gen_factory_samples.c`; build/run instructions are at its top. `web/test_sampler.mjs` packs this JSON through the editor's actual encoder. `tests/factory_bank_test.c` then transfers, indexes and plays every slot through the firmware's real C sampler.

In the editor, **Load factory samples** stages the bank locally. Review the list, then **Send to the device** confirms replacement of all stored samples. Use SAMPLE's **KIT KEYS** preset to play slots across the keyboard. Loading a bank does not automatically change tracks or their selected preset. Keep the original WAV files: project backups currently do not contain SAMPLE's audio bank. Neither the editor nor the installer writes these factory samples automatically.

## Matching source

The generator and native/browser tests are in [the validated sampler source revision](https://github.com/juanjiant-bit/JIANT/tree/3b013493381de9011031a457db20ea5074cef1de). Check out that revision before running the build commands, so the DSP and generated bank match. Validation: [Chromium and native sampler checks](https://github.com/juanjiant-bit/JIANT/actions/runs/40494253074).

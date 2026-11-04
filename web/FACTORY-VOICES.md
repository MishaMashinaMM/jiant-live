# Voices + instruments bank (beta SAMPLE)

In the editor's **Sound → SAMPLE: your samples** panel, choose **Voices + instruments (7)** next to **Load factory samples**. Click **Load factory samples** to stage it, then **Send to the device** to confirm replacing the entire stored bank. Requires firmware with SAMPLE (par13b beta). Nothing is sent automatically.

| Slot / key (KIT KEYS) | Sound | Rate | Duration |
|---|---|---:|---:|
| 1 / C4 | GUITAR | 8000 Hz | 220 ms |
| 2 / C#4 | BASS | 6000 Hz | 180 ms |
| 3 / D4 | HAT | 12000 Hz | 100 ms |
| 4 / D#4 | OPEN HAT | 12000 Hz | 160 ms |
| 5 / E4 | VOICE 06 | 6000 Hz | 240 ms |
| 6 / F4 | VOICE 10 | 6000 Hz | 280 ms |
| 7 / F#4 | VOICE 14 | 4000 Hz | 650 ms |

6096 of 6144 bytes after IMA ADPCM encoding, including the header. Rates are kept per slot: do not resample every slot to a common rate. The bank contains short excerpts, not full vocal phrases. Mono mixing, filtered polyphase downsampling, brief fades and normalization were applied. The previous synthetic bank is still available as **Synth (12)**.

Source: the contributor's uploaded samples.zip: A03_chorus-guitar-pluck.wav, A07_bass-short-2.wav, A08_hats.wav, A12_hat-semi-open.wav, A06.wav, A10.wav and A14.wav. The contributor selected the three vocal files and requested publication in the editor. No CC0 dedication or third-party library license is asserted for these recordings; original source/license details have not been provided. This bank is separate from the CC0 synthetic bank.

The reviewed interpolated audio preview was generated offline. Publishing this bank does not add interpolation to the firmware: current par13b playback still holds samples. Real-device audition remains necessary. Native tests verified transfer, CRC and non-silent playback of all seven slots; that does not establish subjective audio quality.

Project backups do not contain the SAMPLE audio bank. Keep original audio files before replacing it.

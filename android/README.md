# JIANT on Android

**JIANT Probe** (`android/probe`, 0.4) is built by GitHub Actions (`.github/workflows/android.yml`) on every push that
touches `android/`: the one-screen test of what a phone does with the FM-1's USB audio (channel counts, its own USB
driver, the mic through the FM-1 and back). Kept for diagnosing a phone; see [probe/README.md](probe/README.md).

| | What it is | APK |
|---|---|---|
| **JIANT Probe** (`android/probe`, 0.4) | What a phone does with the FM-1 over USB: the channels Android passes, the stream read directly, the round trip. | `jiant-probe.apk` |

Download it from the site (<https://juanjiant-bit.github.io/JIANT/app/jiant-probe.apk>, once the change is on `main`)
or from any branch: the Actions tab → the **android** run → **Artifacts** → `jiant-probe`. Android 10 or later and a
USB-C data cable.

## On the FM-1

**MENU › SYSTEM › USB**, then unplug and plug the cable again (the FM-1 re-enumerates):

| Mode | Channels to the host |
|---|---|
| **STEMS+IN** (0.8.0) | 10: T1–T6, FX L R, VOX (your voice as the last live track mixed it), DRY (your voice as it came); the host's audio comes in as the voice |
| **STEMS** (0.7.1) | 8: T1–T6, FX L R |
| STEREO / CONSOLE / IN+OUT | 2: the master |

The tracks are each mono, before the master (no leveler, limiter or MASTER knob): their sum with the FX return is the
mix as the FM-1's buses hear it. USB LEVEL at −6 dB or lower gives headroom. Clean STEMS need firmware **0.9.8** or
later.

## Building

JDK 17 and Gradle 8.9 (the workflow's versions):

```sh
gradle -p android/probe assembleDebug
```

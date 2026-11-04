# JIANT Probe

A USB audio study (the design: one Android screen that answers, on a given phone, what the app depends on):

1. **Devices**: every audio input and output Android sees, with the channel counts and index masks each declares,
   and the MIDI devices (the FM-1's USB-MIDI).
2. **Record 10 s of the 8 channels** with the FM-1 in MENU > SYSTEM > USB > STEMS: asks Android for 8 channels (then 6,
   4, 2) by channel index mask at 44.1 kHz, reports what it got, the level of each channel, and saves the take as a
   multichannel WAV in Downloads/JIANT. 8 channels: the app can use the normal API (Oboe / AAudio); fewer: it needs a
   USB driver of its own.
3. **The FM-1 and a USB microphone at once** (through a USB-C hub): whether Android opens two USB audio inputs together.
4. **The phone's microphone into the FM-1** with the FM-1 in USB > IN+OUT: 5 s of the mic played to the USB output.
5. **Our own driver** (0.2): Android's framework hands the FM-1's eight channels on as two on some phones (an S23
   Ultra: T1 T2, the rest zero). This reads the stream itself, as the multichannel recorder apps do: asks for the
   USB permission, claims the streaming interface and does isochronous reads through usbfs (`app/src/main/cpp/usbiso.c`),
   10 s, with the per-channel levels, the frames a second and a WAV. Test **2b** tries the other ways of opening the
   input (sources × 8/6 channels) and shows which channels carry sound.
6. **The stems and the phone's microphone together** (0.3): test 5 with the built-in mic recorded at the same time
   (a mono WAV beside the 8-channel one): the mic as one more track in the app.
7. **The mic through the FM-1 and back** (0.4, firmware 0.8.0, MENU > SYSTEM > USB > STEMS+IN): the phone's mic goes
   out to the FM-1 by our own driver (isochronous OUT, `usbiso.c` duplex) into the last live track, its sends and
   FX; 10 channels come back (T1–T6, FX L R, the mic as mixed, the mic as it came). With the phone near the FM-1's
   speaker the mic hears the FM-1: the lag between the stems and the raw mic is the whole latency, and a second WAV
   has the mic channels moved earlier by it.

Every build signs with `app/probe-debug.keystore` (a debug key, public on purpose) so a new APK installs over the last.

Everything goes into a plain-text report (COPY / SHARE). Built by GitHub Actions (.github/workflows/emulator.yml for
the site, android.yml on the branches): `gradle -p android/probe assembleDebug`. No dependencies beyond the Android
SDK; Java 17, minSdk 29, targetSdk 34.

# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
# (JIANT 0.8.0) "In the lab": the film of what is being tested (stems over USB, the web editor, the voice through the
# FM-1, the Android app, the live visuals, what comes next), in English, drawn by tools/video/tutorial.html as the tutorial is (tutorial_voice.py ... en news).
from tutorial_script_en import SAY, say  # noqa: F401  (the same names spelt out)

META = dict(kicker='IN THE LAB', tagline='WHAT WE ARE TESTING · v0.8.0', intro3='IN THE LAB · TEST VERSIONS')
CHAPTERS = [
    dict(id='intro', k='', title='JIANT', g='intro', lines=[
        ("Hi, this is JIANT.", 0),
        ("Today, a look inside the lab: the new things we're testing right now, before they become official.", 1),
        ("As always, thank you to everyone supporting the project on Ko-fi. This is what your support is building.", 2),
        ("These are test versions, so try them, break them, and tell me how they behave.", 3),
    ]),
    dict(id='stems', k='01', title='STEMS OVER USB', g='stems', lines=[
        ("First: stems over USB. In MENU, SYSTEM, USB, choose STEMS, and the FM-1 becomes an eight-channel audio interface.", 0),
        ("The six tracks arrive on their own channels, plus the effects return in stereo. "
         "Record them in Ableton, Logic, Reaper or any DAW, and mix them later.", 1),
        ("And without the FM-1 at hand, the web emulator renders the same stems from your backup, faster than real time.", 2),
    ]),
    dict(id='editor', k='02', title='THE WEB EDITOR', g='editor', lines=[
        ("Second: the web editor. Open it in Chrome, plug in the FM-1, and your sounds and settings are on your computer screen.", 0),
        ("Edit with the mouse, send DX7 voice banks to FM6, manage your user presets, and back up all your projects.", 1),
        ("It all travels over USB MIDI, live, while the FM-1 keeps playing.", 2),
    ]),
    dict(id='micin', k='03', title='YOUR VOICE THROUGH THE FM-1', g='micin', lines=[
        ("Third, and brand new in 0.8.0: STEMS+IN.", 0),
        ("Your phone's microphone goes into the FM-1, into the last track, through its level, pan and sends. "
         "Your voice gets the same reverb and delay as your synths.", 1),
        ("And ten channels come back: the six tracks, the effects, and your voice on its own channel, "
         "so it can be recorded and mixed apart.", 2),
        ("The round trip takes a few milliseconds, so the app measures that latency, "
         "and moves your voice back into time when you stop recording.", 3),
    ]),
    dict(id='app', k='04', title='THE STEMS APP', g='app', lines=[
        ("That's what the Android app is for. Plug the FM-1 into your phone, press record, and every track is captured live.", 0),
        ("Then edit your takes: trim, cut, mix, and export a ZIP of stems or one mixed file.", 1),
        ("Android passes only some of a USB device's channels to apps, so the app talks to the FM-1 with its own USB driver. "
         "Our first tests already read the FM-1's stream, steady, sample by sample.", 2),
        ("And while you play, the app shows live visuals, each one moved by its own track.", 3),
    ]),
    dict(id='visuals', k='05', title='LIVE VISUALS', g='visuals', lines=[
        ("Here they are. Nebula: a flight through a cloud of light. The bass swells it, the kick tears it apart.", 0),
        ("Smoke: tens of thousands of particles in three dimensions, thrown out by every hit, drifting in and out of focus.", 1),
        ("Membrane: a living petri dish, where the spots divide like cells.", 2),
        ("Scan: a landscape of monitor lines, lifted by the sound.", 3),
        ("Orbits: beads of light, tracing the path of every track.", 4),
        ("And the microscope: the beings of the FM-1, alive under the lens, the focus breathing through the depth.", 5),
    ]),
    dict(id='next', k='06', title='COMING NEXT', g='next', lines=[
        ("And what's coming next.", 0),
        ("Assignable effect buses: four of them, A to D, so each track can choose its own chain of effects.", 1),
        ("The stems app and its visuals, out of the lab and onto your phone.", 2),
        ("And a big one: a port of JIANT to the SMK, the same instrument on new hardware.", 3),
        ("Supporters on Ko-fi get every new version first, before it goes public. "
         "Help me on Ko-fi, and you'll be the first to play them.", 4),
    ]),
    dict(id='outro', k='', title='THANK YOU', g='outro', lines=[
        ("That's what's cooking. If you want to test any of it, the links are in the description.", 0),
        ("If this work resonates with you, you can support it on Ko-fi, or on Cafecito from Argentina.", 1),
        ("Thanks for being part of it. See you soon.", 2),
    ]),
]

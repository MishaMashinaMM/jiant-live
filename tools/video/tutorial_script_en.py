# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
# (JIANT 0.8.0) the tutorial film's script in English (tools/video/tutorial_script.py's chapters, ids, graphics and cues;
# tutorial_voice.py ... en). SAY: how the voice says what the screen spells.
CHAPTERS = [
    dict(id='intro', k='', title='JIANT', g='intro', lines=[
        ("Hi. This is JIANT.", 0),
        ("A free, open firmware that turns the FM-1 into a complete instrument: six tracks, eight synthesis engines, "
         "a deep sequencer and effects, all inside this little keyboard.", 1),
        ("JIANT is made by Juan Jiant, a muralist and visual artist who, after many years in art, fell in love with music.", 1),
        ("Before we start, a heartfelt thank you to everyone supporting the project on Ko-fi. "
         "You are the reason JIANT keeps growing.", 2),
        ("In this tutorial we'll walk through the whole system, step by step.", 3),
    ]),
    dict(id='install', k='01', title='INSTALL', g='install', lines=[
        ("First, installing. Open the web installer in Chrome or Edge, "
         "and connect the FM-1 straight to your computer with a USB data cable.", 0),
        ("Press Install and wait until it finishes, without unplugging.", 1),
        ("If you're coming from another custom firmware, go back to the official one first, with the installer's button.", 2),
    ]),
    dict(id='panel', k='02', title='THE PANEL', g='panel', lines=[
        ("This is the panel. At the top there are three encoders.", 0),
        ("ALGORITHM picks the track. PRESETS browses the sounds. And SELECT steps through the pages of the section you're in.", 1),
        ("The four knobs control the four cards on the screen. "
         "Turn them fast to sweep the whole range; turn them slowly to fine tune.", 2),
        ("The buttons open the pages: ENV, LFO, FX, SCL, EDIT, GLO, SAVE, ARP and SEQ. "
         "And the twenty-seven keys play the selected track.", 3),
    ]),
    dict(id='screen', k='03', title='THE SCREEN', g='screen', lines=[
        ("The screen always keeps the same order. At the top, the header: "
         "transport, tempo, processor load and battery.", 0),
        ("Below it, four cards, one per knob. The value you're turning lights up in orange.", 1),
        ("In the middle, the page's graphic. And at the bottom, the page name and where you are.", 2),
        ("Colours work like a thermal camera: cold is little, hot is a lot. "
         "Violet is what's selected; orange, what's active.", 3),
    ]),
    dict(id='home', k='04', title='HOME', g='home', lines=[
        ("Press HOME and you reach the ecosystem: one creature per track.", 0),
        ("Each species belongs to an engine, and its shape comes from its four knobs. "
         "When the track plays, the creature swells and heats up with its own audio.", 1),
        ("The selected track is drawn bigger. A muted track goes dark.", 2),
    ]),
    dict(id='layers', k='05', title='TAP AND HOLD', g='layers', lines=[
        ("All navigation comes down to two gestures. Tap a button to open its pages; tap it again for the next one.", 0),
        ("Hold it to open its layer: while you hold it, the keys and knobs change roles, "
         "and a map shows what each one does. Let go, and you're back where you were.", 1),
        ("For example, hold GLO to mute tracks with the black keys, or hold FX to play effects live.", 2),
    ]),
    dict(id='engines', k='06', title='EIGHT ENGINES', g='engines', lines=[
        ("JIANT has eight engines, all synthesis, no samples. Any engine can go on any track.", 0),
        ("ANALOG: two oscillators with a filter per voice. FM6: six-operator FM, with thirty-two algorithms. "
         "PHASE: phase distortion.", 1),
        ("LOFI: chip sounds, bytebeat and floatbeat. VOICE: a formant voice. "
         "WHEEL: a drawbar organ with a rotary speaker.", 2),
        ("NOISE: coloured and metallic noise. And DRUM-X, the synthesized drum kit.", 3),
        ("Turn PRESETS to browse every engine's sounds in a single list. "
         "And if you want something new, hold EDIT and turn SELECT: the sound gets a roll of the dice.", 4),
    ]),
    dict(id='signal', k='07', title='THE SIGNAL PATH', g='signal', lines=[
        ("This is how sound travels inside a track.", 0),
        ("The engine makes the voice. The filter shapes it: low pass, high pass, band pass or comb. "
         "The envelope and the LFO move it in time.", 1),
        ("Then it goes through the track's distortion, with six types, including an amp emulator.", 2),
        ("Level and pan place it in the mix, and three sends feed the chorus, the delay and the reverb, "
         "shared by every track.", 3),
        ("At the end, the master: an always-on limiter, with clip, punch and ducking.", 4),
    ]),
    dict(id='drumx', k='08', title='DRUM-X', g='drumx', lines=[
        ("DRUM-X is a drum kit of eight sounds generated in real time: "
         "kick, snare, clap, two hi-hats, tom, rim and cowbell.", 0),
        ("Every sound has two sides, A and B. The MORPH knob moves the whole kit from one side to the other: "
         "it's the knob to play live.", 1),
        ("There are nine factory kits, from 909 and 808 to industrial and acoustic. And every sound can be edited in depth: "
         "pitch, noise, layers, feedback, and its own send to an effect.", 2),
    ]),
    dict(id='seq', k='09', title='THE SEQUENCER', g='seq', lines=[
        ("Every track has its own sixty-four step sequencer, with its own length, division, swing and gate. "
         "Patterns of different lengths turn against each other.", 0),
        ("On a melodic track, the STEP page is a piano roll: each step holds up to four notes, with accent and slide.", 1),
        ("You can write step by step, or record live with REC and PLAY.", 2),
        ("And each step can carry components: probability, ratchets, echoes, arpeggios, "
         "or notes that walk up the scale, loop after loop.", 3),
        ("With PLAYHEADS, up to three more heads read the same pattern at another speed, or backwards. "
         "And MUTATE transforms the sound and the pattern every few bars.", 4),
    ]),
    dict(id='mod', k='10', title='MODULATION', g='mod', lines=[
        ("Modulation brings it to life. Every track has an envelope and an LFO, "
         "with their destinations: pitch, filter, shape and amplitude.", 0),
        ("The MOD matrix adds four more routes per track, with sources like velocity, "
         "the modulation sequencer or noise, and destinations that reach the effects.", 1),
        ("And the four macros: hold LFO, and the knobs become M1 to M4, each moving many parameters at once.", 2),
    ]),
    dict(id='fx', k='11', title='EFFECTS', g='fx', lines=[
        ("The effects. An analog or digital delay, with pitch and ping pong. "
         "A reverb with five models, including SHIMMER, RESO tuned to the scale, and CLOUD, granular.", 0),
        ("A stereo chorus, and the SLICER, which chops the audio into rhythms on the tracks you choose.", 1),
        ("And live: hold FX, and every key is a momentary effect. Repeats, filters, octaves, stutters.", 2),
        ("Everything you play can be recorded into a lane that repeats with the song.", 3),
    ]),
    dict(id='gen', k='12', title='GENERATOR AND DICE', g='gen', lines=[
        ("Out of ideas? On HOME, turn SELECT and the generator opens. Pick a genre, press OCT plus, "
         "and a whole song appears: engines, sounds, parts, scale, tempo and effects.", 0),
        ("The dice work on every part: hold SEQ, LFO or FX, and turn SELECT to vary just that.", 1),
        ("And if you change your mind, hold GLO and press the G4 key: undo.", 2),
    ]),
    dict(id='arrange', k='13', title='SAVE AND ARRANGE', g='arrange', lines=[
        ("To save, every project has four sections, A to D. Save an idea in A, and a variation in B.", 0),
        ("On the ARRANGE page you chain sections in rows, each with its length in bars. "
         "Press PLAY, and the song plays in order.", 1),
        ("And live, hold SAVE: the keys launch sections on the next bar.", 2),
    ]),
    dict(id='usb', k='14', title='USB AND MIDI', g='usb', lines=[
        ("The FM-1 is also a USB audio interface. In STEMS mode it sends eight channels: "
         "the six tracks apart, plus the effects, straight into your DAW.", 0),
        ("With IN+OUT, your computer or phone plays through the FM-1. "
         "And the new STEMS+IN mode joins both: you record the stems, and your voice goes through the last track's effects.", 1),
        ("Plus MIDI over USB, Bluetooth and TRS, with clock to sync with other gear.", 2),
    ]),
    dict(id='web', k='15', title='ON THE WEB', g='web', lines=[
        ("On the web there are three tools: the emulator, which runs JIANT in your browser; the installer; "
         "and the editor, to manage sounds, banks and backups from your computer.", 0),
        ("From the emulator you can also render the stems of a song from your backup, faster than real time.", 1),
    ]),
    dict(id='outro', k='', title='THANK YOU', g='outro', lines=[
        ("That's JIANT. The full manual, in English and Spanish, is on the project page.", 0),
        ("If this work resonates with you, you can support it on Ko-fi, or on Cafecito from Argentina.", 1),
        ("Thanks for watching, and thanks again to everyone who makes it possible. See you next time.", 2),
    ]),
]

SAY = [
    ('STEMS+IN', 'stems plus in'), ('IN+OUT', 'in plus out'), ('FM-1', 'F M one'), ('FM6', 'F M six'),
    ('DRUM-X', 'drum X'), ('M1 to M4', 'M one to M four'), ('JIANT', 'Giant'), ('Juan Jiant', 'Juan Giant'),
    ('Jiant', 'Giant'), ('LFO', 'L F O'), ('FX', 'F X'), ('SCL', 'scale'), ('ENV', 'envelope'), ('GLO', 'G L O'),
    ('SEQ', 'seek'), ('DAW', 'D A W'), ('TRS', 'T R S'), ('G4', 'G four'), ('OCT plus', 'octave plus'),
    ('USB', 'U S B'), ('MOD', 'mod'), ('ALGORITHM', 'algorithm'), ('PRESETS', 'presets'), ('SELECT', 'select'),
    ('LOFI', 'lo-fi'), ('DX7', 'D X seven'), ('SMK', 'S M K'), ('A to D', 'A to D'), ('MENU', 'menu'), ('Ko-fi', 'Ko-fi'), ('Cafecito', 'Cafesito'), ('909', 'nine oh nine'), ('808', 'eight oh eight'),
]


def say(text):
    import re
    for a, b in SAY:
        text = re.sub(r'(?<![\w-])' + re.escape(a) + r'(?![\w-])', b, text)
    return text

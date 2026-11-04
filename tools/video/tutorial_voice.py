# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
# (JIANT 0.8.0) the tutorial film's voice-over: each line of tools/video/tutorial_script.py spoken (Kokoro, an ONNX
# text-to-speech model, run locally: pip install kokoro-onnx soundfile; the model and voices from the kokoro-onnx
# releases in $KOKORO, default ~/tts), laid end to end with pauses; the timeline the capture and the film follow.
#   python3 tools/video/tutorial_voice.py build/tut [voice] [speed] [es|en] [tutorial|news]   (en: tutorial_script_en.py, an
#   English voice; news: news_script_en.py, the lab film)
#   -> build/tut/voice.wav (24 kHz mono), build/tut/timeline.json, build/tut/plan.txt (capture_tutorial.c's chapters)
import json, os, sys
from pathlib import Path
import numpy as np
import soundfile as sf
sys.path.insert(0, str(Path(__file__).resolve().parent))
LANG = sys.argv[4] if len(sys.argv) > 4 else 'es'
FILM = sys.argv[5] if len(sys.argv) > 5 else 'tutorial'   # (news: news_script_en.py, the lab film)
import importlib
M = importlib.import_module('news_script_en' if FILM == 'news' else 'tutorial_script_en' if LANG == 'en' else 'tutorial_script')
CHAPTERS, say, META = M.CHAPTERS, M.say, getattr(M, 'META', {})
from kokoro_onnx import Kokoro

D = Path(sys.argv[1]); D.mkdir(parents=True, exist_ok=True)
VOICE = sys.argv[2] if len(sys.argv) > 2 else ('am_michael' if LANG == 'en' else 'em_alex')
SPEED = float(sys.argv[3]) if len(sys.argv) > 3 else 0.94
KD = Path(os.environ.get('KOKORO', Path.home() / 'tts'))
k = Kokoro(str(KD / 'kokoro.onnx'), str(KD / 'voices.bin'))
SR = 24000
LEAD, LINE_GAP, CH_GAP, TAIL = 4.0, 0.55, 2.2, 9.0       # (the intro's beings form first; the outro breathes after)
out, t, chapters = [np.zeros(int(LEAD * SR), np.float32)], LEAD, []
for ci, c in enumerate(CHAPTERS):
    ch = dict(id=c['id'], k=c['k'], title=c['title'], g=c['g'], start=t, lines=[])
    for text, cue in c['lines']:
        a, sr = k.create(say(text), voice=VOICE, speed=SPEED, lang='en-us' if LANG == 'en' else 'es')
        assert sr == SR
        a = a.astype(np.float32)
        nz = np.nonzero(np.abs(a) > 0.004)[0]             # (the model's own silence at both ends trimmed)
        if len(nz): a = a[max(0, nz[0] - 240): nz[-1] + 1200]
        ch['lines'].append(dict(text=text, cue=cue, start=round(t, 3), end=round(t + len(a) / SR, 3)))
        out.append(a); t += len(a) / SR
        out.append(np.zeros(int(LINE_GAP * SR), np.float32)); t += LINE_GAP
        print(f'{c["id"]:8s} {t:7.2f}  {text[:60]}')
    gap = CH_GAP if ci + 1 < len(CHAPTERS) else TAIL
    out.append(np.zeros(int(gap * SR), np.float32)); t += gap
    ch['end'] = round(t, 3)
    chapters.append(ch)
v = np.concatenate(out)
v = v / (np.abs(v).max() + 1e-9) * 0.89
sf.write(D / 'voice.wav', v, SR)
json.dump(dict(end=round(t, 3), voice=VOICE, lang=LANG, meta=META, chapters=chapters), open(D / 'timeline.json', 'w'), ensure_ascii=False, indent=1)
with open(D / 'plan.txt', 'w') as f:                    # capture_tutorial.c: "seconds chapter cue" a line
    f.write(f'{t:.3f} end 0\n')
    for ch in chapters:
        for ln in ch['lines']:
            f.write(f'{ln["start"]:.3f} {ch["id"]} {ln["cue"]}\n')
print(D / 'voice.wav', f'{t:.1f} s')

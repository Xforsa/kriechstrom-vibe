#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# KRIECHSTROM for VIBE FM: the tests. Needs Zig 0.13.0 (tools/build.sh), a C compiler, Node.js 18+ and Python 3.
#   sh tests/run.sh
set -e
cd "$(dirname "$0")/.."
NODE="${NODE:-node}"
fail=0
ok() { printf '%-64s %s\n' "$1" "$2"; [ "$2" = ok ] || fail=1; }
sh tools/build.sh >/dev/null
sh tools/make_demo.sh >/dev/null
ok "build: build/kriechstrom.wasm" ok
# 1. the core at 44.1 kHz, bit for bit: the firmware against the core built for this computer
cc -O2 -w -D_FORTIFY_SOURCE=0 -Ibuild/gen -Ikriechstrom/gen -Ikriechstrom -o build/core_ref tests/core_ref.c -lm
build/core_ref 10 build/ref.raw
mkdir -p build/t-bx build/t-in build/t-midi build/t-sa build/t-sb build/t-outs
"$NODE" tools/kx-test.mjs build/kriechstrom.wasm tests/bitexact.json build/t-bx 44100 >/dev/null
python3 tests/compare.py build/ref.raw build/t-bx/audio.wav && ok "44.1 kHz: the FM-1's core, bit for bit (10 s of the demo)" ok || ok "44.1 kHz bit for bit" FAIL
# 2. the SDK's own check
"$NODE" tools/vibe-check.mjs build/kriechstrom.wasm > build/vibe-check.txt 2>&1 && ok "vibe-check" ok || { cat build/vibe-check.txt; ok "vibe-check" FAIL; }
# 3. at 48 kHz and 96 kHz: no NaN, sound
for r in 48000 96000; do
    o=$("$NODE" tools/kx-test.mjs build/kriechstrom.wasm tests/bitexact.json build/t-bx $r)
    echo "$o" | python3 -c "import json,sys; d=json.load(sys.stdin); sys.exit(0 if d['nan']==0 and d['rms']>0.05 else 1)" && ok "$r Hz: plays, no NaN" ok || ok "$r Hz" FAIL
done
# 4. the audio input: it is heard, GATE by a stopped track closes it
o=$("$NODE" tools/kx-test.mjs build/kriechstrom.wasm tests/input.json build/t-in)
python3 - build/t-in/audio.wav <<'PY' && ok "input: in the mix, GATE closes it" ok || ok "input" FAIL
import wave, struct, math, sys
w = wave.open(sys.argv[1]); sr = w.getframerate(); d = w.readframes(w.getnframes())
a = struct.unpack('<%dh' % (len(d) // 2), d)[0::2]
rms = lambda s0, s1: math.sqrt(sum(x * x for x in a[int(s0 * sr):int(s1 * sr)]) / ((s1 - s0) * sr)) / 32768
sys.exit(0 if rms(0.5, 1.9) > 0.02 and rms(3.0, 3.9) < rms(0.5, 1.9) / 3 else 1)
PY
# 5. MIDI OUT: four channels, the clock
o=$("$NODE" tools/kx-test.mjs build/kriechstrom.wasm tests/midi.json build/t-midi)
python3 - build/t-midi/midi.json <<'PY' && ok "MIDI OUT: the tracks on channels 1-4, the clock" ok || ok "MIDI OUT" FAIL
import json, sys
m = json.load(open(sys.argv[1]))
ch = {x['bytes'][0] & 15 for x in m if x['bytes'][0] & 0xF0 == 0x90}
clk = sum(1 for x in m if x['bytes'][0] == 0xF8)
sys.exit(0 if ch == {0, 1, 2, 3} and clk > 50 else 1)
PY
# 6. storage: an edit kept, the next start has it
rm -rf build/t-store; mkdir -p build/t-store
"$NODE" tools/kx-test.mjs build/kriechstrom.wasm tests/store_a.json build/t-sa >/dev/null
"$NODE" tools/kx-test.mjs build/kriechstrom.wasm tests/store_b.json build/t-sb >/dev/null
[ -s build/t-store/session.krp ] && ok "storage: the session kept for the next start" ok || ok "storage" FAIL
# 7. MY PROJECTS: two saved, one exported as a file, the music changed and loaded back, the file imported
rm -rf build/t-proj; mkdir -p build/t-proj
"$NODE" tools/kx-test.mjs build/kriechstrom.wasm tests/projects.json build/t-proj >/dev/null
[ -s build/t-proj/store/proj/p01.kxp ] && [ -s build/t-proj/store/proj/p02.kxp ] && [ -s build/t-proj/store/proj/p03.kxp ] &&
  [ -s "build/t-proj/media-sd/VIBE FM/kriechstrom/PROJECT 01.kxp" ] && ok "projects: saved, exported, imported" ok || ok "projects" FAIL
# 8. more outputs: 14 of them, the four tracks and the effects on their own, nothing on the input's
"$NODE" tools/kx-test.mjs build/kriechstrom.wasm tests/outputs.json build/t-outs >/dev/null
python3 - build/t-outs <<'PY' && ok "outputs: T1-T4 and the effects on OUT 3-12" ok || ok "outputs" FAIL
import sys, wave, array, math
d = sys.argv[1]
def rms(f):
    w = wave.open(d + '/' + f); a = array.array('h'); a.frombytes(w.readframes(w.getnframes()))
    return math.sqrt(sum(x * x for x in a) / max(1, len(a)))
r = [rms(f) for f in ('out3-4.wav', 'out5-6.wav', 'out7-8.wav', 'out9-10.wav', 'out11-12.wav')]
sys.exit(0 if all(x > 50 for x in r) and rms('out13-14.wav') < 1 else 1)
PY
# 9. SPINE: its followers' steps move with its chords (T1 on its own output), the drums never; on i, as without it
mkdir -p build/t-spoff build/t-spon
"$NODE" tools/kx-test.mjs build/kriechstrom.wasm tests/spine_off.json build/t-spoff >/dev/null
"$NODE" tools/kx-test.mjs build/kriechstrom.wasm tests/spine_on.json build/t-spon >/dev/null
python3 - <<'PY' && ok "SPINE: the followers move with the chords, the drums stay" ok || ok "SPINE" FAIL
import wave, struct, math, sys
def load(p):
    w = wave.open(p); d = w.readframes(w.getnframes()); return struct.unpack('<%dh' % (len(d) // 2), d)[0::2], w.getframerate()
def diff(o, s0, s1):
    a, sr = load('build/t-spoff/' + o); b, _ = load('build/t-spon/' + o)
    i0, i1 = int(s0 * sr), int(s1 * sr)
    return math.sqrt(sum((a[i] - b[i]) ** 2 for i in range(i0, i1, 4)) / (sum(a[i] ** 2 for i in range(i0, i1, 4)) + 1))
ok = diff('out3-4.wav', 3.4, 4.3) < 0.01 and diff('out3-4.wav', 4.5, 6.7) > 0.5 and diff('out3-4.wav', 12.3, 13.9) > 0.5
ok = ok and max(diff('out9-10.wav', a, a + 2.0) for a in (3.4, 4.5, 6.9, 12.3)) < 0.01
sys.exit(0 if ok else 1)
PY
# 10. the VIBE's own effects: each comes in softly, grows while held, never jumps louder than the mix
mkdir -p build/t-fx build/t-fxd
"$NODE" tools/kx-test.mjs build/kriechstrom.wasm tests/fx_held.json build/t-fx >/dev/null
"$NODE" tools/kx-test.mjs build/kriechstrom.wasm tests/fx_dry.json build/t-fxd >/dev/null
python3 - <<'PY' && ok "own effects: soft in, growing, level kept (8)" ok || ok "own effects" FAIL
import wave, struct, math, sys
def load(p):
    w = wave.open(p); d = w.readframes(w.getnframes()); return struct.unpack('<%dh' % (len(d) // 2), d)[0::2], w.getframerate()
a, sr = load('build/t-fx/audio.wav'); b, _ = load('build/t-fxd/audio.wav')
def rms(x, s0, s1): seg = x[int(s0 * sr):int(s1 * sr)]; return math.sqrt(sum(v * v for v in seg) / max(1, len(seg)))
def dif(s0, s1):
    i0, i1 = int(s0 * sr), int(s1 * sr)
    return math.sqrt(sum((a[i] - b[i]) ** 2 for i in range(i0, i1, 3)) / (sum(b[i] ** 2 for i in range(i0, i1, 3)) + 1))
bad = []
for k in range(8):
    t = 4.4 + k * 7.2
    if not 0.6 < rms(a, t, t + 0.05) / (rms(b, t, t + 0.05) + 1) < 1.5: bad.append((k, 'in'))
    if rms(a, t + 3.6, t + 4.8) > 1.5 * rms(b, t + 3.6, t + 4.8): bad.append((k, 'loud'))
    if k in (0, 1, 2, 3, 6, 7) and not dif(t + 3.6, t + 4.8) > dif(t, t + 1.2) + 0.1: bad.append((k, 'grow'))
print(bad) if bad else None
sys.exit(1 if bad else 0)
PY
# the screens for the site, README and manual, made anew (a change you can see shows in git as changed docs/*.png)
python3 tools/make_shots.py >/dev/null && ok "screens: docs/*.png from this build" ok || ok "screens" FAIL
[ $fail = 0 ] && echo "ALL PASSED" || { echo "FAILED"; exit 1; }

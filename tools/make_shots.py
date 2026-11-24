#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The screens for the site, the README and the manual, from the firmware as it is now: tools/shots/*.txt played by
# tools/kx-test.mjs, the named shots copied to docs/. Run after every change you can see (tools/make_site.sh does).
#   python3 tools/make_shots.py
# A script, one action a line: b KEY (press), bd/bu KEY (down/up), p PAD, e ENC detents [times], t X Y (a click),
# tile N (MENU's tile N, 0-9), kd/ku NOTE (a key down/up), s NAME (a shot of the display), ss NAME (of the stage), w SECONDS, # a remark.
import json, os, shutil, subprocess, sys
B = dict(F1=0, F2=1, F3=2, F4=3, F5=4, F6=5, SHIFT=6, OCTM=7, OCTP=8, ENTER=9, K1=10, K2=11, K3=12, K4=13, K5=14, K6=15,
         RETURN=16, HOME=17, LEFT=18, MENU=19, RIGHT=20, REC=21, STOP=22, PLAY=23, PUSH=24)
E = dict(E1=0, E2=1, E3=2, E4=3, A=4, B=5, DATA=6)
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
os.chdir(root)
node = os.environ.get('NODE', 'node')

def script(lines):
    t, st, names = 1.0, [], []
    def ev(*a):
        st.append({"t": round(t, 3), "ev": list(a)})
    for line in lines:
        a = line.split()
        if not a or a[0].startswith('#'):
            continue
        c = a[0]
        if c == 'b': ev(3, B[a[1]], 1, 0); t += 0.03; ev(3, B[a[1]], 0, 0); t += 0.25
        elif c == 'bd': ev(3, B[a[1]], 1, 0); t += 0.03
        elif c == 'bu': ev(3, B[a[1]], 0, 0); t += 0.25
        elif c == 'p': ev(1, int(a[1]), 1, 0.8); t += 0.15; ev(1, int(a[1]), 0, 0); t += 0.25
        elif c == 'e':
            for _ in range(int(a[3]) if len(a) > 3 else 1):
                ev(4, E[a[1]], 0, float(a[2])); t += 0.03
            t += 0.25
        elif c in ('t', 'tile'):
            if c == 'tile':
                i = int(a[1]); x, y = 4 + 95 * (i % 5) + 44, 96 + 44 * (i // 5) + 19
            else:
                x, y = int(a[1]), int(a[2])
            ev(16, 0, 1, y * 512 + x); t += 0.03; ev(16, 2, 0, y * 512 + x); t += 0.3
        elif c in ('s', 'ss'):
            st.append({"t": round(t, 3), ("shot" if c == 's' else "stage_shot"): a[1]}); names.append(a[1]); t += 0.1
        elif c == 'kd': ev(9, int(a[1]), 1, 0.8); t += 0.05
        elif c == 'ku': ev(9, int(a[1]), 0, 0); t += 0.05
        elif c == 'w': t += float(a[1])
    return {"seconds": t + 0.5, "steps": st}, names

shutil.rmtree('build/shots', ignore_errors=True)
for f in sorted(os.listdir('tools/shots')):
    if not f.endswith('.txt'):
        continue
    sc, names = script(open(os.path.join('tools/shots', f)).read().splitlines())
    out = os.path.join('build/shots', f[:-4])
    os.makedirs(out)
    json.dump(sc, open(out + '.json', 'w'))
    subprocess.run([node, 'tools/kx-test.mjs', 'build/kriechstrom.wasm', out + '.json', out], check=True, stdout=subprocess.DEVNULL)
    for n in names:
        shutil.copy(os.path.join(out, n + '.png'), os.path.join('docs', n + '.png'))
    print(f, len(names), 'shots')

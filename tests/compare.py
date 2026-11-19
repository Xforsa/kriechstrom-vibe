#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The firmware at 44.1 kHz (a float WAV from tools/kx-test.mjs) against the core's reference (tests/core_ref.c, Q15
# int32): every sample the same. Usage: compare.py REF.raw AUDIO.wav
import struct, sys
ref = open(sys.argv[1], 'rb').read()
r = struct.unpack('<%di' % (len(ref) // 4), ref)
w = open(sys.argv[2], 'rb').read()
i, d = 12, b''
while i < len(w):
    cid, ln = w[i:i + 4], struct.unpack('<I', w[i + 4:i + 8])[0]
    if cid == b'data':
        d = w[i + 8:i + 8 + ln]
        break
    i += 8 + ln
f = struct.unpack('<%df' % (len(d) // 4), d)
n = min(len(f), len(r))
bad = [k for k in range(n) if abs(f[k]) < 1.0 and int(round(f[k] * 32768)) != r[k]]
loud = sum(1 for x in r[:n] if x)
print('bit-exact: %d samples, %d differ, %d not silent' % (n, len(bad), loud))
sys.exit(1 if bad or n < 44100 or loud < n // 2 else 0)

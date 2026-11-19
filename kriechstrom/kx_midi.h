/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: MIDI OUT and CV, which the FM-1 does not have for its sequencer.
 *   MIDI OUT  each track's notes as they play (the sequencer, the arpeggiator, the keys), track 1 on CHANNEL, track 2
 *             on the next one ..; DICE's parts play another synth. The core's own MIDI OUT (the keys) goes too
 *   CLOCK     24 per quarter at the tempo, START and STOP with PLAY (the SDK makes the clock; a DAW that leads silences it)
 *   CV TRACK  that track's note on CV OUT (1 V per octave, C2 = 0 V) and its GATE on GATE OUT
 * All from the audio flow, the one that writes the SDK's MIDI queue. */

static uint8_t kx_mo_on[NTRK][16];                /* notes this side sent on, per track (a bitset would do; 16 kept) */
static uint8_t kx_mo_n[NTRK];
static int kx_mo_play;
static float kx_mo_bpm;

static int kx_mo_has(uint32_t k, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < kx_mo_n[k]; i++)
        if (kx_mo_on[k][i] == note)
            return (int)i;
    return -1;
}

/* the notes a track sounds now: its voices that are held (gate), for every engine (drums: their hits) */
static uint32_t kx_trk_notes(const track_t *t, uint8_t *out)
{
    uint32_t i, n = 0, j;
    for (i = 0; i < NVOICE; i++) {
        const voice_t *v = &t->v[i];
        if (!v->active || !v->gate || v->stage == 4u)
            continue;
        for (j = 0; j < n && out[j] != v->note; j++)
            ;
        if (j == n && n < 16u)
            out[n++] = v->note;
    }
    return n;
}

static void kx_midi_block(void)                   /* after each core block */
{
    uint32_t k, i;
    int on = kx_ic[IC_MOUT] != 0;
    for (k = 0; k < NTRK; k++) {
        uint8_t now[16];
        uint32_t n = on && !trk[k].p[P_MUTE] ? kx_trk_notes(&trk[k], now) : 0u, ch = (uint32_t)(kx_ic[IC_MCH] - 1 + (int)k) & 15u;
        for (i = 0; i < kx_mo_n[k];) {            /* off: what no longer sounds */
            uint32_t j;
            for (j = 0; j < n && now[j] != kx_mo_on[k][i]; j++)
                ;
            if (j == n) {
                vibe_midi_send((uint8_t)(0x80u | ch), kx_mo_on[k][i], 0);
                kx_mo_on[k][i] = kx_mo_on[k][--kx_mo_n[k]];
            } else
                i++;
        }
        for (i = 0; i < n; i++)                   /* on: what started */
            if (kx_mo_has(k, now[i]) < 0 && kx_mo_n[k] < 16u) {
                uint32_t vel = 100u, j;
                for (j = 0; j < NVOICE; j++)
                    if (trk[k].v[j].active && trk[k].v[j].note == now[i])
                        vel = trk[k].v[j].vel ? trk[k].v[j].vel : 100u;
                vibe_midi_send((uint8_t)(0x90u | ch), now[i], (uint8_t)(vel > 127u ? 127u : vel));
                kx_mo_on[k][kx_mo_n[k]++] = now[i];
            }
    }
    /* CV / GATE: the CV TRACK's last note started, held while it sounds */
    if (kx_ic[IC_CV] > 0 && kx_ic[IC_CV] <= NTRK) {
        uint32_t t = (uint32_t)kx_ic[IC_CV] - 1u;
        if (kx_mo_n[t] || !on) {
            uint8_t now[16];
            uint32_t n = on ? kx_mo_n[t] : kx_trk_notes(&trk[t], now);
            uint32_t note = on ? kx_mo_on[t][kx_mo_n[t] - 1u] : n ? now[n - 1u] : 0u;
            if (n) {
                float v = ((float)note - 36.0f) / 12.0f;
                vibe_cv_out(v < 0.0f ? 0.0f : v > 5.0f ? 5.0f : v);
            }
            vibe_gate_out(n != 0);
        } else
            vibe_gate_out(0);
    }
}

static void kx_midi_audio(void)                   /* vibe_on_audio, once a block: the clock and the transport */
{
    float bpm = (float)song.g[G_BPM];
    vibe_clock_out(kx_ic[IC_CLK] != 0);
    if (bpm != kx_mo_bpm) {
        kx_mo_bpm = bpm;
        vibe_tempo_set(bpm);
    }
    if (song.playing != kx_mo_play) {
        kx_mo_play = song.playing;
        if (kx_ic[IC_CLK])
            vibe_transport_out(kx_mo_play);
    }
}

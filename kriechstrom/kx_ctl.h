/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: what the commands do (in the audio flow, between two core blocks), the power-on, the notes
 * of the VIBE's keys and pads, and the core's MIDI OUT to the host. */

enum {
    KXC_NOTE = 1,      /* a = note, b = velocity 1..127 (0: off), c = source (0 keys / pads: the selected track, 1 MIDI) */
    KXC_MIDI,          /* a b c: a raw MIDI message from outside (channel messages; the core routes it as MIDI IN) */
    KXC_PLAY,          /* a: 1 start, 2 stop, 3 toggle */
    KXC_SEL,           /* a: the track to select */
    KXC_PARAM,         /* a: track (4 = global), b c: the parameter id (b | c << 8), v: the new value */
    KXC_PANIC,
};

/* the VIBE's notes (keys, pads as notes) on the selected track, as the FM-1's keys play (seq.c key_on / key_off): the
 * chord keys, the arpeggiator, live recording and MIDI OUT all as there. kx_kb: the notes each note started */
static uint8_t kx_kb[128][8], kx_kbn[128], kx_kbt[128];
static void kx_key_on(uint32_t note, uint32_t vel)
{
    track_t *t = TSEL;
    uint32_t i, n, mc = trk_midi_ch(trk_index(t));
    if (kx_kbn[note])
        return;
    n = chord_build(t, note, kx_kb[note]);
    kx_kbt[note] = song.sel;
    for (i = 0; i < n; i++) {
        uint32_t x = kx_kb[note][i];
        if (midi_local_held(t, x))
            continue;
        input_on(t, x, vel);
        midi_out_event(0x09u | (0x90u | mc) << 8 | x << 16 | vel << 24);
    }
    kx_kbn[note] = (uint8_t)n;
    last_note = (uint8_t)note;
}
static void kx_key_off(uint32_t note)
{
    track_t *t = &trk[kx_kbt[note] % NTRK];
    uint32_t n = kx_kbn[note], i, mc = trk_midi_ch(trk_index(t));
    kx_kbn[note] = 0;
    for (i = 0; i < n; i++) {
        uint32_t x = kx_kb[note][i];
        if (midi_local_held(t, x))
            continue;
        input_off(t, x);
        midi_out_event(0x08u | (0x80u | mc) << 8 | x << 16);
    }
}

static void kx_apply(const kx_cmd *k)
{
    switch (k->op) {
    case KXC_NOTE:
        if (k->c == 1u)
            midi_enqueue((k->b ? 0x09u | (0x90u | (k->v & 15u)) << 8 : 0x08u | (0x80u | (k->v & 15u)) << 8) |
                         (uint32_t)k->a << 16 | (uint32_t)k->b << 24, 1u);
        else if (k->b)
            kx_key_on(k->a & 127u, k->b);
        else
            kx_key_off(k->a & 127u);
        break;
    case KXC_MIDI:
        midi_enqueue((uint32_t)(k->a >> 4) | (uint32_t)k->a << 8 | (uint32_t)k->b << 16 | (uint32_t)k->c << 24, 1u);
        break;
    case KXC_PLAY:
        if (k->a == 3u)
            transport_req = song.playing ? 2u : 1u;
        else
            transport_req = k->a;
        break;
    case KXC_SEL:
        if (k->a < NTRK)
            song.sel = k->a;
        break;
    case KXC_PARAM: {
        uint32_t id = k->b | (uint32_t)k->c << 8;
        if (k->a < NTRK && id < P_COUNT)
            trk[k->a].p[id] = (int16_t)k->v;
        else if (k->a == NTRK && id < G_ALL)
            song.g[id] = (int16_t)k->v;
        break;
    }
    case KXC_PANIC:
        panic_req = (1u << NTRK) - 1u;
        break;
    }
}

/* the core's MIDI OUT (bsp/chip/usb.c midi_out_q: the sequencer's and the keys' notes, its clock) to the host, from
 * the audio flow only (the SDK's MIDI queue has one writer) */
static void kx_midi_out(int frame)
{
    while (mo_r != mo_w) {
        uint32_t pkt = midi_out_q[mo_r % MQ];
        uint32_t st = (pkt >> 8) & 0xFFu;
        mo_r++;
        if (st >= 0x80u)
            vibe_midi_send((uint8_t)st, (uint8_t)((pkt >> 16) & 0x7Fu), (uint8_t)((pkt >> 24) & 0x7Fu));
    }
    (void)frame;
}

/* power-on, as the FM-1's (main.c felucca_init): the four tracks with KRIECHSTROM's sounds and patterns, which play
 * when PLAY is pressed */
static void kx_power_on(void)
{
    uint32_t i;
    chain_defaults(&chain_config);
    for (i = 0; i < G_ALL; i++)
        song.g[i] = GP[i].def;
    undo_depth++;
    fm6_init();
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        track_defaults(t);
        set_engine_of(t, TRK_DEF[i][0]);
        apply_preset_to(t, TRK_DEF[i][1]);
        t->engine = t->eng_req;
        eng_mem_claim(t);
        track_defaults_steps(t);
        if (TRK_DEF[i][2])
            load_pat16(t, PATTERNS[TRK_DEF[i][2] - 1u].note, PATTERNS[TRK_DEF[i][2] - 1u].flags);
        pat_sig[i] = steps_sig(t);
        pat_last[i] = TRK_DEF[i][2];
    }
    undo_depth--;
    song.sel = 0;
    song.master_q12 = 4096;
    panel = PANEL_DEFAULT;
    palette_set(KRST_PALETTE);
    ui.home = 1;
    ui.force = 1;
    usb.config = 1;                               /* (the core's MIDI OUT queue runs: kx_midi_out takes it) */
}

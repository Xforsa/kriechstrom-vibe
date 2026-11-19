/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov */
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

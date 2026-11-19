/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: the display and the controls (first cut). */

static void kx_ui_init(void) {}

static void kx_ui_event(const vibe_event *e)
{
    if (e->type == VIBE_EV_NOTE) {
        uint32_t vel = (uint32_t)(e->value * 127.0f + 0.5f);
        int midi = e->midi[1] == VIBE_NOTE_MIDI || e->midi[1] == VIBE_NOTE_HOST;
        if (e->down && !vel)
            vel = 1;
        kx_post(KXC_NOTE, e->id, e->down ? vel : 0u, midi ? 1u : 0u, e->midi[0]);
        return;
    }
    if (e->type == VIBE_EV_BUTTON && e->down) {
        if (e->id == VB_PLAY)
            kx_post(KXC_PLAY, 3, 0, 0, 0);
        else if (e->id == VB_STOP)
            kx_post(KXC_PLAY, 2, 0, 0, 0);
        else if (e->id >= VB_F1 && e->id <= VB_F4)
            kx_post(KXC_SEL, e->id - VB_F1, 0, 0, 0);
    }
}

static void kx_ui_draw(void)
{
    uint32_t i, w = kx_scope_w;
    char b[24];
    vg_clear(0x0A0A0B);
    i = (uint32_t)vg_text("KRIECH", 16, 12, VG_BIG, 0xF2F2F2, VG_LEFT);
    vg_text("STROM", 16 + (int)i, 12, VG_BIG, 0xE01818, VG_LEFT);
    for (i = 0; i < 479u; i++) {
        int32_t a = kx_scope[(w - 960u + i * 2u) & (KX_SCOPE - 1u)], c = kx_scope[(w - 958u + i * 2u) & (KX_SCOPE - 1u)];
        vg_line((float)i, 160.0f - (float)a * (90.0f / 32768.0f), (float)i + 1.0f, 160.0f - (float)c * (90.0f / 32768.0f),
                1.5f, 0xE01818);
    }
    for (i = 0; i < NTRK; i++) {
        uint32_t x = 8u + i * 118u;
        vg_round((int)x, 286, 110, 28, 4, i == song.sel ? 0x3A0A0A : 0x1A1A1C);
        vg_text(ENGINES[eng_idx(trk[i].engine)]->name, (int)x + 55, 292, VG_BOLD, 0xD0D0D0, VG_CENTER);
    }
    vg_text(song.playing ? "PLAY" : "STOP", 464, 16, VG_BOLD, song.playing ? 0xE01818 : 0x808080, VG_RIGHT);
    vg_text(vg_itoa(song.g[G_BPM], b), 400, 16, VG_BOLD, 0x808080, VG_RIGHT);
}

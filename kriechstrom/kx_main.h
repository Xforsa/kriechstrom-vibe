/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: the four functions the host calls. */

VIBE_PARALLEL_AUDIO

void vibe_on_init(float sample_rate)
{
    kx_power_on();
    kx_store_boot();
    kx_audio_start(sample_rate);
    vibe_notes(VIBE_NOTES);
    kx_daw_init();
    kx_ui_init();
}

void vibe_on_event(const vibe_event *e)
{
    if (e->type == VIBE_EV_SAVE) {
        kx_store_save_now();
        return;
    }
    if (e->type == VIBE_EV_PARAM) {
        if (e->id < DW_N)
            kx_post(KXC_DAW, e->id & 0xFFu, (uint32_t)e->id >> 8, 0, kx_fbits(e->value));
        return;
    }
    kx_ui_event(e);
    kx_store_poll(0);
}

void vibe_on_draw(uint16_t *fb)
{
    vg_begin(fb);
    kx_ui_draw();
    kx_store_poll(0);
}

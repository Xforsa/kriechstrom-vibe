/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: the four functions the host calls. */

VIBE_PARALLEL_AUDIO

void vibe_on_init(float sample_rate)
{
    kx_power_on();
    kx_audio_start(sample_rate);
    vibe_notes(VIBE_NOTES);
    kx_ui_init();
}

void vibe_on_event(const vibe_event *e)
{
    kx_ui_event(e);
}

void vibe_on_draw(uint16_t *fb)
{
    vg_begin(fb);
    kx_ui_draw();
}

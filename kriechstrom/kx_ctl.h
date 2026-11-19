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
    KXC_EDIT,         /* a = PAGES index, b = its knob 0..3, v = detents: the page's knob, as the FM-1's (edit_param) */
    KXC_PRESET,       /* v = detents: the selected track's sound, one list over every engine (preset_step) */
    KXC_ENGINE,       /* v = detents: the selected track's engine (select_engine) */
    KXC_TOOL,         /* a = track, b = TL_*: SEQ TOOLS (ui_tools.c tl_do: DICE, CLEAR, REVERSE ..), one undo */
    KXC_OOPS,         /* a = track: HAPPY ACCIDENT (krst_dice.c accident_of) */
    KXC_UNDO,         /* the last edit back (ui.c undo_swap: SAVE held on the FM-1) */
    KXC_MUTE,         /* a = track, b: 0 toggle, 1 mute, 2 unmute */
    KXC_PERF,         /* a = PF_* effect, b = 1 held / 0 let go (perform.c: the FX layer's effects) */
    KXC_STEP,         /* a = track, b = step, c = lane (DRUM) or 255 (a note step: on with the last note / off) */
    KXC_CURSOR,       /* v = the SEQ cursor's step */
    KXC_LANE,         /* a = the DRUM lane the SEQ page edits */
    KXC_REC,          /* the selected track armed for live recording, or not */
    KXC_HIT,          /* a = track, b = note, v = velocity 1..127 (0: off): a drum pad on any track */
    KXC_FOLLOW,       /* a = 1: the SEQ cursor follows the playhead (follow_poll) */
    KXC_DAW,          /* a b: the DAW's parameter (a | b << 8), v: its value (a float's bits) */
    KXC_SLOT,         /* a = slot 0..3, b: 0 load, 1 save, 2 clear the music (project_new) */
    KXC_VIEW,         /* a = the PAGES index on the display: the core's page follows (its cards and knobs depend on it) */
    KXC_ACT,          /* a = PAGES index: its picked action (ui_input.c act_do: AUTOMATION's PLAY, CLEAR, ADD ..) */
    KXC_SONG,         /* SONG: play the chain from its first section, or stop */
    KXC_USER,         /* a = user sound slot, b: 0 load, 1 save (named by itself), 2 erase */
    KXC_DEMO,         /* the demo song into the empty slots, and as the music now */
    KXC_INCFG,        /* a = IC_*, v = detents (b = 1: v is the value): the audio input's settings (kx_input.h) */
};
static int kx_silent_engine(uint32_t e) { return e == ENGI_SAMPLE || (FELUCCA_SLICE && e == 13u); }
static inline int32_t kx_fbits(float f) { union { float f; int32_t i; } u; u.f = f; return u.i; }
static inline float kx_bitsf(int32_t i) { union { float f; int32_t i; } u; u.i = i; return u.f; }
static void kx_daw_apply(uint32_t j, float v);    /* kx_daw.h */
static void kx_slot_saved(uint32_t s);            /* kx_store.h */
static void kx_slot_info(void);
static void kx_user_saved(void);
static int kx_demo_load(int boot);
static void kx_tool(track_t *t, uint32_t p)
{
    const char *m;
    if (chain_busy()) {
        ui_message("STOP TO EDIT");
        return;
    }
    m = tl_do(t, p, ui.lane);
    if (m)
        ui_message(m);
}

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
        track_select(k->a);
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
    case KXC_EDIT:
        if (k->a < NPAGES) {
            ui.home = 0;
            ui.menu = 0;
            ui.confirm = 0;
            if (ui.page != k->a) {
                ui.page = k->a;
                ui.act = 0;
            }
            edit_param(k->b & 3u, k->v);
        }
        break;
    case KXC_PRESET: {                            /* (SAMPLE and SLICE play recorded sound, which is not in this
                                                   * firmware: their sounds are skipped, as silent) */
        uint32_t total, n;
        preset_pos(&total);
        for (n = 0; n < total + 1u; n++) {
            preset_step(k->v > 0 ? 1 : -1);
            if (!kx_silent_engine(TSEL->eng_req))
                break;
        }
        break;
    }
    case KXC_ENGINE: {
        uint32_t e = TSEL->eng_req, n;
        for (n = 0; n < NENGINES; n++) {
            e = eng_step(e, k->v);
            if (!kx_silent_engine(e))
                break;
        }
        select_engine(e);
        break;
    }
    case KXC_TOOL:
        if (k->b == TL_MOOD)
            ui_message(mood_next());
        else
            kx_tool(&trk[k->a % NTRK], k->b);
        break;
    case KXC_OOPS:
        if (chain_busy())
            ui_message("STOP TO EDIT");
        else
            ui_message(accident_of(&trk[k->a % NTRK]));
        break;
    case KXC_UNDO:
        undo_swap();
        break;
    case KXC_MUTE: {
        track_t *t = &trk[k->a % NTRK];
        t->p[P_MUTE] = (int16_t)(k->b == 1u ? 1 : k->b == 2u ? 0 : !t->p[P_MUTE]);
        break;
    }
    case KXC_PERF:
        if (k->a < PF_NFX)
            perf_press(k->a, k->b);
        break;
    case KXC_STEP: {
        track_t *t = &trk[k->a % NTRK];
        step_t *st = &t->step[k->b % NSTEP];
        if (chain_busy()) {
            ui_message("STOP TO EDIT");
            break;
        }
        step_undo_take(t, 0x5000u | k->b);
        if (k->c < NLANE)
            grid_hit(t, k->b, k->c, 2);
        else if (step_on(st))
            step_clear(st);
        else {
            uint8_t ch[CHORD_MAX];
            int32_t r;
            uint16_t mask;
            uint32_t n = chord_make(t, last_note ? last_note : 48u, ch, &r, &mask), i;
            step_clear(st);
            st->time = ST_NOTE;
            for (i = 0; i < n && i < 4u && (i == 0u || !(t->p[P_VOICE] && !ENGINES[t->engine]->oneshot)); i++)
                st->note[st->n++] = ch[i];
        }
        motion_undo_done(t);
        break;
    }
    case KXC_CURSOR:
        cursor_set(k->v);
        break;
    case KXC_LANE:
        ui.lane = (uint8_t)(k->a % NLANE);
        break;
    case KXC_REC:
        song.rec ^= (uint8_t)(1u << song.sel);
        break;
    case KXC_HIT: {
        track_t *t = &trk[k->a % NTRK];
        if (k->v)
            input_on(t, k->b & 127u, (uint32_t)k->v & 127u);
        else
            input_off(t, k->b & 127u);
        break;
    }
    case KXC_DAW:
        kx_daw_apply(k->a | (uint32_t)k->b << 8, kx_bitsf(k->v));
        break;
    case KXC_SLOT:
        if (k->b == 1u) {
            if (!project_save_as(k->a & 3u, 0))
                kx_slot_saved(k->a & 3u);
        } else if (k->b == 0u)
            project_load(k->a & 3u);
        else
            project_new();
        kx_slot_info();
        break;
    case KXC_VIEW:
        if (k->a < NPAGES && ui.page != k->a) {
            ui.home = ui.menu = ui.confirm = 0;
            ui.page = k->a;
            ui.act = 0;
        }
        break;
    case KXC_ACT:
        if (k->a < NPAGES) {
            ui.home = ui.menu = ui.confirm = 0;
            ui.page = k->a;
            act_do();
            ui.confirm = 0;                       /* (no FM-1 dialog here: what needs one is not offered) */
        }
        break;
    case KXC_SONG:
        if (song.playing || chain_busy() || seq_counting())
            transport_req = 2;
        else if (chain_config.count)
            chain_play_ui();
        else
            ui_message("NO SECTIONS: TURN KNOB 3");
        break;
    case KXC_USER: {
        uint32_t s = k->a % UP_SLOTS;
        if (k->b == 0u) {
            if (up_used(s))
                up_load(s);
            else
                ui_message("EMPTY SLOT");
        } else if (k->b == 1u) {
            char nm[16];
            int rc;
            up_auto_name(nm, TSEL->eng_req, s);
            rc = up_store(s, nm);                 /* (no flash here: 3, kept in RAM, which kx_store.h writes out) */
            ui_message(rc == 0 || rc == 3 ? "SOUND SAVED" : "SAVE ERROR");
            if (rc == 0 || rc == 3)
                kx_user_saved();
        } else if (up_used(s)) {
            int rc = up_put(s, 0);
            ui_message(rc == 0 || rc == 3 ? "ERASED" : "ERASE ERROR");
            kx_user_saved();
        }
        break;
    }
    case KXC_DEMO:
        if (chain_busy() || song.playing)
            transport_req = 2;
        kx_demo_load(0);
        ui_message("DEMO SONG: SEQ > SONG, F5");
        break;
    case KXC_INCFG:
        if (k->a < IC_N)
            kx_ic[k->a] = (int16_t)clamp(k->b ? k->v : kx_ic[k->a] + k->v, IC_DESC[k->a].min, IC_DESC[k->a].max);
        break;
    case KXC_FOLLOW:
        if (k->a)
            follow.hold = 0;
        else
            follow_hold();
        break;
    }
}

/* the core's own MIDI OUT queue (bsp/chip/usb.c midi_out_q: the keys' notes, their pressure): emptied, as the FM-1's USB
 * would. What goes out to the host is kx_midi.h's: every track's notes as they sound, the keys' among them */
static void kx_midi_out(int frame)
{
    mo_r = mo_w;
    (void)frame;
}


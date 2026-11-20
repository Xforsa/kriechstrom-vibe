/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: storage. Files in the firmware's own space (vibe_store_*: in a DAW project per instance,
 * the browser's storage, the VIBE's card), in KRIECHSTROM's own formats, so an FM-1's project loads as it is:
 *   session.krp      the music as it is now (written a moment after a change, and when the DAW saves)
 *   slot-a .. -d.krp the four PROJECT slots (SONG plays them in a chain), written when saved
 *   user.krs         the 32 user sounds (both banks)
 * The core's state is the audio flow's: it packs a copy (kx_snap) when something changed; the control flow writes the
 * copy. Files are written from vibe_on_event and vibe_on_draw only, never from the audio. */

#define KX_SNAP_SZ ((int)sizeof(project_store_t))
static project_store_t kx_snap;                   /* the newest copy */
static uint32_t kx_snap_seq;                      /* a seqlock: odd while the audio flow writes kx_snap */
static uint32_t kx_snap_sig, kx_snap_poll;
static uint32_t kx_snap_req, kx_snap_done;        /* a copy asked for now (VIBE_EV_SAVE): the control flow's, the audio's */
static project_store_t kx_slot_out[4];
static uint32_t kx_slot_seq[4];                   /* +1 when a slot was saved (the audio flow copied it into kx_slot_out) */
static up_bank_t kx_up_out[UP_SLOTS / UP_PER_BANK];
static uint32_t kx_up_seq;

/* the audio flow, every block: a new copy when the music changed (checked every ~0.25 s), or at once when asked */
static void kx_snap_poll_audio(void)
{
    uint32_t sig, req = __atomic_load_n(&kx_snap_req, __ATOMIC_ACQUIRE);
    if (req == kx_snap_done && kx_blocks - kx_snap_poll < 344u)
        return;
    kx_snap_poll = kx_blocks;
    sig = autosave_sig();
    if (sig != kx_snap_sig || req != kx_snap_done) {
        project_capture(&proj_scratch);
        __atomic_store_n(&kx_snap_seq, kx_snap_seq + 1u, __ATOMIC_RELEASE);
        if (!proj_pack(&kx_snap, &proj_scratch))
            sig = kx_snap_sig;
        __atomic_store_n(&kx_snap_seq, kx_snap_seq + 1u, __ATOMIC_RELEASE);
        kx_snap_sig = sig;
    }
    __atomic_store_n(&kx_snap_done, req, __ATOMIC_RELEASE);
}

static project_store_t kx_rd;                     /* (a file read in, the demo's copy) */
/* the PROJECT page's slots, as the audio flow last saw them (names and whether used) */
static uint8_t kx_slot_used[4];
static char kx_slot_name[4][16];
static void kx_slot_info(void)                    /* audio flow (or init): project_info uses proj_scratch */
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        char b[PROJ_NAME_LEN + 1u];
        int u = project_name(i, b);
        if (u && !b[0])
            str_cpy(b, "NO NAME", sizeof b);
        str_cpy(kx_slot_name[i], b, sizeof kx_slot_name[i]);
        kx_slot_used[i] = (uint8_t)(u != 0);
    }
}

/* the audio flow, after a command that saved a slot or a user sound */
static void kx_slot_saved(uint32_t s)
{
    memcpy(&kx_slot_out[s & 3u], &proj_slot[s & 3u], sizeof(project_store_t));
    __atomic_store_n(&kx_slot_seq[s & 3u], kx_slot_seq[s & 3u] + 1u, __ATOMIC_RELEASE);
}
static void kx_user_saved(void)
{
    memcpy(kx_up_out, up_bank, sizeof kx_up_out);
    __atomic_store_n(&kx_up_seq, kx_up_seq + 1u, __ATOMIC_RELEASE);
}

/* ---- the control flow ---------------------------------------------------------------------------------------- */
static uint32_t kx_snap_wrote, kx_slot_wrote[4], kx_up_wrote;
static uint64_t kx_snap_seen_t;
static uint32_t kx_snap_seen;
static const char *const KX_SLOT_FILE[4] = {"slot-a.krp", "slot-b.krp", "slot-c.krp", "slot-d.krp"};
static int kx_store_ok = 1;                       /* 0: the host keeps no files (a write failed) */

static void kx_store_write(const char *name, const void *p, int n)
{
    if (vibe_store_write(name, p, n) != 0)
        kx_store_ok = 0;
}

/* now = 1: everything that changed, at once (VIBE_EV_SAVE); else the session a second after its last change */
static void kx_cfg_poll(void);
static void kx_store_poll(int now)
{
    kx_cfg_poll();
    kx_ped_poll(now);
    uint32_t s = __atomic_load_n(&kx_snap_seq, __ATOMIC_ACQUIRE), i;
    uint64_t ms = vibe_frames() * 1000u / (uint64_t)(kx_sr > 0 ? kx_sr : 48000.0f);
    if (s != kx_snap_seen) {
        kx_snap_seen = s;
        kx_snap_seen_t = ms;
    }
    if (!(s & 1u) && s != kx_snap_wrote && (now || ms - kx_snap_seen_t > 1000u)) {
        static project_store_t b;
        memcpy(&b, &kx_snap, sizeof b);
        if (__atomic_load_n(&kx_snap_seq, __ATOMIC_ACQUIRE) == s) {   /* (not torn by a copy made meanwhile) */
            kx_store_write("session.krp", &b, KX_SNAP_SZ);
            kx_snap_wrote = s;
        }
    }
    for (i = 0; i < 4u; i++) {
        uint32_t q = __atomic_load_n(&kx_slot_seq[i], __ATOMIC_ACQUIRE);
        if (q != kx_slot_wrote[i]) {
            kx_store_write(KX_SLOT_FILE[i], &kx_slot_out[i], KX_SNAP_SZ);
            kx_slot_wrote[i] = q;
        }
    }
    i = __atomic_load_n(&kx_up_seq, __ATOMIC_ACQUIRE);
    if (i != kx_up_wrote) {
        kx_store_write("user.krs", kx_up_out, (int)sizeof kx_up_out);
        kx_up_wrote = i;
    }
}

/* VIBE_EV_SAVE: the host keeps the state now. The audio may not run until it is done (a DAW saving): the newest copy
 * the audio flow made is written (it is at most ~0.25 s old), and a fresh one is asked for the next autosave */
static void kx_store_save_now(void)
{
    __atomic_store_n(&kx_snap_req, kx_snap_req + 1u, __ATOMIC_RELEASE);
    kx_store_poll(1);
}

/* the demo song (gen/kx_demo.h, tools/make_demo.c): its four slots into the empty ones, its music now. At the first
 * start (no session yet), and from PROJECT (SHIFT + ENTER); a slot the player saved stays theirs */
#include "kx_demo.h"
static int kx_demo_load(int boot)
{
    static const uint8_t *const D[4] = {KX_DEMO_A, KX_DEMO_B, KX_DEMO_C, KX_DEMO_D};
    uint32_t i;
    for (i = 0; i < 4u; i++)
        if (!project_used(i)) {
            memcpy(&proj_slot[i], D[i], sizeof(project_store_t));
            if (boot)
                memcpy(&kx_slot_out[i], D[i], sizeof(project_store_t));
            else
                kx_slot_saved(i);
        }
    memcpy(&kx_rd, KX_DEMO_NOW, sizeof kx_rd);
    if (!proj_import(&proj_scratch, &kx_rd, (int)sizeof kx_rd))
        return 0;
    proj_bound(&proj_scratch);
    project_restore_runtime(&proj_scratch);
    transport_req = 0;
    ui.msg_t = 0;
    kx_slot_info();
    return 1;
}

/* vibe_on_init (alone, before any audio): the files back into the core */
static int kx_had_session;                        /* the start found the last session (the start screen offers it) */
static void kx_store_boot(void)
{
    int n, i;
    for (i = 0; i < 4; i++) {
        n = vibe_store_read(KX_SLOT_FILE[i], &kx_rd, KX_SNAP_SZ);
        if (n > 0 && proj_import(&proj_scratch, &kx_rd, n)) {
            memcpy(&proj_slot[i], &kx_rd, sizeof kx_rd);
            memcpy(&kx_slot_out[i], &kx_rd, sizeof kx_rd);
        }
    }
    n = vibe_store_read("user.krs", kx_up_out, (int)sizeof kx_up_out);
    if (n == (int)sizeof kx_up_out) {
        memcpy(up_bank, kx_up_out, sizeof up_bank);
        for (i = 0; i < (int)(UP_SLOTS / UP_PER_BANK); i++)
            up_bank_check((uint32_t)i, (int)sizeof up_bank[i]);
    }
    n = vibe_store_read("session.krp", &kx_rd, KX_SNAP_SZ);
    if (n > 0 && proj_import(&proj_scratch, &kx_rd, n)) {
        proj_bound(&proj_scratch);
        project_restore_runtime(&proj_scratch);
        transport_req = 0;
        ui.msg_t = 0;                             /* (no "LOADED" at every start) */
        kx_had_session = 1;
    } else
        kx_demo_load(1);                          /* the first start: the demo song */
    kx_slot_info();
    kx_snap_sig = autosave_sig();                 /* (what is there now is what is stored) */
    proj_kept_now();
}

/* vibe.cfg: the VIBE's own settings (the audio input, MIDI OUT, CV), not the FM-1 project's: "KXC1", their count, the
 * values (int16, little-endian) in IC_* order. A newer firmware adds settings at the end: an older file keeps the rest */
static int16_t kx_ic_wrote[IC_N];
static int kx_ic_loaded;
static void kx_cfg_boot(void)                     /* vibe_on_init, after kx_input_start */
{
    uint8_t b[8 + 2 * 64];
    int n = vibe_store_read("vibe.cfg", b, (int)sizeof b), i, c;
    if (n >= 8 && b[0] == 'K' && b[1] == 'X' && b[2] == 'C' && b[3] == '1') {
        c = b[4] | b[5] << 8;
        for (i = 0; i < c && i < IC_N && 8 + 2 * i + 1 < n; i++)
            kx_ic[i] = (int16_t)clamp((int16_t)(b[8 + 2 * i] | b[9 + 2 * i] << 8), IC_DESC[i].min, IC_DESC[i].max);
    }
    memcpy(kx_ic_wrote, kx_ic, sizeof kx_ic_wrote);
    kx_ic_loaded = 1;
    kx_xo_cfg = kx_ic[IC_XOUT];
}
static void kx_cfg_poll(void)                     /* control flow */
{
    uint8_t b[8 + 2 * IC_N];
    int i;
    if (!kx_ic_loaded || !memcmp(kx_ic_wrote, kx_ic, sizeof kx_ic_wrote))
        return;
    memcpy(kx_ic_wrote, kx_ic, sizeof kx_ic_wrote);
    memset(b, 0, sizeof b);
    b[0] = 'K', b[1] = 'X', b[2] = 'C', b[3] = '1', b[4] = IC_N;
    for (i = 0; i < IC_N; i++) {
        b[8 + 2 * i] = (uint8_t)(kx_ic_wrote[i] & 0xFF);
        b[9 + 2 * i] = (uint8_t)((uint16_t)kx_ic_wrote[i] >> 8);
    }
    kx_store_write("vibe.cfg", b, (int)sizeof b);
}

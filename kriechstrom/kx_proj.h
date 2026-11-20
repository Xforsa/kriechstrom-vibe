/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: your projects. A project is everything the music is now, in one file (.kxp):
 *   the music as it plays (KRIECHSTROM's project, the FM-1's format), the four PROJECT slots A-D that SONG chains,
 *   the pedals of every board, and the sampler's slots USR1-3 that hold a recording.
 * MENU > MY PROJECTS keeps 32 of them in the firmware's own space (proj/p01.kxp ..). MENU > PROJECT FILES moves them in
 * and out as files: an export goes into "VIBE FM/kriechstrom/" on the host's writing place (the VIBE's card, the
 * plugin's folder, a folder the browser was given); an import reads a .kxp from there, or one dropped onto the
 * plugin's window. The file:
 *   0    "KXP1", version 1, its size, flags (bit 0..3: slot A..D used, 4..6: USR1..3 held), name (24), each
 *        sample's bytes (3 x u32), 12 spare
 *   64   the music now, slot A, B, C, D (KX_SNAP_SZ each), the pedals (kx_ped as it is), then the samples held
 *        (the FM-1's user slot image: its header and IMA ADPCM)
 * The core's state is the audio flow's: it copies what a save needs (KXC_PSAVE) and puts in what a load brings
 * (KXC_PLOAD); the files are the control flow's. */

#define KXP_MAX 32
#define KXP_HDR 64
typedef struct {
    char magic[4];
    uint32_t ver, size, flags;
    char name[24];
    uint32_t samp[3];
    uint8_t spare[12];
} kxp_hdr;
#define KXP_FIXED (KXP_HDR + 5 * KX_SNAP_SZ + (int)sizeof kx_ped)
#define KXP_BUF (KXP_FIXED + 3 * 0x14000)
static uint8_t kxp_buf[KXP_BUF];                  /* one project being saved, loaded, exported or imported */
static char kxp_name[KXP_MAX][24];               /* the library: each entry's name ("" = free) */
static int kxp_sel, kxp_cur;                      /* the entry picked on the page; the name's letter being edited */
static char kxp_edit[24];                         /* the name the next save gives it */
static char kxp_msg[24];                          /* a word for the display (kx_draw_toast) */
static project_store_t kxp_rd;                    /* (the control flow's own copies: proj_scratch and kx_rd are */
static project_t kxp_scratch;                     /* the audio flow's) */
static uint32_t kxp_load_req, kxp_load_done;
static struct { int mode, h, off, len; char path[VIBE_MEDIA_PATH], nm[40]; } kxp_x;   /* 1 export, 2 import, 3 export waiting for the host */   /* 1 export, 2 import (a file being moved) */      /* a load the audio flow has not taken yet: kxp_buf is its */
static uint32_t kxp_msg_n;

static void kxp_say(const char *s)
{
    str_cpy(kxp_msg, s, sizeof kxp_msg);
    kxp_msg_n++;
}
static void kxp_file(int e, char *b)              /* "proj/p01.kxp" */
{
    str_cpy(b, "proj/p00.kxp", 16);
    b[6] = (char)('0' + (e + 1) / 10), b[7] = (char)('0' + (e + 1) % 10);
}
static void kxp_default_name(int e, char *b)
{
    str_cpy(b, "PROJECT 00", 24);
    b[8] = (char)('0' + (e + 1) / 10), b[9] = (char)('0' + (e + 1) % 10);
}

/* ---- the audio flow ---------------------------------------------------------------------------------------- */
static project_store_t kxp_now, kxp_slots[4];
static uint8_t kxp_used;
static int16_t kxp_ped_copy[PO_N][PED_SLOTS][PD_N];
static uint32_t kxp_cap_seq, kxp_cap_entry;

static void kxp_capture_audio(uint32_t entry)     /* KXC_PSAVE: what the save takes, as it is now */
{
    uint32_t i;
    project_capture(&proj_scratch);
    proj_pack(&kxp_now, &proj_scratch);
    kxp_used = 0;
    for (i = 0; i < 4u; i++) {
        memcpy(&kxp_slots[i], &proj_slot[i], sizeof kxp_slots[i]);
        kxp_used |= (uint8_t)(project_used(i) ? 1u << i : 0u);
    }
    memcpy(kxp_ped_copy, kx_ped, sizeof kxp_ped_copy);
    kxp_cap_entry = entry;
    __atomic_store_n(&kxp_cap_seq, kxp_cap_seq + 1u, __ATOMIC_RELEASE);
}

static void kxp_load_audio(void)                  /* KXC_PLOAD: kxp_buf (checked by the control flow) into the core */
{
    const kxp_hdr *h = (const kxp_hdr *)kxp_buf;
    const uint8_t *p = kxp_buf + KXP_HDR;
    uint32_t i;
    if (song.playing)
        transport_req = 2;
    for (i = 0; i < 4u; i++) {
        if ((h->flags >> i) & 1u)
            memcpy(&proj_slot[i], p + KX_SNAP_SZ * (1u + i), KX_SNAP_SZ);
        else
            memset(&proj_slot[i], 0, sizeof proj_slot[i]);
        kx_slot_saved(i);
    }
    memcpy(&kx_rd, p, KX_SNAP_SZ);
    if (proj_import(&proj_scratch, &kx_rd, KX_SNAP_SZ)) {
        proj_bound(&proj_scratch);
        project_restore_runtime(&proj_scratch);
    }
    memcpy(kx_ped, p + 5 * KX_SNAP_SZ, sizeof kx_ped);
    kx_slot_info();
    ui.msg_t = 0;
    __atomic_store_n(&kxp_load_done, kxp_load_req, __ATOMIC_RELEASE);
}
static int kxp_busy(void)                         /* kxp_buf still waits for the audio flow */
{
    return __atomic_load_n(&kxp_load_done, __ATOMIC_ACQUIRE) != kxp_load_req;
}

/* ---- the control flow: the library ---------------------------------------------------------------------------- */
static void kxp_scan(void)
{
    int e;
    for (e = 0; e < KXP_MAX; e++) {
        kxp_hdr h;
        char f[16];
        kxp_file(e, f);
        kxp_name[e][0] = 0;
        if (vibe_store_read_at(f, 0, &h, (int)sizeof h) == (int)sizeof h && !memcmp(h.magic, "KXP1", 4)) {
            memcpy(kxp_name[e], h.name, sizeof kxp_name[e]);
            kxp_name[e][23] = 0;
            if (!kxp_name[e][0])
                kxp_default_name(e, kxp_name[e]);
        }
    }
}
static void kxp_pick(int e)                       /* the entry picked: its name to edit */
{
    kxp_sel = clamp(e, 0, KXP_MAX - 1);
    if (kxp_name[kxp_sel][0])
        str_cpy(kxp_edit, kxp_name[kxp_sel], sizeof kxp_edit);
    else
        kxp_default_name(kxp_sel, kxp_edit);
    kxp_cur = clamp(kxp_cur, 0, 22);
}

/* is kxp_buf (n bytes) a project? its size, 0 if not */
static int kxp_check(int n)
{
    const kxp_hdr *h = (const kxp_hdr *)kxp_buf;
    uint32_t k, need = KXP_FIXED;
    if (n < KXP_FIXED || memcmp(h->magic, "KXP1", 4) || h->ver != 1u)
        return 0;
    for (k = 0; k < 3u; k++)
        if ((h->flags >> (4u + k)) & 1u) {
            if (h->samp[k] < 512u || h->samp[k] > 0x14000u)
                return 0;
            need += h->samp[k];
        }
    if ((uint32_t)n < need)
        return 0;
    memcpy(&kxp_rd, kxp_buf + KXP_HDR, KX_SNAP_SZ);   /* (the music must be one the core takes) */
    if (!proj_import(&kxp_scratch, &kxp_rd, KX_SNAP_SZ))
        return 0;
    return (int)need;
}

static uint32_t kxp_cap_seen;
static int kxp_save_to = -1;
static void kxp_save(int e)                       /* F5 on MY PROJECTS: the music now into entry e */
{
    kxp_save_to = e;
    kx_post(KXC_PSAVE, (uint32_t)e, 0, 0, 0);
}
static void kxp_build(void)                       /* the audio flow's copy (and the samples) into kxp_buf */
{
    kxp_hdr *h = (kxp_hdr *)kxp_buf;
    uint8_t *p = kxp_buf + KXP_HDR;
    uint32_t k, at = KXP_FIXED;
    memset(h, 0, sizeof *h);
    memcpy(h->magic, "KXP1", 4);
    h->ver = 1;
    h->flags = kxp_used;
    str_cpy(h->name, kxp_edit, sizeof h->name);
    memcpy(p, &kxp_now, KX_SNAP_SZ);
    for (k = 0; k < 4u; k++)
        memcpy(p + KX_SNAP_SZ * (1u + k), &kxp_slots[k], KX_SNAP_SZ);
    memcpy(p + 5 * KX_SNAP_SZ, kxp_ped_copy, sizeof kxp_ped_copy);
    for (k = 0; k < 3u; k++)
        if (usr_nz[k]) {
            uint32_t len = 512u + ((const smp_user_hdr_t *)kx_usr_img[k])->data_len;
            len = len > 0x14000u ? 0x14000u : len;
            memcpy(kxp_buf + at, kx_usr_img[k], len);
            h->samp[k] = len;
            h->flags |= 1u << (4u + k);
            at += len;
        }
    h->size = at;
}

static int kxp_x_mode(void);
static void kxp_load(int e)                       /* ENTER on MY PROJECTS (twice): entry e becomes the music now */
{
    char f[16];
    int n, k;
    const kxp_hdr *h = (const kxp_hdr *)kxp_buf;
    if (kxp_busy() || kxp_x_mode())
        return;
    kxp_file(e, f);
    n = vibe_store_read(f, kxp_buf, KXP_BUF);
    if (n <= 0 || !kxp_check(n)) {
        kxp_say(n <= 0 ? "EMPTY" : "NOT A PROJECT");
        return;
    }
    {   /* the samples: as the sampler writes a slot (kx_sampler.h), here in the control flow */
        const uint8_t *s = kxp_buf + KXP_FIXED;
        for (k = 0; k < 3; k++) {
            usr_nz[k] = 0;
            memset(kx_usr_img[k], 0, 0x14000);
            if ((h->flags >> (4u + (uint32_t)k)) & 1u) {
                memcpy(kx_usr_img[k], s, h->samp[k]);
                s += h->samp[k];
            }
            smp_user_scan((uint32_t)k);
            kx_usr_redraw((uint32_t)k);
            __atomic_store_n(&kx_usr_seq[k], kx_usr_seq[k] + 1u, __ATOMIC_RELEASE);
        }
    }
    kxp_load_req++;
    kx_post(KXC_PLOAD, (uint32_t)e, 0, 0, 0);
    {
        char b[24];
        str_cpy(b, "LOADED ", sizeof b);
        str_cpy(b + 7, kxp_name[e], sizeof b - 7);
        kxp_say(b);
    }
}

static void kxp_delete(int e)
{
    char f[16];
    kxp_file(e, f);
    vibe_store_delete(f);
    kxp_name[e][0] = 0;
    kxp_pick(e);
    kxp_say("DELETED");
}

/* ---- files: export, import, a drop ------------------------------------------------------------------------- */
static char kxp_src[64];                          /* the writing place's key ("" none yet) */
static char kxp_list[KXP_MAX][64];                /* PROJECT FILES: the .kxp files there */
static int kxp_nlist, kxp_lsel, kxp_list_want = 1;

static void kxp_sources(void)                     /* the writing place: the source flagged 'w' */
{
    static char b[2048];
    int n = vibe_media_sources(b, (int)sizeof b - 1), i = 0;
    kxp_src[0] = 0;
    if (n <= 0)
        return;
    b[n] = 0;
    while (i < n) {
        int s = i, t1 = -1, t2 = -1;
        while (i < n && b[i] != '\n') {
            if (b[i] == '\t')
                t1 < 0 ? (t1 = i) : t2 < 0 ? (t2 = i) : 0;
            i++;
        }
        if (t1 > s && t2 > t1) {
            int k, w = 0;
            for (k = t1 + 1; k < t2; k++)
                w |= b[k] == 'w';
            if (w && t1 - s < (int)sizeof kxp_src) {
                memcpy(kxp_src, b + s, (size_t)(t1 - s));
                kxp_src[t1 - s] = 0;
                return;
            }
        }
        i++;
    }
}

static int kxp_ends(const char *s, const char *e)
{
    int a = str_len(s), b = str_len(e), i;
    if (a < b)
        return 0;
    for (i = 0; i < b; i++) {
        char c = s[a - b + i];
        c = c >= 'a' && c <= 'z' ? (char)(c - 32) : c;
        if (c != e[i])
            return 0;
    }
    return 1;
}

static void kxp_list_poll(void)                   /* PROJECT FILES: what is there (asked again until the host has it) */
{
    static char b[4096];
    char folder[128];
    int n, i = 0;
    if (!kxp_list_want)
        return;
    if (!kxp_src[0])
        kxp_sources();
    if (!kxp_src[0])
        return;
    str_cpy(folder, kxp_src, sizeof folder);
    str_cpy(folder + str_len(folder), "/VIBE FM/kriechstrom", sizeof folder - (size_t)str_len(folder));
    n = vibe_media_list(folder, 0, b, (int)sizeof b - 1);
    if (n == VIBE_MEDIA_PENDING)
        return;
    kxp_list_want = 0;
    kxp_nlist = 0;
    if (n <= 0)
        return;
    b[n] = 0;
    while (i < n && kxp_nlist < KXP_MAX) {
        int s = i, tab = 0, name = -1;
        while (i < n && b[i] != '\n') {
            if (b[i] == '\t' && ++tab == 2)
                name = i + 1;
            i++;
        }
        if (b[s] == 'f' && name > 0) {
            b[i] = 0;
            if (kxp_ends(b + name, ".KXP") && str_len(folder) + 1 + str_len(b + name) < 64) {
                str_cpy(kxp_list[kxp_nlist], folder, 64);
                str_cpy(kxp_list[kxp_nlist] + str_len(folder), "/", 2);
                str_cpy(kxp_list[kxp_nlist] + str_len(folder) + 1, b + name, 64 - (size_t)str_len(folder) - 1);
                kxp_nlist++;
            }
        }
        i++;
    }
    kxp_lsel = clamp(kxp_lsel, 0, kxp_nlist ? kxp_nlist - 1 : 0);
}

static void kxp_export(int e)                     /* SHIFT + F5 on MY PROJECTS: entry e as a file */
{
    char f[16], nm[40];
    int n, i;
    if (kxp_x.mode || kxp_busy())
        return;
    kxp_file(e, f);
    n = vibe_store_read(f, kxp_buf, KXP_BUF);
    if (n <= 0 || !kxp_check(n)) {
        kxp_say("SAVE IT FIRST");
        return;
    }
    str_cpy(nm, kxp_name[e][0] ? kxp_name[e] : "PROJECT", 32);
    for (i = 0; nm[i]; i++)                       /* (a file name: no slashes) */
        if (nm[i] == '/' || nm[i] == '\\' || nm[i] == '.')
            nm[i] = '-';
    str_cpy(nm + str_len(nm), ".kxp", 8);
    kxp_sources();
    str_cpy(kxp_x.nm, nm, sizeof kxp_x.nm);
    kxp_x.h = vibe_media_create(nm, kxp_x.path, (int)sizeof kxp_x.path);
    if (kxp_x.h == VIBE_MEDIA_PENDING) {          /* (the host makes the folder first: asked again from kxp_poll) */
        kxp_x.mode = 3;
        kxp_x.len = n;
        return;
    }
    if (kxp_x.h <= 0) {
        if (vibe_media_add(0) == 1) {
            vibe_media_add(1);
            kxp_say("PICK A FOLDER, F5 AGAIN");
        } else
            kxp_say("NO PLACE TO WRITE");
        return;
    }
    kxp_x.mode = 1;
    kxp_x.off = 0;
    kxp_x.len = n;
}

static int kxp_free(void)
{
    int e;
    for (e = 0; e < KXP_MAX; e++)
        if (!kxp_name[e][0])
            return e;
    return -1;
}

static int kxp_x_mode(void) { return kxp_x.mode; }
static void kxp_import(const char *path)          /* a .kxp file into the first free entry */
{
    if (kxp_x.mode || kxp_busy())
        return;
    if (kxp_free() < 0) {
        kxp_say("MY PROJECTS IS FULL");
        return;
    }
    str_cpy(kxp_x.path, path, sizeof kxp_x.path);
    kxp_x.mode = 2;
    kxp_x.off = 0;
    kxp_x.len = KXP_BUF;
}

/* the control flow, every event and frame: a save the audio flow copied for, a file moving */
static void kxp_poll(void)
{
    uint32_t c = __atomic_load_n(&kxp_cap_seq, __ATOMIC_ACQUIRE);
    if (c != kxp_cap_seen) {
        char f[16], b[24];
        int e = (int)kxp_cap_entry;
        kxp_cap_seen = c;
        if (e == kxp_save_to && e >= 0 && e < KXP_MAX && !kxp_busy() && !kxp_x.mode) {
            kxp_build();
            kxp_file(e, f);
            if (vibe_store_write(f, kxp_buf, (int)((kxp_hdr *)kxp_buf)->size) == 0) {
                str_cpy(kxp_name[e], kxp_edit, sizeof kxp_name[e]);
                str_cpy(b, "SAVED ", sizeof b);
                str_cpy(b + 6, kxp_edit, sizeof b - 6);
                kxp_say(b);
            } else
                kxp_say("SAVE ERROR: FULL?");
        }
        kxp_save_to = -1;
    }
    if (kxp_x.mode == 3) {                        /* export: the host had the folder to make */
        kxp_x.h = vibe_media_create(kxp_x.nm, kxp_x.path, (int)sizeof kxp_x.path);
        if (kxp_x.h > 0)
            kxp_x.mode = 1, kxp_x.off = 0;
        else if (kxp_x.h != VIBE_MEDIA_PENDING) {
            kxp_x.mode = 0;
            kxp_say("NO PLACE TO WRITE");
        }
    } else if (kxp_x.mode == 1) {                 /* export: 64 KB a frame */
        int k = kxp_x.len - kxp_x.off, r;
        k = k > 65536 ? 65536 : k;
        r = k ? vibe_media_append(kxp_x.h, kxp_buf + kxp_x.off, k) : 0;
        if (r == 0)
            kxp_x.off += k;
        if (r == -1 || kxp_x.off >= kxp_x.len) {
            vibe_media_close(kxp_x.h, r != -1);
            kxp_say(r == -1 ? "EXPORT FAILED" : "EXPORTED");
            kxp_x.mode = 0;
            kxp_list_want = 1;
        }
    } else if (kxp_x.mode == 2) {                 /* import: read on until the end */
        int r = vibe_media_read(kxp_x.path, (uint64_t)kxp_x.off, kxp_buf + kxp_x.off, KXP_BUF - kxp_x.off);
        if (r > 0 && kxp_x.off + r < KXP_BUF)
            kxp_x.off += r;
        else if (r != VIBE_MEDIA_PENDING) {
            int n = kxp_x.off + (r > 0 ? r : 0), e = kxp_free();
            char f[16];
            kxp_x.mode = 0;
            if (r == -1 || !kxp_check(n) || e < 0) {
                kxp_say(e < 0 ? "MY PROJECTS IS FULL" : "NOT A PROJECT");
                return;
            }
            kxp_file(e, f);
            if (vibe_store_write(f, kxp_buf, kxp_check(n)) == 0) {
                char b[24];
                kxp_scan();
                kxp_pick(e);
                str_cpy(b, "IMPORTED TO 00", sizeof b);
                b[12] = (char)('0' + (e + 1) / 10), b[13] = (char)('0' + (e + 1) % 10);
                kxp_say(b);
            } else
                kxp_say("IMPORT ERROR: FULL?");
        }
    }
}

static void kxp_media_event(const vibe_event *e)  /* VIBE_EV_MEDIA: a source came or went, a .kxp dropped */
{
    if (e->id == VIBE_MEDIA_EV_SOURCES) {
        kxp_sources();
        kxp_list_want = 1;
    } else if (e->id == VIBE_MEDIA_EV_READY)
        kxp_list_want |= kxp_nlist == 0;
    else if (e->id == VIBE_MEDIA_EV_DROPPED) {
        char p[VIBE_MEDIA_PATH];
        int pad, x, y;
        while (vibe_media_dropped(p, (int)sizeof p, &pad, &x, &y) == 1)
            if (kxp_ends(p, ".KXP"))
                kxp_import(p);
    }
}

static void kxp_boot(void)                        /* vibe_on_init */
{
    char p[8];
    int pad, x, y;
    kxp_scan();
    kxp_pick(0);
    kxp_sources();
    vibe_media_dropped(p, 0, &pad, &x, &y);       /* (asked once: the plugin then takes drops) */
}

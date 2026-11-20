/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: the sampler. What comes in (a guitar, a voice) or the whole mix, recorded for 1, 2 or 4 bars
 * from the next bar's first beat, into a user slot USR1..3: the selected track then plays it as SLICE, cut where its
 * hits are (SLICE's own AUTO slices), on the keys and the steps, and SAMPLE plays it across the keys. The FM-1 has the
 * slots but no input and no memory to record into.
 * The slots are the FM-1's (eng_sample.c: a header and IMA ADPCM, 80 KiB, about 7 s at 22 kHz); here in RAM (kx_hal.h
 * SMP_USER_XIP), kept in usr1.kxu .. usr3.kxu. The audio flow records (16 bit, 22.05 kHz, mono) into kx_rec; the
 * control flow encodes it into the slot and has the core read it (smp_user_scan), as the FM-1's main loop does after an
 * upload. */

#define KX_REC_MAX (2u * (0x14000u - 512u))         /* the samples a slot holds */
static int16_t kx_rec[KX_REC_MAX];
static uint32_t kx_rec_n;                          /* recorded so far (the audio flow) */
static uint32_t kx_rec_want;                       /* .. how many it records (0: up to KX_REC_MAX) */
static uint8_t kx_rec_state;                       /* 0 idle, 1 waiting for the bar, 2 recording (the audio flow's) */
static uint32_t kx_rec_done;                       /* +1 when a recording is complete (the audio flow) */
static uint8_t kx_rec_src, kx_rec_slot;
static int32_t kx_rec_acc;                         /* (the pair being averaged down to 22.05 kHz) */
static uint8_t kx_rec_odd, kx_rec_last16 = 0xFF;

/* the audio flow, KXC_SREC: arm (or stop) a recording: src 0 the input, 1 the mix; bars 1, 2, 4 or 0 (as long as fits) */
static void kx_rec_arm(uint32_t src, uint32_t bars, uint32_t slot)
{
    if (kx_rec_state) {                            /* (armed or running: REC again stops it, what is there is kept) */
        if (kx_rec_state == 2u && kx_rec_n > 2205u)
            __atomic_store_n(&kx_rec_done, kx_rec_done + 1u, __ATOMIC_RELEASE);
        kx_rec_state = 0;
        return;
    }
    kx_rec_src = (uint8_t)(src & 1u);
    kx_rec_slot = (uint8_t)(slot % 3u);
    kx_rec_want = bars ? (uint32_t)((uint64_t)bars * 4u * 60u * 22050u / (uint32_t)clamp(song.g[G_BPM], 20, 300)) : 0u;
    if (kx_rec_want > KX_REC_MAX)
        kx_rec_want = KX_REC_MAX;
    kx_rec_n = 0;
    kx_rec_odd = 0;
    kx_rec_acc = 0;
    kx_rec_state = song.playing ? 1u : 2u;         /* (stopped: at once) */
    kx_rec_last16 = 0xFF;
}

/* the audio flow, after each core block (o: the mix, interleaved Q15) */
static void kx_rec_block(const int32_t *o)
{
    uint32_t i;
    if (kx_rec_state == 1u) {                      /* waiting: the next bar of the selected track starts it */
        uint32_t s = trk[song.sel].seq_idx % 16u;
        if (s == 0u && kx_rec_last16 != 0u && kx_rec_last16 != 0xFF)
            kx_rec_state = 2;
        kx_rec_last16 = (uint8_t)s;
        if (!song.playing)
            kx_rec_state = 2;
        if (kx_rec_state != 2u)
            return;
    }
    if (kx_rec_state != 2u)
        return;
    for (i = 0; i < CTL; i++) {
        int32_t x = kx_rec_src ? (o[2u * i] + o[2u * i + 1u]) >> 1 : kx_in_mono[i];
        kx_rec_acc += clamp(x, -32767, 32767);
        if (kx_rec_odd ^= 1u)
            continue;
        if (kx_rec_n < KX_REC_MAX)
            kx_rec[kx_rec_n++] = (int16_t)(kx_rec_acc >> 1);
        kx_rec_acc = 0;
        if (kx_rec_n >= KX_REC_MAX || (kx_rec_want && kx_rec_n >= kx_rec_want)) {
            kx_rec_state = 0;
            __atomic_store_n(&kx_rec_done, kx_rec_done + 1u, __ATOMIC_RELEASE);
            return;
        }
    }
}

/* ---- the slots ------------------------------------------------------------------------------------------------- */
static uint32_t kx_usr_seq[3], kx_usr_wrote[3];    /* +1 when a slot changed (the control flow writes it out) */
static int8_t kx_usr_wave[3][2][120];               /* the display's picture of each slot: min / max per column */
static const char *const KX_USR_FILE[3] = {"usr1.kxu", "usr2.kxu", "usr3.kxu"};

static uint32_t kx_crc32(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xFFFFFFFFu, k;
    while (n--) {
        c ^= *p++;
        for (k = 0; k < 8u; k++)
            c = c & 1u ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    }
    return ~c;
}

static void kx_usr_picture(uint32_t k, const int16_t *x, uint32_t n)
{
    uint32_t c, i;
    for (c = 0; c < 120u; c++) {
        int32_t lo = 0, hi = 0;
        uint32_t a = c * n / 120u, b = (c + 1u) * n / 120u;
        for (i = a; i < b; i++) {
            lo = x[i] < lo ? x[i] : lo;
            hi = x[i] > hi ? x[i] : hi;
        }
        kx_usr_wave[k][0][c] = (int8_t)(lo >> 8);
        kx_usr_wave[k][1][c] = (int8_t)(hi >> 8);
    }
}

/* the recording (n samples at 22.05 kHz) into slot k: brought up to -1 dB, IMA ADPCM as the core decodes it (eng_sample.c
 * sample_next: the low nibble first), one zone over the whole keyboard (root C4), not looped. The control flow */
static int16_t kx_usr_tmp[KX_REC_MAX];
static void kx_usr_write(uint32_t k, const int16_t *src, uint32_t n)
{
    uint8_t *img = kx_usr_img[k % 3u], *d = img + 512;
    smp_user_hdr_t h;
    int32_t pred = 0, idx = 0, pk = 1;
    uint32_t i, g;
    for (i = 0; i < n; i++)
        pk = (src[i] < 0 ? -src[i] : src[i]) > pk ? (src[i] < 0 ? -src[i] : src[i]) : pk;
    g = (uint32_t)(29204 * 4096 / pk);             /* (to -1 dB, Q12; a quiet take is lifted at most 16 times) */
    g = g > 65536u ? 65536u : g;
    for (i = 0; i < n; i++)
        kx_usr_tmp[i] = (int16_t)clamp((int32_t)(((int64_t)src[i] * g) >> 12), -32767, 32767);
    usr_nz[k % 3u] = 0;                            /* (the slot is changing: nothing plays from it meanwhile) */
    memset(img, 0, 0x14000);
    for (i = 0; i < n; i++) {                      /* IMA ADPCM */
        int32_t step = IMA_STEP[idx], diff = kx_usr_tmp[i] - pred, code = 0, vd = step >> 3;
        if (diff < 0) {
            code = 8;
            diff = -diff;
        }
        if (diff >= step) { code |= 4; diff -= step; vd += step; }
        if (diff >= step >> 1) { code |= 2; diff -= step >> 1; vd += step >> 1; }
        if (diff >= step >> 2) { code |= 1; vd += step >> 2; }
        pred = clamp(pred + ((code & 8) ? -vd : vd), -32768, 32767);
        idx = clamp(idx + IMA_IDX[code & 7], 0, 88);
        if (i & 1u)
            d[i >> 1] |= (uint8_t)(code << 4);
        else
            d[i >> 1] = (uint8_t)code;
    }
    memset(&h, 0, sizeof h);
    h.magic = SMP_USER_MAGIC;
    h.version = 1;
    h.nz = 1;
    h.name[0] = 'R', h.name[1] = 'E', h.name[2] = 'C', h.name[3] = (char)('1' + k % 3u);
    h.data_len = (n + 1u) / 2u;
    h.zone[0].off = 0;
    h.zone[0].n = n;
    h.zone[0].ls = n - 1u;
    h.zone[0].le = n - 1u;
    h.zone[0].rate = 32768u;                       /* (22.05 kHz / 44.1 kHz, Q16) */
    h.zone[0].root16 = 60 * 16;
    h.zone[0].lo = 0;
    h.zone[0].hi = 127;
    h.crc = kx_crc32(d, h.data_len);
    memcpy(img, &h, sizeof h);
    kx_usr_picture(k % 3u, kx_usr_tmp, n);
    smp_user_scan(k % 3u);                         /* (the core reads it, SLICE finds its hits: as after an upload) */
    __atomic_store_n(&kx_usr_seq[k % 3u], kx_usr_seq[k % 3u] + 1u, __ATOMIC_RELEASE);
}

/* the control flow, every event and frame: a recording done -> its slot; changed slots -> their files */
static uint32_t kx_rec_seen;
static void kx_sampler_poll(void)
{
    uint32_t d = __atomic_load_n(&kx_rec_done, __ATOMIC_ACQUIRE), k;
    if (d != kx_rec_seen) {
        uint32_t n = kx_rec_n;
        kx_rec_seen = d;
        if (n > 2205u) {
            memcpy(kx_usr_tmp, kx_rec, n * sizeof kx_rec[0]);
            kx_usr_write(kx_rec_slot, kx_usr_tmp, n);
            kx_post(KXC_SLICE, kx_rec_slot, 0, 0, 0);
        }
    }
    for (k = 0; k < 3u; k++)
        if (kx_usr_seq[k] != kx_usr_wrote[k]) {
            kx_usr_wrote[k] = kx_usr_seq[k];
            vibe_store_write(KX_USR_FILE[k], kx_usr_img[k], (int)(512u + ((const smp_user_hdr_t *)kx_usr_img[k])->data_len));
        }
}

/* vibe_on_init: the slots from their files, read by the core (and their pictures) */
static void kx_sampler_boot(void)
{
    uint32_t k;
    for (k = 0; k < 3u; k++) {
        int n = vibe_store_read(KX_USR_FILE[k], kx_usr_img[k], 0x14000);
        if (n > 512) {
            const smp_user_hdr_t *h = (const smp_user_hdr_t *)kx_usr_img[k];
            smp_user_scan(k);
            if (usr_nz[k]) {                       /* (its picture: decoded once) */
                uint32_t i, m = h->zone[0].n < KX_REC_MAX ? h->zone[0].n : KX_REC_MAX;
                int32_t pred = 0, idx = 0;
                const uint8_t *d = kx_usr_img[k] + 512;
                for (i = 0; i < m; i++) {
                    uint32_t code = (i & 1u) ? (d[i >> 1] >> 4) : (d[i >> 1] & 15u);
                    int32_t step = IMA_STEP[idx], vd = step >> 3;
                    if (code & 4u) vd += step;
                    if (code & 2u) vd += step >> 1;
                    if (code & 1u) vd += step >> 2;
                    pred = clamp(pred + ((code & 8u) ? -vd : vd), -32768, 32767);
                    idx = clamp(idx + IMA_IDX[code & 7u], 0, 88);
                    kx_usr_tmp[i] = (int16_t)pred;
                }
                kx_usr_picture(k, kx_usr_tmp, m);
            }
        }
        kx_usr_wrote[k] = kx_usr_seq[k];
    }
}

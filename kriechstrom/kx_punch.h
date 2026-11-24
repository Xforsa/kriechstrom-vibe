/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM: its own punch-in effects, held on the pads (MENU > PAD FX gives them a pad, the first
 * eight pads 9-16 at first), worked on the whole mix after the core's master, in the audio flow. The FM-1's core is
 * not touched: with none held (and their tails gone) the mix passes as it was, bit for bit.
 *   PRESS      a hydraulic press: the mix squeezed, ducked at each beat and let up through it
 *   SHRED      cut into 1/32 grains, each taken from the last beat, some backwards, some an octave off
 *   BLACKOUT   the power fails: the tape slows, the treble goes, a hum, then nothing; let go: it comes back
 *   SHORTWAVE  a shortwave radio: a narrow band, its tuning drifting, static and a whistle
 *   CORRODE    eaten away the longer it is held: the rate and the bits fall, crackle grows
 *   HAMMER     a hammer on every 1/8: the mix struck, metal ringing after it
 *   VOID       the dry gone: only a long dark tail of it (and the tail stays a while when let go)
 *   STROBE     cut at 1/32, hard, the screen with it
 * Each one's level moves in 5 ms (no clicks); kx_pn_lv says how far each is in (the display reads it). */

#define PN_N 8
enum { PN_PRESS, PN_SHRED, PN_BLACKOUT, PN_SHORTWAVE, PN_CORRODE, PN_HAMMER, PN_VOID, PN_STROBE };
static const char *const PN_NAME[PN_N] = {"PRESS", "SHRED", "BLACKOUT", "SHORTWAVE", "CORRODE", "HAMMER", "VOID", "STROBE"};
static const char *const PN_CELL[PN_N] = {"PRESS", "SHRED", "BLACK", "RADIO", "RUST", "HAMMR", "VOID", "STROB"};
static const char *const PN_WHAT[PN_N] = {
    "the mix squeezed, pumping on the beat", "cut into grains, flung back and forth",
    "the power fails, it sinks into hum", "a shortwave radio, its tuning drifting", "eaten away, the longer the worse",
    "a hammer on every 1/8, metal ringing", "the dry gone, only its dark tail", "chopped at 1/32, the screen too"};

#define KX_NFX (PF_NFX + PN_N)                    /* the pads' effects: the core's 18, then these 8 */
static const char *kx_fx_cell(int fx) { return fx < PF_NFX ? PF_CELL[fx] : PN_CELL[(fx - PF_NFX) % PN_N]; }
static const char *kx_fx_name(int fx) { return fx < PF_NFX ? PF_NAME[fx] : PN_NAME[(fx - PF_NFX) % PN_N]; }

static uint8_t kx_fx_on[KX_NFX];                  /* the effects held now, from the pads, the stage, the foot (control flow) */
static uint64_t kx_fx_t0[KX_NFX];                 /* when each was pressed */
static int kx_fx_any(void)
{
    int i;
    for (i = 0; i < KX_NFX; i++)
        if (kx_fx_on[i])
            return 1;
    return 0;
}

static uint32_t kx_pn_held;                       /* (the audio flow's: KXC_PUNCH sets and clears it) */
static float kx_pn_lv[PN_N];                      /* each one's level now, 0 .. 1 (the display reads it) */
static float kx_pn_held_s[PN_N];                  /* how long each has been held, in seconds */
static uint32_t kx_pn_hits;                       /* HAMMER's blows (the display shakes on each) */

#define PN_HN 65536u                              /* the last 1.5 s, for SHRED and BLACKOUT */
static float pn_h[2][PN_HN];
static uint32_t pn_hw;
static uint32_t pn_rng = 0x2611BEEFu;
static float pn_rand(void) { pn_rng ^= pn_rng << 13; pn_rng ^= pn_rng >> 17; pn_rng ^= pn_rng << 5; return (float)(pn_rng >> 8) * (1.0f / 16777216.0f); }

/* SHRED's grain */
static float pn_gpos, pn_grate;
static uint32_t pn_glen, pn_gat;
/* BLACKOUT: the power (1 on, 0 off), the tape's reader */
static float pn_pow = 1.0f, pn_bread, pn_blp[2];
/* SHORTWAVE: a band-pass (two one-poles each side), the tuning's phase */
static float pn_sw_lo[2], pn_sw_hi[2], pn_sw_ph, pn_sw_wh;
/* CORRODE: the held sample, its counter, a low-pass */
static float pn_cr_hold[2], pn_cr_cnt, pn_cr_lp[2];
/* HAMMER: its envelope, three resonators (two-pole) */
static float pn_hm_env, pn_hm_y1[3], pn_hm_y2[3];
static uint32_t pn_hm_last = 0xFFFFFFFFu;
/* VOID: a dark reverb (four combs, two all-passes), its energy */
#define PN_VC 4
static float pn_vc[PN_VC][2400], pn_vc_lp[PN_VC], pn_va[2][700];
static uint32_t pn_vc_i[PN_VC], pn_va_i[2];
static const uint32_t PN_VC_LEN[PN_VC] = {1557, 1617, 1491, 2277};
static const uint32_t PN_VA_LEN[2] = {556, 441};
static float pn_v_energy;

static void kx_pn_set(uint32_t fx, int down)      /* KXC_PUNCH: the audio flow */
{
    if (fx >= PN_N)
        return;
    if (down) {
        kx_pn_held |= 1u << fx;
        kx_pn_held_s[fx] = 0.0f;
        if (fx == PN_SHRED)
            pn_glen = 0;
    } else
        kx_pn_held &= ~(1u << fx);
}

static float pn_void(float x)                     /* the dark reverb: one sample in, one out (mono) */
{
    float s = 0.0f, y;
    uint32_t k;
    for (k = 0; k < PN_VC; k++) {
        float *b = pn_vc[k], o = b[pn_vc_i[k]];
        pn_vc_lp[k] += (o - pn_vc_lp[k]) * 0.35f;   /* (dark: the treble dies in it) */
        b[pn_vc_i[k]] = x + pn_vc_lp[k] * 0.89f;
        pn_vc_i[k] = pn_vc_i[k] + 1u == PN_VC_LEN[k] ? 0u : pn_vc_i[k] + 1u;
        s += o;
    }
    y = s * 0.25f;
    for (k = 0; k < 2u; k++) {
        float *b = pn_va[k], o = b[pn_va_i[k]], in = y + o * 0.5f;
        b[pn_va_i[k]] = in;
        y = o - in * 0.5f;
        pn_va_i[k] = pn_va_i[k] + 1u == PN_VA_LEN[k] ? 0u : pn_va_i[k] + 1u;
    }
    return y;
}

static void kx_punch_block(int32_t *o, uint32_t n)
{
    uint32_t i, k, active = kx_pn_held;
    float step = (float)n / (0.005f * (float)FS), beat = (float)beat_samples(), dt = (float)n / (float)FS;
    for (k = 0; k < PN_N; k++) {                  /* the levels, toward held or not */
        float tgt = (kx_pn_held >> k) & 1u ? 1.0f : 0.0f;
        kx_pn_lv[k] += tgt > kx_pn_lv[k] ? vd_minf(step, tgt - kx_pn_lv[k]) : -vd_minf(step, kx_pn_lv[k] - tgt);
        if ((kx_pn_held >> k) & 1u)
            kx_pn_held_s[k] += dt;
        if (kx_pn_lv[k] > 0.0f)
            active |= 1u << k;
    }
    if (pn_pow < 1.0f)
        active |= 1u << PN_BLACKOUT;
    if (pn_v_energy > 1e-5f)
        active |= 1u << PN_VOID;
    if (pn_hm_env > 1e-4f)
        active |= 1u << PN_HAMMER;
    for (i = 0; i < n; i++) {                     /* (the history is always kept: an effect starts from what was) */
        pn_h[0][pn_hw & (PN_HN - 1u)] = (float)o[2u * i] * (1.0f / 32768.0f);
        pn_h[1][pn_hw & (PN_HN - 1u)] = (float)o[2u * i + 1u] * (1.0f / 32768.0f);
        pn_hw++;
        if (!active)
            continue;
        {
            float l = pn_h[0][(pn_hw - 1u) & (PN_HN - 1u)], r = pn_h[1][(pn_hw - 1u) & (PN_HN - 1u)], a;
            float ph = (float)((beat_pos + i) % (uint32_t)(beat > 1.0f ? beat : 1.0f)) / beat;   /* (where in the beat) */
            if ((a = kx_pn_lv[PN_SHRED]) > 0.0f) {         /* SHRED: grains from the last beat */
                float gl, gr, w;
                uint32_t at;
                if (pn_gat >= pn_glen) {
                    pn_glen = (uint32_t)(beat / 8.0f);
                    pn_gat = 0;
                    pn_gpos = (float)(pn_hw - 1u) - pn_rand() * beat;
                    pn_grate = pn_rand() < 0.35f ? -1.0f : 1.0f;
                    pn_grate *= pn_rand() < 0.25f ? 2.0f : pn_rand() < 0.2f ? 0.5f : 1.0f;
                    if (pn_grate > 0.0f)
                        pn_gpos -= (float)pn_glen * pn_grate;
                }
                at = (uint32_t)(pn_gpos + (float)pn_gat * pn_grate) & (PN_HN - 1u);
                w = (float)pn_gat / (float)(pn_glen ? pn_glen : 1u);
                w = w < 0.08f ? w / 0.08f : w > 0.92f ? (1.0f - w) / 0.08f : 1.0f;
                gl = pn_h[0][at] * w, gr = pn_h[1][at] * w;
                pn_gat++;
                l += (gl - l) * a, r += (gr - r) * a;
            }
            if ((a = kx_pn_lv[PN_PRESS]) > 0.0f) {         /* PRESS: squeezed, ducked at each beat */
                float duck = ph < 0.5f ? 0.12f + 0.88f * vd_sqrt(ph * 2.0f) : 1.0f;
                float pl = vd_tanh(l * 4.0f) * 0.42f * duck, pr = vd_tanh(r * 4.0f) * 0.42f * duck;
                l += (pl - l) * a, r += (pr - r) * a;
            }
            if ((kx_pn_held >> PN_BLACKOUT) & 1u || pn_pow < 1.0f) {   /* BLACKOUT: the tape slows to nothing */
                float tgt = (kx_pn_held >> PN_BLACKOUT) & 1u ? 0.0f : 1.0f;
                float bl, br, hum;
                uint32_t at;
                pn_pow += tgt < pn_pow ? -1.0f / (0.9f * (float)FS) : 1.0f / (0.12f * (float)FS);
                pn_pow = pn_pow < 0.0f ? 0.0f : pn_pow > 1.0f ? 1.0f : pn_pow;
                if (pn_pow >= 1.0f)
                    pn_bread = (float)(pn_hw - 1u);
                else
                    pn_bread += pn_pow * pn_pow;   /* (the reader slows as the power goes) */
                if ((float)(pn_hw - 1u) - pn_bread > (float)(PN_HN - 64u))
                    pn_bread = (float)(pn_hw - 1u) - (float)(PN_HN - 64u);
                at = (uint32_t)pn_bread & (PN_HN - 1u);
                bl = pn_h[0][at], br = pn_h[1][at];
                pn_blp[0] += (bl - pn_blp[0]) * (0.02f + 0.9f * pn_pow * pn_pow);
                pn_blp[1] += (br - pn_blp[1]) * (0.02f + 0.9f * pn_pow * pn_pow);
                hum = vd_sin((float)(pn_hw % 882u) / 882.0f) * 0.06f * vd_sqrt(1.0f - pn_pow) * (pn_pow > 0.02f ? 1.0f : 0.3f);
                if (pn_pow < 1.0f)
                    l = pn_blp[0] * pn_pow + hum, r = pn_blp[1] * pn_pow + hum;
            }
            if ((a = kx_pn_lv[PN_SHORTWAVE]) > 0.0f) {     /* SHORTWAVE: a narrow band, drifting, static, a whistle */
                float c, m = (l + r) * 0.5f, y, hi, lo, st;
                pn_sw_ph += 0.35f / (float)FS;
                c = 0.06f + 0.04f * vd_sin(pn_sw_ph);
                pn_sw_hi[0] += (m - pn_sw_hi[0]) * 0.02f;   /* (the low end gone) */
                hi = m - pn_sw_hi[0];
                pn_sw_lo[0] += (hi - pn_sw_lo[0]) * (c + 0.12f);
                pn_sw_lo[1] += (pn_sw_lo[0] - pn_sw_lo[1]) * (c + 0.12f);
                lo = pn_sw_lo[1];
                st = (pn_rand() - 0.5f) * 0.05f * (0.6f + 0.4f * vd_sin(pn_sw_ph * 7.0f));
                pn_sw_wh += (2400.0f + 600.0f * vd_sin(pn_sw_ph * 3.0f)) / (float)FS;
                pn_sw_wh -= (float)(int)pn_sw_wh;
                y = vd_tanh(lo * 3.0f) * 0.6f + st + vd_sin(pn_sw_wh) * 0.008f;
                l += (y - l) * a, r += (y - r) * a;
            }
            if ((a = kx_pn_lv[PN_CORRODE]) > 0.0f) {       /* CORRODE: worse the longer held */
                float hs = kx_pn_held_s[PN_CORRODE], div = 1.0f + hs * hs * 6.0f, q, cl, cr;
                div = div > 48.0f ? 48.0f : div;
                q = 64.0f / (1.0f + hs * 3.0f);
                q = q < 3.0f ? 3.0f : q;
                if ((pn_cr_cnt += 1.0f) >= div)
                    pn_cr_cnt -= div, pn_cr_hold[0] = (float)(int)(l * q) / q, pn_cr_hold[1] = (float)(int)(r * q) / q;
                pn_cr_lp[0] += (pn_cr_hold[0] - pn_cr_lp[0]) * 0.6f;
                pn_cr_lp[1] += (pn_cr_hold[1] - pn_cr_lp[1]) * 0.6f;
                cl = pn_cr_lp[0], cr = pn_cr_lp[1];
                if (pn_rand() < 0.0004f * (1.0f + hs * 4.0f))   /* (crackle) */
                    cl += (pn_rand() - 0.5f) * 0.5f, cr += (pn_rand() - 0.5f) * 0.5f;
                l += (cl - l) * a, r += (cr - r) * a;
            }
            if ((a = kx_pn_lv[PN_HAMMER]) > 0.0f || pn_hm_env > 1e-4f) {   /* HAMMER: a blow each 1/8, ringing */
                static const float F[3] = {523.0f, 811.0f, 1307.0f};
                uint32_t eighth = (uint32_t)(ph * 2.0f);
                float ring = 0.0f, m = (l + r) * 0.5f, hit;
                if (a > 0.5f && eighth != pn_hm_last && ph * 2.0f - (float)eighth < 0.02f)
                    pn_hm_env = 1.0f, kx_pn_hits++;
                pn_hm_last = eighth;
                hit = pn_hm_env;
                pn_hm_env *= 0.9992f;
                for (k = 0; k < 3u; k++) {          /* (two-pole resonators, struck by the blow) */
                    float w = 6.2831853f * F[k] / (float)FS, c2 = 2.0f * 0.9997f * (1.0f - w * w * 0.5f);
                    float y = c2 * pn_hm_y1[k] - 0.9994f * pn_hm_y2[k] + m * hit * hit * 0.02f;
                    pn_hm_y2[k] = pn_hm_y1[k], pn_hm_y1[k] = y;
                    ring += y;
                }
                {
                    float g = 0.25f + 0.75f * hit * hit;   /* (the mix: struck, then let down between blows) */
                    float hl = vd_tanh((l * g + ring * 0.3f) * 1.6f), hr = vd_tanh((r * g + ring * 0.3f) * 1.6f);
                    l += (hl - l) * a, r += (hr - r) * a;
                }
            }
            if ((a = kx_pn_lv[PN_VOID]) > 0.0f || pn_v_energy > 1e-5f) {   /* VOID: only the dark tail */
                float y = pn_void((l + r) * 0.5f * a);
                pn_v_energy = pn_v_energy * 0.9999f + y * y * 0.0001f;
                l = l * (1.0f - a) + y * 1.25f, r = r * (1.0f - a) + y * 1.25f;
            }
            if ((a = kx_pn_lv[PN_STROBE]) > 0.0f) {        /* STROBE: cut at 1/32 */
                float s32 = ph * 8.0f;
                float g = s32 - (float)(int)s32 < 0.5f ? 1.0f : 0.0f;
                l *= 1.0f - a + a * g, r *= 1.0f - a + a * g;
            }
            o[2u * i] = (int32_t)clamp((int32_t)(l * 32768.0f), -32768, 32767);
            o[2u * i + 1u] = (int32_t)clamp((int32_t)(r * 32768.0f), -32768, 32767);
        }
    }
}


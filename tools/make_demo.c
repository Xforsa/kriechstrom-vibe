/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * The demo song the firmware starts with (kriechstrom/gen/kx_demo.h), made by the KRIECHSTROM core itself: four project
 * slots in E minor at 100 BPM, after the WALL piece of KRIECHSTROM's site (tests/krst_demos.c), and a SONG over them.
 *   A  the theme: a fuzz bass riff, fuzz fifths as guitars, a pad, ROOM KIT's drums
 *   B  the drive: the bass in sixteenths on E, the fifths cut short, ROT KIT's broken beat
 *   C  the break: the pad and the bass held, GATE KIT's gated snare on 3, a rim
 *   D  the weight: the fifths in eighths, SLAB KIT, toms into the next bar
 *   SONG  C, A x2, B x2, C, D x2, A
 * Built and run by tools/make_demo.sh:  make_demo OUT.h */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define KX_REF 1
#include "../kriechstrom/kx_core.h"
#undef __attribute__
#include "../kriechstrom/kx_boot.h"

static uint32_t pidx(uint32_t e, const char *name)
{
    uint32_t i;
    for (i = 0; i < ENGINES[e]->npresets; i++)
        if (str_eq(ENGINES[e]->presets[i].name, name))
            return i;
    fprintf(stderr, "make_demo: no sound %s\n", name);
    exit(1);
}
static void sound(uint32_t k, uint32_t e, const char *name, int level)
{
    track_t *t = &trk[k];
    if (t->eng_req != e)
        set_engine_of(t, e);
    apply_preset_to(t, pidx(e, name));
    t->engine = t->eng_req;
    eng_mem_claim(t);
    t->p[P_LEVEL] = (int16_t)level;
}
static void put(track_t *t, uint32_t i, uint32_t n, const uint8_t *nt, uint32_t time, uint32_t flags)
{
    step_t *s = &t->step[i % NSTEP];
    uint32_t k;
    s->n = (uint8_t)n;
    for (k = 0; k < 4u; k++)
        s->note[k] = k < n ? nt[k] : 0;
    s->time = (uint8_t)time;
    s->flags = (uint8_t)flags;
    s->vel = n ? 100 : 0;
}
static void note(uint32_t k, uint32_t s, uint32_t len, uint32_t n, const uint8_t *nt, uint32_t acc)
{
    uint32_t i;
    for (i = 0; i < len && s + i < 64u; i++)
        put(&trk[k], s + i, n, nt, i ? ST_TIE : ST_NOTE, !i && acc ? SF_ACCENT : 0u);
}
static void note1(uint32_t k, uint32_t s, uint32_t len, uint8_t nt) { note(k, s, len, 1, &nt, 0); }
static void note1a(uint32_t k, uint32_t s, uint32_t len, uint8_t nt) { note(k, s, len, 1, &nt, 1); }
/* the drums (track 4): lanes 1 KICK, 2 SNARE, 4 CLAP, 8 HAT, 16 OPEN HAT, 32 TOM, 64 RIM, 128 BELL */
static void hit(uint32_t s, uint32_t lanes, uint32_t vel)
{
    step_t *st = &trk[3].step[s];
    if (!st->hit)
        put(&trk[3], s, 0, 0, ST_NOTE, 0);
    st->hit |= (uint8_t)lanes;
    st->vel = (uint8_t)(vel > st->vel ? vel : st->vel);
}
static void clear_all(void)
{
    uint32_t k, i;
    for (k = 0; k < NPART; k++) {
        memset(trk[k].step, 0, sizeof trk[k].step);
        for (i = 0; i < NSTEP; i++)
            trk[k].step[i].time = ST_REST;
        trk[k].p[P_SLEN] = 64;
        trk[k].p[P_ITYPE] = 0;
    }
}

static const uint8_t RIFF[16] = {28, 0, 28, 31, 28, 0, 33, 31, 28, 0, 28, 35, 33, 31, 26, 0};   /* E1 .. (eighths) */
static const uint8_t PWR[4][2] = {{52, 59}, {48, 55}, {50, 57}, {47, 54}};   /* E5 C5 D5 B5 (fifths) */
static const uint8_t PAD[4][3] = {{52, 59, 64}, {48, 55, 64}, {50, 57, 62}, {47, 54, 62}};

static void sounds(const char *kit)
{
    sound(0, 7, "FUZZ BASS", 100);
    sound(1, 7, "SQR FUZZ", 78);
    sound(2, 2, "VAULT", 72);
    sound(3, 10, kit, 108);
    clear_all();
}

static void slot_a(void)                          /* the theme */
{
    uint32_t b, s;
    sounds("ROOM KIT");
    for (b = 0; b < 4u; b++) {
        for (s = 0; s < 16u; s += 2u)
            if (RIFF[(b % 2u) * 8u + s / 2u])
                note1(0, b * 16u + s, 2, RIFF[(b % 2u) * 8u + s / 2u]);
        note(1, b * 16u, 8, 2, PWR[b], 0);
        note(1, b * 16u + 8u, 6, 2, PWR[b], 0);
        if (b % 2u == 0u)
            note(2, b * 16u, 32, 3, PAD[b], 0);
        hit(b * 16u, 1, 125);
        hit(b * 16u + 6u, 1, 110);
        hit(b * 16u + 4u, 2, 120);
        hit(b * 16u + 12u, 2, 120);
        for (s = 0; s < 16u; s += 2u)
            hit(b * 16u + s, 8, s % 4u ? 75 : 95);
        if (b == 3u) {
            hit(b * 16u + 13u, 32, 110);
            hit(b * 16u + 14u, 32, 115);
            hit(b * 16u + 15u, 32, 120);
        }
    }
}

static void slot_b(void)                          /* the drive */
{
    static const uint8_t KICK[16] = {1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0};   /* broken, never on every beat */
    uint32_t b, s;
    sounds("ROT KIT");
    for (b = 0; b < 4u; b++) {
        for (s = 0; s < 16u; s++)
            if (s % 4u == 0u)
                note1a(0, b * 16u + s, 1, b == 3u && s >= 8u ? 26 : 28);
            else
                note1(0, b * 16u + s, 1, b == 3u && s >= 8u ? 26 : 28);
        note(1, b * 16u, 3, 2, PWR[b], 1);
        note(1, b * 16u + 6u, 2, 2, PWR[b], 0);
        note(1, b * 16u + 10u, 3, 2, PWR[b], 1);
        for (s = 0; s < 16u; s++) {
            if (KICK[s])
                hit(b * 16u + s, 1, s == 0u ? 127 : 112);
            if (s == 4u || s == 12u)
                hit(b * 16u + s, 2, 122);
            if (s == 7u || s == 15u)
                hit(b * 16u + s, 2, 60);      /* (ghosts) */
            if (s % 2u)
                hit(b * 16u + s, 8, 70);
        }
    }
}

static void slot_c(void)                          /* the break */
{
    uint32_t b;
    sounds("GATE KIT");
    trk[1].p[P_LEVEL] = 0;                        /* (the fifths rest: the pad and the bass carry it) */
    for (b = 0; b < 4u; b++) {
        note1(0, b * 16u, 12, RIFF[(b % 2u) * 8u]);
        note(2, b * 16u, 16, 3, PAD[b], 0);
        hit(b * 16u, 1, 118);
        hit(b * 16u + 8u, 2, 124);                /* (the gated snare on 3, half time) */
        hit(b * 16u + 14u, 64, 80);
        if (b == 3u) {
            hit(b * 16u + 10u, 1, 110);
            hit(b * 16u + 12u, 2, 100);
            hit(b * 16u + 14u, 2, 110);
            hit(b * 16u + 15u, 2, 120);
        }
    }
}

static void slot_d(void)                          /* the weight */
{
    uint32_t b, s;
    sounds("SLAB KIT");
    for (b = 0; b < 4u; b++) {
        for (s = 0; s < 16u; s += 2u)
            if (RIFF[(b % 2u) * 8u + s / 2u])
                note1(0, b * 16u + s, 2, RIFF[(b % 2u) * 8u + s / 2u]);
        for (s = 0; s < 16u; s += 2u)
            note(1, b * 16u + s, 1, 2, PWR[b], s % 4u == 0u);
        note(2, b * 16u, 16, 3, PAD[b], 0);
        hit(b * 16u, 1, 127);
        hit(b * 16u + 3u, 1, 105);
        hit(b * 16u + 8u, 1, 120);
        hit(b * 16u + 4u, 2, 124);
        hit(b * 16u + 12u, 2, 124);
        for (s = 0; s < 16u; s += 4u)
            hit(b * 16u + s + 2u, 16, 80);
        if (b % 2u) {
            hit(b * 16u + 12u, 32, 110);
            hit(b * 16u + 13u, 32, 115);
            hit(b * 16u + 14u, 32, 120);
            hit(b * 16u + 15u, 32, 125);
        }
    }
}

static void dump(FILE *f, const char *name, const void *p, uint32_t n)
{
    const uint8_t *b = p;
    uint32_t i;
    fprintf(f, "static const uint8_t %s[%u] = {", name, (unsigned)n);
    for (i = 0; i < n; i++)
        fprintf(f, "%s%u,", i % 32u ? "" : "\n    ", b[i]);
    fprintf(f, "\n};\n");
}

int main(int argc, char **argv)
{
    static void (*const SLOT[4])(void) = {slot_a, slot_b, slot_c, slot_d};
    static const char *const NAME[4] = {"DEMO THEME", "DEMO DRIVE", "DEMO BREAK", "DEMO WEIGHT"};
    static const uint8_t ORDER[][2] = {{2, 1}, {0, 2}, {1, 2}, {2, 1}, {3, 2}, {0, 1}};   /* slot, repeats */
    static project_store_t out;
    FILE *f = fopen(argc > 1 ? argv[1] : "kx_demo.h", "w");
    uint32_t i, k;
    if (!f)
        return 1;
    kx_power_on();
    chain_defaults(&chain_config);
    for (i = 0; i < NELEM(ORDER); i++) {
        for (k = 0; k < NTRK; k++)
            chain_config.row[i].slot[k] = ORDER[i][0];
        chain_config.row[i].repeat = ORDER[i][1];
    }
    chain_config.count = (uint8_t)NELEM(ORDER);
    for (i = 0; i < 4u; i++) {
        SLOT[i]();
        song.g[G_BPM] = 100;
        song.g[G_WALL] = 40;                      /* (a little wall, a little hiss: the mix as KRIECHSTROM's) */
        song.g[G_HISS] = 18;
        song.g[G_DMG] = 0;
        song.sel = 0;
        if (project_save_as(i, NAME[i])) {
            fprintf(stderr, "make_demo: slot %u not saved\n", (unsigned)i);
            return 1;
        }
    }
    project_load(0);                              /* the music at the start: the theme */
    project_capture(&proj_scratch);
    if (!proj_pack(&out, &proj_scratch))
        return 1;
    fprintf(f, "/* generated by tools/make_demo.sh (tools/make_demo.c): the demo song, four project slots and the music\n"
               " * the firmware starts with, in KRIECHSTROM's project format */\n#pragma once\n");
    for (i = 0; i < 4u; i++) {
        char nm[16];
        snprintf(nm, sizeof nm, "KX_DEMO_%c", 'A' + (int)i);
        dump(f, nm, &proj_slot[i], sizeof(project_store_t));
    }
    dump(f, "KX_DEMO_NOW", &out, sizeof out);
    fclose(f);
    printf("make_demo: 4 slots, a song of %u sections\n", (unsigned)chain_config.count);
    return 0;
}

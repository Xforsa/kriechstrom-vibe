/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * The reference: the KRIECHSTROM core built for this computer, as the FM-1's host tests build it, powered on as the
 * firmware does (kriechstrom/kx_boot.h), PLAY pressed, N seconds of fx.c mix_block at 44.1 kHz. Its output (Q15,
 * interleaved int32) must be the firmware's at 44.1 kHz, bit for bit (tests/run.sh compares).
 *   core_ref SECONDS OUT.raw */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define KX_REF 1
#include "../kriechstrom/kx_core.h"
#undef __attribute__
#include "../kriechstrom/kx_boot.h"

int main(int argc, char **argv)
{
    uint32_t secs = argc > 1 ? (uint32_t)atoi(argv[1]) : 10u, b, nb;
    FILE *f = fopen(argc > 2 ? argv[2] : "ref.raw", "wb");
    int32_t o[2 * CTL];
    if (!f)
        return 1;
    kx_power_on();
    transport_req = 1;
    nb = secs * FS / CTL;
    for (b = 0; b < nb; b++) {
        mix_block(o, CTL);
        fwrite(o, sizeof o, 1, f);
    }
    fclose(f);
    return 0;
}

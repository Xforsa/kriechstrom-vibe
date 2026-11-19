/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM. One translation unit, as on the FM-1: the KRIECHSTROM sources (built on Felucca by Leo
 * Kuroshita, Hügelton Instruments, with parts of Glitchbox by erikdevos) from core/kriechstrom, then the VIBE side. */
#include <stdint.h>
#include <stddef.h>
#include "vibe.h"
#include "vibe_gfx.h"
#include "vibe_dsp.h"

#define memset felucca_memset
#define memcpy felucca_memcpy
#define memcmp felucca_memcmp
#include "felucca_tables.h"
#include "../core/kriechstrom/firmware/bsp/chip/libc.c"
#undef memset
#undef memcpy
#undef memcmp
#include "kx_hal.h"
#include "../core/kriechstrom/firmware/src/core.h"
#include "../core/kriechstrom/firmware/src/engines.c"
#include "../core/kriechstrom/firmware/src/params.c"
#include "../core/kriechstrom/firmware/src/mod.c"
#include "../core/kriechstrom/firmware/src/voice.c"
#include "../core/kriechstrom/firmware/src/slicer.c"
#include "../core/kriechstrom/firmware/src/fx.c"
#include "../core/kriechstrom/firmware/bsp/chip/usb.c"
#include "../core/kriechstrom/firmware/src/midi_uart.c"
#include "../core/kriechstrom/firmware/src/song_chain.c"
#include "../core/kriechstrom/firmware/src/seq.c"
#include "../core/kriechstrom/firmware/src/gfx.c"
#include "../core/kriechstrom/firmware/src/panel.c"
#include "../core/kriechstrom/firmware/src/ui.c"
#include "../core/kriechstrom/firmware/src/menu_items.c"
#include "../core/kriechstrom/firmware/src/icons.c"
#include "../core/kriechstrom/firmware/src/ui_graph.c"
#include "../core/kriechstrom/firmware/src/ui_draw.c"
#include "../core/kriechstrom/firmware/src/ui_menu.c"
#include "../core/kriechstrom/firmware/src/ui_input.c"
#include "../core/kriechstrom/firmware/src/ui_layer.c"
#include "../core/kriechstrom/firmware/src/upreset.c"
#include "../core/kriechstrom/firmware/src/project.c"
#undef __attribute__

/* ---- the VIBE side -------------------------------------------------------------------------------------------- */
#include "kx_audio.h"
#include "kx_input.h"
#include "kx_ctl.h"
#include "kx_store.h"
#include "kx_daw.h"
#include "kx_ui.h"
#include "kx_main.h"

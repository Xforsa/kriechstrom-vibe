/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Grigorii Bezborodov
 * KRIECHSTROM for VIBE FM. One translation unit, as on the FM-1: the KRIECHSTROM sources (built on Felucca by Leo
 * Kuroshita, Hügelton Instruments, with parts of Glitchbox by erikdevos) from core/kriechstrom, then the VIBE side. */
#include <stdint.h>
#include <stddef.h>
#include "vibe.h"
#include "vibe_gfx.h"
#include "vibe_dsp.h"

#include "kx_core.h"
#undef __attribute__

/* ---- the VIBE side -------------------------------------------------------------------------------------------- */
#include "kx_outs.h"
#include "kx_punch.h"
#include "kx_audio.h"
#include "kx_pedals.h"
#include "kx_input.h"
#include "kx_spine.h"
#include "kx_midi.h"
#include "kx_ctl.h"
#include "kx_boot.h"
#include "kx_sampler.h"
#include "kx_store.h"
#include "kx_proj.h"
#include "kx_daw.h"
#include "kx_ui.h"
#include "kx_creep.h"
#include "kx_wear.h"
#include "kx_live.h"
#include "kx_pics.h"
#include "kx_te.h"
#include "kx_fxart.h"
#include "kx_draw.h"
#include "kx_stage.h"
#include "kx_main.h"

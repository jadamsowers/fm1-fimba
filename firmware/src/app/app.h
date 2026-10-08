/* SPDX-License-Identifier: GPL-3.0-only */
/* FiMba-1 app: the project (everything that is saved) and the app's entry points. Included by the unity
 * build after gfx.c (drawing helpers in scope) and by the host simulator. (FoMni's app.h, reshaped.) */
#pragma once
#include <stdint.h>
#include "plat.h"
#include "../dsp/kalimba.h"
#include "../dsp/fastmath.h"                /* the UI draws with it (tine lengths) */

#ifndef OM_VERSION                       /* a release build passes its own (tools/build.py --release) */
#define OM_VERSION "DEV"
#endif
#define PROJ_MAGIC 0x41424C4Bu           /* "KLBA": never a FoMni project */
/* The format counts the settings a project holds: a newer version reads an older project's values and
 * gives the settings it lacks their defaults (its spare bytes, zeros, are never taken for settings).
 * Add a setting at the end of P_*, bump the format, and add its count here. */
#define PROJ_FORMAT 3u                    /* 3: Worn; 2: Color and Pattern; 1: the first */
#define PROJ_NPAR_OF(f) ((f) == 1u ? P_TRANSPOSE + 1 : (f) == 2u ? P_TUNING + 1 : P_NPARAMS)

typedef struct {
    uint32_t magic, format;
    int16_t par[P_NPARAMS];              /* kalimba.h P_* */
    uint8_t rsv[32];                     /* room to grow: an older, shorter project loads (zeros here) */
} project_t;

extern project_t proj;

void project_defaults(void);
int project_load(void);                  /* 0 = loaded; else defaults are in place */
int project_save(void);                  /* 0 = saved and verified */
void project_apply(void);                /* every value to the engine (boot, load) */

void ui_init(void);
void ui_frame(void);
void ui_input_only(void);
int ui_dirty(void);
void ui_say(const char *a, const char *b);

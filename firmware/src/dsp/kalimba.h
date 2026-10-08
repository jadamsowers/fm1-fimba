/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument: a kalimba (thumb piano) by modal synthesis. Four parts:
 *
 *   TINES   KM_NVOICE voices, one per sounding tine. A tine is a clamped-free bar: each voice is a
 *           bank of KM_NMODE two-pole resonators (the bending modes, plus a twin of the first a
 *           hair away, the beating of a real tine that bends in two planes). A pluck is a
 *           half-sine force pulse: a soft thumb is a long pulse, which cannot excite the high
 *           modes; a nail is a short one (and a click), which can. The material sets the modes'
 *           ratios, levels and decays (km_material()).
 *   BODY    the instrument the tines sit on: none, a board, a box (its sound hole a Helmholtz
 *           resonator, which a finger over the hole lowers and darkens: the wah), a gourd. Mbira
 *           buzzers (bottle caps, shells) rattle when the body moves past a threshold.
 *   FX      a chorus and a granular cloud on the dry sound (record, freeze, pitch), a ping-pong
 *           delay, a plate reverb (Dattorro's, from FoMni); then on everything a filter (low-pass
 *           one way, high-pass the other) and a tape (wow, flutter, saturation; lo-fi: dark, hiss).
 *   PATTERN mbira-style patterns over a chord: bass on the left, treble on the right, on a cycle of
 *           12 pulses (3 a beat), at a tempo or following MIDI clock.
 *   MUSIC   scales, key layouts, the black keys' chords and just intonation (km_white_cents and
 *           friends): pure functions, used by the UI and by the tests.
 *
 * Threads: km_render() runs in the audio interrupt. Everything else posts commands, which the
 * render drains at the start of a block; the state marked (UI) is written by the render and only
 * read elsewhere. Float only, no libm: fastmath.h. */
#pragma once
#include <stdint.h>

#define KM_SR 44100.0f
#define KM_NVOICE 16                   /* tines sounding at once (12 measured 48% at worst on the FM-1) */
#define KM_NMODE 5                     /* resonators per tine: the first mode, its twin, three more */
#define KM_NWHITE 16                   /* the white keys: one tine each */
#define KM_NBLACK 11

/* parameters: value ranges in km_param_info(). Some are the UI's (layout, MIDI): kept here so one
 * table names and bounds every value that is saved. */
enum {
    P_MATERIAL, P_HARD, P_DECAY, P_TONE,             /* Tine page */
    P_BODY, P_BUZZ, P_WAH, P_WAHRATE,                /* Body page */
    P_REVERB, P_SIZE, P_DELAY, P_TIME,               /* Space page */
    P_GRAIN, P_GSIZE, P_GDENS, P_GPITCH,             /* Grain page */
    P_LAYOUT, P_SCALE, P_KEY, P_BLACK,               /* Keys page */
    P_FEEDBACK, P_GSPRAY, P_STRUM, P_RELEASE,        /* More page */
    P_TUNE, P_MIDICH, P_MIDIOUT, P_WIDTH,            /* Setup page */
    P_OCTAVE,                                        /* OCT- / OCT+ */
    P_TRANSPOSE,                                     /* the Keyboard layout's transpose (Keys page) */
    P_TAPE, P_LOFI, P_CHORUS, P_FILTER,              /* Color page */
    P_PATTERN, P_PTEMPO, P_GLIDE, P_TUNING,          /* Pattern page: Pattern, Tempo; Character page: Glide, Tuning */
    P_WORN,                                          /* Character page */
    P_NPARAMS
};
typedef struct {
    const char *name;
    int16_t lo, hi, def;
} km_param_t;
const km_param_t *km_param_info(int i);
void km_param_text(int i, int v, char *buf);   /* the value as the screen shows it (12 bytes) */

enum { MAT_STEEL, MAT_BRASS, MAT_BRONZE, MAT_ALU, MAT_BAMBOO, MAT_GLASS, MAT_N };
enum { BODY_NONE, BODY_BOARD, BODY_BOX, BODY_GOURD, BODY_N };
enum { LAY_TINE, LAY_KEYBOARD, LAY_MIRROR, LAY_N };   /* Mirror: Tine right for left (last: saves keep their values) */
enum { BLK_CHORDS, BLK_SHARPS, BLK_PERFORM, BLK_N };
enum { PAT_THUMBS, PAT_CASCADE, PAT_HEMIOLA, PAT_INTERLOCK, PAT_N };
enum { TUNE_EQUAL, TUNE_JUST, TUNE_N };
enum { GP_DOWN12, GP_DOWN7, GP_UNISON, GP_UP7, GP_UP12, GP_UP19, GP_SHIMMER, GP_REVERSE, GP_N };
#define KM_NSCALE 12
extern const char *const KM_NOTE_NAME[12];

/* a tine's material: its modes (ratio to the first, level), the first mode's T60 at C4 (shorter up
 * the keyboard by (f / C4)^-pitch_k), how much faster the upper modes die (loss), the twin's
 * detuning (ratio) and level, and the nail click's level */
typedef struct {
    const char *name;
    float ratio[KM_NMODE - 1], amp[KM_NMODE - 1];
    float t60, loss, pitch_k, beat, beat_amp, click;
} km_material_t;
const km_material_t *km_material(int i);

/* ---- music: everything in cents over MIDI note 0 (6000 = middle C) */
int km_scale_len(int scale);
int km_degree_cents(int scale, int degree);        /* the degree's cents over the tonic (any degree, < 0 too) */
/* Tine (or Mirror) layout: the tine a white key plays (scale, key 0..11, octave; w 0..15 left to right) */
int km_white_cents(int layout, int scale, int key, int octave, int w);
/* Keyboard layout: the note of key k (0..26, F3..G5, white and black as printed), transposed */
int km_keyboard_cents(int transpose, int octave, int k);
/* the black key k's chord (BLK_CHORDS): up to 4 notes, returns how many */
int km_chord_cents(int scale, int key, int octave, int k, int out[4]);
const char *km_chord_name(int scale, int key, int k, char *buf);   /* "C", "Dm", "G7" (8 bytes) */
/* where on the instrument a tine sits: -1 (left) .. +1 (right), for its pan */
float km_white_pos(int w);
/* just intonation: a note on the 12-step grid, retuned to the pure ratio of its interval over the
 * tonic (5-limit: 16/15 9/8 6/5 5/4 4/3 45/32 3/2 8/5 5/3 9/5 15/8); off the grid (Mbira), unchanged */
int km_just_cents(int cents, int tonic_cents);
int km_tonic_cents(int key, int octave);            /* the Tine layout's tonic (its lowest tine) */

/* ---- commands (main loop -> render). The main loop is the only producer. */
void km_init(void);
void km_set(int param, int value);
/* pluck the tine at `cents` (0..12700), velocity 1..127, pan -100..100, delay_ms later (0..2000) */
void km_pluck(int cents, int vel, int pan, int delay_ms);
void km_damp(int cents);                           /* a finger on that tine (a key let go, Release Damp) */
void km_damp_all(void);                            /* a palm over all the tines */
void km_sustain(int on);                           /* the pedal (CC64): km_damp waits for it to lift */
enum { HOLE_MIDI, HOLE_PRESS, HOLE_KEY, HOLE_N };
void km_hole(int src, int amount);                 /* a finger over the sound hole, 0..127, per source */
void km_bend(int v);                               /* pitch bend, -8192..8191: +-2 semitones */
void km_freeze(int on);                            /* the grain buffer stops recording */
/* the pattern: on / off, and the chord it plays over (up to KM_NPOOL notes, any order) */
#define KM_NPOOL 6
void km_pattern(int on);
void km_pool(const int *cents, int n);
/* MIDI clock in: tick (F8), start (FA), continue (FB), stop (FC). While ticks come (the last within
 * ~0.5 s) the pattern follows them, 8 a pulse; otherwise its own Tempo */
enum { KM_CLK_TICK, KM_CLK_START, KM_CLK_CONTINUE, KM_CLK_STOP };
void km_clock(int msg);
void km_panic(void);                               /* everything quiet, now */

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void km_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);

/* state for the screen (UI) */
extern volatile float km_voice_level[KM_NVOICE];   /* each voice's level, 0..~1 */
extern volatile int16_t km_voice_cents[KM_NVOICE]; /* the tine it plays, -1 none */
extern volatile uint8_t km_frozen;
extern volatile uint8_t km_grains_on;              /* grains sounding now */
extern volatile float km_hole_open;                /* 1 open .. 0.1 covered */
extern volatile float km_buzz_level;
extern volatile uint32_t km_dropped;               /* commands lost to a full queue (never, we hope) */
extern volatile float km_out_level;                /* the output's peak over the last block (reverb and echo tails too) */
extern volatile uint8_t km_pat_on, km_pat_pulse;   /* the pattern running, its pulse (0..11) */
extern volatile uint8_t km_ext;                    /* following MIDI clock */

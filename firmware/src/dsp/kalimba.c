/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument (kalimba.h). One compilation unit, -O2. The plate reverb is FoMni's (Dattorro's
 * figure-eight), the command queue and the float-only maths follow FoMni / X0X. */
#include "kalimba.h"
#include "fastmath.h"

#ifdef OM_HOST
#define KM_POOL
#else
#define KM_POOL __attribute__((section(".pool")))
#endif
#define BARRIER() __asm__ volatile("" ::: "memory")

const char *const KM_NOTE_NAME[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};

static const km_param_t PARAMS[P_NPARAMS] = {
    {"Material", 0, MAT_N - 1, MAT_STEEL}, {"Hardness", 0, 100, 45}, {"Decay", 0, 100, 50}, {"Tone", 0, 100, 55},
    {"Body", 0, BODY_N - 1, BODY_BOX}, {"Buzz", 0, 100, 0}, {"Wah", 0, 100, 0}, {"Wah rate", 0, 100, 0},
    {"Reverb", 0, 100, 30}, {"Size", 0, 100, 60}, {"Delay", 0, 100, 0}, {"Time", 40, 740, 330},
    {"Grains", 0, 100, 0}, {"Grain ms", 20, 500, 140}, {"Density", 1, 60, 14}, {"Pitch", 0, GP_N - 1, GP_UP12},
    {"Layout", 0, LAY_N - 1, LAY_TINE}, {"Scale", 0, KM_NSCALE - 1, 0}, {"Key", 0, 11, 0}, {"Black", 0, BLK_N - 1, BLK_CHORDS},
    {"Feedback", 0, 95, 45}, {"Spray", 0, 100, 30}, {"Strum ms", 0, 120, 30}, {"Release", 0, 1, 0},
    {"Tune", -50, 50, 0}, {"MIDI ch", 0, 16, 0}, {"MIDI out", 0, 1, 1}, {"Width", 0, 100, 70},
    {"Octave", -2, 2, 0}, {"Transpose", -12, 12, 0},
    {"Tape", 0, 100, 0}, {"Lo-fi", 0, 100, 0}, {"Chorus", 0, 100, 0}, {"Filter", -100, 100, 0},
    {"Pattern", 0, PAT_N - 1, PAT_THUMBS}, {"Tempo", 40, 200, 96}, {"Glide", 0, 100, 30}, {"Tuning", 0, TUNE_N - 1, TUNE_EQUAL},
    {"Worn", 0, 100, 0},
};
const km_param_t *km_param_info(int i) { return (i >= 0 && i < P_NPARAMS) ? &PARAMS[i] : &PARAMS[0]; }

/* ---------------------------------------------------------- materials --- */
/* Ratios: a uniform clamped-free bar has its bending modes at 1, 6.27, 17.55, 34.39 (Euler-Bernoulli,
 * (beta_n / beta_1)^2); a real tine is thinner at the tip and bent up, which pulls them down a little,
 * more so in soft brass. Bronze and glass are played as bars free at both ends (1, 2.76, 5.40, 8.93),
 * a bell's brighter spectrum. Bamboo tines are short-lived and woody (high internal loss). */
static const km_material_t MATS[MAT_N] = {
    {"Steel", {1.0028f, 5.90f, 16.2f, 31.5f}, {0.35f, 0.30f, 0.10f, 0.040f}, 5.0f, 0.10f, 0.55f, 0.0028f, 0.35f, 0.50f},
    {"Brass", {1.0020f, 5.60f, 15.3f, 29.8f}, {0.30f, 0.22f, 0.06f, 0.020f}, 3.6f, 0.20f, 0.50f, 0.0020f, 0.30f, 0.30f},
    {"Bronze", {1.0015f, 2.76f, 5.40f, 8.93f}, {0.40f, 0.45f, 0.25f, 0.120f}, 4.5f, 0.06f, 0.60f, 0.0015f, 0.40f, 0.60f},
    {"Alumin.", {1.0040f, 6.27f, 17.55f, 34.4f}, {0.30f, 0.50f, 0.22f, 0.100f}, 2.6f, 0.12f, 0.50f, 0.0040f, 0.30f, 0.70f},
    {"Bamboo", {1.0000f, 4.60f, 11.8f, 22.3f}, {0.00f, 0.30f, 0.12f, 0.050f}, 0.55f, 0.90f, 0.35f, 0.0f, 0.0f, 0.25f},
    {"Glass", {1.0010f, 3.01f, 6.24f, 10.4f}, {0.20f, 0.55f, 0.35f, 0.200f}, 2.0f, 0.01f, 0.70f, 0.0010f, 0.20f, 0.90f},
};
const km_material_t *km_material(int i) { return &MATS[(unsigned)i < MAT_N ? i : 0]; }

/* --------------------------------------------------------------- music --- */
typedef struct {
    const char *name;
    uint8_t n;
    int16_t c[12];
} scale_t;
/* Mbira: an approximation of a Shona Nyamaropa tuning (no two mbira are tuned alike; this is close to
 * an equal seven-step octave, with a near-pure fifth) */
static const scale_t SCALES[KM_NSCALE] = {
    {"Major", 7, {0, 200, 400, 500, 700, 900, 1100}},
    {"Minor", 7, {0, 200, 300, 500, 700, 800, 1000}},
    {"Penta", 5, {0, 200, 400, 700, 900}},
    {"Penta m", 5, {0, 300, 500, 700, 1000}},
    {"Dorian", 7, {0, 200, 300, 500, 700, 900, 1000}},
    {"Mixolyd.", 7, {0, 200, 400, 500, 700, 900, 1000}},
    {"Lydian", 7, {0, 200, 400, 600, 700, 900, 1100}},
    {"Harm. m", 7, {0, 200, 300, 500, 700, 800, 1100}},
    {"Hirajoshi", 5, {0, 200, 300, 700, 800}},
    {"Blues", 6, {0, 300, 500, 600, 700, 1000}},
    {"Mbira", 7, {0, 165, 340, 505, 685, 860, 1030}},
    {"Chromatic", 12, {0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100}},
};

int km_scale_len(int s) { return SCALES[(unsigned)s < KM_NSCALE ? s : 0].n; }

int km_degree_cents(int s, int d)
{
    const scale_t *sc = &SCALES[(unsigned)s < KM_NSCALE ? s : 0];
    int n = sc->n, o = d >= 0 ? d / n : -((-d + n - 1) / n);
    return sc->c[d - o * n] + 1200 * o;
}

/* the tonic, the lowest tine of the Tine layout: within F#3..F4 (a kalimba in C starts on C4, as the
 * 17-key one does; one in B on B3, not B4), then the octave */
static int tonic(int key, int octave) { return 6000 + 100 * (key >= 6 ? key - 12 : key) + 1200 * octave; }

/* Tine: the kalimba's own layout, the longest (lowest) tine in the middle and the scale alternating
 * outward, left right left right, so each thumb has every other note and thirds lie side by side:
 *   w:      0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
 *   degree 14  12  10   8   6   4   2   0   1   3   5   7   9  11  13  15 */
static int white_degree(int w) { return w <= 7 ? 2 * (7 - w) : 2 * (w - 8) + 1; }

int km_tonic_cents(int key, int octave) { return tonic(key, octave); }

int km_just_cents(int cents, int tonic_cents)
{
    static const int16_t JUST[12] = {0, 112, 204, 316, 386, 498, 590, 702, 814, 884, 1018, 1088};
    int d = cents - tonic_cents, pc = (d % 1200 + 1200) % 1200;
    if (pc % 100)
        return cents;                             /* off the equal grid (Mbira): its own tuning */
    return cents - pc + JUST[pc / 100];
}

int km_white_cents(int layout, int scale, int key, int octave, int w)
{
    (void)layout;                                 /* the Keyboard layout plays km_keyboard_cents */
    return tonic(key, octave) + km_degree_cents(scale, white_degree(w & 15));
}

/* Keyboard: the FM-1's keys as printed, F3 (MIDI 53) on the lowest, chromatic up to G5 */
int km_keyboard_cents(int transpose, int octave, int k) { return 5300 + 100 * (k + transpose) + 1200 * octave; }

float km_white_pos(int w) { return ((float)(w & 15) - 7.5f) * (1.0f / 7.5f); }

/* the black keys' chords: the triads on the scale's first seven degrees (stacked in thirds of the
 * scale: every other note), then four sevenths: I7 IV7 V7 vi7 */
static const uint8_t CH_DEG[KM_NBLACK] = {0, 1, 2, 3, 4, 5, 6, 0, 3, 4, 5};

int km_chord_cents(int scale, int key, int octave, int k, int out[4])
{
    int d, i, n = k >= 7 ? 4 : 3, len = km_scale_len(scale), base = tonic(key, octave);
    if ((unsigned)k >= KM_NBLACK)
        return 0;
    d = CH_DEG[k] % len;
    for (i = 0; i < n; i++)
        out[i] = base + km_degree_cents(scale, d + 2 * i);
    if (n == 3)                                   /* a triad gets its root an octave up: a fuller roll */
        out[n++] = base + km_degree_cents(scale, d) + 1200;
    return n;
}

const char *km_chord_name(int scale, int key, int k, char *b)
{
    int c[4], n = km_chord_cents(scale, key, 0, k, c), root = (c[0] + 50) / 100, third = c[1] - c[0],
        fifth = c[2] - c[0], i = 0;
    const char *nm = KM_NOTE_NAME[root % 12];
    while (*nm)
        b[i++] = *nm++;
    if (third < 350)
        b[i++] = 'm';
    if (fifth < 650 && third < 350) {             /* diminished */
        b[i - 1] = 'd';
        b[i++] = 'i';
        b[i++] = 'm';
    } else if (fifth > 750) {
        b[i++] = '+';
    }
    if (n == 4 && k >= 7) {                       /* the seventh: major (M7) or minor (7, m7) */
        if (c[3] - c[0] > 1050)
            b[i++] = 'M';
        b[i++] = '7';
    }
    b[i] = 0;
    return b;
}

/* ------------------------------------------------------- param text --- */
static void itoa_s(int v, char *b)
{
    char t[8];
    int n = 0, neg = v < 0;
    if (neg)
        v = -v;
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && n < 6);
    if (neg)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static void copy_s(char *b, const char *s)
{
    int k = 0;
    while (s[k] && k < 11) {
        b[k] = s[k];
        k++;
    }
    b[k] = 0;
}

void km_param_text(int i, int v, char *b)
{
    static const char *const ONOFF[2] = {"Off", "On"};
    static const char *const BODY[BODY_N] = {"None", "Board", "Box", "Gourd"};
    static const char *const LAY[LAY_N] = {"Tine", "Keyboard"};
    static const char *const BLK[BLK_N] = {"Chords", "Sharps", "Perform"};
    static const char *const GP[GP_N] = {"-12", "-7", "0", "+7", "+12", "+19", "Shimmer", "Reverse"};
    static const char *const REL[2] = {"Ring", "Damp"};
    static const char *const PAT[PAT_N] = {"Thumbs", "Cascade", "3 over 2", "Interlock"};
    static const char *const TUN[TUNE_N] = {"Equal", "Just"};
    switch (i) {
    case P_MATERIAL: copy_s(b, km_material(v)->name); return;
    case P_BODY: copy_s(b, BODY[(unsigned)v < BODY_N ? v : 0]); return;
    case P_LAYOUT: copy_s(b, LAY[(unsigned)v < LAY_N ? v : 0]); return;
    case P_BLACK: copy_s(b, BLK[(unsigned)v < BLK_N ? v : 0]); return;
    case P_GPITCH: copy_s(b, GP[(unsigned)v < GP_N ? v : 0]); return;
    case P_SCALE: copy_s(b, SCALES[(unsigned)v < KM_NSCALE ? v : 0].name); return;
    case P_KEY: copy_s(b, KM_NOTE_NAME[(unsigned)v % 12u]); return;
    case P_RELEASE: copy_s(b, REL[v ? 1 : 0]); return;
    case P_PATTERN: copy_s(b, PAT[(unsigned)v < PAT_N ? v : 0]); return;
    case P_TUNING: copy_s(b, TUN[v ? 1 : 0]); return;
    case P_FILTER:                                /* one knob: left a low-pass, right a high-pass */
        if (!v) {
            copy_s(b, "Off");
            return;
        }
        *b++ = v < 0 ? 'L' : 'H';
        *b++ = 'P';
        *b++ = ' ';
        v = v < 0 ? -v : v;
        break;
    case P_MIDIOUT: copy_s(b, ONOFF[v ? 1 : 0]); return;
    case P_MIDICH:
        if (!v) {
            copy_s(b, "Omni");
            return;
        }
        break;
    case P_WAHRATE:
        if (!v) {
            copy_s(b, "Hand");                    /* no LFO: the knob, the mod wheel, a black key */
            return;
        }
        break;
    case P_TUNE: case P_OCTAVE: case P_TRANSPOSE:
        if (v > 0)
            *b++ = '+';
        break;
    default: break;
    }
    itoa_s(v, b);
}

/* ------------------------------------------------------------ commands --- */
enum { C_SET, C_PLUCK, C_DAMP, C_DAMPALL, C_SUSTAIN, C_HOLE, C_BEND, C_FREEZE, C_PANIC, C_PATTERN, C_POOLCLR, C_POOLADD,
       C_CLOCK };
typedef struct {
    uint8_t c;
    int8_t pan;
    int16_t a, b, d;
} cmd_t;
#define QN 256u                                   /* a MIDI burst, a chord roll: room for all of it */
static cmd_t q[QN];
static volatile uint32_t q_w, q_r;
volatile uint32_t km_dropped;
static void post(int c, int a, int b, int pan, int d)
{
    uint32_t w = q_w;
    cmd_t *e;
    if (w - q_r >= QN) {
        km_dropped++;
        return;
    }
    e = &q[w % QN];
    e->c = (uint8_t)c;
    e->a = (int16_t)a;
    e->b = (int16_t)b;
    e->pan = (int8_t)pan;
    e->d = (int16_t)d;
    BARRIER();
    q_w = w + 1;
}
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
void km_set(int p, int v) { post(C_SET, p, v, 0, 0); }
void km_pluck(int cents, int vel, int pan, int delay_ms)
{
    post(C_PLUCK, clampi(cents, 0, 12700), clampi(vel, 1, 127), clampi(pan, -100, 100), clampi(delay_ms, 0, 2000));
}
void km_damp(int cents) { post(C_DAMP, cents, 0, 0, 0); }
void km_damp_all(void) { post(C_DAMPALL, 0, 0, 0, 0); }
void km_sustain(int on) { post(C_SUSTAIN, on, 0, 0, 0); }
void km_hole(int src, int amount) { post(C_HOLE, src, clampi(amount, 0, 127), 0, 0); }
void km_bend(int v) { post(C_BEND, clampi(v, -8192, 8191), 0, 0, 0); }
void km_freeze(int on) { post(C_FREEZE, on, 0, 0, 0); }
void km_panic(void) { post(C_PANIC, 0, 0, 0, 0); }
void km_pattern(int on) { post(C_PATTERN, on, 0, 0, 0); }
void km_pool(const int *cents, int n)
{
    int i;
    post(C_POOLCLR, 0, 0, 0, 0);
    for (i = 0; i < n && i < KM_NPOOL; i++)
        post(C_POOLADD, clampi(cents[i], 0, 12700), 0, 0, 0);
}
void km_clock(int msg) { post(C_CLOCK, msg, 0, 0, 0); }

/* -------------------------------------------------------------- state --- */
volatile float km_voice_level[KM_NVOICE];
volatile int16_t km_voice_cents[KM_NVOICE];
volatile uint8_t km_frozen, km_grains_on;
volatile float km_hole_open = 1.0f, km_buzz_level, km_out_level;

static int16_t par[P_NPARAMS];
static float tunefac = 1.0f, bendfac = 1.0f, width = 0.7f;
static uint32_t rng = 0x12345678u;
static inline uint32_t rnd(void)
{
    uint32_t x = rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return rng = x;
}
static inline float rndf(void) { return (float)(rnd() >> 8) * (1.0f / 16777216.0f); }   /* 0..1 */
static inline float rnds(void) { return rndf() * 2.0f - 1.0f; }                          /* -1..1 */

/* ---------------------------------------------------------------- tines --- */
typedef struct {
    float b1[KM_NMODE], b2[KM_NMODE], g[KM_NMODE];    /* y = b1 y1 + b2 y2 + g x */
    float y1[KM_NMODE], y2[KM_NMODE];
    float pl, pr, level, damp;                        /* damp: < 1 while a finger is on it (folded into b1, b2) */
    float ex_amp, ex_ph, ex_inc;                      /* the pluck: a half-sine pulse, ex_ph 0..0.5 turns */
    float click, click_dec;                           /* the nail: a burst of noise */
    int16_t cents;
    float fade, fade_step;                            /* a stolen voice, fading out (fade slots only) */
    float w0[KM_NMODE], rr[KM_NMODE], amp[KM_NMODE];  /* each mode at rest: frequency (rad), radius, level */
    float c0[KM_NMODE], s0[KM_NMODE];                 /* cos and sin of w0: the glide moves from them */
    float cw[KM_NMODE], sw[KM_NMODE];                 /* cos and sin of the frequency now (the silence test) */
    float glide, glide_pend, glide_rise;              /* cents sharp now (a hard pluck stretches the tine); to come,
                                                       * and how much more each chunk (a ringing tine eases into it) */
    uint8_t on, nm, quiet, released, contact, rise_n;
    uint8_t live;                                     /* the modes still sounding (bit k): the rest are skipped */         /* contact: chunks left of a thumb on it (re-pluck) */
} voice_t;
/* Two spare slots where a stolen voice fades out over a few milliseconds while its slot takes the new
 * tine: cutting it dead was a pop (as loud as the tine was) */
#define KM_NFADE 2
#define FADE_S (0.006f * KM_SR)
static voice_t vc[KM_NVOICE + KM_NFADE];
/* Glide: a tine plucked hard swings wide, and the stretch of its bending raises its pitch a little until
 * the swing narrows (tension modulation). Up to glide_max cents (Glide: 0..40), settling in ~45 ms */
static float glide_max = 12.0f;
#define GLIDE_K 0.96826f                               /* per chunk of 64: exp(-64 / (0.045 s * SR)) */
/* Re-plucking a ringing tine: the thumb lands on it, damping it to ~0.55 over CONTACT_N chunks, then
 * slips off (the new pluck). Done through the resonators (their decay), never a jump in the waveform */
#define CONTACT_N 3u                                  /* chunks of BLK (64): ~4.4 ms */
#define CONTACT_D 0.99689f                            /* per sample: 0.55 over 192 samples */
#define FINGER_D 0.99978f                             /* a finger left on it (Release Damp, palm mute) */

static float cents_hz(int c) { return 8.17579892f * fm_exp2f((float)c * (1.0f / 1200.0f)) * tunefac * bendfac; }

/* Worn: an old, played-in instrument. Each tine has its own quirks, the same each time it's plucked
 * (they come from its pitch): a little out of tune (up to +-12 cents), a sustain longer or shorter than
 * its neighbours', its two bending planes further apart or closer (a faster or slower beat), its upper
 * modes brighter or duller, and a little louder or softer. */
static float worn;                                    /* 0..1 */
static float wear(int cents, int i)                   /* the tine's quirk i, -1..1 */
{
    uint32_t h = (uint32_t)cents * 2654435761u ^ (uint32_t)i * 0x9E3779B9u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    return (float)(h & 0xFFFFu) * (2.0f / 65535.0f) - 1.0f;
}

/* the modes' coefficients for the voice's tine, its material, Decay and Tone (damp: the finger) */
/* The resonators' settings, from their frequency (with the glide) and radius: b1 and g follow the
 * frequency, b2 the radius. A ringing mode simply carries on from its last two outputs, which is the
 * smoothest join a retune can have (keeping its amplitude and phase instead bends it away from the
 * samples already played: a sharper edge, measured). Cheap enough to run every chunk while a pluck's
 * glide settles. */
static void voice_freq(voice_t *v)
{
    const float e = v->glide * 0.000577791f;           /* cents -> the fraction the frequency rises */
    int k;
    for (k = 0; k < KM_NMODE; k++) {
        float w = v->w0[k] * (1.0f + e), r = v->rr[k], c, sn;
        if (k >= v->nm || v->amp[k] <= 0.0f || w > 0.45f * FM_TWO_PI) {
            v->b1[k] = v->b2[k] = v->g[k] = 0.0f;
            continue;
        }
        if (e == 0.0f) {
            c = v->c0[k];
            sn = v->s0[k];
        } else {   /* cos and sin of w0 + d from those of w0: the glide is small (d < 0.07 rad), to d^3 */
            const float d = v->w0[k] * e, d2 = 0.5f * d * d, d3 = d * d2 * (1.0f / 3.0f);
            c = v->c0[k] * (1.0f - d2) - v->s0[k] * (d - d3);
            sn = v->s0[k] * (1.0f - d2) + v->c0[k] * (d - d3);
        }
        v->b1[k] = 2.0f * r * c;
        v->b2[k] = -r * r;
        v->g[k] = v->amp[k] * sn;                     /* an impulse of area 1 rings at amplitude amp */
        v->cw[k] = c;
        v->sw[k] = sn;
    }
}

static void voice_coefs(voice_t *v)
{
    const km_material_t *m = km_material(par[P_MATERIAL]);
    float f0 = cents_hz(v->cents), dscale = fm_exp2f((float)(par[P_DECAY] - 50) * 0.04f),
          tonefac = fm_exp2f((float)(50 - par[P_TONE]) * 0.04f), t1, lvl = 1.0f, upper = 1.0f, spread = 1.0f;
    int k, nm = 0;
    if (worn > 0.0f) {                                  /* this tine's quirks */
        f0 *= fm_exp2f(worn * 12.0f * wear(v->cents, 0) * (1.0f / 1200.0f));
        dscale *= 1.0f + 0.35f * worn * wear(v->cents, 1);
        spread = 1.0f + 1.5f * worn * wear(v->cents, 2);
        upper = 1.0f + 0.5f * worn * wear(v->cents, 3);
        lvl = 1.0f + 0.2f * worn * wear(v->cents, 4);
    }
    t1 = m->t60 * dscale * fm_powf(f0 * (1.0f / 261.63f), -m->pitch_k);
    for (k = 0; k < KM_NMODE; k++) {
        float ratio = k ? m->ratio[k - 1] : 1.0f, amp = k ? m->amp[k - 1] : 1.0f, f, t60, r;
        if (k == 1)                                     /* the twin: its beat */
            ratio = 1.0f + (ratio - 1.0f) * spread;
        if (k >= 2)
            amp *= upper;
        amp *= lvl;
        f = f0 * ratio;
        if (k == 1 && m->beat_amp <= 0.0f)
            amp = 0.0f;
        v->amp[k] = 0.0f;
        if (f > 0.45f * KM_SR || amp <= 0.0f)
            continue;
        t60 = k <= 1 ? t1 : t1 / (1.0f + m->loss * tonefac * (ratio - 1.0f));
        if (t60 < 0.01f)
            t60 = 0.01f;
        r = fm_expf(-6.9077553f / (t60 * KM_SR)) * v->damp;
        v->w0[k] = FM_TWO_PI * f / KM_SR;
        v->c0[k] = fm_cosf(v->w0[k]);
        v->s0[k] = fm_sinf(v->w0[k]);
        v->rr[k] = r;
        v->amp[k] = amp;
        nm = k + 1;
    }
    v->nm = (uint8_t)nm;
    voice_freq(v);
}

static void voice_damp(voice_t *v)
{
    if (v->damp == FINGER_D || !v->on)
        return;
    v->contact = 0;
    v->damp = FINGER_D;                               /* ~0.7 s T60 for the first mode... */
    voice_coefs(v);                                    /* ...and the upper ones die in a moment */
}

/* the voice for a tine: the one already ringing it (a tine is one bar: plucked again, it is the same
 * vibration), else a free one, else the quietest */
static voice_t *voice_for(int cents)
{
    int i, best = 0;
    float lo = 1e9f;
    for (i = 0; i < KM_NVOICE; i++)
        if (vc[i].on && vc[i].cents == cents)
            return &vc[i];
    for (i = 0; i < KM_NVOICE; i++)
        if (!vc[i].on)
            return &vc[i];
    for (i = 0; i < KM_NVOICE; i++) {
        float l = vc[i].level * (vc[i].damp == FINGER_D ? 0.25f : 1.0f);   /* a damped tine goes first */
        if (l < lo) {
            lo = l;
            best = i;
        }
    }
    {   /* the quietest goes to a fade slot (the one nearest silence, if both are busy) and fades out */
        voice_t *v = &vc[best], *f = &vc[KM_NVOICE];
        int k;
        for (i = KM_NVOICE; i < KM_NVOICE + KM_NFADE; i++)
            if (!vc[i].on || vc[i].fade < f->fade)
                f = &vc[i];
        *f = *v;
        f->fade = 1.0f;
        f->fade_step = 1.0f / FADE_S;
        f->ex_ph = 0.5f;                                /* no more pluck, no more click */
        f->click = 0.0f;
        for (k = 0; k < KM_NMODE; k++)
            v->y1[k] = v->y2[k] = 0.0f;
        v->on = 0;
        km_voice_level[best] = 0.0f;
        return v;
    }
}

static void pluck(int cents, int vel, int pan)
{
    voice_t *v = voice_for(cents);
    const km_material_t *m = km_material(par[P_MATERIAL]);
    float hard = fm_clampf((float)par[P_HARD] * 0.01f + (float)(vel - 90) * (0.35f / 127.0f), 0.0f, 1.0f),
          /* a pluck is the tine slipping off the thumb: the force falls away in tp, slow for the pad of a
           * thumb (1.2 ms: the upper modes hardly start), quick for a nail (0.05 ms: they all ring) */
          tp = 0.0012f * fm_exp2f(-4.5f * hard),
          vg = (float)vel * (1.0f / 127.0f), a, n;
    int k;
    int again = v->on;
    if (!again)
        for (k = 0; k < KM_NMODE; k++)
            v->y1[k] = v->y2[k] = 0.0f;
    v->cents = (int16_t)cents;
    v->live = 0xFFu;                                    /* every mode, until it falls silent */
    v->damp = again ? CONTACT_D : 1.0f;                 /* ringing: the thumb lands on it first */
    v->contact = again ? (uint8_t)CONTACT_N : 0u;
    v->released = 0;
    voice_coefs(v);
    n = tp * KM_SR;                                     /* pulse samples */
    if (n < 1.0f)
        n = 1.0f;
    a = vg * vg * 0.55f;
    {   /* sharp at first: tension, as it swings wide. A ringing tine takes it as the thumb slips off */
        float gl = glide_max * vg * vg * (0.4f + 0.6f * hard);
        v->glide_rise = 0.0f;
        v->rise_n = 0;
        if (again) {
            v->glide_pend = gl;
        } else {
            v->glide = gl;
            v->glide_pend = 0.0f;
            voice_freq(v);
        }
    }
    v->ex_inc = 0.5f / n;                               /* half a turn over the contact */
    v->ex_ph = again ? -(float)(CONTACT_N * 64u) * v->ex_inc : 0.0f;   /* re-pluck: it slips off after the contact */
    v->ex_amp = a * (FM_PI * 0.5f) / n;                 /* area a: the first mode rings at ~a */
    v->click = a * m->click * hard * hard * 0.12f;
    v->click_dec = fm_expf(-1.0f / (0.0025f * KM_SR));
    a = ((float)pan * 0.01f * width + 1.0f) * (FM_PI * 0.25f);   /* pan: equal power */
    v->pl = fm_cosf(a);
    v->pr = fm_sinf(a);
    v->on = 1;
    v->quiet = 0;
    v->level = vg;
    km_voice_cents[v - vc] = (int16_t)cents;
}

static void retune_all(void)
{
    int i;
    for (i = 0; i < KM_NVOICE + KM_NFADE; i++)
        if (vc[i].on)
            voice_coefs(&vc[i]);
}

/* plucks to come (the black keys' rolls) */
#define NPEND 48
static struct {
    uint32_t due;
    int16_t cents;
    int8_t pan;
    uint8_t vel, on;
} pend[NPEND];
static uint32_t now_s;                                  /* samples rendered since km_init */

/* ----------------------------------------------------------------- body --- */
typedef struct {
    float s1, s2, a1, a2, a3, gain;
} svf_t;
/* Simper's SVF (trapezoidal), the band-pass output: g = tan(pi f / sr), k = 1 / q,
 * a1 = 1 / (1 + g (g + k)), a2 = g a1, a3 = g a2 */
static inline float svf_bp(svf_t *s, float x)
{
    float v3 = x - s->s2, v1 = s->a1 * s->s1 + s->a2 * v3, v2 = s->s2 + s->a2 * s->s1 + s->a3 * v3;
    s->s1 = 2.0f * v1 - s->s1;
    s->s2 = 2.0f * v2 - s->s2;
    return v1;
}
/* The bodies. What the ear tells them apart by: how the direct sound is coloured (a plank radiates
 * little bass and sounds thin; a box warms it; a gourd is hollow and dark), the body's own resonances
 * among the tines' frequencies, and the knock of each pluck through the wood (the thumb pushes the
 * instrument as well as the tine). Per body:
 *   modes  four band-pass resonances: the air mode first (the sound hole: Helmholtz, which the wah
 *          moves), then the plate or shell modes; hz, q, level (peak gain)
 *   tone   the direct sound: low cut and high cut (Hz; 0 none)
 *   knock  how much of the pluck's force reaches the body
 *   out    the level, so that a body changes the character and not the volume */
typedef struct {
    float m[4][3];
    float lo_cut, hi_cut, knock, out;
} body_t;
static const body_t BODIES[BODY_N] = {
    {{{0}}, 0.0f, 0.0f, 0.0f, 1.0f},                                                       /* None: the bare tine */
    {{{0.0f, 1.0f, 0.0f}, {380.0f, 3.0f, 0.55f}, {980.0f, 4.0f, 0.45f}, {2300.0f, 5.0f, 0.35f}},
     260.0f, 0.0f, 0.6f, 1.05f},                                                           /* Board: a plank, thin and bright */
    {{{210.0f, 7.0f, 2.6f}, {460.0f, 5.0f, 0.9f}, {1120.0f, 6.0f, 0.4f}, {2700.0f, 6.0f, 0.18f}},
     0.0f, 6500.0f, 1.0f, 0.58f},                                                          /* Box: warm, a bloom at the hole */
    {{{135.0f, 6.0f, 2.2f}, {330.0f, 4.0f, 1.1f}, {760.0f, 5.0f, 0.8f}, {1650.0f, 4.0f, 0.25f}},
     0.0f, 3800.0f, 1.4f, 0.5f},                                                           /* Gourd: hollow, boomy, dark */
};
static svf_t bf[4];
static float hole_amt[HOLE_N], wah_ph, wah_inc;
static float tone_lo_a, tone_hi_a, tlo_l, tlo_r, thi_l, thi_r, knock_g, body_out = 1.0f;

static float onepole(float hz) { return hz > 0.0f ? 1.0f - fm_expf(-FM_TWO_PI * hz / KM_SR) : 1.0f; }

static void body_coefs(void)
{
    const body_t *bd = &BODIES[(unsigned)par[P_BODY] < BODY_N ? par[P_BODY] : 0];
    float cover = (float)par[P_WAH] * 0.01f, o, lfo;
    int i;
    for (i = 0; i < HOLE_N; i++)
        cover += hole_amt[i];
    if (par[P_WAHRATE]) {                               /* the hand over the hole, in time */
        lfo = 0.5f - 0.5f * fm_cosf(wah_ph * FM_TWO_PI);
        cover = cover + lfo * (1.0f - cover);
    }
    cover = fm_clampf(cover, 0.0f, 1.0f);
    o = 1.0f - 0.9f * cover;
    km_hole_open = o;
    for (i = 0; i < 4; i++) {
        float hz = bd->m[i][0], qq = bd->m[i][1], gain = bd->m[i][2];
        if (i == 0 && hz > 0.0f) {                      /* Helmholtz: f ~ sqrt(the hole's area) */
            hz *= fm_sqrtf(o);
            gain *= 0.35f + 0.65f * o;
            qq *= 1.0f + 0.8f * (1.0f - o);
        }
        if (hz <= 0.0f || gain <= 0.0f) {
            bf[i].gain = 0.0f;
            continue;
        }
        {
            float g = fm_tanf(FM_PI * hz / KM_SR), k = 1.0f / qq;
            bf[i].a1 = 1.0f / (1.0f + g * (g + k));
            bf[i].a2 = g * bf[i].a1;
            bf[i].a3 = g * bf[i].a2;
            bf[i].gain = gain * k;                      /* the band-pass peaks at 1/k: level by gain */
        }
    }
    tone_lo_a = bd->lo_cut > 0.0f ? onepole(bd->lo_cut) : 0.0f;
    tone_hi_a = onepole(bd->hi_cut);
    knock_g = bd->knock;
    body_out = bd->out;
}

/* ------------------------------------------------------------ reverb --- */
/* Dattorro's plate (J. Dattorro, "Effect Design, Part 1", JAES 1997), as FoMni has it: a predelay, four
 * input diffusers, then a figure-eight tank of two halves, each a modulated allpass, a delay, a damping
 * lowpass, an allpass and a delay, feeding the other half. Stereo out from seven taps a side. The
 * paper's lengths are for 29761 Hz; DS() scales them to 44.1 kHz. */
#define DS(n) (((n) * 14818 + 5000) / 10000)
#define PRE_N 442
#define EXC DS(16)
enum { L_PRE, L_IN1, L_IN2, L_IN3, L_IN4, L_APL, L_D1L, L_AP2L, L_D2L, L_APR, L_D1R, L_AP2R, L_D2R, L_N };
static const int DL_LEN[L_N] = {PRE_N, DS(142), DS(107), DS(379), DS(277), DS(672) + EXC + 2, DS(4453), DS(1800),
                                DS(3720), DS(908) + EXC + 2, DS(4217), DS(2656), DS(3163)};
#define DL_TOTAL (PRE_N + DS(142) + DS(107) + DS(379) + DS(277) + DS(672) + DS(4453) + DS(1800) + DS(3720) + DS(908) + \
                  DS(4217) + DS(2656) + DS(3163) + 2 * (EXC + 2))
static float pl_buf[DL_TOTAL] KM_POOL;
typedef struct {
    float *b;
    int n, i;
} dl_t;
static dl_t dl[L_N];
static float pl_bw, pl_damp_l, pl_damp_r, pl_decay = 0.5f, pl_dd2 = 0.5f, lfo_s, lfo_c = 1.0f;

static inline float tap(const dl_t *d, int k)
{
    int j = d->i - k;
    return d->b[j < 0 ? j + d->n : j];
}
static inline void push(dl_t *d, float x)
{
    d->b[d->i] = x;
    if (++d->i >= d->n)
        d->i = 0;
}
static inline float allpass(dl_t *d, int len, float x, float g)
{
    float z = tap(d, len), v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}
static inline float allpass_mod(dl_t *d, float len, float x, float g)
{
    int k = (int)len;
    float f = len - (float)k, z = tap(d, k) + (tap(d, k + 1) - tap(d, k)) * f, v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}

static inline void reverb(float in, float *ol, float *or_)
{
    float x, a, b, fb_l = tap(&dl[L_D2R], DS(3163)), fb_r = tap(&dl[L_D2L], DS(3720));
    push(&dl[L_PRE], in);
    x = tap(&dl[L_PRE], PRE_N);
    pl_bw += 0.7f * (x - pl_bw);
    x = allpass(&dl[L_IN1], DS(142), pl_bw, 0.75f);
    x = allpass(&dl[L_IN2], DS(107), x, 0.75f);
    x = allpass(&dl[L_IN3], DS(379), x, 0.625f);
    x = allpass(&dl[L_IN4], DS(277), x, 0.625f);
    {
        float s = lfo_s + 0.000128f * lfo_c, c = lfo_c - 0.000128f * s;
        lfo_s = s;
        lfo_c = c;
    }
    a = allpass_mod(&dl[L_APL], (float)DS(672) + (float)EXC * lfo_s, x + pl_decay * fb_l, -0.7f);
    push(&dl[L_D1L], a);
    b = tap(&dl[L_D1L], DS(4453));
    pl_damp_l += 0.62f * (b - pl_damp_l);
    b = allpass(&dl[L_AP2L], DS(1800), pl_damp_l * pl_decay, pl_dd2);
    push(&dl[L_D2L], b);
    a = allpass_mod(&dl[L_APR], (float)DS(908) + (float)EXC * lfo_c, x + pl_decay * fb_r, -0.7f);
    push(&dl[L_D1R], a);
    b = tap(&dl[L_D1R], DS(4217));
    pl_damp_r += 0.62f * (b - pl_damp_r);
    b = allpass(&dl[L_AP2R], DS(2656), pl_damp_r * pl_decay, pl_dd2);
    push(&dl[L_D2R], b);
    *ol = 0.6f * (tap(&dl[L_D1R], DS(266)) + tap(&dl[L_D1R], DS(2974)) - tap(&dl[L_AP2R], DS(1913)) +
                  tap(&dl[L_D2R], DS(1996)) - tap(&dl[L_D1L], DS(1990)) - tap(&dl[L_AP2L], DS(187)) -
                  tap(&dl[L_D2L], DS(1066)));
    *or_ = 0.6f * (tap(&dl[L_D1L], DS(353)) + tap(&dl[L_D1L], DS(3627)) - tap(&dl[L_AP2L], DS(1228)) +
                   tap(&dl[L_D2L], DS(2673)) - tap(&dl[L_D1R], DS(2111)) - tap(&dl[L_AP2R], DS(335)) -
                   tap(&dl[L_D2R], DS(121)));
}

static void plate_init(void)
{
    float *p = pl_buf;
    int k, i;
    for (k = 0; k < L_N; k++) {
        dl[k].b = p;
        dl[k].n = DL_LEN[k];
        dl[k].i = 0;
        p += DL_LEN[k];
    }
    for (i = 0; i < DL_TOTAL; i++)
        pl_buf[i] = 0.0f;
    pl_bw = pl_damp_l = pl_damp_r = 0.0f;
    lfo_s = 0.0f;
    lfo_c = 1.0f;
}

/* -------------------------------------------------- half-rate lines --- */
/* The delay and the grain buffer run at 22.05 kHz in int16: twice the time in a quarter of the memory
 * of a float line. A kalimba's echo is dark anyway; the grains are mostly pitched up, and the tines
 * have little above 11 kHz. */
#define DLY_N 32768u                                    /* 1.49 s: a ping-pong of up to 0.74 s */
#define GRB_N 24576u                                    /* 1.11 s of grain source */
static int16_t dly_buf[DLY_N] KM_POOL;
static int16_t grb_buf[GRB_N] KM_POOL;
/* Stored at 0.3 of the signal through a soft limiter (tanh), read back x 1/0.3: a chord of loud tines
 * sums to several times full scale before the output stage, which hard-clipped these lines (crackle in
 * the grains and the echoes). Linear within ~3 % up to 1.0, soft above, never a hard edge. */
#define LINE_IN 0.3f
#define S16 32767.0f
#define S16I (1.0f / (32767.0f * LINE_IN))
static inline int16_t to16(float x)
{
    x = fm_tanhf(x * LINE_IN) * S16;
    return (int16_t)(x < 0.0f ? x - 0.5f : x + 0.5f);
}

#ifdef OM_HOST
/* the device's .pool is 0x54000 bytes and the build keeps 8 KiB spare; the canvas (gfx.c) takes 240 x 144
 * x 2. Checked here because the device build cannot run where these tests do. */
_Static_assert(sizeof pl_buf + sizeof dly_buf + sizeof grb_buf + 4u * (1024u + 2u * 512u) + 240u * 144u * 2u <=
                   0x54000u - 8192u,
               "the .pool would overflow");                  /* (+ the chorus and tape lines, below) */
#endif

/* ---- the delay: one line, two taps. Written x = in + fb^2 * line(2T); the left hears line(T), the right
 * line(2T) * fb: echoes left, right, left... each fb of the one before. Its loop is darkened. */
static uint32_t dw;
static float d_t = 330.0f * 22.05f, d_tt, d_fb, d_lp, d_mix, d_prev_l, d_prev_r, d_cur_l, d_cur_r, d_in;

/* the line `back` (22.05 kHz samples, < DLY_N) behind the write position; dw stays wrapped, so the
 * integer part and the fraction keep every bit however long the device has been on */
static inline float dly_read(float back)
{
    uint32_t ib = (uint32_t)back, i0 = (dw - ib) & (DLY_N - 1u), i1 = (i0 - 1u) & (DLY_N - 1u);
    float f = back - (float)ib;
    return ((float)dly_buf[i0] + ((float)dly_buf[i1] - (float)dly_buf[i0]) * f) * S16I;
}

static inline void delay_tick(float in)                 /* one 22.05 kHz step */
{
    float l, r;
    d_t += 0.0015f * (d_tt - d_t);                      /* a turn of Time glides (a tape's pitch bend) */
    l = dly_read(d_t);
    r = dly_read(2.0f * d_t);
    d_lp += 0.45f * (r - d_lp);
    dly_buf[dw] = to16(in + d_fb * d_fb * d_lp);
    dw = (dw + 1u) & (DLY_N - 1u);
    d_prev_l = d_cur_l;
    d_prev_r = d_cur_r;
    d_cur_l = l;
    d_cur_r = r * d_fb;
}

/* ---- the grains: a ring of the dry sound, and up to NGRAIN windowed readers of it */
#define NGRAIN 10
typedef struct {
    float pos, rate, wph, winc, amp, pl, pr;
    uint8_t on;
} grain_t;
static grain_t gr[NGRAIN];
static uint32_t gw;                                     /* the write position, 0..GRB_N-1 */
static float g_timer, g_acc, win[257];

static inline float grb_read(float p)                   /* p in 0..GRB_N */
{
    int i;
    float f;
    i = (int)p;
    f = p - (float)i;
    {
        uint32_t a = (uint32_t)i >= GRB_N ? 0u : (uint32_t)i, b = a + 1u >= GRB_N ? 0u : a + 1u;
        return ((float)grb_buf[a] + ((float)grb_buf[b] - (float)grb_buf[a]) * f) * S16I;
    }
}

static void grain_spawn(void)
{
    static const float RATIO[GP_N] = {0.5f, 0.6674f, 1.0f, 1.4983f, 2.0f, 2.9966f, 1.0f, -1.0f};
    grain_t *g = 0;
    int i, gp = par[P_GPITCH];
    float spray = (float)par[P_GSPRAY] * 0.01f, len, ratio, back, rb;
    for (i = 0; i < NGRAIN; i++)
        if (!gr[i].on) {
            g = &gr[i];
            break;
        }
    if (!g)
        return;
    ratio = RATIO[(unsigned)gp < GP_N ? gp : 2];
    if (gp == GP_SHIMMER) {                             /* unison, fifth up, octave up: the shimmer */
        uint32_t c = rnd() % 3u;
        ratio = c == 0u ? 1.0f : c == 1u ? 1.4983f : 2.0f;
    }
    ratio *= fm_exp2f(rnds() * spray * (0.15f / 12.0f));   /* spray detunes a little too */
    len = (float)par[P_GSIZE] * (KM_SR / 1000.0f) * (1.0f + 0.4f * spray * rnds());
    rb = ratio * 0.5f;                                  /* buffer samples per output sample */
    if (len * fm_fabsf(rb) > (float)GRB_N * 0.45f)
        len = (float)GRB_N * 0.45f / fm_fabsf(rb);
    if (len < 256.0f)
        len = 256.0f;
    {   /* where it starts: behind the write head (moving at h) by enough that a fast grain never
         * reaches it, and not so far that a slow (or reversed) one falls off the oldest end */
        float h = km_frozen ? 0.0f : 0.5f, lo = 64.0f + fm_maxf(0.0f, (rb - h) * len),
              hi = (float)GRB_N - 64.0f - fm_maxf(0.0f, (h - rb) * len);
        back = lo + spray * rndf() * (float)GRB_N * 0.5f;
        if (back > hi)
            back = hi;
    }
    g->pos = (float)gw - back;                          /* positions stay within the ring */
    if (g->pos < 0.0f)
        g->pos += (float)GRB_N;
    g->rate = rb;
    g->wph = 0.0f;
    g->winc = 256.0f / len;
    {
        float dens = (float)par[P_GDENS], over = dens * len / KM_SR, p = rnds() * (0.3f + 0.7f * spray) * width;
        g->amp = 1.0f / fm_sqrtf(1.0f + over);
        g->pl = fm_sqrtf(0.5f - 0.5f * p);
        g->pr = fm_sqrtf(0.5f + 0.5f * p);
    }
    g->on = 1;
}

/* ------------------------------------------------- chorus, filter, tape --- */
/* Chorus: the dry sound (mono) into a 23 ms line, read by two taps that wander +-3 ms around 12 ms, a
 * quarter cycle apart, left and right: a slow ensemble shimmer, wide. On the instrument, before the
 * echoes and the room. */
#define CH_N 1024u
static float ch_buf[CH_N] KM_POOL;
static uint32_t ch_w;
static float ch_ph, ch_amt, ch_t, ch_at, ch_ad;      /* LFO phase (turns); amount now, target; this chunk's
                                                       * end value and step a sample (every setting glides
                                                       * sample by sample: a step a chunk was a click) */
static float ch_dl0, ch_dr0;                          /* the taps' delays at the start of this chunk */

static inline float line_read(const float *b, uint32_t mask, uint32_t w, float back)
{
    uint32_t ib = (uint32_t)back, i0 = (w - ib) & mask, i1 = (i0 - 1u) & mask;
    float f = back - (float)ib;
    return b[i0] + (b[i1] - b[i0]) * f;
}

/* Filter: one knob. Left of centre a low-pass from 20 kHz down to ~200 Hz, right a high-pass from 20 Hz
 * up to ~3.9 kHz, the resonance rising with the sweep; centre is off. Simper's SVF per channel, its
 * setting glides (no zipper, no click) */
static svf_t flt[2];
static float flt_t, flt_s;                     /* 1/q; target (-1..1) and how far now (0..1) */
static int flt_mode = -1;                             /* -1 low-pass, +1 high-pass (off: flt_w 0) */
static float flt_w, flt_wd;                           /* how much of the filtered sound (crossfade in and out) */
static float flt_c[4], flt_cd[4];                     /* a1 a2 a3 k now, and their step a sample: the setting
                                                       * moves sample by sample (a high-pass shows any jump) */

static float flt_inv = 1.0f / 64.0f;                  /* 1 / the chunk's length */
static void filter_coefs(void)
{
    float a = fm_fabsf(flt_s), hz = flt_mode < 0 ? 20000.0f * fm_exp2f(-6.6f * a) : 20.0f * fm_exp2f(7.6f * a), g;
    int i;
    if (hz > 19000.0f)
        hz = 19000.0f;
    g = fm_tanf(FM_PI * hz / KM_SR);
    {
        float k = 1.0f / (0.707f + 0.9f * a), a1 = 1.0f / (1.0f + g * (g + k)), to[4];
        to[0] = a1;
        to[1] = g * a1;
        to[2] = g * g * a1;
        to[3] = k;
        for (i = 0; i < 4; i++)
            flt_cd[i] = (to[i] - flt_c[i]) * flt_inv;
    }
}

static inline float filter_run(svf_t *f, float x)
{
    float v3 = x - f->s2, v1 = flt_c[0] * f->s1 + flt_c[1] * v3, v2 = f->s2 + flt_c[1] * f->s1 + flt_c[2] * v3;
    f->s1 = 2.0f * v1 - f->s1;
    f->s2 = 2.0f * v2 - f->s2;
    return flt_mode < 0 ? v2 : x - flt_c[3] * v1 - v2;
}

/* Tape: the whole output through a short line whose length wanders, slowly (wow, 0.55 Hz, and a slower
 * drift) and quickly (flutter, 7.3 Hz): the pitch wavers, up to ~12 cents at Tape 100. And saturates, as
 * tape does when it's driven. Lo-fi: a cheaper machine. The top rolls off (down to ~2.8 kHz), the image
 * narrows, and there is hiss, which follows the music down (as a noise reducer does), so silence is
 * silent. */
#define TP_N 512u
static float tp_l[TP_N] KM_POOL, tp_r[TP_N] KM_POOL;
static uint32_t tp_w;
static float tp_amt, tp_t, tp_mix, wow_ph, flut_ph, drift, drift_t, tp_d0, tp_ad, tp_md, tp_at, tp_mt;
static float lf_amt, lf_t, lf_a = 1.0f, lf_l, lf_r, hiss_env, lf_ad, lf_at, lf_aa, lf_aad;

/* the three, on one stereo sample each; the chunk's settings come from fx_chunk() */
static float ch_dl, ch_dr, ch_dd_l, ch_dd_r, tp_d, tp_dd;
static void fx_chunk(uint32_t n)
{
    float c;
    const float inv = 1.0f / (float)n;
    /* chorus: the amount glides; the taps' delays move linearly across the chunk */
    ch_amt = ch_at;                                    /* (where the last chunk's ramp ended) */
    ch_at += 0.2f * (ch_t - ch_at);
    if (ch_at < 1e-4f && ch_t == 0.0f)
        ch_at = 0.0f;
    ch_ad = (ch_at - ch_amt) * inv;
    ch_ph += 0.8f * (float)n / KM_SR;
    if (ch_ph >= 1.0f)
        ch_ph -= 1.0f;
    c = 529.2f + 132.3f * fm_sin_turns(ch_ph);          /* 12 ms +- 3 ms, in samples */
    ch_dd_l = (c - ch_dl0) / (float)n;
    ch_dl = ch_dl0;
    ch_dl0 = c;
    c = 529.2f + 132.3f * fm_sin_turns(ch_ph + 0.25f);
    ch_dd_r = (c - ch_dr0) / (float)n;
    ch_dr = ch_dr0;
    ch_dr0 = c;
    /* filter: always running, at its last setting when it is off, so it always holds the sound's
     * state: it fades in and out (~6 ms) without the burst of a filter started from rest, and goes from
     * low-pass to high-pass (the same filter, read the other way) by fading out, turning, fading in */
    {
        const int want = flt_t < 0.0f ? -1 : flt_t > 0.0f ? 1 : 0;
        float target;
        if (want && want != flt_mode && flt_w <= 0.0f)
            flt_mode = want;
        target = want && want == flt_mode ? 1.0f : 0.0f;
        if (target > 0.0f)
            flt_s += 0.12f * (fm_fabsf(flt_t) - flt_s);   /* (how far, on its side) */
        if (target == 0.0f && flt_w < 0.005f)
            flt_w = 0.0f;
        flt_wd = (target - flt_w) * 0.25f * inv;        /* (a quarter of the way a chunk) */
        flt_inv = inv;
        filter_coefs();
    }
    /* tape: in and out of the line with a quick crossfade; the line's length from wow, drift, flutter */
    tp_amt = tp_at;
    tp_mix = tp_mt;
    tp_at += 0.1f * (tp_t - tp_at);
    tp_mt += 0.25f * ((tp_t > 0.0f ? 1.0f : 0.0f) - tp_mt);
    if (tp_mt < 1e-4f && tp_t == 0.0f)
        tp_mt = tp_at = 0.0f;
    tp_ad = (tp_at - tp_amt) * inv;
    tp_md = (tp_mt - tp_mix) * inv;
    wow_ph += 0.55f * (float)n / KM_SR;
    flut_ph += 7.3f * (float)n / KM_SR;
    wow_ph -= wow_ph >= 1.0f ? 1.0f : 0.0f;
    flut_ph -= flut_ph >= 1.0f ? 1.0f : 0.0f;
    if ((now_s & 4095u) < n)
        drift_t = rnds();                              /* a new place to drift to, every ~0.1 s */
    drift += 0.004f * (drift_t - drift);
    c = 220.5f + tp_at * (88.2f * fm_sin_turns(wow_ph) + 30.0f * drift + 3.5f * fm_sin_turns(flut_ph));
    tp_dd = (c - tp_d0) / (float)n;
    tp_d = tp_d0;
    tp_d0 = c;
    /* lo-fi */
    lf_amt = lf_at;
    lf_a = lf_aa;
    lf_at += 0.1f * (lf_t - lf_at);
    if (lf_at < 1e-4f && lf_t == 0.0f)
        lf_at = 0.0f;
    lf_aa = onepole(18000.0f * fm_exp2f(-2.7f * lf_at));
    lf_ad = (lf_at - lf_amt) * inv;
    lf_aad = (lf_aa - lf_a) * inv;
}

static inline void fx_chorus(float *l, float *r)
{
    ch_buf[ch_w] = (*l + *r) * 0.5f;
    if (ch_amt > 0.0f || ch_ad > 0.0f) {
        float a = line_read(ch_buf, CH_N - 1u, ch_w, ch_dl), b = line_read(ch_buf, CH_N - 1u, ch_w, ch_dr);
        *l = *l * (1.0f - 0.25f * ch_amt) + a * 0.75f * ch_amt;
        *r = *r * (1.0f - 0.25f * ch_amt) + b * 0.75f * ch_amt;
    }
    ch_w = (ch_w + 1u) & (CH_N - 1u);
    ch_dl += ch_dd_l;
    ch_dr += ch_dd_r;
    ch_amt += ch_ad;
}

static inline void fx_out(float *l, float *r)
{
    {
        float a = filter_run(&flt[0], *l), b = filter_run(&flt[1], *r);
        int c;
        *l += (a - *l) * flt_w;
        *r += (b - *r) * flt_w;
        flt_w += flt_wd;
        for (c = 0; c < 4; c++)
            flt_c[c] += flt_cd[c];
        if (flt_w < 0.0f)
            flt_w = 0.0f;
    }
    tp_l[tp_w] = *l;
    tp_r[tp_w] = *r;
    if (tp_mix > 0.0f || tp_md > 0.0f) {               /* through the tape: wavering, driven */
        float a = line_read(tp_l, TP_N - 1u, tp_w, tp_d), b = line_read(tp_r, TP_N - 1u, tp_w, tp_d),
              drive = 1.0f + 2.0f * tp_amt, mk = 1.0f / drive;
        a = fm_tanhf(a * drive) * mk;
        b = fm_tanhf(b * drive) * mk;
        *l += (a - *l) * tp_mix;
        *r += (b - *r) * tp_mix;
    }
    tp_w = (tp_w + 1u) & (TP_N - 1u);
    tp_d += tp_dd;
    tp_amt += tp_ad;
    tp_mix += tp_md;
    if (lf_amt > 0.0f || lf_ad > 0.0f) {               /* a cheaper machine */
        float m = (*l + *r) * 0.5f, sd = (*l - *r) * 0.5f * (1.0f - 0.5f * lf_amt), e = fm_fabsf(m);
        lf_l += lf_a * (m + sd - lf_l);
        lf_r += lf_a * (m - sd - lf_r);
        hiss_env = e > hiss_env ? e : hiss_env * 0.99997f;   /* the hiss follows the music, ~1.5 s down */
        *l = lf_l + rnds() * lf_amt * 0.006f * fm_minf(1.0f, hiss_env * 8.0f);
        *r = lf_r + rnds() * lf_amt * 0.006f * fm_minf(1.0f, hiss_env * 8.0f);
        lf_amt += lf_ad;
        lf_a += lf_aad;
    } else {
        lf_l = *l;
        lf_r = *r;
    }
}

/* ---------------------------------------------------------- pattern --- */
/* Mbira-style patterns over a chord: twelve pulses a cycle, three a beat. Each pulse plucks up to two
 * tines, chosen by role: the left thumb's two bass tines (L1 the chord's root an octave down, L2 its
 * fifth or what stands for it), the right thumb's treble, R1..R6 up through the chord and its octave.
 * Bass sits left, treble right, as the thumbs do. The first pulse of the cycle is accented, and no two
 * plucks are quite the same strength. */
enum { RL1 = 1, RL2, RR1, RR2, RR3, RR4, RR5, RR6 };
static const uint8_t PATS[3][12][2] = {
    /* Thumbs: left and right in turn, bass under a treble line that rises and falls */
    {{RL1}, {RR3}, {RL2}, {RR4}, {RL1}, {RR5}, {RL2}, {RR4}, {RL1}, {RR3}, {RL2}, {RR2}},
    /* Cascade: down the tines, both sides, into the bass, and back up */
    {{RR6}, {RR5}, {RR4}, {RR3}, {RR2}, {RR1}, {RL2}, {RL1}, {RR2}, {RR3}, {RR4}, {RR5}},
    /* 3 over 2: the bass every three pulses against the treble every two */
    {{RL1, RR3}, {0}, {RR4}, {RL2}, {RR5}, {0}, {RL1, RR4}, {0}, {RR3}, {RL2}, {RR2}, {0}},
};
static int pool[KM_NPOOL], npool, pat_on, pat_step, pat_run = 1;
static float pat_left;
static uint32_t ext_ticks, ext_age;
volatile uint8_t km_pat_on, km_pat_pulse, km_ext;

/* The chord's tones, at least three: one note gets its fifth and octave, two get the octave of the
 * lower. The treble climbs through them by octaves but never above two octaves over the lowest (one note
 * held would otherwise send R6 five octaves up); the bass is the root and the third tone, an octave down */
static int role_cents(int r)
{
    int t[KM_NPOOL + 2], m = npool, i, c;
    for (i = 0; i < npool; i++)
        t[i] = pool[i];
    if (m == 1)
        t[m++] = pool[0] + 700;
    if (m == 2)
        t[m++] = pool[0] + 1200;
    if (r == RL1)
        c = t[0] - 1200;
    else if (r == RL2)
        c = t[2] - 1200;
    else {
        i = r - RR1;
        c = t[i % m] + 1200 * (i / m);
        while (c > t[0] + 2400)
            c -= 1200;
    }
    return c < 0 ? 0 : c > 12700 ? 12700 : c;
}

static void pluck(int cents, int vel, int pan);
/* one step: a pulse (or, for Interlock, half of one: the second part answers between the first's) */
static void pat_fire(void)
{
    const int steps = par[P_PATTERN] == PAT_INTERLOCK ? 24 : 12, st = pat_step % steps;
    int pulse = steps == 24 ? st / 2 : st, k;
    uint8_t roles[2];
    if (steps == 24) {
        if (!(st & 1)) {
            roles[0] = PATS[0][pulse][0];
            roles[1] = PATS[0][pulse][1];
        } else {                                       /* the answering part: six pulses on, a tine higher */
            const uint8_t *q = PATS[0][(pulse + 6) % 12];
            for (k = 0; k < 2; k++)
                roles[k] = q[k] == RL1 ? RL2 : q[k] == RL2 ? RL1 : q[k] >= RR1 && q[k] < RR6 ? (uint8_t)(q[k] + 1) : q[k];
        }
    } else {
        const int p = par[P_PATTERN] < 3 ? par[P_PATTERN] : 0;
        roles[0] = PATS[p][pulse][0];
        roles[1] = PATS[p][pulse][1];
    }
    for (k = 0; k < 2; k++) {
        int r = roles[k], vel;
        if (!r)
            continue;
        vel = 84 + (pulse == 0 && (steps == 12 || !(st & 1)) ? 20 : 0) + (r <= RL2 ? 6 : 0) + (int)(rnds() * 6.0f);
        pluck(role_cents(r), vel, r <= RL2 ? -60 : 60);
    }
    km_pat_pulse = (uint8_t)pulse;
    pat_step = (st + 1) % steps;
}

/* the pattern's clock, per chunk: its own tempo, or MIDI clock (the ticks come in through drain) */
static void pat_chunk(uint32_t n)
{
    float len;
    if (!pat_on || !npool) {
        pat_left = 0.0f;
        return;
    }
    if (km_ext || !pat_run)                             /* (MIDI clock leads, or it was stopped) */
        return;
    len = KM_SR * 60.0f / ((float)par[P_PTEMPO] * 3.0f);
    if (par[P_PATTERN] == PAT_INTERLOCK)
        len *= 0.5f;
    pat_left -= (float)n;
    if (pat_left <= 0.0f) {
        pat_fire();
        pat_left += len;
        if (pat_left < 0.0f)
            pat_left = len;
    }
}

/* ---------------------------------------------------------- parameters --- */
static float rsend, g_mix, buzz_amt, buzz_lp;

static void apply(int p, int v)
{
    const km_param_t *pi = &PARAMS[p];
    if (v < pi->lo)
        v = pi->lo;
    if (v > pi->hi)
        v = pi->hi;
    par[p] = (int16_t)v;
    switch (p) {
    case P_MATERIAL: case P_DECAY: case P_TONE: retune_all(); break;
    case P_TUNE:
        tunefac = fm_exp2f((float)v * (1.0f / 1200.0f));
        retune_all();
        break;
    case P_BODY: case P_WAH: body_coefs(); break;
    case P_WAHRATE: wah_inc = v ? 0.08f * fm_exp2f((float)v * 0.066f) / KM_SR : 0.0f; break;   /* 0.08..~8 Hz */
    case P_BUZZ: buzz_amt = (float)v * 0.01f; break;
    case P_REVERB: rsend = (float)v * 0.01f * (float)v * 0.01f; break;
    case P_SIZE:
        pl_decay = 0.35f + 0.6f * (float)v * 0.01f;
        pl_dd2 = fm_clampf(pl_decay + 0.15f, 0.25f, 0.5f);
        break;
    case P_DELAY: d_mix = (float)v * 0.01f; break;
    case P_TIME: d_tt = (float)v * 22.05f; break;
    case P_FEEDBACK: d_fb = (float)v * 0.01f; break;
    case P_GRAIN: g_mix = (float)v * 0.01f * 1.4f; break;
    case P_WIDTH: width = (float)v * 0.01f; break;
    case P_TAPE: tp_t = (float)v * 0.01f; break;
    case P_LOFI: lf_t = (float)v * 0.01f; break;
    case P_CHORUS: ch_t = (float)v * 0.01f; break;
    case P_FILTER: flt_t = (float)v * 0.01f; break;
    case P_GLIDE: glide_max = (float)v * 0.4f; break;
    case P_WORN:
        worn = (float)v * 0.01f;
        retune_all();                                   /* the ringing tines take their quirks at once */
        break;
    default: break;
    }
}

static void quiet(void)
{
    int i;
    for (i = 0; i < KM_NVOICE + KM_NFADE; i++)
        vc[i].on = 0;
    for (i = 0; i < KM_NVOICE; i++) {
        km_voice_level[i] = 0.0f;
        km_voice_cents[i] = -1;
    }
    for (i = 0; i < NPEND; i++)
        pend[i].on = 0;
    pat_on = 0;                                         /* (the UI's ARP goes off with it: ui_init) */
    km_pat_on = 0;
    for (i = 0; i < NGRAIN; i++)
        gr[i].on = 0;
    for (i = 0; i < (int)DLY_N; i++)
        dly_buf[i] = 0;
    for (i = 0; i < (int)CH_N; i++)
        ch_buf[i] = 0.0f;
    for (i = 0; i < (int)TP_N; i++)
        tp_l[i] = tp_r[i] = 0.0f;
    flt[0].s1 = flt[0].s2 = flt[1].s1 = flt[1].s2 = 0.0f;
    lf_l = lf_r = hiss_env = 0.0f;
    for (i = 0; i < 4; i++)
        bf[i].s1 = bf[i].s2 = 0.0f;
    tlo_l = tlo_r = thi_l = thi_r = 0.0f;
    d_lp = d_cur_l = d_cur_r = d_prev_l = d_prev_r = 0.0f;
    plate_init();
}

void km_init(void)
{
    int i;
    q_r = q_w = 0;
    for (i = 0; i < 257; i++)                           /* Hann */
        win[i] = 0.5f - 0.5f * fm_cosf(FM_TWO_PI * (float)i / 256.0f);
    for (i = 0; i < (int)GRB_N; i++)
        grb_buf[i] = 0;
    for (i = 0; i < HOLE_N; i++)
        hole_amt[i] = 0.0f;
    gw = dw = 0;
    km_frozen = 0;
    bendfac = 1.0f;
    for (i = 0; i < P_NPARAMS; i++)
        apply(i, PARAMS[i].def);
    d_t = d_tt;
    ch_amt = ch_at = ch_t;
    flt_s = fm_fabsf(flt_t);
    flt_mode = flt_t > 0.0f ? 1 : -1;
    flt_w = flt_t != 0.0f ? 1.0f : 0.0f;
    flt_inv = 1.0f;
    filter_coefs();                                     /* a step of the whole way: in place at once */
    {
        int c;
        for (c = 0; c < 4; c++) {
            flt_c[c] += flt_cd[c];
            flt_cd[c] = 0.0f;
        }
    }
    tp_amt = tp_at = tp_t;
    tp_mix = tp_mt = tp_t > 0.0f ? 1.0f : 0.0f;
    lf_amt = lf_at = lf_t;
    lf_a = lf_aa = onepole(18000.0f * fm_exp2f(-2.7f * lf_t));
    ch_dl0 = ch_dr0 = 529.2f;
    tp_d0 = 220.5f;
    npool = 0;
    pat_on = 0;
    km_pat_on = 0;
    km_ext = 0;
    quiet();
    body_coefs();
}

static uint8_t sustain;

static void drain(void)
{
    int bent = 0;
    while (q_r != q_w) {
        cmd_t c = q[q_r % QN];
        int i;
        BARRIER();
        q_r++;
        switch (c.c) {
        case C_SET:
            if (c.a >= 0 && c.a < P_NPARAMS)
                apply(c.a, c.b);
            break;
        case C_PLUCK:
            if (c.d > 0) {
                for (i = 0; i < NPEND; i++)
                    if (!pend[i].on) {
                        pend[i].on = 1;
                        pend[i].due = now_s + (uint32_t)c.d * 44u + (uint32_t)c.d / 10u;
                        pend[i].cents = c.a;
                        pend[i].vel = (uint8_t)c.b;
                        pend[i].pan = c.pan;
                        break;
                    }
            } else {
                pluck(c.a, c.b, c.pan);
            }
            break;
        case C_DAMP:
            for (i = 0; i < KM_NVOICE; i++)
                if (vc[i].on && vc[i].cents == c.a) {
                    if (sustain)
                        vc[i].released = 1;
                    else
                        voice_damp(&vc[i]);
                }
            for (i = 0; i < NPEND; i++)                  /* a roll let go before it got there */
                if (pend[i].on && pend[i].cents == c.a && !sustain)
                    pend[i].on = 0;
            break;
        case C_DAMPALL:
            for (i = 0; i < KM_NVOICE; i++)
                voice_damp(&vc[i]);
            for (i = 0; i < NPEND; i++)
                pend[i].on = 0;
            break;
        case C_SUSTAIN:
            sustain = (uint8_t)(c.a != 0);
            if (!sustain)
                for (i = 0; i < KM_NVOICE; i++)
                    if (vc[i].released)
                        voice_damp(&vc[i]);
            break;
        case C_HOLE:
            if (c.a >= 0 && c.a < HOLE_N)
                hole_amt[c.a] = (float)c.b * (1.0f / 127.0f);
            body_coefs();
            break;
        case C_BEND:
            bendfac = fm_exp2f((float)c.a * (2.0f / 12.0f / 8192.0f));
            bent = 1;                                    /* a wheel sends dozens a block: retune once */
            break;
        case C_FREEZE:
            km_frozen = (uint8_t)(c.a != 0);
            break;
        case C_PANIC:
            quiet();
            break;
        case C_PATTERN:
            pat_on = c.a != 0;
            km_pat_on = (uint8_t)pat_on;
            pat_run = 1;                             /* (ARP starts it, whatever a Stop said) */
            pat_step = 0;
            pat_left = 0.0f;                         /* on: the first pulse at once */
            break;
        case C_POOLCLR:
            npool = 0;
            break;
        case C_POOLADD: {
            int at = npool, x = c.a;
            if (npool >= KM_NPOOL)
                break;
            while (at > 0 && pool[at - 1] > x) {      /* kept in order, low to high */
                pool[at] = pool[at - 1];
                at--;
            }
            pool[at] = x;
            npool++;
            break;
        }
        case C_CLOCK:
            ext_age = 0;
            if (c.a == KM_CLK_TICK) {
                km_ext = 1;                          /* someone else leads */
                if (pat_on && npool && pat_run) {
                    const uint32_t per = par[P_PATTERN] == PAT_INTERLOCK ? 4u : 8u;   /* 24 a beat, 3 pulses */
                    if (ext_ticks % per == 0u)
                        pat_fire();
                }
                ext_ticks++;
            } else if (c.a == KM_CLK_START) {
                ext_ticks = 0;
                pat_step = 0;
                pat_run = 1;
            } else if (c.a == KM_CLK_CONTINUE) {
                pat_run = 1;
            } else {
                pat_run = 0;
            }
            break;
        }
    }
    if (bent)
        retune_all();
}

/* ------------------------------------------------------------- render --- */
#define BLK 64                                          /* a chunk: plucks due land within 1.5 ms */
static float mixl[BLK], mixr[BLK], mono[BLK], xb[BLK], knock[BLK];   /* knock: the plucks' force */

static void render_voices(uint32_t n)
{
    int i, k;
    uint32_t j;
    for (i = 0; i < KM_NVOICE + KM_NFADE; i++) {        /* the tines, then the fade slots */
        voice_t *v = &vc[i];
        float pk = 0.0f, vb[BLK];
        int excite = v->ex_ph < 0.5f;
        if (!v->on)
            continue;
        if (excite) {                                   /* the pulse (ex_ph < 0: the thumb still resting on it) */
            float ph = v->ex_ph;
            for (j = 0; j < n; j++) {
                xb[j] = ph >= 0.0f && ph < 0.5f ? v->ex_amp * fm_sin_turns(ph) : 0.0f;
                knock[j] += xb[j];
                ph += v->ex_inc;
            }
            v->ex_ph = ph;
        }
        for (j = 0; j < n; j++)
            vb[j] = 0.0f;
        for (k = 0; k < v->nm; k++) {
            float y1 = v->y1[k], y2 = v->y2[k];
            const float b1 = v->b1[k], b2 = v->b2[k], g = v->g[k];
            if (g == 0.0f || !(v->live >> k & 1u))
                continue;
            if (excite) {
                for (j = 0; j < n; j++) {
                    float y = b1 * y1 + b2 * y2 + g * xb[j];
                    y2 = y1;
                    y1 = y;
                    vb[j] += y;
                }
            } else {
                for (j = 0; j < n; j++) {
                    float y = b1 * y1 + b2 * y2;
                    y2 = y1;
                    y1 = y;
                    vb[j] += y;
                }
            }
            v->y1[k] = fm_flush(y1);
            v->y2[k] = fm_flush(y2);
            if (!excite) {   /* below ~-110 dB it is silence: skip it until the next pluck. Its amplitude C from
                              * the last two outputs: (C sin w)^2 = (y1 sin w)^2 + (y1 cos w - r y2)^2 */
                const float s = v->sw[k], q = y1 * v->cw[k] - v->rr[k] * y2;
                if (y1 * y1 * s * s + q * q < 1e-11f * s * s) {
                    v->live &= (uint8_t)~(1u << k);
                    v->y1[k] = v->y2[k] = 0.0f;
                }
            }
        }
        if (v->click > 1e-6f && v->ex_ph >= 0.0f) {     /* the nail's tick, as it slips off */
            float c = v->click;
            for (j = 0; j < n; j++) {
                vb[j] += c * rnds();
                c *= v->click_dec;
            }
            v->click = c;
        }
        if (i >= KM_NVOICE) {                           /* a stolen voice: out in FADE_S, smoothly */
            float g = v->fade;
            const float st = v->fade_step;
            for (j = 0; j < n; j++) {
                vb[j] *= g * g * (3.0f - 2.0f * g);     /* an S: flat where it starts and where it ends */
                g = g > st ? g - st : 0.0f;
            }
            v->fade = g;
            if (g <= 0.0f)
                v->on = 0;
        }
        {
            const float pl = v->pl, pr = v->pr;
            for (j = 0; j < n; j++) {
                float o = vb[j], a = fm_fabsf(o);
                pk = a > pk ? a : pk;
                mixl[j] += o * pl;
                mixr[j] += o * pr;
                mono[j] += o;
            }
        }
        if (i >= KM_NVOICE)
            continue;
        if (v->contact && !--v->contact) {              /* the thumb slips off: free again, and stretched */
            if (v->damp == CONTACT_D)
                v->damp = 1.0f;
            v->glide_rise = v->glide_pend * 0.25f;     /* over four chunks (~6 ms): a ringing tine eases into it */
            v->glide_pend = 0.0f;
            voice_coefs(v);
        }
        if (v->glide_rise > 0.0f) {                     /* rising (a re-pluck) */
            v->glide += v->glide_rise;
            if (++v->rise_n >= 4u)
                v->glide_rise = 0.0f, v->rise_n = 0;
            voice_freq(v);
        } else if (v->glide != 0.0f && v->ex_ph >= 0.0f) {   /* then it settles as the swing narrows: ~45 ms */
            v->glide = v->glide > 0.05f ? v->glide * GLIDE_K : 0.0f;
            voice_freq(v);
        }
        v->level = pk > v->level ? pk : v->level * 0.9f + pk * 0.1f;
        km_voice_level[i] = v->level;
        if (pk < 2e-5f && !excite) {
            if (++v->quiet > 24u) {                     /* ~35 ms of nothing: the tine has stopped */
                v->on = 0;
                km_voice_level[i] = 0.0f;
                km_voice_cents[i] = -1;
            }
        } else {
            v->quiet = 0;
        }
    }
}

static void render_body(uint32_t n)
{
    uint32_t j;
    int i;
    if (par[P_BODY] == BODY_NONE && buzz_amt <= 0.0f)
        return;
    if (wah_inc > 0.0f) {
        wah_ph += wah_inc * (float)n;
        if (wah_ph >= 1.0f)
            wah_ph -= 1.0f;
        body_coefs();
    }
    if (par[P_BODY] != BODY_NONE) {
        const float la = tone_lo_a, ha = tone_hi_a, kg = knock_g * 6.0f, out = body_out;
        for (j = 0; j < n; j++) {
            float x = mono[j] + knock[j] * kg, b = 0.0f, l = mixl[j], r = mixr[j];
            for (i = 0; i < 4; i++)
                if (bf[i].gain > 0.0f)
                    b += svf_bp(&bf[i], x) * bf[i].gain;
            thi_l += ha * (l - thi_l);                  /* the direct sound through the body's tone */
            thi_r += ha * (r - thi_r);
            tlo_l += la * (thi_l - tlo_l);
            tlo_r += la * (thi_r - tlo_r);
            mixl[j] = (thi_l - tlo_l + b) * out;
            mixr[j] = (thi_r - tlo_r + b) * out;
            mono[j] = (mono[j] + b) * out;
        }
        thi_l = fm_flush(thi_l);
        thi_r = fm_flush(thi_r);
        tlo_l = fm_flush(tlo_l);
        tlo_r = fm_flush(tlo_r);
    }
    if (buzz_amt > 0.0f) {                              /* the buzzers: they touch past a gap */
        float th = 0.05f + 0.2f * (1.0f - buzz_amt), lvl = 0.0f;
        for (j = 0; j < n; j++) {
            float x = mono[j], e = x - fm_clampf(x, -th, th), r;
            e = fm_fabsf(e) * (rnds() + (e > 0.0f ? 0.6f : -0.6f));   /* rattling: never the same twice */
            buzz_lp += 0.25f * (e - buzz_lp);
            r = e - buzz_lp;                            /* the bright part: a sizzle */
            r *= buzz_amt * 2.5f;
            mixl[j] += r;
            mixr[j] += r * 0.9f;
            lvl = fm_maxf(lvl, fm_fabsf(r));
        }
        km_buzz_level = lvl;
    } else {
        km_buzz_level = 0.0f;
    }
}

static void render_chunk(int32_t *out, uint32_t n, float gain)
{
    uint32_t j;
    int i, active = 0;
    for (j = 0; j < n; j++)
        mixl[j] = mixr[j] = mono[j] = knock[j] = 0.0f;
    render_voices(n);
    render_body(n);
    fx_chunk(n);
    for (j = 0; j < n; j++)                             /* the chorus, on the instrument */
        fx_chorus(&mixl[j], &mixr[j]);
    for (i = 0; i < NGRAIN; i++)
        active += gr[i].on;
    km_grains_on = (uint8_t)active;
    if (g_mix > 0.0f) {                                 /* time for a grain? */
        g_timer -= (float)n;
        if (g_timer <= 0.0f) {
            float iv = KM_SR / (float)par[P_GDENS];
            grain_spawn();
            g_timer += iv * (1.0f + 0.8f * (float)par[P_GSPRAY] * 0.01f * rnds());
            if (g_timer < 64.0f)
                g_timer = 64.0f;
        }
    }
    for (j = 0; j < n; j++) {
        float l = mixl[j], r = mixr[j], gl = 0.0f, grr = 0.0f, wl, wr, dlo, dro, fr;
        /* the grain buffer: the dry sound at half rate */
        if (!km_frozen) {
            if (now_s & 1u) {
                grb_buf[gw] = to16((g_acc + l + r) * 0.35f);
                if (++gw >= GRB_N)
                    gw = 0;
            } else {
                g_acc = l + r;
            }
        }
        if (active) {
            for (i = 0; i < NGRAIN; i++) {
                grain_t *g = &gr[i];
                int wi;
                float s, wf;
                if (!g->on)
                    continue;
                wi = (int)g->wph;
                wf = g->wph - (float)wi;
                s = grb_read(g->pos) * (win[wi] + (win[wi + 1] - win[wi]) * wf) * g->amp;
                gl += s * g->pl;
                grr += s * g->pr;
                g->pos += g->rate;
                if (g->pos >= (float)GRB_N)
                    g->pos -= (float)GRB_N;
                else if (g->pos < 0.0f)
                    g->pos += (float)GRB_N;
                g->wph += g->winc;
                if (g->wph >= 256.0f)
                    g->on = 0;
            }
            gl *= g_mix;
            grr *= g_mix;
        }
        /* the delay: in at half rate, out interpolated between its steps */
        if (now_s & 1u) {
            delay_tick((d_in + (l + r) * 0.5f + (gl + grr) * 0.5f) * 0.5f);
            fr = 0.0f;
        } else {
            d_in = (l + r) * 0.5f + (gl + grr) * 0.5f;
            fr = 0.5f;
        }
        dlo = (d_prev_l + (d_cur_l - d_prev_l) * fr) * d_mix;
        dro = (d_prev_r + (d_cur_r - d_prev_r) * fr) * d_mix;
        reverb(((l + r) * 0.5f + (gl + grr) * 0.8f + (dlo + dro) * 0.5f) * rsend * 0.6f, &wl, &wr);
        l = l + gl + dlo * (0.5f + 0.5f * width) + dro * (0.5f - 0.5f * width) + wl;
        r = r + grr + dro * (0.5f + 0.5f * width) + dlo * (0.5f - 0.5f * width) + wr;
        fx_out(&l, &r);                                 /* the filter and the tape, on everything */
        l *= 0.55f * gain;
        r *= 0.55f * gain;
        out[2 * j] = (int32_t)(fm_tanhf(l) * 8300000.0f);
        out[2 * j + 1] = (int32_t)(fm_tanhf(r) * 8300000.0f);
        now_s++;
    }
}

void km_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    const float gain = (float)gain_q12 * (1.0f / 4096.0f) * 2.0f;
    int32_t *const out0 = out, pk = 0;
    const uint32_t n0 = n;
    uint32_t j;
    drain();
    if (km_ext && ++ext_age > 86u)                      /* ~0.5 s without a tick: back to its own tempo */
        km_ext = 0;
    while (n) {
        uint32_t k = n > BLK ? BLK : n;
        int i;
        pat_chunk(k);
        for (i = 0; i < NPEND; i++)                     /* the rolls' plucks that are due */
            if (pend[i].on && (int32_t)(pend[i].due - now_s) <= 0) {
                pend[i].on = 0;
                pluck(pend[i].cents, pend[i].vel, pend[i].pan);
            }
        render_chunk(out, k, gain);
        out += 2 * k;
        n -= k;
    }
    for (j = 0; j < 2u * n0; j++) {                     /* how loud it is, tails and all (UI: autosave) */
        int32_t a = out0[j] < 0 ? -out0[j] : out0[j];
        pk = a > pk ? a : pk;
    }
    km_out_level = (float)pk * (1.0f / 8388608.0f);
}

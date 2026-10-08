/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument on the host (no libm: the device has none, so neither does this test):
 *   the music: scales, the three layouts, the black keys' chords
 *   the tines: pitch of every material within a few cents, decay order, no NaN, bounded peaks,
 *              damping, re-pluck of the same tine reusing its voice, voice stealing, rolls
 *   the FX: delay echoes where Time says, grains sounding and freezing, buzz and wah moving
 * and one WAV per material in build/host/materials/ (a C major roll), for listening.
 *   kalimba_test [OUTDIR] */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "kalimba.h"
#include "fastmath.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static int32_t blk[2 * 256];
#define MAXS (44100 * 4)
static float bufl[MAXS], bufr[MAXS];

/* render n frames into bufl / bufr from offset at; peak and NaN check */
static float peak;
static int nan_seen;
static uint32_t master = 4096 / 2;                /* the MASTER pot, Q12 */
static void render(uint32_t at, uint32_t n)
{
    uint32_t k;
    while (n) {
        uint32_t m = n > 256 ? 256 : n;
        km_render(blk, m, master);
        for (k = 0; k < m; k++) {
            float l = (float)blk[2 * k] / 8388608.0f, r = (float)blk[2 * k + 1] / 8388608.0f;
            if (l != l || r != r)
                nan_seen = 1;
            if (fm_fabsf(l) > peak)
                peak = fm_fabsf(l);
            if (fm_fabsf(r) > peak)
                peak = fm_fabsf(r);
            if (at + k < MAXS) {
                bufl[at + k] = l;
                bufr[at + k] = r;
            }
        }
        at += m;
        n -= m;
    }
}

static void fresh(void)
{
    km_init();
    km_set(P_REVERB, 0);
    km_set(P_BODY, BODY_NONE);
    peak = 0.0f;
    nan_seen = 0;
}

/* the DTFT magnitude of bufl[a..a+n) at f Hz, Hann window */
static float mag(uint32_t a, uint32_t n, float f)
{
    float re = 0.0f, im = 0.0f, w = FM_TWO_PI * f / 44100.0f;
    uint32_t i;
    for (i = 0; i < n; i++) {
        float h = 0.5f - 0.5f * fm_cosf(FM_TWO_PI * (float)i / (float)n), x = bufl[a + i] * h;
        re += x * fm_cosf(w * (float)i);
        im -= x * fm_sinf(w * (float)i);
    }
    return fm_sqrtf(re * re + im * im);
}

/* the strongest frequency within +-60 cents of f, to ~0.3 cent */
static float find_pitch(uint32_t a, uint32_t n, float f)
{
    float best = 0.0f, bf = f, c;
    for (c = -60.0f; c <= 60.0f; c += 1.0f) {
        float ff = f * fm_exp2f(c / 1200.0f), m = mag(a, n, ff);
        if (m > best) {
            best = m;
            bf = ff;
        }
    }
    f = bf;
    for (c = -1.0f; c <= 1.0f; c += 0.1f) {
        float ff = f * fm_exp2f(c / 1200.0f), m = mag(a, n, ff);
        if (m > best) {
            best = m;
            bf = ff;
        }
    }
    return bf;
}

static float cents_off(float got, float want) { return 1200.0f * fm_log2f(got / want); }

static float rms(const float *b, uint32_t a, uint32_t n)
{
    float s = 0.0f;
    uint32_t i;
    for (i = 0; i < n; i++)
        s += b[a + i] * b[a + i];
    return fm_sqrtf(s / (float)n);
}

static void wav_write(const char *path, uint32_t n)
{
    FILE *f = fopen(path, "wb");
    uint32_t i, v;
    uint16_t h16;
    if (!f)
        return;
    fwrite("RIFF", 1, 4, f);
    v = 36 + n * 4;
    fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f);
    v = 16;
    fwrite(&v, 4, 1, f);
    h16 = 1;
    fwrite(&h16, 2, 1, f);
    h16 = 2;
    fwrite(&h16, 2, 1, f);
    v = 44100;
    fwrite(&v, 4, 1, f);
    v = 44100 * 4;
    fwrite(&v, 4, 1, f);
    h16 = 4;
    fwrite(&h16, 2, 1, f);
    h16 = 16;
    fwrite(&h16, 2, 1, f);
    fwrite("data", 1, 4, f);
    v = n * 4;
    fwrite(&v, 4, 1, f);
    for (i = 0; i < n; i++) {
        int16_t s[2];
        s[0] = (int16_t)(fm_clampf(bufl[i], -1.0f, 1.0f) * 32767.0f);
        s[1] = (int16_t)(fm_clampf(bufr[i], -1.0f, 1.0f) * 32767.0f);
        fwrite(s, 2, 2, f);
    }
    fclose(f);
}

static void test_music(void)
{
    int i, c[4], n;
    char b[12];
    /* Tine: C4 in the middle (w8), D4 left of it, E4 right of it: the 17-key kalimba in C */
    CHECK(km_white_cents(LAY_TINE, 0, 0, 0, 8) == 6000, "tine w8 %d", km_white_cents(LAY_TINE, 0, 0, 0, 8));
    CHECK(km_white_cents(LAY_TINE, 0, 0, 0, 7) == 6200, "tine w7");
    CHECK(km_white_cents(LAY_TINE, 0, 0, 0, 9) == 6400, "tine w9");
    CHECK(km_white_cents(LAY_TINE, 0, 0, 0, 0) == 8600, "tine w0 (degree 15: D6) %d", km_white_cents(LAY_TINE, 0, 0, 0, 0));
    CHECK(km_white_cents(LAY_TINE, 0, 0, 0, 15) == 6000 + 2400, "tine w15 (C6)");
    for (i = 1; i < 8; i++) {                    /* the V: each step out from the middle goes up */
        CHECK(km_white_cents(LAY_TINE, 0, 0, 0, 7 - i) > km_white_cents(LAY_TINE, 0, 0, 0, 8 - i), "tine left V %d", i);
        CHECK(km_white_cents(LAY_TINE, 0, 0, 0, 8 + i) > km_white_cents(LAY_TINE, 0, 0, 0, 7 + i), "tine right V %d", i);
    }
    CHECK(km_white_cents(LAY_TINE, 3, 9, 0, 8) == 5700, "A minor-penta: A3 in the middle %d", km_white_cents(LAY_TINE, 3, 9, 0, 8));
    CHECK(km_white_cents(LAY_TINE, 0, 5, 0, 8) == 6500 && km_white_cents(LAY_TINE, 0, 6, 0, 8) == 5400, "keys fold at F#");
    CHECK(km_white_cents(LAY_TINE, 2, 0, 0, 5) == 7200, "penta wraps at 5 (w5 is degree 5)");
    /* Mirror: the same V right for left: C4 on w7, D4 right of it, E4 left of it */
    CHECK(km_white_cents(LAY_MIRROR, 0, 0, 0, 7) == 6000, "mirror w7 %d", km_white_cents(LAY_MIRROR, 0, 0, 0, 7));
    CHECK(km_white_cents(LAY_MIRROR, 0, 0, 0, 8) == 6200, "mirror w8");
    CHECK(km_white_cents(LAY_MIRROR, 0, 0, 0, 6) == 6400, "mirror w6");
    CHECK(km_white_cents(LAY_MIRROR, 0, 0, 0, 0) == 8400 && km_white_cents(LAY_MIRROR, 0, 0, 0, 15) == 8600, "mirror ends (C6, D6)");
    for (i = 0; i < 16; i++)
        CHECK(km_white_cents(LAY_MIRROR, 4, 2, 1, i) == km_white_cents(LAY_TINE, 4, 2, 1, 15 - i), "mirror of tine %d", i);
    /* Keyboard: the keys as printed, F3 on the lowest, chromatic, transposed */
    for (i = 0; i < 27; i++)
        CHECK(km_keyboard_cents(0, 0, i) == 5300 + 100 * i, "keyboard key %d", i);
    CHECK(km_keyboard_cents(0, 0, 26) == 7900, "the top key is G5");
    CHECK(km_keyboard_cents(2, 0, 0) == 5500 && km_keyboard_cents(-12, 0, 0) == 4100, "transpose");
    CHECK(km_keyboard_cents(-1, 1, 0) == 6400, "transpose and octave");
    CHECK(km_degree_cents(0, -1) == -100, "degree -1 of major (B below) %d", km_degree_cents(0, -1));
    CHECK(km_degree_cents(2, -5) == -1200, "degree -5 of penta");
    /* chords in C major: C Dm Em F G Am Bdim, then C7?.. the diatonic sevenths: Cmaj7 shows as C7 here */
    n = km_chord_cents(0, 0, 0, 0, c);
    CHECK(n == 4 && c[0] == 6000 && c[1] == 6400 && c[2] == 6700 && c[3] == 7200, "C chord");
    n = km_chord_cents(0, 0, 0, 4, c);
    CHECK(n == 4 && c[0] == 6700 && c[1] == 7100 && c[2] == 7400 && c[3] == 7900, "G chord");
    n = km_chord_cents(0, 0, 0, 9, c);
    CHECK(n == 4 && c[3] - c[0] == 1000, "V7 has a minor seventh");
    CHECK(!strcmp(km_chord_name(0, 0, 1, b), "Dm"), "Dm %s", b);
    CHECK(!strcmp(km_chord_name(0, 0, 6, b), "Bdim"), "Bdim %s", b);
    CHECK(!strcmp(km_chord_name(0, 7, 0, b), "G"), "G %s", b);
    CHECK(!strcmp(km_chord_name(1, 9, 0, b), "Am"), "Am %s", b);
    CHECK(!strcmp(km_chord_name(0, 0, 7, b), "CM7"), "CM7 %s", b);
    CHECK(!strcmp(km_chord_name(0, 0, 9, b), "G7"), "G7 %s", b);
    CHECK(!strcmp(km_chord_name(0, 0, 10, b), "Am7"), "Am7 %s", b);
    for (i = 0; i < P_NPARAMS; i++) {
        const km_param_t *p = km_param_info(i);
        km_param_text(i, p->hi, b);
        CHECK(p->lo <= p->def && p->def <= p->hi && b[0], "param %d", i);
    }
}

static void test_pitch(void)
{
    int m;
    static const int NOTES[3] = {6000, 6900, 8400};
    for (m = 0; m < MAT_N; m++) {
        int i;
        for (i = 0; i < 3; i++) {
            float want = 8.17579892f * fm_exp2f((float)NOTES[i] / 1200.0f), got, off;
            fresh();
            km_set(P_MATERIAL, m);
            km_pluck(NOTES[i], 100, 0, 0);
            render(0, 16384);
            got = find_pitch(2048, 8192, want);
            off = cents_off(got, want);
            CHECK(off > -4.0f && off < 4.0f, "%s %d: %.2f Hz, %.1f cents off", km_material(m)->name, NOTES[i], (double)got,
                  (double)off);
            CHECK(!nan_seen && peak < 0.9f && peak > 0.02f, "%s level %.3f", km_material(m)->name, (double)peak);
        }
    }
}

static void test_decay(void)
{
    float steel, bamboo, d_short, d_long;
    fresh();
    km_pluck(6000, 100, 0, 0);
    render(0, 44100 * 2);
    steel = rms(bufl, 44100, 4410);
    fresh();
    km_set(P_MATERIAL, MAT_BAMBOO);
    km_pluck(6000, 100, 0, 0);
    render(0, 44100 * 2);
    bamboo = rms(bufl, 44100, 4410);
    CHECK(steel > 20.0f * bamboo, "steel rings longer than bamboo (%.5f %.5f)", (double)steel, (double)bamboo);
    fresh();
    km_set(P_DECAY, 0);
    km_pluck(6000, 100, 0, 0);
    render(0, 44100);
    d_short = rms(bufl, 30000, 4410);
    fresh();
    km_set(P_DECAY, 100);
    km_pluck(6000, 100, 0, 0);
    render(0, 44100);
    d_long = rms(bufl, 30000, 4410);
    CHECK(d_long > 4.0f * d_short, "Decay (%.5f %.5f)", (double)d_short, (double)d_long);
}

static void test_damp(void)
{
    float ring, damped;
    fresh();
    km_pluck(6000, 100, 0, 0);
    render(0, 44100);
    ring = rms(bufl, 30000, 4410);
    fresh();
    km_pluck(6000, 100, 0, 0);
    render(0, 4410);
    km_damp(6000);
    render(4410, 44100 - 4410);
    damped = rms(bufl, 30000, 4410);
    CHECK(damped < ring * 0.05f, "damp (%.5f vs %.5f)", (double)damped, (double)ring);
    /* sustain holds a damp back */
    fresh();
    km_sustain(1);
    km_pluck(6000, 100, 0, 0);
    render(0, 4410);
    km_damp(6000);
    render(4410, 44100 - 4410);
    CHECK(rms(bufl, 30000, 4410) > ring * 0.8f, "sustain kept it ringing");
    km_sustain(0);
    render(0, 44100);
    CHECK(rms(bufl, 30000, 4410) < ring * 0.05f, "pedal up damps");
    /* damp all */
    fresh();
    km_pluck(6000, 100, 0, 0);
    km_pluck(6700, 100, 0, 0);
    render(0, 4410);
    km_damp_all();
    render(0, 44100);
    CHECK(rms(bufl, 30000, 4410) < ring * 0.05f, "damp all");
}

static void test_voices(void)
{
    int i, n = 0;
    fresh();
    km_pluck(6000, 100, 0, 0);
    render(0, 1000);
    km_pluck(6000, 100, 0, 0);                   /* the same tine: the same voice */
    render(0, 1000);
    for (i = 0; i < KM_NVOICE; i++)
        n += km_voice_cents[i] == 6000;
    CHECK(n == 1, "one voice per tine (%d)", n);
    for (i = 0; i < 30; i++) {                   /* more tines than voices: no crash, all bounded */
        km_pluck(4800 + 100 * i, 127, (i & 1) ? 100 : -100, 0);
        render(0, 300);
    }
    render(0, 44100);
    CHECK(!nan_seen && peak < 1.0f, "30 plucks: peak %.3f", (double)peak);
    for (i = 0; i < KM_NVOICE; i++)
        CHECK(km_voice_level[i] == km_voice_level[i], "level not NaN");
}

static void test_roll(void)
{
    uint32_t i, first = 0, last = 0;
    fresh();
    km_pluck(6000, 100, 0, 0);
    km_pluck(6400, 100, 0, 50);                  /* 50 ms later */
    for (i = 0; i < 40; i++) {
        int k, on = 0;
        render(0, 64);
        for (k = 0; k < KM_NVOICE; k++)
            on += km_voice_cents[k] == 6400;
        if (on && !first)
            first = (i + 1) * 64;
        last = (i + 1) * 64;
    }
    CHECK(first >= 2205 && first <= 2205 + 128, "the roll's second note at %u samples (2205 wanted, in 64-sample chunks)", first);
    (void)last;
    fresh();
    km_pluck(6000, 100, 0, 100);
    km_damp(6000);                               /* let go before it sounds: it never does */
    render(0, 8820);
    CHECK(peak < 1e-4f, "a cancelled roll note sounded (%.5f)", (double)peak);
}

static void delay_check(void)
{
    float before, at;
    km_set(P_REVERB, 0);
    km_set(P_GRAIN, 0);
    km_set(P_DELAY, 100);
    km_set(P_TIME, 200);
    km_set(P_FEEDBACK, 50);
    km_set(P_MATERIAL, MAT_BAMBOO);              /* short: the echo stands out */
    km_set(P_DECAY, 0);
    render(0, 44100);                            /* Time glides to 200 ms */
    km_pluck(7200, 120, 0, 0);
    render(0, 44100);
    before = rms(bufl, 6000, 1000);
    at = rms(bufl, 8820, 1000);
    CHECK(at > 3.0f * before, "an echo at 200 ms (%.5f before, %.5f at)", (double)before, (double)at);
    at = rms(bufr, 17640, 1000);
    CHECK(at > 3.0f * rms(bufr, 13000, 1000), "the second echo, right, at 400 ms (%.5f)", (double)at);
}

static void test_delay(void)
{
    fresh();
    delay_check();
}

static void test_grains(void)
{
    float dry, wet;
    int i, on = 0;
    fresh();
    km_set(P_MATERIAL, MAT_BAMBOO);
    km_pluck(7200, 120, 0, 0);
    render(0, 44100 * 2);
    dry = rms(bufl, 60000, 20000);
    fresh();
    km_set(P_MATERIAL, MAT_BAMBOO);
    km_set(P_GRAIN, 100);
    km_set(P_GSPRAY, 80);
    km_pluck(7200, 120, 0, 0);
    render(0, 22050);
    km_freeze(1);                                /* the buffer keeps the pluck */
    for (i = 0; i < 40; i++) {
        render(22050 + (uint32_t)i * 2205, 2205);
        on |= km_grains_on;
    }
    wet = rms(bufl, 60000, 20000);
    CHECK(on && wet > 10.0f * dry, "frozen grains keep sounding (%.5f vs dry %.5f)", (double)wet, (double)dry);
    CHECK(km_frozen == 1, "frozen");
    CHECK(!nan_seen && peak < 1.0f, "grains peak %.3f", (double)peak);
    for (i = 0; i < GP_N; i++) {                 /* every pitch: no NaN, bounded */
        km_set(P_GPITCH, i);
        render(0, 22050);
    }
    CHECK(!nan_seen && peak < 1.0f, "grain pitches peak %.3f", (double)peak);
}

static void test_body(void)
{
    float open_lvl;
    fresh();
    km_set(P_BODY, BODY_BOX);
    render(0, 256);
    open_lvl = km_hole_open;
    km_hole(HOLE_MIDI, 127);
    render(0, 256);
    CHECK(open_lvl > 0.99f && km_hole_open < 0.15f, "the hole: %.2f open, %.2f covered", (double)open_lvl, (double)km_hole_open);
    km_hole(HOLE_MIDI, 0);
    km_set(P_WAHRATE, 60);
    {
        float lo = 1.0f, hi = 0.0f;
        int i;
        for (i = 0; i < 200; i++) {
            render(0, 256);
            lo = fm_minf(lo, km_hole_open);
            hi = fm_maxf(hi, km_hole_open);
        }
        CHECK(hi - lo > 0.5f, "the wah LFO moves the hole (%.2f..%.2f)", (double)lo, (double)hi);
    }
    fresh();
    km_set(P_BODY, BODY_GOURD);
    km_set(P_BUZZ, 100);
    km_pluck(4800, 127, 0, 0);
    {
        float bz = 0.0f;
        int i;
        for (i = 0; i < 20; i++) {
            render(0, 256);
            bz = fm_maxf(bz, km_buzz_level);
        }
        CHECK(bz > 0.01f, "the buzzers rattle (%.4f)", (double)bz);
    }
    CHECK(!nan_seen && peak < 1.0f, "body peak %.3f", (double)peak);
}

/* hours on: the delay and grain rings must not slow down or lose precision (their positions once grew
 * without bound: CPU climbed and grains stalled after ~13 minutes). 20 minutes, dense grains, echoes. */
static void test_long_run(void)
{
    uint32_t i, blocks = 20u * 60u * 44100u / 256u;
    clock_t t0 = 0, t_first = 0, t_last = 0;
    fresh();
    km_set(P_GRAIN, 100);
    km_set(P_GDENS, 60);
    km_set(P_GSPRAY, 100);
    km_set(P_DELAY, 50);
    km_set(P_FEEDBACK, 60);
    for (i = 0; i < blocks; i++) {
        if (i % 200u == 0u)
            km_pluck(6000 + (int)(i / 200u % 24u) * 100, 100, 0, 0);
        if (i == 0u || i == blocks - 1723u)
            t0 = clock();
        km_render(blk, 256, 2048);
        if (i == 1722u)
            t_first = clock() - t0;
        if (i == blocks - 1u)
            t_last = clock() - t0;
    }
    CHECK(t_last < t_first * 3 / 2 + CLOCKS_PER_SEC / 50, "the last 10 s took %ld ticks, the first %ld", (long)t_last,
          (long)t_first);
    km_damp_all();
    km_set(P_GRAIN, 0);
    render(0, 44100 * 3);                        /* the echoes and the tines die away */
    CHECK(!nan_seen, "NaN after 20 minutes");
    delay_check();                               /* the echo still lands where Time says, no km_init */
}

/* the bodies sound different (they once only nudged the level): the same phrase through each, its
 * loudness, its brightness (the energy of the signal's slope against the signal's) and its low end
 * (below ~180 Hz). Board thin and bright, Box warmer and darker, Gourd boomiest and darkest; all
 * about as loud as the bare tine, so a body changes the character and not the volume */
static void body_measure(int body, float *loud, float *bright, float *low)
{
    uint32_t i;
    float e = 0.0f, d = 0.0f, lo = 0.0f, lp = 0.0f, lp2 = 0.0f, lp3 = 0.0f, prev = 0.0f;
    fresh();
    km_set(P_BODY, body);
    for (i = 0; i < 8; i++)
        km_pluck(6000 + km_degree_cents(0, (int)i), 100, 0, 220 * (int)i);
    km_pluck(4800, 100, 0, 2000);
    km_pluck(5500, 100, 0, 2030);
    render(0, 44100 * 3);
    for (i = 0; i < 44100 * 3; i++) {
        float x = bufl[i];
        lp += 0.025f * (x - lp);                 /* three poles at ~180 Hz: the air modes' octave */
        lp2 += 0.025f * (lp - lp2);
        lp3 += 0.025f * (lp2 - lp3);
        e += x * x;
        d += (x - prev) * (x - prev);
        lo += lp3 * lp3;
        prev = x;
    }
    *loud = 10.0f * fm_log2f(e) * 0.30103f;
    *bright = 10.0f * fm_log2f(d / e) * 0.30103f;
    *low = 10.0f * fm_log2f(lo / e) * 0.30103f;
}

/* the sharpest edge (fourth difference) in bufl[a, a+n) */
static float edge_max(uint32_t a, uint32_t n)
{
    uint32_t i;
    float m = 0.0f;
    for (i = a + 4; i < a + n; i++)
        m = fm_maxf(m, fm_fabsf(bufl[i] - 4.0f * bufl[i - 1] + 6.0f * bufl[i - 2] - 4.0f * bufl[i - 3] + bufl[i - 4]));
    return m;
}

static void test_bodies(void)
{
    float l[BODY_N], b[BODY_N], lo[BODY_N];
    int i;
    for (i = 0; i < BODY_N; i++) {
        body_measure(i, &l[i], &b[i], &lo[i]);
        CHECK(fm_fabsf(l[i] - l[BODY_NONE]) < 3.0f, "body %d loudness %+.1f dB from none", i, (double)(l[i] - l[BODY_NONE]));
    }
    CHECK(b[BODY_BOARD] > b[BODY_BOX] + 2.0f && b[BODY_BOX] > b[BODY_GOURD] + 1.0f && b[BODY_NONE] > b[BODY_GOURD] + 3.0f,
          "brightness: board %.1f, none %.1f, box %.1f, gourd %.1f", (double)b[BODY_BOARD], (double)b[BODY_NONE],
          (double)b[BODY_BOX], (double)b[BODY_GOURD]);
    CHECK(lo[BODY_GOURD] > lo[BODY_BOARD] + 3.0f && lo[BODY_BOX] > lo[BODY_BOARD] + 3.0f,
          "low end: board %.1f, box %.1f, gourd %.1f dB", (double)lo[BODY_BOARD], (double)lo[BODY_BOX], (double)lo[BODY_GOURD]);
}

/* What a pluck adds to what was already sounding: the session rendered twice, with and without it
 * (the engine is deterministic: no nail noise at Hardness 0, no grains, no buzz), the difference kept.
 * That is the new pluck, plus whatever happened to the tine it stole or plucked again. Its sharpest
 * edge, by the fourth difference: a pop (a step, a corner) keeps it, a tone hardly does (a fading low
 * tine by its frequency^4: ~1e-6 of its level), so a pop is an edge far sharper than a soft pluck's. */
static float with_out[44100];
static int corner_glide;                           /* Glide for pluck_corner (0: the mechanisms alone) */
static float pluck_corner(int n_ring, int ring_from, int ring_vel, int cents, int vel)
{
    int pass, i;
    float m = 0.0f;
    for (pass = 0; pass < 2; pass++) {
        fresh();
        km_set(P_HARD, 0);
        km_set(P_DECAY, 100);
        km_set(P_GLIDE, corner_glide);
        for (i = 0; i < n_ring; i++)
            km_pluck(ring_from + 100 * i, ring_vel, 0, 0);
        render(0, 22050);
        if (pass)
            km_pluck(cents, vel, 0, 20);         /* 20 ms in: the moment it lands is inside the window */
        render(0, 8820);
        if (!pass)
            for (i = 0; i < 8820; i++)
                with_out[i] = bufl[i];
    }
    for (i = 4; i < 8820; i++) {                 /* (MASTER low: the output's limiter stays linear) */
        float d[5];
        int k;
        for (k = 0; k < 5; k++)
            d[k] = bufl[i - k] - with_out[i - k];
        m = fm_maxf(m, fm_fabsf(d[0] - 4.0f * d[1] + 6.0f * d[2] - 4.0f * d[3] + d[4]));
    }
    return m;
}

/* no pops: a stolen voice fades out, a re-plucked tine is damped by the thumb through its own decay;
 * either way no sharper than the same soft pluck on a free tine (they were a cut and a step) */
static void test_no_pops(void)
{
    float alone, steal, again, fresh_tine;
    master = 4096 / 16;
    alone = pluck_corner(0, 0, 0, 8400, 50);
    steal = pluck_corner(KM_NVOICE, 4300, 127, 8400, 50);              /* twelve ring: one is stolen */
    again = pluck_corner(1, 6000, 127, 6000, 50);                      /* a loud tine plucked again, softly */
    fresh_tine = pluck_corner(1, 6000, 127, 6100, 50);                 /* the same, on the tine beside it */
    CHECK(steal < alone * 3.0f, "a stolen voice pops: edge %.6f, the pluck alone %.6f", (double)steal, (double)alone);
    CHECK(again < fresh_tine * 3.0f, "a re-plucked tine pops: edge %.6f, a fresh tine %.6f", (double)again, (double)fresh_tine);
    printf("  edges: pluck alone %.6f, stealing %.6f | fresh tine %.6f, plucked again %.6f\n", (double)alone,
           (double)steal, (double)fresh_tine, (double)again);
    /* the glide settles in a step a chunk: on a loud ringing tine plucked again, each leaves a trace of an
     * edge (~1e-6 at full scale per step): bounded at full Glide far below the old pops (0.07, 0.009) */
    corner_glide = 100;
    again = pluck_corner(1, 6000, 127, 6000, 50);
    fresh_tine = pluck_corner(1, 6000, 127, 6100, 50);
    corner_glide = 0;
    CHECK(again < fresh_tine * 10.0f, "Glide 100 on a re-plucked tine: edge %.6f, a fresh tine %.6f", (double)again,
          (double)fresh_tine);
    printf("  glide 100: fresh tine %.6f, plucked again %.6f\n", (double)fresh_tine, (double)again);
    master = 4096 / 2;
}

/* the pitch at bufl[a, a+n) near f (find_pitch), in cents from f */
static float cents_at(uint32_t a, uint32_t n, float f) { return cents_off(find_pitch(a, n, f), f); }

static void test_glide_just(void)
{
    float c4 = 261.6256f, early, late;
    int i;
    /* glide: a hard pluck starts sharp and settles; none at Glide 0 */
    fresh();
    km_set(P_GLIDE, 100);
    km_set(P_HARD, 100);
    km_set(P_MATERIAL, MAT_BAMBOO);                  /* one clean mode, no twin */
    km_pluck(6000, 127, 0, 0);
    render(0, 22050);
    early = cents_at(0, 1024, c4);
    late = cents_at(8820, 4096, c4);
    CHECK(early > late + 8.0f && fm_fabsf(late) < 3.0f, "glide: %.1f cents at once, %.1f later", (double)early, (double)late);
    fresh();
    km_set(P_GLIDE, 0);
    km_set(P_HARD, 100);
    km_set(P_MATERIAL, MAT_BAMBOO);
    km_pluck(6000, 127, 0, 0);
    render(0, 22050);
    CHECK(fm_fabsf(cents_at(0, 1024, c4)) < 3.0f, "no glide at 0: %.1f cents", (double)cents_at(0, 1024, c4));
    /* just intonation, over C: the major third 386, the fifth 702, the minor seventh 1018; off-grid kept */
    CHECK(km_just_cents(6400, 6000) == 6386 && km_just_cents(6700, 6000) == 6702 && km_just_cents(7000, 6000) == 7018,
          "just: E %d G %d Bb %d", km_just_cents(6400, 6000), km_just_cents(6700, 6000), km_just_cents(7000, 6000));
    CHECK(km_just_cents(5900, 6000) == 5888 && km_just_cents(6000, 6000) == 6000 && km_just_cents(7200, 6000) == 7200,
          "just below and at the octave");
    CHECK(km_just_cents(6165, 6000) == 6165, "an Mbira degree is left alone");
    for (i = 0; i < 12; i++)
        CHECK(fm_fabsf((float)(km_just_cents(6000 + 100 * i, 6000) - 6000 - 100 * i)) < 20.0f, "just within 20 cents %d", i);
}

/* Worn: each tine its own quirks, the same every time; none at 0 */
static float worn_pitch(int worn, int cents)
{
    fresh();
    km_set(P_MATERIAL, MAT_BAMBOO);                  /* one clean mode, no twin */
    km_set(P_GLIDE, 0);
    km_set(P_WORN, worn);
    km_pluck(cents, 100, 0, 0);
    render(0, 16384);
    return cents_off(find_pitch(2048, 8192, 8.17579892f * fm_exp2f((float)cents / 1200.0f)),
                     8.17579892f * fm_exp2f((float)cents / 1200.0f));
}

static void test_worn(void)
{
    static const int T[6] = {6000, 6200, 6400, 6700, 6900, 7200};
    float off[6], lo = 99.0f, hi = -99.0f, e1, e2;
    int i;
    for (i = 0; i < 6; i++) {
        CHECK(fm_fabsf(worn_pitch(0, T[i])) < 1.0f, "Worn 0: tine %d in tune", T[i]);
        off[i] = worn_pitch(100, T[i]);
        lo = fm_minf(lo, off[i]);
        hi = fm_maxf(hi, off[i]);
        CHECK(fm_fabsf(off[i]) < 13.0f, "Worn 100: tine %d within 12 cents (%.1f)", T[i], (double)off[i]);
    }
    CHECK(hi - lo > 4.0f, "Worn 100: the tines differ (%.1f .. %.1f cents)", (double)lo, (double)hi);
    CHECK(fm_fabsf(worn_pitch(100, T[2]) - off[2]) < 0.3f, "Worn: the same tine, the same quirk");
    /* the sustains differ too: two tines' ring a second on, against the clean instrument's */
    fresh();
    km_set(P_WORN, 0);
    km_pluck(6000, 100, 0, 0);
    render(0, 44100 * 2);
    e1 = rms(bufl, 44100, 8820);
    fresh();
    km_set(P_WORN, 100);
    km_pluck(6000, 100, 0, 0);
    render(0, 44100 * 2);
    e2 = rms(bufl, 44100, 8820);
    CHECK(fm_fabsf(20.0f * fm_log2f(e2 / e1) * 0.30103f) > 0.3f, "Worn: a tine's sustain changes (%.2f dB)",
          (double)(20.0f * fm_log2f(e2 / e1) * 0.30103f));
}

/* the effects on one ringing tine: what each does, that none clicks as it comes in or goes */
static void test_color(void)
{
    float c4 = 261.6256f, lo = 1e9f, hi = -1e9f;
    uint32_t i;
    /* tape: the pitch wavers */
    fresh();
    km_set(P_TAPE, 100);
    km_set(P_DECAY, 100);
    km_set(P_MATERIAL, MAT_BAMBOO);
    km_set(P_GLIDE, 0);
    for (i = 0; i < 4; i++) {                    /* the same tine, plucked again and again: four looks */
        float c;
        km_pluck(6000, 110, 0, 0);
        render(0, 11025);
        c = cents_at(3000, 8000, c4);
        lo = fm_minf(lo, c);
        hi = fm_maxf(hi, c);
    }
    CHECK(hi - lo > 3.0f, "tape wow: the pitch moved %.1f cents", (double)(hi - lo));
    CHECK(!nan_seen && peak < 1.0f, "tape peak %.3f", (double)peak);
    /* filter: the low-pass takes a high tine down, the high-pass a low one; neither touches the other */
    {
        static const int SET[3] = {0, -80, 80}, NOTE[2] = {8400, 4800};
        float lv[3][2];
        int k, nn;
        master = 4096 / 16;                      /* the output's limiter linear */
        for (k = 0; k < 3; k++)
            for (nn = 0; nn < 2; nn++) {
                fresh();
                km_set(P_FILTER, SET[k]);
                render(0, 4410);                 /* the filter in place before the pluck */
                km_pluck(NOTE[nn], 110, 0, 0);
                render(0, 22050);
                lv[k][nn] = 20.0f * fm_log2f(rms(bufl, 0, 22050)) * 0.30103f;
            }
        master = 4096 / 2;
        CHECK(lv[1][0] < lv[0][0] - 8.0f && fm_fabsf(lv[1][1] - lv[0][1]) < 3.0f,
              "low-pass -80: C7 %+.1f dB, C3 %+.1f dB", (double)(lv[1][0] - lv[0][0]), (double)(lv[1][1] - lv[0][1]));
        CHECK(lv[2][1] < lv[0][1] - 10.0f && fm_fabsf(lv[2][0] - lv[0][0]) < 3.0f,
              "high-pass +80: C3 %+.1f dB, C7 %+.1f dB", (double)(lv[2][1] - lv[0][1]), (double)(lv[2][0] - lv[0][0]));
    }
    /* chorus: left and right part */
    {
        double sl = 0, sr = 0, slr = 0;
        fresh();
        km_set(P_CHORUS, 100);
        km_set(P_WIDTH, 0);                      /* a mono source: only the chorus can widen it */
        km_pluck(6700, 110, 0, 0);
        render(0, 44100);
        for (i = 4410; i < 44100; i++) {
            sl += (double)bufl[i] * (double)bufl[i];
            sr += (double)bufr[i] * (double)bufr[i];
            slr += (double)bufl[i] * (double)bufr[i];
        }
        CHECK(slr / (sl > sr ? sl : sr) < 0.97, "chorus widens: correlation %.3f", slr / (sl > sr ? sl : sr));
    }
    /* lo-fi: hiss while it plays, silence after */
    fresh();
    km_set(P_LOFI, 100);
    km_set(P_MATERIAL, MAT_BAMBOO);
    km_pluck(6000, 110, 0, 0);
    render(0, 44100 * 4);
    CHECK(rms(bufl, 44100 * 3, 44100) < 1e-4f, "lo-fi hiss dies with the music: %.6f", (double)rms(bufl, 44100 * 3, 44100));
    /* none clicks in or out: a ringing tine, each effect switched on, then off, mid-note. The edges
     * are measured on the change and compared with the same effect settled (Lo-fi's hiss has edges of
     * its own; the filter changes the tine's), and with the tine alone */
    {
        static const int P[5] = {P_TAPE, P_LOFI, P_CHORUS, P_FILTER, P_FILTER};
        static const int V[5] = {100, 100, 100, -60, 60};
        int k;
        corner_glide = 0;
        for (k = 0; k < 5; k++) {
            float on, off, alone, settled;
            master = 4096 / 16;
            fresh();
            km_set(P_DECAY, 100);
            km_pluck(6000, 110, 0, 0);
            render(0, 22050);
            alone = edge_max(11025, 11025);
            km_set(P[k], V[k]);
            render(0, 22050);
            on = edge_max(0, 4410);                  /* the 100 ms after it comes in */
            settled = edge_max(11025, 11025);
            km_set(P[k], 0);
            render(0, 22050);
            off = edge_max(0, 4410);
            master = 4096 / 2;
            CHECK(fm_maxf(on, off) < fm_maxf(alone, settled) * 3.0f + 2e-6f,
                  "%s %d in / out mid-note: edge %.6f / %.6f, settled %.6f, the tine alone %.6f", km_param_info(P[k])->name,
                  V[k], (double)on, (double)off, (double)settled, (double)alone);
        }
        /* and the filter straight through the middle, low-pass to high-pass */
        master = 4096 / 16;
        fresh();
        km_set(P_DECAY, 100);
        km_set(P_FILTER, -40);
        km_pluck(6000, 110, 0, 0);
        render(0, 22050);
        {
            float a = edge_max(11025, 11025), b;
            km_set(P_FILTER, 40);
            render(0, 8820);
            b = edge_max(0, 8820);
            master = 4096 / 2;
            CHECK(b < a * 3.0f + 2e-6f, "filter low to high mid-note: edge %.6f, settled %.6f", (double)b, (double)a);
        }
    }
}

/* the pattern: pulses at the tempo (3 a beat), over the chord it is given; MIDI clock leads when it comes */
static void test_pattern(void)
{
    int chord[3] = {6000, 6400, 6700}, i, pulses = 0, last = -1, low = 0, high = 0;
    fresh();
    km_set(P_PTEMPO, 120);                       /* 6 pulses a second */
    km_pool(chord, 3);
    km_pattern(1);
    for (i = 0; i < 344; i++) {                  /* 2 s, in blocks of 256 */
        int v;
        render(0, 256);
        if (km_pat_pulse != last) {
            pulses++;
            last = km_pat_pulse;
        }
        for (v = 0; v < KM_NVOICE; v++) {
            low |= km_voice_cents[v] == 4800;        /* L1: the root an octave down */
            high |= km_voice_cents[v] == 7600;       /* R5: the third an octave up */
        }
    }
    CHECK(km_pat_on && pulses >= 11 && pulses <= 13, "pattern at 120: %d pulses in 2 s (12 wanted)", pulses);
    CHECK(low && high, "the pattern reaches the bass and the treble (%d %d)", low, high);
    km_pattern(0);
    render(0, 256);
    last = km_pat_pulse;
    render(0, 44100);
    CHECK(!km_pat_on && km_pat_pulse == last, "the pattern stops");
    /* MIDI clock: 8 ticks a pulse */
    fresh();
    km_pool(chord, 3);
    km_pattern(1);
    km_clock(KM_CLK_START);
    pulses = 0;
    last = -1;
    for (i = 0; i < 96; i++) {                   /* four beats of clock, as fast as it likes */
        km_clock(KM_CLK_TICK);
        render(0, 64);
        if (km_pat_pulse != last) {
            pulses++;
            last = km_pat_pulse;
        }
    }
    CHECK(km_ext && pulses == 12, "MIDI clock: %d pulses in 96 ticks (12 wanted)", pulses);
    /* a Stop holds it, ticks or no ticks; ARP starts it again */
    km_clock(KM_CLK_STOP);
    render(0, 256);
    last = km_pat_pulse;
    render(0, 44100);
    CHECK(km_pat_pulse == last, "MIDI stop: the pattern held (pulse %d -> %d)", last, km_pat_pulse);
    km_pattern(1);
    render(0, 22050);
    CHECK(km_pat_pulse != last, "ARP after a stop: the pattern runs");
    /* one, two, three notes held: the pattern stays near them (bass an octave down, treble within two up) */
    for (i = 1; i <= 3; i++) {
        int one[3] = {6000, 6400, 6700}, k, lo = 99999, hi = 0;
        fresh();
        km_set(P_PATTERN, PAT_CASCADE);          /* (reaches R6, the highest role) */
        km_set(P_PTEMPO, 200);
        km_pool(one, i);
        km_pattern(1);
        for (k = 0; k < 172; k++) {
            int v;
            render(0, 256);
            for (v = 0; v < KM_NVOICE; v++)
                if (km_voice_cents[v] >= 0) {
                    lo = km_voice_cents[v] < lo ? km_voice_cents[v] : lo;
                    hi = km_voice_cents[v] > hi ? km_voice_cents[v] : hi;
                }
        }
        CHECK(lo >= 6000 - 1200 && hi <= 6000 + 2400, "%d notes: the pattern plays %d..%d cents", i, lo, hi);
    }
    for (i = 0; i < PAT_N; i++) {                /* every pattern: bounded, no NaN */
        fresh();
        km_set(P_PATTERN, i);
        km_set(P_PTEMPO, 200);
        km_pool(chord, 3);
        km_pattern(1);
        render(0, 44100 * 2);
        CHECK(!nan_seen && peak < 1.0f && peak > 0.05f, "pattern %d peak %.3f", i, (double)peak);
    }
}

static void listen(const char *dir)
{
    int m, i, c[4];
    char path[512];
    for (m = 0; m < MAT_N; m++) {
        int n;
        fresh();
        km_set(P_MATERIAL, m);
        km_set(P_BODY, BODY_BOX);
        km_set(P_REVERB, 25);
        for (i = 0; i < 8; i++)                  /* up the C major scale, alternating sides as on the tines */
            km_pluck(6000 + km_degree_cents(0, i), 100, i & 1 ? 50 : -50, 180 * i);
        n = km_chord_cents(0, 0, 0, 5, c);
        for (i = 0; i < n; i++)
            km_pluck(c[i], 90, -60 + 40 * i, 1700 + 35 * i);
        render(0, MAXS);
        snprintf(path, sizeof path, "%s/%s.wav", dir, km_material(m)->name);
        for (i = 0; path[i]; i++)
            if (path[i] == '.' && path[i + 1] != 'w')
                path[i] = '_';
        wav_write(path, MAXS);
    }
    printf("  wrote %s/*.wav (one per material)\n", dir);
}

int main(int argc, char **argv)
{
    test_music();
    test_pitch();
    test_decay();
    test_damp();
    test_voices();
    test_roll();
    test_delay();
    test_grains();
    test_body();
    test_bodies();
    test_no_pops();
    test_glide_just();
    test_worn();
    test_color();
    test_pattern();
    test_long_run();
    if (argc > 1)
        listen(argv[1]);
    CHECK(km_dropped == 0, "dropped commands %u", km_dropped);
    printf(fails ? "kalimba: %d FAILED\n" : "kalimba: all passed\n", fails);
    return fails != 0;
}

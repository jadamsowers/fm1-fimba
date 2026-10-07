/* SPDX-License-Identifier: GPL-3.0-only */
/* FiMba-1 UI: the panel and the screen. Part of the unity build (after gfx.c and project.c); the host
 * simulator includes it too. (The frame of it, knobs, bands and autosave, is FoMni's ui.c.)
 *
 *   white keys     the tines: one each, laid out by Layout (Tine: the kalimba's V, Linear, Split)
 *   black keys     by Black: Chords (a thumb roll of the scale's chords), Sharps (the white key's
 *                  tine a semitone up: a chromatic kalimba's second row) or Perform (palm mute, hole,
 *                  freeze, octave down / up while held, six materials)
 *   PLAY freeze the grains     REC palm mute (every tine)     ARP Release: Ring / Damp
 *   OCT- / OCT+    the octave
 *   HOME ENV FX LFO SEL SEQ GLO   the pages: Tine, Body, Space, Grain, Keys, More, Setup (EDIT: the next)
 *   SELECT material   ALGORITHM scale   PRESETS key   KNOB 1-4 the page's four values
 *   SAVE           saves (it also saves by itself, a few seconds after a change, when quiet) */

enum { V_TINE, V_BODY, V_SPACE, V_GRAIN, V_KEYS, V_MORE, V_SETUP, NVIEWS };
static const char *const VIEW_NAME[NVIEWS] = {"Tine", "Body", "Space", "Grain", "Keys", "More", "Setup"};
static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, B_ENV, B_FX, B_LFO, B_SEL, B_SEQ, B_GLO};
static const uint8_t VIEW_KNOB[NVIEWS][4] = {
    {P_MATERIAL, P_HARD, P_DECAY, P_TONE},
    {P_BODY, P_BUZZ, P_WAH, P_WAHRATE},
    {P_REVERB, P_SIZE, P_DELAY, P_TIME},
    {P_GRAIN, P_GSIZE, P_GDENS, P_GPITCH},
    {P_LAYOUT, P_SCALE, P_KEY, P_BLACK},
    {P_FEEDBACK, P_GSPRAY, P_STRUM, P_RELEASE},
    {P_TUNE, P_MIDICH, P_MIDIOUT, P_WIDTH},
};

#define NWHITE KM_NWHITE
#define NBLACK KM_NBLACK
static const uint8_t WHITE_K[NWHITE] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t BLACK_K[NBLACK] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
/* the white key left of each black key (its Sharps tine is that one, a semitone up) */
static const uint8_t BLACK_LEFT[NBLACK] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};

/* Perform: what each black key does */
enum { PF_MUTE, PF_HOLE, PF_FREEZE, PF_OCTDN, PF_OCTUP, PF_MAT0 };
static const char *const PF_NAME[NBLACK] = {"Mut", "Hol", "Frz", "O-", "O+", "St", "Br", "Bz", "Al", "Bm", "Gl"};

/* the screen's colours: a kalimba's: pale wood, dark ink, the tines in their metal */
#define K_BG RGB(246, 238, 224)
#define K_PANEL RGB(232, 220, 200)
#define K_LINE RGB(212, 196, 172)
#define K_DIM RGB(150, 132, 110)
#define K_TEXT RGB(64, 48, 36)
#define K_WHITE RGB(255, 255, 255)
#define K_WOOD RGB(196, 142, 92)
#define K_WOOD_D RGB(150, 100, 60)
#define K_HOLE RGB(70, 44, 26)
#define K_BRIDGE RGB(92, 66, 46)
#define K_ACC RGB(226, 104, 60)
#define K_ACC_T RGB(246, 204, 180)
#define K_BLUE RGB(52, 140, 160)
#define K_BLUE_T RGB(194, 226, 230)
static const uint16_t MAT_COL[MAT_N] = {RGB(196, 204, 214), RGB(226, 186, 92), RGB(204, 128, 82), RGB(226, 230, 236),
                                        RGB(232, 210, 150), RGB(186, 232, 240)};

static struct {
    uint8_t view;
    uint32_t btn, keys, btn_used;
    uint32_t enc_t[NE];
    uint8_t dirty;
    uint32_t act_t;
    char msg[2][24];
    uint32_t msg_until;
    int8_t touched;
    uint32_t touch_until;
    uint32_t frame;
    uint32_t sig[3];
    int8_t oct_shift;                  /* Perform: OCT- / OCT+ black keys held */
    uint8_t frozen;
    int16_t key_cents[NKEYS][4];       /* what each key plucked (to damp it, and its MIDI note off) */
    uint8_t key_n[NKEYS];
    uint8_t midi_note[NKEYS][4];
} ui;

static void say(const char *a, const char *b)
{
    uint32_t i;
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
}
void ui_say(const char *a, const char *b) { say(a, b); }
int ui_dirty(void) { return ui.dirty != 0; }

static void itoa_u(uint32_t v, char *b)
{
    char t[12];
    int n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static void mark_dirty(void) { ui.dirty = 1; }

/* (no C library on the device: libc.c has only mem*) */
static int32_t s_len(const char *s)
{
    int32_t n = 0;
    while (s[n])
        n++;
    return n;
}
static void s_cpy(char *d, const char *s)
{
    while ((*d++ = *s++))
        ;
}

/* ------------------------------------------------------------ values --- */
static void say_param(int k)
{
    char t[16];
    km_param_text(k, proj.par[k], t);
    say(km_param_info(k)->name, t);
}

static void knob_set(int k, int v)
{
    const km_param_t *p = km_param_info(k);
    v = v < p->lo ? p->lo : v > p->hi ? p->hi : v;
    if (v == proj.par[k])
        return;
    proj.par[k] = (int16_t)v;
    km_set(k, v);
    mark_dirty();
}

/* knob acceleration by speed: a slow turn is one step a detent, a quick one up to 6 (Time, Grain ms:
 * hundreds of steps) */
static uint8_t accel_off;              /* the host simulator's "spin": exact steps */
static int32_t accel(int role, int32_t s, int range)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_t[role], a = (uint32_t)(s < 0 ? -s : s), m;
    ui.enc_t[role] = now;
    if (range <= 24 || !a || accel_off)
        return s;
    if (a > 1)
        dt /= a;
    m = dt < 15u ? 8u : dt < 30u ? 5u : dt < 60u ? 2u : 1u;
    return s * (int32_t)m;
}

static void turn(int role, int k, int32_t e)
{
    const km_param_t *p = km_param_info(k);
    if (!e)
        return;
    knob_set(k, proj.par[k] + accel(role, e, p->hi - p->lo));
}

/* ------------------------------------------------------------- tines --- */
static int octave_now(void) { return proj.par[P_OCTAVE] + ui.oct_shift; }
static int white_cents(int w)
{
    return km_white_cents(proj.par[P_LAYOUT], proj.par[P_SCALE], proj.par[P_KEY], octave_now(), w);
}
static int midi_ch(void) { return proj.par[P_MIDICH] ? proj.par[P_MIDICH] - 1 : 0; }
static uint8_t cents_note(int c) { int n = (c + 50) / 100; return (uint8_t)(n < 0 ? 0 : n > 127 ? 127 : n); }

static void note_out(int on, uint8_t n, uint8_t vel)
{
    uint32_t st;
    if (!proj.par[P_MIDIOUT])
        return;
    st = (on ? 0x90u : 0x80u) | (uint32_t)midi_ch();
    plat_midi_out(st >> 4 | st << 8 | (uint32_t)n << 16 | (uint32_t)(on ? vel : 0) << 24);
}

/* key k plucks a tine (or a roll of them): remembered so its release can damp it */
static void key_pluck(int k, int cents, int vel, int pan, int delay)
{
    uint8_t i = ui.key_n[k];
    km_pluck(cents, vel, pan, delay);
    if (i < 4u) {
        ui.key_cents[k][i] = (int16_t)cents;
        ui.midi_note[k][i] = cents_note(cents);
        ui.key_n[k] = (uint8_t)(i + 1u);
    }
    note_out(1, cents_note(cents), (uint8_t)vel);
}

static void key_release(int k)
{
    uint32_t i;
    for (i = 0; i < ui.key_n[k]; i++) {
        if (proj.par[P_RELEASE])
            km_damp(ui.key_cents[k][i]);
        note_out(0, ui.midi_note[k][i], 0);
    }
    ui.key_n[k] = 0;
}

static void set_freeze(int on)
{
    ui.frozen = (uint8_t)on;
    km_freeze(on);
    say("GRAINS", on ? "FROZEN" : "LIVE");
}

static void white_down(int w)
{
    key_pluck(WHITE_K[w], white_cents(w), 100, (int)(km_white_pos(w) * 100.0f), 0);
}

static void black_down(int b)
{
    int k = BLACK_K[b];
    switch (proj.par[P_BLACK]) {
    case BLK_CHORDS: {
        int c[4], n = km_chord_cents(proj.par[P_SCALE], proj.par[P_KEY], octave_now(), b, c), i;
        for (i = 0; i < n; i++)                      /* a thumb across the tines: low to high, side to side */
            key_pluck(k, c[i], i ? 92 : 100, (i & 1) ? 55 : -55, i * proj.par[P_STRUM]);
        break;
    }
    case BLK_SHARPS: {
        int w = BLACK_LEFT[b];
        key_pluck(k, white_cents(w) + 100, 100, (int)(km_white_pos(w) * 100.0f), 0);
        break;
    }
    default:                                         /* Perform */
        if (b == PF_MUTE) {
            km_damp_all();
        } else if (b == PF_HOLE) {
            km_hole(HOLE_KEY, 127);
        } else if (b == PF_FREEZE) {
            set_freeze(!ui.frozen);
        } else if (b == PF_OCTDN || b == PF_OCTUP) {
            ui.oct_shift = (int8_t)(b == PF_OCTDN ? -1 : 1);
        } else {
            knob_set(P_MATERIAL, b - PF_MAT0);
            say_param(P_MATERIAL);
        }
        break;
    }
}

static void black_up(int b)
{
    if (proj.par[P_BLACK] == BLK_PERFORM) {
        if (b == PF_HOLE)
            km_hole(HOLE_KEY, 0);
        else if (b == PF_OCTDN || b == PF_OCTUP)
            ui.oct_shift = 0;
    }
    key_release(BLACK_K[b]);
}

/* ------------------------------------------------------------ buttons --- */
static void set_view(int v)
{
    ui.view = (uint8_t)v;
    ui.touched = -1;
}

static void button(int b)
{
    int v;
    for (v = 0; v < NVIEWS; v++)
        if (VIEW_BTN[v] == b) {
            set_view(v);
            return;
        }
    switch (b) {
    case B_EDIT: set_view((ui.view + 1) % NVIEWS); break;
    case B_PLAY: set_freeze(!ui.frozen); break;
    case B_REC:
        km_damp_all();
        say("PALM", "MUTE");
        break;
    case B_ARP:
        knob_set(P_RELEASE, !proj.par[P_RELEASE]);
        say("RELEASE", proj.par[P_RELEASE] ? "DAMP" : "RING");
        break;
    case B_OCTDN: case B_OCTUP:
        knob_set(P_OCTAVE, proj.par[P_OCTAVE] + (b == B_OCTUP ? 1 : -1));
        say_param(P_OCTAVE);
        break;
    case B_SAVE:
        if (project_save() == 0) {
            ui.dirty = 0;
            say("SAVED", 0);
        } else {
            say("NOT SAVED", "FLASH ERROR");
        }
        break;
    default: break;
    }
}

/* ------------------------------------------------------------- MIDI in --- */
/* where a MIDI note's tine sits: the white key that plays it now, else alternate sides by pitch */
static int midi_pan(int cents)
{
    int w;
    for (w = 0; w < NWHITE; w++)
        if (white_cents(w) == cents)
            return (int)(km_white_pos(w) * 100.0f);
    return ((cents / 100) & 1) ? 40 : -40;
}

static void midi_cc(uint32_t cc, uint32_t v)
{
    switch (cc) {
    case 1: km_hole(HOLE_MIDI, (int)v); break;              /* mod wheel: a finger over the hole */
    case 64: km_sustain(v >= 64u); break;
    case 72: knob_set(P_DECAY, (int)(v * 100u / 127u)); break;
    case 73: knob_set(P_HARD, (int)(v * 100u / 127u)); break;
    case 74: knob_set(P_TONE, (int)(v * 100u / 127u)); break;
    case 91: knob_set(P_REVERB, (int)(v * 100u / 127u)); break;
    case 93: knob_set(P_GRAIN, (int)(v * 100u / 127u)); break;
    case 94: knob_set(P_DELAY, (int)(v * 100u / 127u)); break;
    case 120: case 123: km_damp_all(); break;               /* all sound / notes off */
    default: break;
    }
}

/* MIDI in (USB and the TRS jack: one queue). Notes pluck the tine of their pitch (12-TET, Tune applied),
 * velocity is how hard; note off damps it when Release is Damp (the pedal holds it). Channel: Setup's
 * MIDI ch (Omni: all). */
static void midi_in(void)
{
    uint32_t pkt;
    while (plat_midi_in(&pkt)) {
        uint32_t st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu, type = st & 0xF0u;
        if (st >= 0xF0u)
            continue;                                       /* clock, sysex: not ours */
        if (proj.par[P_MIDICH] && (st & 0x0Fu) + 1u != (uint32_t)proj.par[P_MIDICH])
            continue;
        if (type == 0x90u && d2) {
            int c = (int)d1 * 100;
            km_pluck(c, (int)d2, midi_pan(c), 0);
        } else if (type == 0x80u || type == 0x90u) {
            if (proj.par[P_RELEASE])
                km_damp((int)d1 * 100);
        } else if (type == 0xB0u) {
            midi_cc(d1, d2);
        } else if (type == 0xC0u) {
            knob_set(P_MATERIAL, (int)(d1 % MAT_N));
        } else if (type == 0xD0u) {
            km_hole(HOLE_PRESS, (int)d1);
        } else if (type == 0xE0u) {
            km_bend((int)(d2 << 7 | d1) - 8192);
        }
        ui.act_t = plat_ms();
    }
}

static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    int32_t e;
    ch = btn ^ ui.btn;
    ui.btn = btn;
    for (i = 0; i < NB; i++) {
        uint32_t m = 1u << i;
        if (!(ch & m))
            continue;
        if (btn & m) {
            ui.btn_used &= ~m;
            if (i == B_PLAY || i == B_REC)             /* the playing buttons act on the press */
                button((int)i);
        } else if (!(ui.btn_used & m) && i != B_PLAY && i != B_REC) {
            button((int)i);
        }
    }
    ch = keys ^ ui.keys;
    ui.keys = keys;
    for (i = 0; i < NBLACK; i++)                       /* black first: an octave key held changes the tines */
        if (ch >> BLACK_K[i] & 1u) {
            if (keys >> BLACK_K[i] & 1u)
                black_down((int)i);
            else
                black_up((int)i);
        }
    for (i = 0; i < NWHITE; i++)
        if (ch >> WHITE_K[i] & 1u) {
            if (keys >> WHITE_K[i] & 1u)
                white_down((int)i);
            else
                key_release(WHITE_K[i]);
        }
    if (btn || keys || ch)
        ui.act_t = plat_ms();
    midi_in();
    if ((e = plat_enc(EN_SELECT)) != 0) {
        knob_set(P_MATERIAL, proj.par[P_MATERIAL] + (e > 0 ? 1 : -1));
        say_param(P_MATERIAL);
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {
        knob_set(P_SCALE, proj.par[P_SCALE] + (e > 0 ? 1 : -1));
        say_param(P_SCALE);
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {
        knob_set(P_KEY, (proj.par[P_KEY] + (e > 0 ? 1 : 11)) % 12);
        say_param(P_KEY);
    }
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0) {
            turn(EN_K1 + (int)i, VIEW_KNOB[ui.view][i], e);
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u;
            ui.act_t = plat_ms();
        }
}

/* ------------------------------------------------------------- screen --- */
/* three bands: the header (28 px), the instrument (144), the knobs (68) */
#define HDR_H 28
#define MAIN_Y 28
#define MAIN_H 144
#define KNB_Y 172
#define KNB_H 68

static uint32_t hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

static void cv_round(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    static const uint8_t IN[7][6] = {{0}, {1}, {1, 0}, {2, 1, 0}, {2, 1, 0, 0}, {3, 2, 1, 0, 0}, {4, 2, 1, 1, 0, 0}};
    int32_t j;
    if (r > 6)
        r = 6;
    if (r > h / 2)
        r = h / 2;
    cv_rect(x, y + r, w, h - 2 * r, c);
    for (j = 0; j < r; j++) {
        int32_t d = IN[r][j];
        cv_rect(x + d, y + j, w - 2 * d, 1, c);
        cv_rect(x + d, y + h - 1 - j, w - 2 * d, 1, c);
    }
}

static void cv_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t y, x;
    for (y = -r; y <= r; y++) {
        for (x = 0; x * x + y * y <= r * r; x++)
            ;
        cv_rect(cx - x + 1, cy + y, 2 * x - 1, 1, c);
    }
}

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}

static void draw_header(void)
{
    char t[24];
    int msg = plat_ms() < ui.msg_until;
    uint32_t h = hash(hash(2166136261u, ui.view | (uint32_t)ui.frozen << 8 | (uint32_t)proj.par[P_RELEASE] << 9 |
                                            (uint32_t)ui.dirty << 10 | (uint32_t)msg << 11),
                      (uint32_t)proj.par[P_KEY] | (uint32_t)proj.par[P_SCALE] << 4 | (uint32_t)proj.par[P_MATERIAL] << 12);
    if (msg)
        h = hash(hash(h, (uint32_t)ui.msg[0][0] << 8 | ui.msg[0][1]), ui.msg_until);
    if (ui.view == V_SETUP)
        h = hash(h, plat_cpu_pct() | plat_xruns() << 8);
    if (h == ui.sig[0])
        return;
    ui.sig[0] = h;
    cv_begin(240, HDR_H, K_BG);
    if (msg) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], K_TEXT);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], K_ACC);
    } else {
        int32_t x = 232, w;
        cv_text(10, 6, &FONT_B, VIEW_NAME[ui.view], K_TEXT);
        if (ui.view == V_SETUP) {                     /* the audio load, for a check on the hardware */
            itoa_u(plat_cpu_pct(), t);
            w = s_len(t);
            t[w++] = '%';
            t[w] = 0;
            if (plat_xruns()) {
                t[w++] = ' ';
                t[w++] = 'X';
                itoa_u(plat_xruns(), t + w);
            }
            cv_text(x - text_w(&FONT_S, t), 6, &FONT_S, t, K_DIM);
            x -= text_w(&FONT_S, t) + 6;
            cv_text(x - text_w(&FONT_S, "CPU"), 6, &FONT_S, "CPU", K_DIM);
        } else {
            char s[16];
            km_param_text(P_KEY, proj.par[P_KEY], t);
            km_param_text(P_SCALE, proj.par[P_SCALE], s);
            w = s_len(t);
            t[w++] = ' ';
            s_cpy(t + w, s);
            x -= text_w(&FONT_S, t);
            cv_text(x, 6, &FONT_S, t, K_TEXT);
        }
        x = 18 + text_w(&FONT_B, VIEW_NAME[ui.view]);
        if (ui.frozen) {
            w = text_w(&FONT_XS, "FRZ") + 10;
            cv_round(x, 6, w, 16, 6, K_BLUE_T);
            cv_text(x + 5, 7, &FONT_XS, "FRZ", K_BLUE);
            x += w + 4;
        }
        if (proj.par[P_RELEASE]) {
            w = text_w(&FONT_XS, "DAMP") + 10;
            cv_round(x, 6, w, 16, 6, K_ACC_T);
            cv_text(x + 5, 7, &FONT_XS, "DAMP", K_ACC);
            x += w + 4;
        }
        if (ui.dirty)
            cv_round(x, 12, 5, 5, 2, K_DIM);
    }
    cv_blit(0, 0);
}

static uint16_t mix(uint16_t a, uint16_t b, int t)   /* t 0..16: a -> b */
{
    int r = ((a >> 11) * (16 - t) + (b >> 11) * t) / 16, g = (((a >> 5) & 63) * (16 - t) + ((b >> 5) & 63) * t) / 16,
        bl = ((a & 31) * (16 - t) + (b & 31) * t) / 16;
    return (uint16_t)(r << 11 | g << 5 | bl);
}

/* how loud the tine at `cents` rings now (0..1) */
static float tine_level(int cents)
{
    int v;
    float l = 0.0f;
    for (v = 0; v < KM_NVOICE; v++)
        if (km_voice_cents[v] == cents && km_voice_level[v] > l)
            l = km_voice_level[v];
    return l * 2.5f > 1.0f ? 1.0f : l * 2.5f;
}

/* the kalimba from above: the board, the sound hole (smaller as a finger covers it), the bridge, and
 * the tines hanging from it, as long as a tine of that pitch would be (a cantilever: f ~ 1 / L^2) */
static void draw_kalimba(void)
{
    int w, cmin = 99999, body = proj.par[P_BODY];
    uint16_t metal = MAT_COL[proj.par[P_MATERIAL] % MAT_N];
    int32_t y0 = 8, bridge = 14, tx;
    for (w = 0; w < NWHITE; w++)
        if (white_cents(w) < cmin)
            cmin = white_cents(w);
    if (body != BODY_NONE)
        cv_round(2, y0 - 6, 236, 104, 6, body == BODY_GOURD ? RGB(176, 120, 64) : K_WOOD);
    if (body == BODY_BOX || body == BODY_GOURD) {
        int32_t r = 6 + (int32_t)(km_hole_open * 12.0f);
        cv_disc(120, 82, r + 2, K_WOOD_D);
        cv_disc(120, 82, r, K_HOLE);
    }
    cv_rect(8, y0 + bridge - 4, 224, 5, K_BRIDGE);
    for (w = 0; w < NWHITE; w++) {
        int c = white_cents(w);
        float lv = tine_level(c), len = 84.0f * fm_exp2f((float)(cmin - c) * (1.0f / 2400.0f));
        int t = (int)(lv * 16.0f + 0.5f), amp = (int)(lv * 3.0f), yy, h = (int)len < 26 ? 26 : (int)len;
        uint16_t col = mix(metal, K_ACC, t > 16 ? 16 : t);
        tx = 10 + w * 14;
        if (amp && (ui.frame & 1u))
            amp = -amp;
        for (yy = 0; yy < h; yy++) {                  /* the free end swings as it rings */
            int d = yy * amp / h;
            cv_rect(tx + 1 + d, y0 + bridge + yy, 8, 1, col);
        }
        cv_rect(tx + 1, y0 + bridge, 8, 1, K_BRIDGE);
        {   /* room for one letter: an accidental is in its colour */
            const char *nm = KM_NOTE_NAME[((c + 50) / 100) % 12];
            char n[2] = {nm[0], 0};
            text_c(tx + 5, y0 + bridge + h - 14, &FONT_XS, n, nm[1] ? K_ACC : K_TEXT);
        }
        if (((c - 100 * proj.par[P_KEY]) % 1200 + 1200) % 1200 == 0)
            cv_round(tx + 3, y0 + bridge + 6, 4, 4, 2, K_TEXT);   /* the tonic: engraved, as on a real one */
    }
}

static void draw_black(int32_t y0)
{
    int b;
    for (b = 0; b < NBLACK; b++) {
        char t[12];
        int32_t x = 4 + b * 20 + ((b >= 3) + (b >= 5) + (b >= 8) + (b >= 10)) * 3;   /* grouped as the keys are */
        int on = (ui.keys >> BLACK_K[b] & 1u) != 0;
        uint16_t bg = K_BLUE_T, fg = K_TEXT;
        switch (proj.par[P_BLACK]) {
        case BLK_CHORDS: km_chord_name(proj.par[P_SCALE], proj.par[P_KEY], b, t); break;
        case BLK_SHARPS: {
            int c = white_cents(BLACK_LEFT[b]) + 100;
            const char *n = KM_NOTE_NAME[((c + 50) / 100) % 12];
            s_cpy(t, n);
            break;
        }
        default:
            s_cpy(t, PF_NAME[b]);
            if (b >= PF_MAT0 && proj.par[P_MATERIAL] == b - PF_MAT0)
                on = 1;
            if (b == PF_FREEZE && ui.frozen)
                on = 1;
            break;
        }
        if (on) {
            bg = K_BLUE;
            fg = K_WHITE;
        }
        cv_round(x, y0, 18, 26, 5, bg);
        if (text_w(&FONT_XS, t) > 18 && t[2]) {       /* two lines: "Bd" "im", "Am" "7" */
            char a[3] = {t[0], t[1], 0};
            text_c(x + 9, y0 + 1, &FONT_XS, a, fg);
            text_c(x + 9, y0 + 12, &FONT_XS, t + 2, fg);
        } else {
            text_c(x + 9, y0 + 6, &FONT_XS, t, fg);
        }
    }
}

static void draw_main(void)
{
    uint32_t h = hash(2166136261u, (uint32_t)proj.par[P_LAYOUT] | (uint32_t)proj.par[P_SCALE] << 4 |
                                       (uint32_t)proj.par[P_KEY] << 8 | (uint32_t)(octave_now() + 4) << 12 |
                                       (uint32_t)proj.par[P_BLACK] << 16 | (uint32_t)proj.par[P_MATERIAL] << 20 |
                                       (uint32_t)proj.par[P_BODY] << 24 | (uint32_t)ui.frozen << 28);
    int v;
    int ringing = 0;
    for (v = 0; v < KM_NVOICE; v++) {
        h = hash(h, (uint32_t)(km_voice_level[v] * 40.0f) | (uint32_t)(uint16_t)km_voice_cents[v] << 8);
        ringing |= km_voice_level[v] > 0.01f;
    }
    h = hash(hash(h, (uint32_t)(km_hole_open * 20.0f)), ui.keys);
    if (ringing)
        h = hash(h, ui.frame & 1u);                  /* the tines swing: every frame while they ring */
    if (h == ui.sig[1])
        return;
    ui.sig[1] = h;
    cv_begin(240, MAIN_H, K_BG);
    draw_kalimba();
    draw_black(114);
    cv_blit(0, MAIN_Y);
}

static void draw_knobs(void)
{
    uint32_t h = hash(2166136261u, ui.view | (uint32_t)ui.touched << 8), i;
    int now_touch = plat_ms() < ui.touch_until;
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)proj.par[VIEW_KNOB[ui.view][i]]);
    h = hash(h, (uint32_t)now_touch);
    if (h == ui.sig[2])
        return;
    ui.sig[2] = h;
    cv_begin(240, KNB_H, K_BG);
    cv_round(4, 2, 232, KNB_H - 6, 6, K_PANEL);
    for (i = 0; i < 4; i++) {
        int k = VIEW_KNOB[ui.view][i], lo = km_param_info(k)->lo, hi = km_param_info(k)->hi, v = proj.par[k];
        int32_t x = (int32_t)i * 60, cx = x + 30, bw;
        int hot = now_touch && ui.touched == (int)i;
        char t[16];
        if (i)
            cv_rect(x, 14, 1, KNB_H - 30, K_LINE);
        text_c(cx, 6, &FONT_XS, km_param_info(k)->name, hot ? K_TEXT : K_DIM);
        km_param_text(k, v, t);
        {   /* the value: big, unless it has lower case (the big face has none) or is long */
            const char *q = t;
            int lower = 0;
            for (; *q; q++)
                lower |= *q >= 'a' && *q <= 'z';
            if (lower || text_w(&FONT_M, t) > 56)
                text_c(cx, 26, &FONT_B, t, hot ? K_ACC : K_TEXT);
            else
                text_c(cx, 22, &FONT_M, t, hot ? K_ACC : K_TEXT);
        }
        bw = hi > lo ? (int32_t)(44 * (v - lo) / (hi - lo)) : 0;
        cv_round(cx - 22, 50, 44, 6, 3, K_LINE);
        if (lo < 0) {                                /* centred values: a bar from the middle */
            int32_t m = cx - 22 + 44 * (0 - lo) / (hi - lo), e = cx - 22 + bw;
            cv_round(e < m ? e : m, 50, (e < m ? m - e : e - m) + 6, 6, 3, hot ? K_ACC : K_BLUE);
        } else if (bw) {
            cv_round(cx - 22, 50, bw < 6 ? 6 : bw, 6, 3, hot ? K_ACC : K_BLUE);
        }
    }
    cv_blit(0, KNB_Y);
}

static void leds(void)
{
    uint32_t b = 1u << VIEW_BTN[ui.view], k = 0, i;
    if (ui.frozen)
        b |= 1u << B_PLAY;
    if (proj.par[P_RELEASE])
        b |= 1u << B_ARP;
    for (i = 0; i < NWHITE; i++)                     /* the tines that ring */
        if (tine_level(white_cents((int)i)) > 0.08f)
            k |= 1u << WHITE_K[i];
    k |= ui.keys;
    if (proj.par[P_BLACK] == BLK_PERFORM) {
        if (ui.frozen)
            k |= 1u << BLACK_K[PF_FREEZE];
        k |= 1u << BLACK_K[PF_MAT0 + proj.par[P_MATERIAL]];
    }
    plat_leds(b, k);
}

/* AUTOSAVE: a few seconds after a change, when nothing rings and nothing is touched (a flash erase
 * silences the audio for a moment) */
#define AUTOSAVE_QUIET 4000u
static void autosave(void)
{
    uint32_t now = plat_ms(), i;
    /* frozen grains are a drone that never ends: no erase under it. Live grains only replay what the
     * tines just played, so they don't count as sound (with Grains up they never stop spawning) */
    if (!ui.dirty || ui.btn || ui.keys || now - ui.act_t < AUTOSAVE_QUIET || ui.frozen)
        return;
    for (i = 0; i < KM_NVOICE; i++)
        if (km_voice_level[i] > 0.0f)
            return;
    if (project_save() == 0)
        ui.dirty = 0;
}

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
    ui.touched = -1;
    lcd_fill(0, 0, 240, 240, K_BG);
}

void ui_frame(void)
{
    input();
    autosave();
    draw_header();
    draw_main();
    draw_knobs();
    leds();
    ui.frame++;
}

void ui_input_only(void) { input(); }

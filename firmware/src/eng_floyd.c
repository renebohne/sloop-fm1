/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FLOYD: 4-operator FM synthesizer with visual spectral gradient and Option B Decay/Sustain macro. */
/* Four sine operators, 4 Floyd Steinberg algorithms (ALG = P_E0),
 * op 4 with feedback, Option B Decay/Sustain combined macro (DSUS = P_E5).
 * Phase modulation wraps naturally in the 32-bit phase. */

static const char *const N_FLOYD_ALG[] = {"STACK", "(3+4)>2>1", "(2+4)>1", "(2+3)>1", "DUAL", "3-TO-1", "4>3, 1, 2", "ORGAN"};
static const char *const N_FLOYD_RATIO[] = {".5", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "14", "16"};
static const uint16_t FLOYD_RATIO_Q8[15] = {128, 256, 512, 768, 1024, 1280, 1536, 1792, 2048, 2304, 2560, 2816,
                                            3072, 3584, 4096};
static const char *const N_FLOYD_OP[] = {"OP1", "OP2", "OP3", "OP4", "ALGO"};

static void floyd_note_on(track_t *t, voice_t *v)
{
    (void)t;
    v->ph[0] = v->ph[1] = v->ph[2] = 0;
    v->s[7] = 0;                 /* op 4 phase */
    v->s[4] = 1 << 24;           /* modulator envelope, Q24 */
    v->s[5] = v->s[6] = 0;       /* feedback history */
}

static inline uint32_t floyd_ratio_inc(uint32_t inc, uint32_t r)
{
    uint32_t lim = 0x73000000u / r;
    return (inc >> 8) > lim ? 0x73000000u : (inc >> 8) * r;
}

static inline uint32_t floyd_mod(int32_t x, int32_t idx) { return (uint32_t)(x * idx) * 2065u; }

static void floyd_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t alg = (uint32_t)p[P_E0] & 7u, i;
    uint32_t i1 = m->inc;
    uint32_t i2 = floyd_ratio_inc(m->inc, FLOYD_RATIO_Q8[p[P_E1] % 15]);
    uint32_t i3 = floyd_ratio_inc(m->inc, FLOYD_RATIO_Q8[p[P_E2] % 15]);
    uint32_t i4 = floyd_ratio_inc(m->inc, FLOYD_RATIO_Q8[p[P_E3] % 15]);
    int32_t me, idx, fb = p[P_E6], fb1, fb2;                 /* fb1, fb2: op 4 feedback history */
    int32_t dsus = p[P_E5] & 127;
    uint32_t ph0, ph1, ph2, ph4;

    /* Option B Decay / Sustain Macro Envelope */
    if (dsus <= 63) {
        /* Percussive range: Sustain = 0, Decay 10ms..4000ms */
        uint32_t decay_coeff = ENV_EXP[(dsus * 2) & 127];
        v->s[4] += mulq16(0 - v->s[4], decay_coeff);
    } else {
        /* Sustained / Pad range: Decay = MAX, Sustain = 0..100% */
        int32_t target_sustain = (dsus - 63) * ((1 << 24) / 64);
        uint32_t decay_coeff = ENV_EXP[127];
        v->s[4] += mulq16(target_sustain - v->s[4], decay_coeff);
    }

    me = v->s[4] >> 9;                                         /* Q15 */
    idx = (p[P_E4] * me) >> 15;                                 /* 0..127 */
    idx = clamp(idx + ((m->cutoff + m->shape - (64 << 8)) >> 8) + (v->vel - 96) / 4, 0, 127);

    ph0 = v->ph[0];
    ph1 = v->ph[1];
    ph2 = v->ph[2];
    ph4 = (uint32_t)v->s[7];
    fb1 = v->s[5];
    fb2 = v->s[6];

    for (i = 0; i < n; i++) {
        int32_t o1, o2, o3, o4, s;
        o4 = sine_i(ph4 + floyd_mod((fb1 + fb2) >> 1, fb));
        fb2 = fb1;
        fb1 = o4;

        switch (alg) {
        case 0:
            /* Alg 1: STACK 4 -> 3 -> 2 -> 1 */
            o3 = sine_i(ph2 + floyd_mod(o4, idx));
            o2 = sine_i(ph1 + floyd_mod(o3, idx));
            s = sine_i(ph0 + floyd_mod(o2, idx));
            break;
        case 1:
            /* Alg 2: (3+4) -> 2 -> 1 */
            o3 = sine_i(ph2);
            o2 = sine_i(ph1 + floyd_mod((o3 + o4) >> 1, idx));
            s = sine_i(ph0 + floyd_mod(o2, idx));
            break;
        case 2:
            /* Alg 3: (2+4) -> 1, 3 -> 2 */
            o3 = sine_i(ph2);
            o2 = sine_i(ph1 + floyd_mod(o3, idx));
            s = sine_i(ph0 + floyd_mod((o2 + o4) >> 1, idx));
            break;
        case 3:
            /* Alg 4: (2+3) -> 1, 4 -> 3 */
            o3 = sine_i(ph2 + floyd_mod(o4, idx));
            o2 = sine_i(ph1);
            s = sine_i(ph0 + floyd_mod((o2 + o3) >> 1, idx));
            break;
        case 4:
            /* Alg 5: DUAL 2 -> 1, 4 -> 3 */
            o3 = sine_i(ph2 + floyd_mod(o4, idx));
            o2 = sine_i(ph1);
            o1 = sine_i(ph0 + floyd_mod(o2, idx));
            s = (o1 + o3) >> 1;
            break;
        case 5:
            /* Alg 6: 3-TO-1 / FAN (4+3+2) -> 1 (Floyd Steinberg default) */
            o3 = sine_i(ph2);
            o2 = sine_i(ph1);
            s = sine_i(ph0 + floyd_mod(mulq15(o2 + o3 + o4, 10923), idx));
            break;
        case 6:
            /* Alg 7: 4 -> 3, 1, 2 */
            o3 = sine_i(ph2 + floyd_mod(o4, idx));
            o2 = sine_i(ph1);
            o1 = sine_i(ph0);
            s = mulq15(o1 + o2 + o3, 10923);
            break;
        default:
            /* Alg 8: ORGAN / PARALLEL 1 + 2 + 3 + 4 -> Out */
            o3 = sine_i(ph2);
            o2 = sine_i(ph1);
            o1 = sine_i(ph0);
            s = (o1 + o2 + o3 + o4) >> 2;
            break;
        }

        ph0 += i1;
        ph1 += i2;
        ph2 += i3;
        ph4 += i4;
        out[i] += mulq15(mulq15(s, amp_at(m, i)), VOICE_FS);
    }

    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
    v->s[7] = (int32_t)ph4;
    v->s[5] = fb1;
    v->s[6] = fb2;
}

static const preset_t FLOYD_PRESETS[] = {
    /* ALG R2 R3 R4 DEPTH DSUS FDBK OP */
    {"FLOYD TINE", {5, 1, 2, 7, 75, 40, 0, 0}, {0, 85, 35, 55}, 0, 0, FX(0, 32, 14, 28), XP(P_LD_AMP + 1, 26, P_LRATE + 1, 84)},
    {"DUAL LEAD",  {4, 1, 2, 3, 80, 85, 20, 0}, {2, 70, 75, 40}, 0, 1, FX(15, 20, 20, 30), XP(P_GLIDE + 1, 25)},
    {"STACK BASS", {0, 1, 1, 1, 60, 32, 16, 0}, {0, 58, 65, 22}, 0, 1, FX(10, 0, 0, 8), XP(P_TRANS + 1, -24, P_GLIDE + 1, 30)},
    {"ORGAN 4",    {7, 2, 3, 4, 0, 100, 0, 0},  {2, 60, 120, 30}, 0, 0, FX(0, 40, 0, 25), XP(P_LD_AMP + 1, 18, P_LRATE + 1, 95)},
    {"CRYSTAL",    {5, 3, 5, 9, 65, 90, 0, 0},  {0, 92, 0, 75}, 0, 0, FX(0, 30, 35, 50)},
    {"METALLIC",   {0, 1, 7, 11, 90, 28, 30, 0}, {0, 65, 20, 45}, 0, 0, FX(20, 10, 25, 35)},
    {"WARM SINE",  {5, 1, 1, 1, 25, 60, 0, 0},  {5, 80, 40, 60}, 0, 0, FX(0, 25, 10, 20)},
    {"VELVET PAD", {4, 1, 1, 2, 45, 110, 0, 0}, {60, 90, 110, 90}, 0, 0, FX(0, 50, 30, 65), XP(P_LD_PIT + 1, 1, P_LRATE + 1, 38)},
};

static const engine_t ENG_FLOYD = {
    "FLOYD", {"ALGO", "MOD"},
    {
        {"ALG", F_ENUM, 0, 7, 5, N_FLOYD_ALG, 0},
        {"R2", F_ENUM, 0, 14, 1, N_FLOYD_RATIO, 0},
        {"R3", F_ENUM, 0, 14, 2, N_FLOYD_RATIO, 0},
        {"R4", F_ENUM, 0, 14, 4, N_FLOYD_RATIO, 0},
        {"DEPTH", F_PCT, 0, 127, 64, 0, 0},
        {"DSUS", F_TIME, 0, 127, 60, 0, 0},
        {"FDBK", F_PCT, 0, 127, 0, 0, 0},
        {"OP", F_ENUM, 0, 4, 0, N_FLOYD_OP, 0},
    },
    FLOYD_PRESETS, sizeof(FLOYD_PRESETS) / sizeof(FLOYD_PRESETS[0]), -1, floyd_note_on, floyd_render,
    0x3BFF, {P_E4, P_E5, P_E6, P_REL},
};

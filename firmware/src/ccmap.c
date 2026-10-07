/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MIDI control change: which parameter a controller knob moves (the CC -> parameter map).
 *
 * The channel picks the track, as it already does for notes and for program change, so one table
 * covers all four. What a controller cannot do is pick a page: the device's page is local UI state
 * and is never sent out, so "CC 20 is whatever KNOB 1 shows" would point at the wrong parameter as
 * soon as you tapped a button. The table therefore names the parameters themselves; the pages are
 * only how the device groups them for its own display.
 *
 * The families follow the pages that a performing musician reaches for, one contiguous range each:
 * HOME, ENV, LFO, FX and SLICER, then the four EDIT pages. 0..6, 8, 9, 11..15 and 60..127 are the
 * standard assignments for things this instrument has no parameter for (modulation, expression,
 * pitch bend) or are simply unassigned.
 *
 * Only the knob parameters are here. The pattern (LEN, DIV, SWING, GATE), the scale, the
 * arpeggiator and the transport are the keyboard's and the device's own business: they are steps and
 * song state, not a continuous sound, and a fader sweep over them is not a performance gesture.
 * GLO is out for the same reason the editor's scopes are separate: it is one machine, not one track.
 *
 * Receive only: the device never sends a CC of its own, so a controller keeps the faders it shows
 * and the FM-1 does not have to be the master of the mapping. Sending the value back is the other
 * half of this work and is deliberately not here.
 */
enum { CC_NONE = 0xFF, CC_HOME = 0xFE };     /* CC_HOME: the engine's four HOME knobs (HOME below) */
#define CC_LEVEL 7u                          /* the usual roles, where this instrument has them */
#define CC_PAN 10u
#define CC_HOME0 16u
#define CC_TOP 59u                           /* one past the highest CC the table names (MUTE): the
                                               * table stops here, so a CC above it is not mapped */

typedef struct {
    uint8_t id;                 /* P_*, or CC_NONE / CC_HOME */
    uint8_t drum;               /* the drum track accepts it too */
} cc_map_t;
/* Every slot below CC_TOP is named: a designated initialiser leaves the rest zeroed, and 0 is
 * P_LEVEL, so an unnamed slot would silently mean LEVEL. CC_NONE is the "nothing here" marker. */
#define CC_ONE(n, i) [n] = {(i), 0}
#define CC_BOTH(n, i) [n] = {(i), 1}
#define CC_NONE_(n) [n] = {CC_NONE, 0}

/* how many CCs the table maps (tests/ui_pages_test.c counts the table against this) */
#define CC_MAP_N 45u

/* indexed by CC number; the gaps are CC_NONE */
static const cc_map_t CC_MAP[CC_TOP] = {
    /* 0..6: breath, foot, expression and the rest -- nothing on this instrument follows them */
    CC_NONE_(0), CC_NONE_(1), CC_NONE_(2), CC_NONE_(3), CC_NONE_(4), CC_NONE_(5), CC_NONE_(6),
    CC_BOTH(CC_LEVEL, P_LEVEL),                  /* the drum track's level is G_DRLVL, not P_LEVEL */
    CC_NONE_(8), CC_NONE_(9),
    CC_BOTH(CC_PAN, P_PAN),
    /* 11..15 unassigned */
    CC_NONE_(11), CC_NONE_(12), CC_NONE_(13), CC_NONE_(14), CC_NONE_(15),
    /* the four HOME knobs: the engine's own (macro[]), or the drum track's four */
    CC_BOTH(CC_HOME0 + 0, CC_HOME), CC_BOTH(CC_HOME0 + 1, CC_HOME),
    CC_BOTH(CC_HOME0 + 2, CC_HOME), CC_BOTH(CC_HOME0 + 3, CC_HOME),
    /* ENV and ENV DEST */
    CC_ONE(CC_HOME0 + 4, P_ATK),    CC_ONE(CC_HOME0 + 5, P_DEC),
    CC_ONE(CC_HOME0 + 6, P_SUS),    CC_ONE(CC_HOME0 + 7, P_REL),
    CC_ONE(CC_HOME0 + 8, P_ED_FLT), CC_ONE(CC_HOME0 + 9, P_ED_PIT),
    CC_ONE(CC_HOME0 + 10, P_ED_SHP),
    /* LFO and LFO DEST */
    CC_ONE(CC_HOME0 + 11, P_LRATE),    CC_ONE(CC_HOME0 + 12, P_LWAVE),
    CC_ONE(CC_HOME0 + 13, P_LPHASE),   CC_ONE(CC_HOME0 + 14, P_LFADE),
    CC_ONE(CC_HOME0 + 15, P_LD_PIT),   CC_ONE(CC_HOME0 + 16, P_LD_FLT),
    CC_ONE(CC_HOME0 + 17, P_LD_SHP),   CC_ONE(CC_HOME0 + 18, P_LD_AMP),
    /* FX, then SLICER (the one of the two the drum track has) */
    CC_ONE(CC_HOME0 + 19, P_DIST),     CC_ONE(CC_HOME0 + 20, P_CHOR),
    CC_ONE(CC_HOME0 + 21, P_DLY),      CC_ONE(CC_HOME0 + 22, P_REV),
    CC_BOTH(CC_HOME0 + 23, P_SLCR),    CC_BOTH(CC_HOME0 + 24, P_SLPAT),
    CC_BOTH(CC_HOME0 + 25, P_SLRATE),  CC_BOTH(CC_HOME0 + 26, P_SLDEPTH),
    /* EDIT 1 and EDIT 2 (the engine's own eight) */
    CC_ONE(CC_HOME0 + 27, P_E0), CC_ONE(CC_HOME0 + 28, P_E1),
    CC_ONE(CC_HOME0 + 29, P_E2), CC_ONE(CC_HOME0 + 30, P_E3),
    CC_ONE(CC_HOME0 + 31, P_E4), CC_ONE(CC_HOME0 + 32, P_E5),
    CC_ONE(CC_HOME0 + 33, P_E6), CC_ONE(CC_HOME0 + 34, P_E7),
    /* VOICE and VOICE 2. PAN is CC 10 as the usual role and here as VOICE 2's third knob: either
     * CC moves it, which is harmless, and the reply half has one number per parameter */
    CC_ONE(CC_HOME0 + 35, P_VOICE),  CC_ONE(CC_HOME0 + 36, P_GLIDE),
    CC_ONE(CC_HOME0 + 37, P_GLMODE), CC_ONE(CC_HOME0 + 38, P_PRIO),
    CC_ONE(CC_HOME0 + 39, P_ALLOC),  CC_ONE(CC_HOME0 + 40, P_DETUNE),
    CC_ONE(CC_HOME0 + 41, P_PAN),    CC_ONE(CC_HOME0 + 42, P_MUTE),
};

/* the parameter a CC addresses on track t: scope 0 = t->p[], 1 = song.g[] (the drum track's
 * LEVEL and REV), as editor.c ed_desc. Returns 0 for a CC this instrument has nothing for. */
static int cc_target(track_t *t, uint32_t cc, uint32_t *scope, uint32_t *id)
{
    uint32_t e, k;
    if (cc >= CC_TOP)                          /* above the table: nothing here is mapped */
        return 0;
    e = CC_MAP[cc].id;
    if (e == CC_NONE)
        return 0;
    if (is_drum(t) && !CC_MAP[cc].drum)
        return 0;                                   /* a synth parameter has no drum track meaning */
    if (e == CC_HOME) {
        k = cc - CC_HOME0;
        if (is_drum(t)) {
            *scope = CC_DRUM_HOME[k][0];
            *id = CC_DRUM_HOME[k][1];
        } else {
            *scope = 0;
            *id = ENGINES[t->eng_req % NENGINES]->macro[k & 3u];
        }
        return 1;
    }
    if (cc == CC_LEVEL && is_drum(t)) {             /* the drum track's level is global, not P_LEVEL */
        *scope = 1;
        *id = G_DRLVL;
        return 1;
    }
    *scope = 0;
    *id = e;
    return 1;
}

/* write a CC value (0..127) to a parameter, absolute: it is scaled over the parameter's range, so
 * 0 and 127 land on its ends and an enum parameter (min 0) snaps onto one of its values */
static void cc_apply(track_t *t, uint32_t scope, uint32_t id, uint32_t val)
{
    const param_desc_t *d;
    int16_t *vp;
    int32_t v;
    if (scope) {
        if (id >= G_COUNT)
            return;
        d = &GP[id];
        vp = &song.g[id];
    } else {
        if (id >= P_COUNT)
            return;
        d = track_desc(t, id);
        vp = &t->p[id];
    }
    if (d->max <= d->min)
        return;                                      /* a fixed value: nothing to move */
    v = d->min + ((int32_t)(d->max - d->min) * (int32_t)(val > 127u ? 127u : val)) / 127;
    if (*vp == (int16_t)v)
        return;                                      /* no write, no redraw, no editor push */
    *vp = (int16_t)v;
    ui.force = 1;                                    /* the page shows the knob it moved */
    sync_reload = 1;                                 /* the editor re-reads: this was not its write */
}

/* a control change on the track its channel plays: cc is the controller number, val its value */
static void cc_change(track_t *t, uint32_t cc, uint32_t val)
{
    uint32_t scope = 0, id = 0;
    if (t && cc_target(t, cc, &scope, &id))
        cc_apply(t, scope, id, val);
}
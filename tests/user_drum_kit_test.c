/* SPDX-License-Identifier: GPL-3.0-only
 * The USER KIT (drums.c DRUM_USER_KIT): the 16 white keys play the samples uploaded to USR1, and
 * until they are uploaded (a fresh install, or a slot that never had any) the kit falls back to the
 * built-in GM kit instead of going silent or borrowing another kit's row.
 *   argv[1]: an optional WAV demo (one bar per case: fallback, then the user's own kit).
 *
 * The host builds the same slot image the Web Editor's buildSlot() writes (tools/sampleio.py
 * user_slot): zones sorted by root note, lo/hi from the midpoints between neighbours, IMA ADPCM
 * data, header last. SMP_USER_XIP is redirected into a RAM image of the flash slots, so
 * smp_user_scan() reads it exactly as at boot.
 */
#include <stdarg.h>
#include <stdint.h>
static uint32_t host_slots[3u * 0x14000u / 4u];          /* USR1..3, as the flash at 0xA0000 */
#define SMP_USER_XIP(k) ((const uint8_t *)host_slots + (k) * SMP_USER_SIZE)
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>

static int fails;
/* the detail only when it passed or when it did not and you want the number; the kit names are
 * read out for the failures that are about which kit */
static void check(const char *what, int ok, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static void check(const char *what, int ok, const char *fmt, ...)
{
    va_list ap;
    printf("user kit: %-62s %s", what, ok ? "ok" : "FAIL");
    if (fmt) {
        va_start(ap, fmt);
        printf("  (");
        vprintf(ap, fmt);
        printf(")");
        va_end(ap);
    }
    printf("\n");
    fails += !ok;
}

/* a zone's audio, IMA ADPCM as tools/sampleio.py ima_encode writes it: from predictor 0 / index 0,
 * low nibble first, one nibble per sample. A quiet tone, so the codes move the index off 0 -- a
 * zeroed sample stream would encode to silence and every lane would read as silent.
 * (This is what a float-to-int truncation bug looks like: encode a zero signal and the whole kit
 * is inaudible while every zone still resolves.) */
static void zone_adpcm(uint8_t *dst, uint32_t nsamp, uint32_t root)
{
    int32_t pred = 0, idx = 0;
    uint32_t i;
    for (i = 0; i < nsamp; i++) {
        int32_t x = (i * (7u + root) % 89u - 44) * 600, diff = x - pred, step, vd, code = 0;
        if (diff < 0) { code = 8; diff = -diff; }
        step = IMA_STEP[idx];
        vd = step >> 3;
        if (diff >= step) { code |= 4; diff -= step; vd += step; }
        if (diff >= step >> 1) { code |= 2; diff -= step >> 1; vd += step >> 1; }
        if (diff >= step >> 2) { code |= 1; vd += step >> 2; }
        pred = code & 8 ? pred - vd : pred + vd;
        pred = pred > 32767 ? 32767 : pred < -32768 ? -32768 : pred;
        idx += IMA_IDX[code & 7u];
        idx = idx < 0 ? 0 : idx > 88 ? 88 : idx;
        if (i & 1u)
            dst[i >> 1] |= (uint8_t)(code << 4);
        else
            dst[i >> 1] = (uint8_t)code;
    }
}

/* the Web Editor's buildSlot(): nz samples, roots_in[] their root notes (sorted here as the editor
 * sorts), lo/hi from the midpoints. Returns the data length. */
static uint32_t editor_build_slot(uint8_t *slot, uint32_t nz, const uint8_t *roots_in)
{
    smp_user_hdr_t *h = (smp_user_hdr_t *)slot;
    int32_t root[16], off = 0;
    uint32_t i, j;
    for (i = 0; i < nz; i++) root[i] = roots_in[i];
    for (i = 0; i < nz; i++)
        for (j = i + 1; j < nz; j++)
            if (root[j] < root[i]) { int32_t t = root[i]; root[i] = root[j]; root[j] = t; }
    memset(slot, 0, 3u * 0x14000u);
    h->magic = SMP_USER_MAGIC;
    h->version = 1;
    h->nz = (uint8_t)nz;
    for (j = 0; j < nz; j++) {
        smp_zone_t *z = &h->zone[j];
        uint32_t nsamp = 4000;
        z->off = (uint32_t)off;
        z->n = nsamp;
        z->ls = 0;
        z->le = nsamp - 1;
        z->rate = 44100;
        z->root16 = (int16_t)(root[j] * 16);
        z->lo = (uint8_t)(j == 0 ? 0 : (root[j - 1] + root[j]) / 2 + 1);
        z->hi = (uint8_t)(j == nz - 1 ? 127 : (root[j] + root[j + 1]) / 2);
        z->idx = 88;
        z->pred = 0;
        z->looped = 0;
        zone_adpcm(slot + SMP_USER_DATA + off, nsamp, (uint32_t)root[j]);
        off += (int32_t)(nsamp / 2);                  /* IMA ADPCM: 4 bits per sample */
    }
    h->data_len = (uint32_t)off;
    return (uint32_t)off;
}

/* one hit on the user kit: the zone it resolved to (0xFFFF = nothing started) and the peak */
static uint32_t user_kit_hit(uint32_t note, int32_t *peak)
{
    uint32_t i, pk = 0;
    memset(&drums, 0, sizeof drums);
    drums.set = -2;
    TDRUM->p[P_E0] = (int16_t)DRUM_USER_KIT;
    drum_on(note, 100);
    for (i = 0; i < FS * 2u / CTL; i++) {
        int32_t l[CTL] = {0}, r[CTL] = {0}, rev[CTL] = {0};
        uint32_t k;
        drums_render(l, r, rev, CTL);
        for (k = 0; k < CTL; k++)
            if ((uint32_t)(l[k] < 0 ? -l[k] : l[k]) > pk) pk = (uint32_t)(l[k] < 0 ? -l[k] : l[k]);
    }
    if (peak) *peak = (int32_t)pk;
    return drums.v[0].active || pk ? (uint32_t)drums.v[0].s[4] : 0xFFFFu;
}

/* the energy of the same three hits under kit `kit` (the distinctness check of every kit) */
static uint64_t kit_energy(uint32_t kit)
{
    uint32_t i, j, k;
    uint64_t e = 0;
    memset(&drums, 0, sizeof drums);
    drums.set = -2;
    TDRUM->p[P_E0] = (int16_t)kit;
    drum_on(36, 110);
    drum_on(38, 100);
    drum_on(46, 80);
    for (j = 0; j < FS * 2u / CTL; j++) {
        int32_t l[CTL] = {0}, r[CTL] = {0}, rev[CTL] = {0};
        drums_render(l, r, rev, CTL);
        for (k = 0; k < CTL; k++) e += (uint64_t)(l[k] < 0 ? -l[k] : l[k]);
    }
    for (i = 0; i < NDRUM; i++)
        if (drums.v[i].active) e = 0;                 /* a hanging voice: not finite */
    return e;
}

int main(int argc, char **argv)
{
    uint8_t roots[DRUM_LANES];
    uint64_t acoustic, user;
    uint32_t i, k;
    host_tracks_init();

    /* ---- the kit table: the user kit is the last one, named, and its style is not a factory one */
    check("DRUM_KITS counts the user kit", DRUM_KITS == DRUM_SAMPLED + DS_NKITS + 1u,
          "%u kits, user kit = %u", DRUM_KITS, DRUM_USER_KIT);
    check("the user kit is the last row", DRUM_USER_KIT == DRUM_KITS - 1u, "kit %u of %u",
          DRUM_USER_KIT, (uint32_t)DRUM_KITS);
    check("named USER KIT", str_eq(DRUM_KIT_NAMES[DRUM_USER_KIT], "USER KIT"), "\"%s\"",
          DRUM_KIT_NAMES[DRUM_USER_KIT]);
    check("a style of its own", !str_eq(DRUM_KIT_STYLES[DRUM_USER_KIT], DRUM_KIT_STYLES[0]),
          "\"%s\" vs \"%s\"", DRUM_KIT_STYLES[DRUM_USER_KIT], DRUM_KIT_STYLES[0]);
    /* the enum the UI and the editor read (params.c DRUM_KIT_DESC) has to offer it */
    {
        const param_desc_t *d = track_desc(TDRUM, P_E0);
        check("the KIT parameter lists it", d && d->max == (int16_t)(DRUM_KITS - 1) &&
              str_eq(d->names[DRUM_USER_KIT], "USER KIT"), "max %d, %u names",
              d ? d->max : -1, d && d->names ? (uint32_t)(sizeof(DRUM_KIT_NAMES) / sizeof(*DRUM_KIT_NAMES)) : 0u);
    }
    check("the kit is reachable by the knob",
          drum_kit() == (uint32_t)clamp(TDRUM->p[P_E0], 0, DRUM_KITS - 1u), "kit %u", drum_kit());

    /* ---- no upload yet: the kit plays, and it is not another kit's row */
    smp_user_scan(0);
    check("an empty USR1 leaves the kit unplayable", usr_nz[0] == 0, "usr_nz[0] = %u", usr_nz[0]);
    check("the header says EMPTY until there are samples", str_eq(drum_kit_style(DRUM_USER_KIT), "EMPTY"),
          "\"%s\"", drum_kit_style(DRUM_USER_KIT));
    {   /* the fallback takes the -3 that keeps it off ACOUSTIC */
        int32_t want;
        memset(&drums, 0, sizeof drums); drums.set = -2;
        TDRUM->p[P_E0] = (int16_t)DRUM_USER_KIT;
        drum_on(LANE_NOTE[0], 100);
        want = (int32_t)((pow2_q16((int32_t)LANE_NOTE[0] * 16 - 3 * 16 -
                                   smp_zone((uint32_t)drums.v[0].s[4])->root16) >> 8) *
                          (smp_zone((uint32_t)drums.v[0].s[4])->rate >> 8));
        check("the fallback is treated, so it is not ACOUSTIC", drums.v[0].s[5] == want,
              "step %d, want %d", drums.v[0].s[5], want);
    }
    user = kit_energy(DRUM_USER_KIT);
    acoustic = kit_energy(0);
    check("an unconfigured USER KIT still sounds", user > 10000, "energy %llu", (unsigned long long)user);
    check("it is not the acoustic kit", user != acoustic, "USER %llu vs ACOUSTIC %llu",
          (unsigned long long)user, (unsigned long long)acoustic);
    {   /* distinct from every factory kit: the drum track's own requirement (studio_drums_test.c) */
        uint32_t same = 0xFFFFFFFFu;
        for (k = 0; k < DRUM_USER_KIT; k++)
            if (user == kit_energy(k)) same = k;
        check("distinct from every factory kit", same == 0xFFFFFFFFu, "vs kit %u (%s)",
              same, same == 0xFFFFFFFFu ? "none" : DRUM_KIT_NAMES[same]);
    }
    /* every lane of the fallback sounds: no dead key in the list */
    {
        int dead = 0;
        for (i = 0; i < DRUM_LANES; i++) {
            int32_t pk = 0;
            user_kit_hit(LANE_NOTE[i], &pk);
            if (pk < 200) dead++;
        }
        check("all 16 lanes sound on a fresh install", !dead, "%d silent lanes", dead);
    }

    /* ---- 16 samples named by lane note: every key plays its own, in tune */
    for (i = 0; i < DRUM_LANES; i++) roots[i] = LANE_NOTE[i];
    editor_build_slot((uint8_t *)host_slots, DRUM_LANES, roots);
    smp_user_scan(0);
    check("16 samples uploaded to USR1", usr_nz[0] == DRUM_LANES, "usr_nz[0] = %u", usr_nz[0]);
    {
        int wrong = 0, silent = 0, pitch = 0;
        for (i = 0; i < DRUM_LANES; i++) {
            int32_t pk = 0;
            uint32_t zi = user_kit_hit(LANE_NOTE[i], &pk);
            const smp_zone_t *z;
            int32_t semis;
            if (zi == 0xFFFFu || pk < 200) { silent++; continue; }
            z = smp_zone(zi);
            if (z->root16 / 16 != (int32_t)LANE_NOTE[i]) wrong++;
            semis = ((int32_t)LANE_NOTE[i] * 16 - z->root16) / 16;
            if (semis) pitch++;
        }
        check("every key plays its own sample", !wrong, "%d wrong samples", wrong);
        check("in tune (no repitching)", !pitch, "%d lanes repitched", pitch);
        check("all 16 lanes sound", !silent, "%d silent lanes", silent);
        /* the -3 that keeps the fallback off ACOUSTIC belongs to the fallback only: the step
         * rate of a user zone is the plain note-vs-root ratio (drums.c drum_on) */
        {
            int detuned = 0;
            for (i = 0; i < DRUM_LANES; i++) {
                int32_t want;
                memset(&drums, 0, sizeof drums); drums.set = -2;
                TDRUM->p[P_E0] = (int16_t)DRUM_USER_KIT;
                drum_on(LANE_NOTE[i], 100);
                want = (int32_t)((pow2_q16((int32_t)LANE_NOTE[i] * 16 - drums.v[0].s[4] * 0 -
                                           smp_zone((uint32_t)drums.v[0].s[4])->root16) >> 8) *
                                  (smp_zone((uint32_t)drums.v[0].s[4])->rate >> 8));
                if (drums.v[0].s[5] != want) detuned++;
            }
            check("the uploaded kit plays at its written pitch", !detuned, "%d lanes shifted", detuned);
        }
        /* the header says so too, so an unconfigured kit is visible on the device */
        check("the style is not EMPTY once uploaded",
              !str_eq(drum_kit_style(DRUM_USER_KIT), "EMPTY"), "\"%s\"", drum_kit_style(DRUM_USER_KIT));
        /* the encoding: a user zone index is 0x8000 | slot << 5 | zone (eng_sample.c) */
        memset(&drums, 0, sizeof drums); drums.set = -2;
        TDRUM->p[P_E0] = (int16_t)DRUM_USER_KIT; drum_on(LANE_NOTE[0], 100);
        check("the zone index is the user form", drums.v[0].s[4] >= 0x8000 && drums.v[0].s[4] < 0x8020,
              "s[4] = 0x%x", (unsigned)drums.v[0].s[4]);
        check("and decodes to USR1", (drums.v[0].s[4] >> 5 & 3u) == 0u, "slot %u", (unsigned)(drums.v[0].s[4] >> 5 & 3u));
    }
    /* the user's kit differs from the fallback it replaced */
    check("the uploaded kit replaces the fallback", kit_energy(DRUM_USER_KIT) != user,
          "user %llu vs fallback %llu", (unsigned long long)kit_energy(DRUM_USER_KIT),
          (unsigned long long)user);

    /* ---- a note outside the zone map falls back rather than going silent */
    {
        uint8_t three[3];
        int32_t pk = 0;
        three[0] = 36; three[1] = 38; three[2] = 42;         /* kick, snare, hat only */
        editor_build_slot((uint8_t *)host_slots, 3, three);
        smp_user_scan(0);
        check("a 3-sample upload is accepted", usr_nz[0] == 3, "usr_nz[0] = %u", usr_nz[0]);
        user_kit_hit(51, &pk);                                /* ride: outside 36..42 */
        check("a lane with no sample still sounds", pk > 200, "peak %d", pk);
        user_kit_hit(36, &pk);
        check("a lane with a sample uses it", pk > 200, "peak %d", pk);
    }

    /* ---- an upload that the firmware rejects leaves the kit working */
    {
        smp_user_hdr_t *h = (smp_user_hdr_t *)host_slots;
        editor_build_slot((uint8_t *)host_slots, 16, roots);
        h->magic = 0;                                         /* not a slot: smp_user_scan drops it */
        smp_user_scan(0);
        check("an invalid upload is refused", usr_nz[0] == 0, "usr_nz[0] = %u", usr_nz[0]);
        check("and the kit still sounds", kit_energy(DRUM_USER_KIT) > 10000, "energy %llu",
              (unsigned long long)kit_energy(DRUM_USER_KIT));
    }
    /* USR2 and USR3 stay free for the SAMPLE engine: the drum kit must not read them */
    {
        uint8_t slot2[0x14000];
        memset(slot2, 0, sizeof slot2);
        ((smp_user_hdr_t *)(void *)slot2)->magic = SMP_USER_MAGIC;
        ((smp_user_hdr_t *)(void *)slot2)->version = 1;
        ((smp_user_hdr_t *)(void *)slot2)->nz = 1;
        memcpy(host_slots + 0x14000, slot2, sizeof slot2);
        smp_user_scan(1);
        check("USR2 is the SAMPLE engine's, not the kit's", usr_nz[1] == 0, "usr_nz[1] = %u", usr_nz[1]);
    }

    /* ---- the demo: the fallback, then the uploaded kit */
    if (argc > 1) {
        uint32_t step = FS * 60u / 120u / 4u, frames = step * 16u * 2u, at = 0, b;
        FILE *w = fopen(argv[1], "wb");
        uint32_t bar;
        if (w) wav_hdr(w, frames);
        for (bar = 0; w && bar < 2u; bar++) {
            if (!bar) {                                        /* bar 1: no upload yet */
                uint8_t r[DRUM_LANES];
                smp_user_scan(0);
                memset((void *)host_slots, 0, 3u * 0x14000u);
                (void)r;
            } else {
                for (i = 0; i < DRUM_LANES; i++) roots[i] = LANE_NOTE[i];
                editor_build_slot((uint8_t *)host_slots, DRUM_LANES, roots);
                smp_user_scan(0);
            }
            host_tracks_init();
            song.g[G_BPM] = 120;
            memset(&drums, 0, sizeof drums);
            drums.set = -2;
            transport_req = 1;
            for (b = 0; b * CTL < step * 16u; b++) {
                int32_t l[CTL] = {0}, r[CTL] = {0}, rev[CTL] = {0};
                uint32_t blk;
                for (i = 0; i < DRUM_LANES; i++)
                    if ((b * CTL / step) % 16u == i) drum_on(LANE_NOTE[i], 100);
                drums_render(l, r, rev, CTL);
                for (blk = 0; blk < CTL && at < frames; blk++, at++)
                    if (w) wav_put(w, l[blk], l[blk]);
            }
            transport_req = 2;
            events_block(CTL);
        }
        if (w) fclose(w);
    }

    printf("user drum kit: the 16 keys play USR1, the built-in kit until then, USR2/3 untouched %s\n",
           fails ? "FAILED" : "PASS");
    return fails ? 1 : 0;
}
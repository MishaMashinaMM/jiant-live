/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (0.8.9) The USB audio input's packet scheduler over minutes (src/usb.c and the audio ISR's order in src/audio.c),
 * against a simulated FM-1 and host, in ns:
 *   the I2S      halves of HALF_FRAMES at 44,117.6 Hz; the audio ISR acks a half first and renders it (0.8.9): a
 *                render past its half is followed at once by the next (a flip while it rendered stays pending); two
 *                flips in one render lose a half (the DAC plays it again, nothing is rendered for it). Render times as
 *                the field test's heavy song: ~70 % of a half on average, ~4 % of the halves late, up to 131 %;
 *                optional CPU starvation bursts (renders of 150..190 % for 50 ms).
 *   core 1       (-DT_C1=1, CORES 2) the 48 kHz half handed over at the render's end, done 150..400 us later, or at
 *                once when the next half's first block waits for it (mix_block's wait); T_C1=0: one core, each block
 *                resampled in the ISR.
 *   TIMER5       every tick (UAC_POLL_HZ, 0.9.10; 0..20 us late), nested in the render: the next packet as soon as the
 *                last left the wire (uac_service: TxPktRdy set for its (bytes + 9) x 8 x 1.04 / 12 MHz after the token).
 *   the host     an IN token each 1 ms frame of its own clock (+-500 ppm), 50..400 us into the frame; a token
 *                with no packet queued is a lost millisecond (a zero-length packet: the capture gets shorter).
 * Checked: every token finds a packet (no zero-length one), every packet n +- 1 frames, no overrun, no underrun in
 * steady state; every frame the ISR rendered reaches the host exactly once: frames sent = frames rendered (as 48 kHz
 * frames) - what the ring holds, exactly, after minutes; the fill (the latency) stays in its band without drifting;
 * the music continuous (no jump between two frames: a 441 Hz sine). After starvation: underruns repeat the last
 * frame (the count kept), and the repeats are made up (uac.owe back to 0), so the stream is in time again.
 * LEGACY=1 in the environment: also the 0.8.8 order (the ack after the render: every late half loses the next one),
 * which the field test's figures match (~230 underruns a second at ~50 late halves a second). */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef HALF_FRAMES
#error "-DHALF_FRAMES=n (src/core.h; run_tests.sh passes it)"
#endif
#ifndef T_C1
#define T_C1 0
#endif
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#define FELUCCA_OTA 0
#define FELUCCA_CDC 1
#define FELUCCA_UAC 1
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
/* core 1, simulated: one job queued, finished at c1_due (or when the ISR waits for it) */
static uint8_t c1_live = T_C1;
static void (*c1_fn)(uint32_t);
static uint32_t c1_job, c1_done;
static uint64_t c1_due, c1_wait_ns, c1_wait_max;
static uint64_t now_ns;
static int c1_finish(void)
{
    if (c1_fn) {
        c1_fn(0);
        c1_fn = 0;
        c1_done = c1_job;
    }
    return 0;
}
static int c1_post(void (*f)(uint32_t))
{
    c1_fn = f;
    c1_job++;
    return 1;
}
static int c1_wait(void)
{
    if (c1_fn && c1_due > now_ns) {                    /* the ISR waits for it */
        uint64_t w = c1_due - now_ns;
        c1_wait_ns += w;
        if (w > c1_wait_max)
            c1_wait_max = w;
    }
    return c1_finish();
}
#define UAC_C1_LIVE() (c1_live != 0)
#define UAC_C1_RUN(f) c1_post(f)
#define UAC_C1_JOB() c1_job
#define UAC_C1_DONE(j) ((int32_t)(c1_done - (j)) >= 0)
#include "../firmware/src/usb.c"

static int fails;
static void check(const char *what, int ok)
{
    printf("%-100s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

static uint32_t lcg = 0x2468ACEu;
static double rnd(void) { lcg = lcg * 1664525u + 1013904223u; return (lcg >> 8) / 16777216.0; }

/* ---------------------------------------------------------------------------------------- the model --- */
static struct {
    double fs_dev, host_ppm, load, p_late, late_max;   /* I2S rate; host clock; mean render / half; late halves' share, peak */
    int legacy, starve, irqoff;                        /* 0.8.8's ack order; starvation bursts; IRQs off bursts (4 ms) */
} M;
static struct {
    uint64_t halves, late, lost, rendered, frames44;   /* I2S halves, late renders, halves lost, rendered, frames */
    uint64_t pkts, frames, missed, sizes_bad, jumps, zeros;
    int64_t lat_min, lat_max;                          /* (48 kHz frames rendered - sent) */
    uint64_t lat_t;                                    /* the time of the last latency extreme */
    uint64_t f44_prime, sent_prime, primes;            /* frames rendered / sent when the ring last primed */
} R;
static int32_t blk[2 * 64];
static double next_starve, next_irqoff;                /* (the next starvation burst, the next IRQs-off one) */
static uint64_t ph44;                                  /* the sine's input frame */
static int32_t last_l;
static int have_last;
static uint32_t pkt[UA_MAXF * UA_W], pkt_n;
static int queued;
static double tx_end;                                  /* the packet on the wire until then */

static void render_block(uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++, ph44++) {
        int32_t v = (int32_t)lrint(16000.0 * sin(2 * M_PI * 441.0 * (double)ph44 / 44100.0));
        blk[2 * i] = v;
        blk[2 * i + 1] = -v;
    }
    uac_tap(blk, n);
    R.frames44 += n;
}

static void host_take(void)
{
    uint32_t i;
    double nominal = 16000.0 * 2 * sin(M_PI * 441.0 / 48000.0);   /* the largest step of the sine at 48 kHz */
    if (!queued) {
        R.missed++;
        return;
    }
    queued = 0;
    tx_end = (double)now_ns + (pkt_n * 4u + 9u) * 8.0 * 1.04 / 12e6 * 1e9;
    R.pkts++;
    R.frames += pkt_n;
    if (pkt_n < 47u || pkt_n > 49u)
        R.sizes_bad++;
    for (i = 0; i < pkt_n; i++) {
        int32_t l = (int16_t)pkt[i], r = (int16_t)(pkt[i] >> 16);
        if (pkt[i] == 0u) {
            R.zeros++;
            have_last = 0;
            continue;
        }
        if ((l + r) > 1 || (l + r) < -1)
            R.jumps++;
        if (have_last && fabs((double)(l - last_l)) > 1.6 * nominal + 4)
            R.jumps++;
        last_l = l;
        have_last = 1;
    }
}

static void poll(void)                                 /* TIMER5's uac_service: TxPktRdy set while queued or on the wire */
{
    if (queued || (double)now_ns < tx_end)
        return;
    if (R.pkts)
        uac.flowing = 1;
    pkt_n = uac_packet(pkt);
    queued = 1;
}

/* run for secs; returns 0 */
static void sim(double secs, uint64_t *t)
{
    double th = HALF_FRAMES * 1e9 / M.fs_dev, tf = 1e6 * (1.0 + M.host_ppm * 1e-6);
    uint64_t end = *t + (uint64_t)(secs * 1e9);
    static double next_flip, next_sof, next_poll, isr_free;     /* (kept across calls) */
    static double blk_t[HALF_FRAMES / 32u + 1u];
    static uint32_t blocks, pending, starve_left;
    static double dl;                                  /* the render's deadline: the flip after it started */
    static double irqoff_end;
    static int init;
    static double next_tok;
    static uint64_t tick;
    if (!init) {
        init = 1;
        next_flip = th;
        next_sof = 0;
        tick = 0;
        next_poll = 0;

        irqoff_end = 0;
    }
    if (next_tok < (double)*t) next_tok = next_sof + 50000 + rnd() * 350000;
    while ((double)*t < (double)end) {
        double tn = next_flip;
        int irq = (double)*t >= irqoff_end;
        if (blocks && blk_t[0] < tn) tn = blk_t[0];
        if (!blocks && pending && isr_free < tn && irq) tn = isr_free > (double)*t ? isr_free : (double)*t;
        if (next_poll < tn) tn = next_poll;
        if (next_tok < tn) tn = next_tok;
        if (c1_fn && (double)c1_due < tn) tn = (double)c1_due;
        if (!irq && irqoff_end < tn) tn = irqoff_end;
        if (tn < (double)*t) tn = (double)*t;
        *t = now_ns = (uint64_t)tn;
        irq = (double)*t >= irqoff_end;
        if (c1_fn && c1_due <= now_ns) {
            c1_finish();
            continue;
        }
        if (tn == next_flip) {                         /* the DMA starts the other half: the free one pending */
            R.halves++;
            if (pending)
                R.lost++;                              /* (pending already: the flip before was never rendered) */
            pending = 1;
            next_flip += th;
            continue;
        }
        if (blocks && tn == blk_t[0]) {                /* a block of the render done */
            uint32_t i;
            uint64_t w0 = c1_wait_ns;
            if (blocks == HALF_FRAMES / 32u)           /* (mix_block: its first block waits for core 1, the half queued before) */
                c1_wait();
            render_block(32u);
            if (c1_wait_ns != w0)                      /* (the first block waited for core 1: the rest later) */
                for (i = 1; i < blocks; i++)
                    blk_t[i] += (double)(c1_wait_ns - w0);
            for (i = 1; i < blocks; i++)
                blk_t[i - 1] = blk_t[i];
            if (!--blocks) {
                uac_render_end();
                if (c1_fn)
                    c1_due = now_ns + 150000u + (uint64_t)(rnd() * 250000);
                if (tn > dl)                           /* the flip after its own came first: late */
                    R.late++;
                isr_free = tn;
                if (M.legacy && tn > dl)               /* 0.8.8: the ack here clears the flip that came meanwhile */
                    pending = 0;
            }
            continue;
        }
        if (!blocks && pending && irq && tn >= isr_free) {   /* the audio ISR starts a half */
            double d, u = rnd();
            uint32_t i;
            pending = 0;
            if (M.starve && (double)*t >= next_starve) {
                starve_left = (uint32_t)(0.05e9 / th);
                next_starve += 10e9;
            }
            if (starve_left) {
                starve_left--;
                d = th * (1.5 + 0.4 * rnd());
            } else if (u < M.p_late) {
                d = th * (1.0 + (M.late_max - 1.0) * rnd());
            } else {
                d = th * (M.load * (0.75 + 0.5 * rnd())) * (1.0 - M.p_late * 1.15) / (1.0 - M.p_late);
                if (d > th * 0.98) d = th * 0.98;
            }
            R.rendered++;
            dl = next_flip;
            {
                int was = uac.go;
                uac_render_start();
                if (!was && uac.go) {
                    R.f44_prime = R.frames44;
                    R.sent_prime = R.frames + (queued ? pkt_n : 0u);
                    R.primes++;
                }
            }
            blocks = HALF_FRAMES / 32u;
            for (i = 0; i < blocks; i++)
                blk_t[i] = tn + d * (i + 1) / blocks;
            continue;
        }
        if (tn == next_poll) {
            next_poll = (double)++tick * (1e9 / UAC_POLL_HZ) + rnd() * 20000;   /* (a hardware period, the ISR late) */
            if (irq)
                poll();
            if (M.irqoff && (double)*t >= next_irqoff && !blocks) {
                irqoff_end = (double)*t + 4e6;          /* 4 ms with interrupts off (a project load, a flash write) */
                next_irqoff += 10e9;
            }
            continue;
        }
        if (tn == next_tok) {                          /* the host's IN token in this frame */
            host_take();
            next_sof += tf;
            next_tok = next_sof + 50000 + rnd() * 350000;
            if (R.pkts > 2000u) {                      /* latency: rendered (48 kHz) - sent */
                int64_t lat = (int64_t)(R.frames44 * 160u / 147u) - (int64_t)R.frames;
                if (lat < R.lat_min) { R.lat_min = lat; R.lat_t = now_ns; }
                if (lat > R.lat_max) { R.lat_max = lat; R.lat_t = now_ns; }
            }
            continue;
        }
        if (!irq && tn == irqoff_end)
            continue;
    }
}

static void reset(double host_ppm)
{
    memset(&uac, 0, sizeof uac);
    memset(&rs, 0, sizeof rs);
    memset(&rq, 0, sizeof rq);
    memset(&R, 0, sizeof R);
    ua_r = ua_w = 0;
    usb.config = 1;
    uac_rate_set(48000);
    uac_stream(1);
    uac.flowing = 1;
    queued = 0;
    tx_end = 0;
    have_last = 0;
    c1_fn = 0;
    c1_job = c1_done = 0;
    c1_wait_ns = c1_wait_max = 0;
    M.host_ppm = host_ppm;
    R.lat_min = 1 << 30;
    R.lat_max = -(1 << 30);
}

int main(void)
{
    static const double PPM[2] = {-500.0, 500.0};
    char s[200];
    uint64_t t = 0;
    uint32_t k;
    printf("-- USB audio input: the packet scheduler over minutes (%s)\n", T_C1 ? "CORES 2: core 1 resamples" : "one core");
    M.fs_dev = 44117.6;
    M.load = 0.70;
    M.p_late = 0.043;                                  /* ~15 late halves a second, as the field test's song */
    M.late_max = 1.31;
    for (k = 0; k < 2u; k++) {
        uint64_t f0, p0, u0, sent0, ren0;
        int64_t drift;
        reset(PPM[k]);
        sim(2.0, &t);                                  /* settled */
        f0 = R.frames; p0 = R.pkts; u0 = uac.underruns; ren0 = R.frames44; R.jumps = 0;
        R.lat_min = 1 << 30; R.lat_max = -(1 << 30); R.missed = 0;
        sim(180.0, &t);                                /* three minutes */
        snprintf(s, sizeof s, "host %+.0f ppm, 3 min, %.1f late halves/s (peak %.0f %%): %llu packets, %llu tokens with none",
                 PPM[k], R.late / 182.0, M.late_max * 100, (unsigned long long)(R.pkts - p0), (unsigned long long)R.missed);
        check(s, R.missed == 0 && R.sizes_bad == 0);
        snprintf(s, sizeof s, "  no underrun, overrun or lost half; music continuous (%llu jumps)", (unsigned long long)R.jumps);
        check(s, uac.underruns == u0 && !uac.overruns && !R.lost && !R.jumps);
        sent0 = R.frames - f0;
        drift = (int64_t)((R.frames44 - ren0) * 160u / 147u) - (int64_t)sent0;
        snprintf(s, sizeof s, "  3 min: %llu frames sent, %llu rendered (x 160 / 147): %+lld; latency span %lld frames (no drift)",
                 (unsigned long long)sent0, (unsigned long long)((R.frames44 - ren0) * 160u / 147u), (long long)drift,
                 (long long)(R.lat_max - R.lat_min));
        check(s, R.lat_max - R.lat_min < UA_REND48 + 3u * UA_MAXF && llabs(drift) < (long long)(UA_REND48 + UA_HI48));
        {   /* exact, from the prime on: every frame the resampler made went out once or is still in the ring */
            uint64_t n44 = R.frames44 - R.f44_prime - rq.nin - (rq.busy ? rq.n : 0u);
            uint64_t made = n44 ? (160u * n44 - 1u) / 147u + 1u : 0u;
            uint64_t out = R.frames - R.sent_prime, prime = UA_PRIME48 + (rq.c1 ? UA_REND48 : 0u);
            int64_t left = (int64_t)prime + (int64_t)made - (int64_t)(ua_w - ua_r) - (int64_t)uac.repaid - (int64_t)out;
            snprintf(s, sizeof s, "  exact: primed %llu + resampled %llu = sent %llu + queued %u + in the ring %u (%+lld), one prime",
                     (unsigned long long)prime, (unsigned long long)made, (unsigned long long)out, queued ? pkt_n : 0u,
                     ua_w - ua_r, (long long)(left - (queued ? (int64_t)pkt_n : 0)));
            check(s, left == (queued ? (int64_t)pkt_n : 0) && R.primes == 1u);
        }
        {
            snprintf(s, sizeof s, "  mean packet %.5f frames (device %.1f Hz / host frames %.6f ms)", (double)sent0 / (R.pkts - p0),
                     M.fs_dev * 160.0 / 147.0, 1.0 + PPM[k] * 1e-6);
            check(s, fabs((double)sent0 / (R.pkts - p0) - M.fs_dev * 160.0 / 147.0 / 1000.0 * (1.0 + PPM[k] * 1e-6)) < 0.002);
        }
        if (T_C1)
            printf("  core 0 waited for core 1's half (a render right after a late one): %.1f us a second, at most %.0f us\n",
                   c1_wait_ns / 1e3 / 182.0, c1_wait_max / 1e3);
    }
    /* starvation: every 10 s, 50 ms of renders at 150..190 % of a half (halves lost: the DAC glitches too) */
    {
        uint64_t u0, lost0;
        reset(0);
        M.starve = 1;
        next_starve = (double)t + 3e9;
        sim(2.0, &t);
        u0 = uac.underruns; lost0 = R.lost; R.missed = 0;
        sim(60.0, &t);
        snprintf(s, sizeof s, "starvation 50 ms every 10 s: %llu halves lost, %u underruns (repeats %u frames, made up %u), owed %u",
                 (unsigned long long)(R.lost - lost0), (unsigned)(uac.underruns - u0), uac.repaid + uac.owe, uac.repaid, uac.owe);
        check(s, R.missed == 0 && R.sizes_bad == 0 && uac.owe == 0 && uac.repaid > 0);
        sim(5.0, &t);
        u0 = uac.underruns;
        R.jumps = 0;
        sim(4.0, &t);                                  /* (between bursts) */
        check("  after each burst: in time again (nothing owed), no underrun, continuous", uac.owe == 0 && uac.underruns == u0 && !R.jumps);
        M.starve = 0;
    }
    /* the main loop with interrupts off for 4 ms every 10 s (a section's project applied, a flash write): TIMER5 cannot
     * queue, so the host gets ~3 empty frames; the stream must come back in time (nothing owed, no drift) */
    {
        uint64_t m0, u0;
        reset(0);
        M.irqoff = 1;
        next_irqoff = (double)t + 3e9;
        sim(2.0, &t);
        R.missed = 0;
        u0 = uac.underruns;
        R.lat_min = 1 << 30; R.lat_max = -(1 << 30);
        sim(40.0, &t);
        m0 = R.missed;
        snprintf(s, sizeof s, "IRQs off 4 ms every 10 s: %llu empty frames (no packet could be queued), %u underruns, owed %u, latency span %lld",
                 (unsigned long long)m0, (unsigned)(uac.underruns - u0), uac.owe, (long long)(R.lat_max - R.lat_min));
        check(s, m0 <= 4u * 5u && uac.owe == 0 && R.sizes_bad == 0 && R.lat_max - R.lat_min < UA_REND48 + 3u * UA_MAXF + 4u * 48u);
        M.irqoff = 0;
    }
    if (getenv("LEGACY")) {
        uint64_t u0, l0, h0;
        M.legacy = 1;
        M.p_late = 0.145;                              /* ~50 late halves a second (the field test with USB on) */
        reset(0);
        sim(2.0, &t);
        u0 = uac.underruns; l0 = R.late; h0 = R.frames;
        sim(30.0, &t);
        printf("0.8.8 order, %.1f late halves/s: %.0f underruns/s, %.1f %% of the music missing (DAC: the same halves "
               "played twice)\n", (R.late - l0) / 30.0, (uac.underruns - u0) / 30.0,
               100.0 * (1.0 - (double)(R.frames44) / (R.halves * HALF_FRAMES)));
        (void)h0;
        M.legacy = 0;
    }
    printf(fails ? "UAC SCHED TEST FAILED (%d)\n" : "uac sched: all ok\n", fails);
    return fails != 0;
}

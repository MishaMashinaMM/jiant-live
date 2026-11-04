/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (JIANT 0.9.12) Safe by default. Some FM-1s crash now and then with both cores at work (the second core's misreads:
 * hal/fm1_multicore.h), and JieLi rates the chip for 320 MHz where the boot stage runs it at 360. So:
 *   MENU > SYSTEM > CPU        2 CORES (the default, as 0.6.71 .. 0.9.11) or 1 CORE. A crash or a hang (the watchdog)
 *                              while both cores ran, or the second core given up (a misread, no answer, a fault), turns
 *                              it to 1 CORE for good (CPU_AUTO: ABOUT and the boot's notice say why); the user may set
 *                              2 CORES again. From the next start (main.c), as the clock
 *   MENU > SYSTEM > CPU CLOCK  360 MHZ (the default) or 320 MHZ, switched at boot before the audio and the second core
 *                              (main.c, hal/fm1_clk320.h: curist's, MIT); from the next power-on (a soft reset keeps the
 *                              PLL)
 *   the last crash             kept in the settings record (crash_log: SLICE's retired favourites row, 32 bytes), so a
 *                              power-off does not lose it: MENU > ABOUT shows it; the editor's backup carries it (the
 *                              settings record, EDITOR_PROTOCOL.md). tools/crash_decode.py reads its numbers
 * The settings byte: favorites.factory[15][17] (a byte no engine uses), 0 in every older record = 2 CORES, 360 MHZ */
#ifndef FELUCCA_VERSION
#define FELUCCA_VERSION "0"                     /* (a host test without felucca.c) */
#endif
#define cpu_cfg (favorites.factory[15][17])
enum { CPU_1CORE = 1u, CPU_AUTO = 2u, CPU_320 = 4u, CPU_WAIT = 8u | 16u, CPU_GOOD = 32u, CPU_TRIED = 64u };   /* (0.9.13:
                                                         * WAIT the power-ons on one core after a crash, 0..2; GOOD for
                                                         * good; TRIED both cores again after one) */
/* a crash's flags (main.c fm1_fault, cpu_boot): both cores ran, a hang (the watchdog), 320 MHz */
enum { CR_2C = 1u, CR_WDT = 4u, CR_320 = 8u };
/* the record: 32 characters, as MENU > ABOUT shows them, "IL01 2C 0.9.12 0201ABCD 0201ABCD": the short code and the
 * vector (crash_code; WDFF a hang), how many cores ran, the firmware, PC, RETS (a hang: the main loop's stage and page) */
#define CRASH_LOG ((char *)favorites.factory[13])   /* (13: SLICE's, retired; nothing reads its stars) */
#define CRASH_LOG_N 32u

#if FELUCCA_DUAL_CORE
#define cpu_is_320() fm1_clk_320()             /* (hal/fm1_sys.h: the PLL now) */
#else
#define cpu_is_320() 0                          /* (the host, the emulator: 360) */
#endif

static char *hex8(char *b, uint32_t v)          /* 8 hex digits and a 0; the end */
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        b[i] = "0123456789ABCDEF"[(v >> (28u - 4u * i)) & 15u];
    b[8] = 0;
    return b + 8;
}

/* the crash's short code, two letters (MANUAL: Troubleshooting): the first cause found, EMU first (MA misaligned, IL
 * illegal instruction, DZ divide by 0, ST stack), then DBG (PC a fetch outside the code, WR a protected write, MM the
 * MMU, BU the bus: an unmapped address), WD a hang, ?? none */
static void crash_code(char *b, uint32_t emu, uint32_t dbg, uint32_t flags)
{
    static const char C[] = "MAILDZSTPCWRMMBU??WD";
    static const uint32_t M[4] = {(1u << 10) | (1u << 12), (1u << 2) | (1u << 11) | (1u << 13),
                                  (1u << 1) | (1u << 3) | (15u << 6), (3u << 4) | (63u << 16)};
    uint32_t k = 0;
    if (flags & CR_WDT)
        k = 9;
    else {
        while (k < 4u && !((emu >> k) & 1u))
            k++;
        while (k >= 4u && k < 8u && !(dbg & M[k - 4u]))
            k++;
    }
    b[0] = C[2u * k];
    b[1] = C[2u * k + 1u];
    b[2] = 0;
}

/* the record for a crash (or a hang) the last run left; the caller saves the settings */
static void crash_log_put(uint32_t flags, uint32_t vec, uint32_t pc, uint32_t rets, uint32_t emu, uint32_t dbg)
{
    char b[48], *p = b;
    crash_code(p, emu, dbg, flags);
    hex8(p + 2, vec);
    p[2] = p[8];
    p[3] = p[9];
    p[4] = ' ';
    p[5] = flags & CR_2C ? '2' : '1';
    p[6] = 'C';
    p[7] = ' ';
    str_cpy(p + 8, FELUCCA_VERSION + (FELUCCA_VERSION[0] == 'v'), 7);
    p += str_len(p);
    *p++ = ' ';
    p = hex8(p, pc);
    *p++ = ' ';
    hex8(p, rets);
    memcpy(CRASH_LOG, b, CRASH_LOG_N);
}
/* MENU > ABOUT's line (b: CRASH_LOG_N + 1); 0 = none kept */
static int crash_log_get(char *b)
{
    memcpy(b, CRASH_LOG, CRASH_LOG_N);
    b[CRASH_LOG_N] = 0;
    return b[4] == ' ' && b[6] == 'C' && b[7] == ' ' && b[0] >= '?' && b[0] <= 'Z';
}

/* the boot's CPU setting after the last run: a crash or a hang while both cores ran turns 2 CORES off; (0.9.13) for
 * three power-ons, then both cores again (cpu_retry: a crash in 0.9.12 left a unit on one core for good, every song past
 * four tracks over its CPU); a crash again after that retry: for good (until MENU > SYSTEM > CPU) */
static uint32_t cpu_after(uint32_t cfg, uint32_t crash_flags)
{
    if (cfg & CPU_1CORE || !(crash_flags & CR_2C))
        return cfg;
    return (cfg & ~(uint32_t)CPU_WAIT) | CPU_1CORE | CPU_AUTO | (cfg & CPU_TRIED ? CPU_GOOD : 0u);
}
/* a power-on with no crash before it: one on one core after a crash counted, the third gives both cores back */
static uint32_t cpu_retry(uint32_t cfg)
{
    uint32_t w = (cfg & CPU_WAIT) >> 3;
    if ((cfg & (CPU_1CORE | CPU_AUTO | CPU_GOOD)) != (CPU_1CORE | CPU_AUTO))
        return cfg;
    if (w < 2u)
        return (cfg & ~(uint32_t)CPU_WAIT) | (w + 1u) << 3;
    return (cfg & ~(uint32_t)(CPU_WAIT | CPU_1CORE | CPU_AUTO)) | CPU_TRIED;
}

/* 2 CORES off for good (cpu_after), saved, and said: why, then where to set it back (main.c: after a crash at boot, the
 * second core given up while it plays) */
static __attribute__((noinline)) void cpu_off(const char *why)
{
    cpu_cfg = (uint8_t)cpu_after(cpu_cfg, CR_2C);
    settings_save();
    ui_message(why);
    str_cpy(ui.msg2, "MENU > SYSTEM > CORES", sizeof ui.msg2);   /* (0.9.18: CPU CORES, was CPU) */
}

static const char CPU_APPLY[] = "SWITCH OFF AND ON TO APPLY";
static const char *const CPU_N[2][2] = {{"1 CORE", "2 CORES"}, {"360 MHZ", "320 MHZ"}};
/* MENU > SYSTEM > CPU (clock 0) / CPU CLOCK (clock 1): the value as the menu steps it */
static uint32_t cpu_get(uint32_t clock) { return clock ? (cpu_cfg & CPU_320) != 0u : !(cpu_cfg & CPU_1CORE); }
static void cpu_put(uint32_t clock, uint32_t v)   /* (from the next start: main.c) */
{
    if (v != cpu_get(clock))
        ui_message(CPU_APPLY);
    cpu_cfg = (uint8_t)(clock ? (v ? cpu_cfg | CPU_320 : cpu_cfg & ~CPU_320) :
                        (v ? cpu_cfg & ~CPU_1CORE : cpu_cfg | CPU_1CORE) & ~(CPU_AUTO | CPU_WAIT | CPU_GOOD | CPU_TRIED));
}
/* MENU > ABOUT: the cores (fx.c mc_status, and why one: the MENU or a crash), the clock */
static const char *cpu_status(void)
{
    if (!mc_live() && (cpu_cfg & CPU_1CORE))     /* (1 CORE set: core 1 never started) */
        return cpu_cfg & CPU_AUTO ? (cpu_cfg & CPU_GOOD ? "CORES 1 (AFTER 2 CRASHES)" : "CORES 1 (AFTER A CRASH)") : "CORES 1 (MENU > CORES)";
    return mc_status();
}

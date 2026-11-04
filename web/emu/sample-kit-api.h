#ifndef JIANT_EMU_SAMPLE_KIT_API_H
#define JIANT_EMU_SAMPLE_KIT_API_H
/* Browser-only helpers: report the indexed bank and make its kit directly playable.
 * No target firmware code or data layout changes. */
EXPORT uint32_t web_sample_count(void)
{
#ifdef SMP_BANK
    return smp_ix.count;
#else
    return 0;
#endif
}
EXPORT uint32_t web_sample_kit(void)
{
#ifdef SMP_BANK
    if (!smp_ix.count) return 1;
    undo_mark();
    song.sel = 0;
    set_engine_of(&trk[0], ENGI_ADD);
    apply_preset_to(&trk[0], 1);
    song.octave = 0;
    song.grid = 0;
    trk[0].p[P_MUTE] = 0;
    trk[0].p[P_LEVEL] = TP[P_LEVEL].def;
    trk[0].p[P_AMODE] = 0;
    trk[0].p[P_AHOLD] = 0;
    trk[0].p[P_TRANS] = 0;
    trk[0].p[P_QUANT] = 0;
    ui.force = 1;
    return 0;
#else
    return 2;
#endif
}

#endif

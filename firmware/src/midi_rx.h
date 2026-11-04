/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef JIANT_MIDI_RX_H
#define JIANT_MIDI_RX_H
/* Global receive-only setup. Never part of a sound or project. One-based channels. */
static uint8_t midi_rx[6] = {1, 2, 3, 4, 5, 6};
static uint32_t midi_rx_channel(uint32_t track)
{
    uint32_t v = midi_rx[track];
    return v >= 1u && v <= 16u ? v : track + 1u;
}
#endif

// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// JIANT Probe: isochronous IN reads through usbfs on the file descriptor UsbDeviceConnection hands us (no root: the
// app owns the device once the user grants it). Android's audio framework passes an 8-channel USB input on as stereo
// on some phones (the rest zero); reading the FM-1's stream ourselves gets every channel, as the commercial
// multichannel recorders do.
#include <jni.h>
#include <errno.h>
#include <linux/usbdevice_fs.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <time.h>

#define NURB 8              /* URBs in flight */
#define NPK 8               /* packets (1 ms each at full speed) an URB */

static long long now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

static void setup(struct usbdevfs_urb *u, int ep, int maxp)
{
    u->type = USBDEVFS_URB_TYPE_ISO;
    u->endpoint = (unsigned char)ep;
    u->flags = USBDEVFS_URB_ISO_ASAP;
    u->buffer_length = NPK * maxp;
    u->number_of_packets = NPK;
    u->status = 0;
    u->actual_length = 0;
    u->error_count = 0;
    for (int k = 0; k < NPK; k++) {
        u->iso_frame_desc[k].length = (unsigned)maxp;
        u->iso_frame_desc[k].actual_length = 0;
        u->iso_frame_desc[k].status = 0;
    }
}

/* fills out (bytes, as the device sends them) until it is full or timeout_ms passes; stats: packets, empty packets,
 * packet errors, errno of a failed ioctl (0 none), the largest packet. Returns the bytes read, or -errno */
JNIEXPORT jint JNICALL Java_fm_jiant_probe_MainActivity_isoRead(JNIEnv *env, jclass cls, jint fd, jint ep, jint maxp,
                                                                jbyteArray out, jint timeout_ms, jintArray stats)
{
    (void)cls;
    jsize cap = (*env)->GetArrayLength(env, out);
    jbyte *dst = (*env)->GetByteArrayElements(env, out, NULL);
    size_t sz = sizeof(struct usbdevfs_urb) + NPK * sizeof(struct usbdevfs_iso_packet_desc);
    struct usbdevfs_urb *u[NURB];
    int got = 0, packets = 0, empty = 0, perr = 0, err = 0, big = 0, live = 0;
    for (int i = 0; i < NURB; i++) {
        u[i] = calloc(1, sz);
        u[i]->buffer = malloc((size_t)NPK * maxp);
        setup(u[i], ep, maxp);
        if (ioctl(fd, USBDEVFS_SUBMITURB, u[i]) < 0) { err = errno; break; }
        live++;
    }
    long long end = now_ms() + timeout_ms;
    while (!err && got < cap && now_ms() < end) {
        struct usbdevfs_urb *r = NULL;
        if (ioctl(fd, USBDEVFS_REAPURBNDELAY, &r) < 0) {
            if (errno == EAGAIN) { struct timespec s = {0, 1000000}; nanosleep(&s, NULL); continue; }
            if (errno == EINTR) continue;
            err = errno;
            break;
        }
        live--;
        const char *b = r->buffer;
        for (int k = 0; k < NPK; k++) {
            int n = (int)r->iso_frame_desc[k].actual_length;
            packets++;
            if (r->iso_frame_desc[k].status) perr++;
            if (!n) { empty++; continue; }
            if (n > big) big = n;
            if (n > cap - got) n = cap - got;
            memcpy(dst + got, b + k * maxp, (size_t)n);   /* iso packets sit at their requested offsets */
            got += n;
        }
        setup(r, ep, maxp);
        if (got < cap) {
            if (ioctl(fd, USBDEVFS_SUBMITURB, r) < 0) { err = errno; break; }
            live++;
        }
    }
    for (int i = 0; i < NURB; i++) ioctl(fd, USBDEVFS_DISCARDURB, u[i]);
    long long stop = now_ms() + 500;
    while (live > 0 && now_ms() < stop) {               /* every URB back before its memory goes */
        struct usbdevfs_urb *r = NULL;
        if (ioctl(fd, USBDEVFS_REAPURBNDELAY, &r) == 0) { live--; continue; }
        if (errno != EAGAIN && errno != EINTR) break;
        struct timespec s = {0, 1000000};
        nanosleep(&s, NULL);
    }
    if (live == 0)
        for (int i = 0; i < NURB; i++) { free(u[i]->buffer); free(u[i]); }   /* (else leak: the kernel may still write) */
    (*env)->ReleaseByteArrayElements(env, out, dst, 0);
    jint st[6] = {packets, empty, perr, err, big, live};
    (*env)->SetIntArrayRegion(env, stats, 0, 6, st);
    return got ? got : -err;
}

/* ---- (0.4) both ways: the phone's mic out to the FM-1 (EP OUT, isochronous adaptive: 44 / 45 stereo frames a ms,
 * the mono mic on both sides) while its stems come in (MENU > USB STEMS+IN). The mic thread pushes into a ring here
 * (pushMic); each OUT packet takes what it holds, kept near 10 ms (more is dropped, less sends silence) */
#define MR_N 32768u
static int16_t mring[MR_N];
static volatile uint32_t mr_w, mr_r;

JNIEXPORT void JNICALL Java_fm_jiant_probe_MainActivity_pushMic(JNIEnv *env, jclass cls, jshortArray a, jint n)
{
    (void)cls;
    jshort *p = (*env)->GetShortArrayElements(env, a, NULL);
    uint32_t w = __atomic_load_n(&mr_w, __ATOMIC_RELAXED);
    for (jint i = 0; i < n; i++)
        mring[(w + (uint32_t)i) & (MR_N - 1u)] = p[i];
    __atomic_store_n(&mr_w, w + (uint32_t)n, __ATOMIC_RELEASE);
    (*env)->ReleaseShortArrayElements(env, a, p, JNI_ABORT);
}

#define NURB_O 4
#define OPK 8
#define OMAXP 184                                       /* 46 stereo frames of 16 bit */
static uint32_t o_acc;                                  /* 44.1 frames a ms: a 45 every tenth */
static int o_go, o_under, o_drop, o_sent;
static void fill_out(struct usbdevfs_urb *u, int ep)
{
    int16_t *d = u->buffer;
    int total = 0;
    uint32_t r = mr_r, w = __atomic_load_n(&mr_w, __ATOMIC_ACQUIRE);
    if (w - r > 2048u) { o_drop += (int)(w - r - 441u); r = w - 441u; }   /* > 46 ms in hand: back to 10 */
    if (!o_go && w - r >= 441u) o_go = 1;               /* primed: 10 ms in hand */
    u->type = USBDEVFS_URB_TYPE_ISO;
    u->endpoint = (unsigned char)ep;
    u->flags = USBDEVFS_URB_ISO_ASAP;
    u->number_of_packets = OPK;
    u->status = 0;
    u->actual_length = 0;
    u->error_count = 0;
    for (int k = 0; k < OPK; k++) {
        int f = 44;
        o_acc += 100u;
        if (o_acc >= 1000u) { o_acc -= 1000u; f = 45; }
        for (int i = 0; i < f; i++) {
            int16_t v = 0;
            if (o_go && r != w) v = mring[r++ & (MR_N - 1u)];
            else if (o_go) o_under++;
            d[2 * (total + i)] = d[2 * (total + i) + 1] = v;
        }
        u->iso_frame_desc[k].length = (unsigned)(f * 4);
        u->iso_frame_desc[k].actual_length = 0;
        u->iso_frame_desc[k].status = 0;
        total += f;
        o_sent += f;
    }
    u->buffer_length = total * 4;
    mr_r = r;
}

/* the stems in (as isoRead) and the mic out together; stats: packets in, empty, errors, errno, largest, left, frames
 * out, out frames short of mic (silence sent), mic frames dropped */
JNIEXPORT jint JNICALL Java_fm_jiant_probe_MainActivity_duplex(JNIEnv *env, jclass cls, jint fd, jint ep, jint maxp, jint epo,
                                                               jbyteArray out, jint timeout_ms, jintArray stats)
{
    (void)cls;
    jsize cap = (*env)->GetArrayLength(env, out);
    jbyte *dst = (*env)->GetByteArrayElements(env, out, NULL);
    size_t sz = sizeof(struct usbdevfs_urb) + NPK * sizeof(struct usbdevfs_iso_packet_desc);
    size_t szo = sizeof(struct usbdevfs_urb) + OPK * sizeof(struct usbdevfs_iso_packet_desc);
    struct usbdevfs_urb *u[NURB], *uo[NURB_O];
    int got = 0, packets = 0, empty = 0, perr = 0, err = 0, big = 0, live = 0;
    o_acc = 0; o_go = 0; o_under = 0; o_drop = 0; o_sent = 0;
    mr_r = __atomic_load_n(&mr_w, __ATOMIC_ACQUIRE);    /* (nothing from before) */
    for (int i = 0; i < NURB; i++) { u[i] = calloc(1, sz); u[i]->buffer = malloc((size_t)NPK * maxp); }
    for (int i = 0; i < NURB_O; i++) { uo[i] = calloc(1, szo); uo[i]->buffer = malloc((size_t)OPK * OMAXP); }
    for (int i = 0; i < NURB && !err; i++) {
        setup(u[i], ep, maxp);
        if (ioctl(fd, USBDEVFS_SUBMITURB, u[i]) < 0) err = errno; else live++;
    }
    for (int i = 0; i < NURB_O && !err; i++) {
        fill_out(uo[i], epo);
        if (ioctl(fd, USBDEVFS_SUBMITURB, uo[i]) < 0) err = errno; else live++;
    }
    long long end = now_ms() + timeout_ms;
    while (!err && got < cap && now_ms() < end) {
        struct usbdevfs_urb *r = NULL;
        if (ioctl(fd, USBDEVFS_REAPURBNDELAY, &r) < 0) {
            if (errno == EAGAIN) { struct timespec s = {0, 500000}; nanosleep(&s, NULL); continue; }
            if (errno == EINTR) continue;
            err = errno;
            break;
        }
        live--;
        if (r->endpoint == (unsigned char)epo) {        /* an OUT URB back: the next from the ring */
            fill_out(r, epo);
            if (ioctl(fd, USBDEVFS_SUBMITURB, r) < 0) { err = errno; break; }
            live++;
            continue;
        }
        const char *b = r->buffer;
        for (int k = 0; k < NPK; k++) {
            int n = (int)r->iso_frame_desc[k].actual_length;
            packets++;
            if (r->iso_frame_desc[k].status) perr++;
            if (!n) { empty++; continue; }
            if (n > big) big = n;
            if (n > cap - got) n = cap - got;
            memcpy(dst + got, b + k * maxp, (size_t)n);
            got += n;
        }
        setup(r, ep, maxp);
        if (got < cap) {
            if (ioctl(fd, USBDEVFS_SUBMITURB, r) < 0) { err = errno; break; }
            live++;
        }
    }
    for (int i = 0; i < NURB; i++) ioctl(fd, USBDEVFS_DISCARDURB, u[i]);
    for (int i = 0; i < NURB_O; i++) ioctl(fd, USBDEVFS_DISCARDURB, uo[i]);
    long long stop = now_ms() + 500;
    while (live > 0 && now_ms() < stop) {
        struct usbdevfs_urb *r = NULL;
        if (ioctl(fd, USBDEVFS_REAPURBNDELAY, &r) == 0) { live--; continue; }
        if (errno != EAGAIN && errno != EINTR) break;
        struct timespec s = {0, 1000000};
        nanosleep(&s, NULL);
    }
    if (live == 0) {
        for (int i = 0; i < NURB; i++) { free(u[i]->buffer); free(u[i]); }
        for (int i = 0; i < NURB_O; i++) { free(uo[i]->buffer); free(uo[i]); }
    }
    (*env)->ReleaseByteArrayElements(env, out, dst, 0);
    jint st[9] = {packets, empty, perr, err, big, live, o_sent, o_under, o_drop};
    (*env)->SetIntArrayRegion(env, stats, 0, 9, st);
    return got ? got : -err;
}

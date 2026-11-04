// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// JIANT Probe: phase 0 of the stems app (android/probe/README.md). One screen that answers, on a given phone, the
// questions the app depends on: does Android open the FM-1's eight USB channels (MENU > SYSTEM > USB > STEMS) at
// 44.1 kHz; can it open a USB microphone at the same time as the FM-1; can it play into the FM-1 (USB > IN+OUT); does
// it see the FM-1's USB-MIDI. Everything it finds goes into a plain-text report to copy or share, and the 8-channel
// test is saved as a WAV in Downloads/JIANT.
// (0.2) Android's audio framework hands an 8-channel USB input on as two channels on some phones (an S23 Ultra: T1 T2,
// the rest zero), so test 2 now counts the channels that carry sound, 2b tries the other ways of opening the input, and
// 5 reads the FM-1's stream itself (usbiso.c: usbfs isochronous reads on the USB permission the user grants).
package fm.jiant.probe;

import android.Manifest;
import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.hardware.usb.UsbConstants;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbEndpoint;
import android.hardware.usb.UsbInterface;
import android.hardware.usb.UsbManager;
import android.media.AudioDeviceInfo;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.provider.MediaStore;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.OutputStream;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class MainActivity extends Activity {
    private static final int RATE = 44100;
    private static final int BG = 0xFF07070A, FG = 0xFFF4F1EA, DIM = 0xFF8B8697, ORANGE = 0xFFFF7A00, VIOLET = 0xFFB870FF;

    private TextView log;
    private final StringBuilder report = new StringBuilder();
    private final Handler ui = new Handler(Looper.getMainLooper());
    private AudioManager am;
    private volatile boolean busy;

    static { System.loadLibrary("jiantusb"); }
    /** usbiso.c: isochronous IN reads on a usbfs descriptor; stats = packets, empty, errors, errno, largest, left */
    private static native int isoRead(int fd, int ep, int maxp, byte[] out, int timeoutMs, int[] stats);
    /** (0.4) usbiso.c: the stems in and the pushed mic out together (MENU > USB STEMS+IN) */
    private static native int duplex(int fd, int ep, int maxp, int epOut, byte[] out, int timeoutMs, int[] stats);
    private static native void pushMic(short[] pcm, int n);

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        am = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().setStatusBarColor(BG);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(BG);
        int pad = dp(16);
        root.setPadding(pad, dp(28), pad, pad);

        TextView title = new TextView(this);
        title.setText("JIANT PROBE");
        title.setTextColor(FG);
        title.setTextSize(24);
        title.setTypeface(Typeface.create("sans-serif-medium", Typeface.BOLD));
        root.addView(title);
        TextView sub = new TextView(this);
        sub.setText("Fase 0 de la app de stems. Enchufá el FM-1 por USB-C, poné MENU › SYSTEM › USB › STEMS y tocá los botones en orden.");
        sub.setTextColor(DIM);
        sub.setTextSize(14);
        sub.setPadding(0, dp(4), 0, dp(12));
        root.addView(sub);

        root.addView(button("1 · DISPOSITIVOS", v -> run(this::listDevices)));
        root.addView(button("2 · GRABAR 10 s DE LOS 8 CANALES", v -> run(this::recordStems)));
        root.addView(button("2b · ¿CUÁNTOS CANALES PASA ANDROID? (20 s)", v -> run(this::scanOpens)));
        root.addView(button("3 · FM-1 + MIC USB A LA VEZ (3 s)", v -> run(this::twoInputs)));
        root.addView(button("4 · ENVIAR EL MIC AL FM-1 (5 s, modo IN+OUT)", v -> run(this::sendMic)));
        root.addView(button("5 · DRIVER PROPIO: 8 CANALES DIRECTO (10 s)", v -> run(this::directStems)));
        root.addView(button("6 · FM-1 DIRECTO + MIC DEL CELU (10 s)", v -> run(this::stemsAndMic)));
        root.addView(button("7 · MIC AL FM-1 Y VUELTA + LATENCIA (STEMS+IN, 10 s)", v -> run(this::stemsIn)));
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        Button copy = button("COPIAR INFORME", v -> copyReport());
        Button share = button("COMPARTIR", v -> shareReport());
        row.addView(copy, new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1));
        row.addView(share, new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1));
        root.addView(row);

        log = new TextView(this);
        log.setTextColor(FG);
        log.setTextSize(12);
        log.setTypeface(Typeface.MONOSPACE);
        log.setTextIsSelectable(true);
        log.setPadding(0, dp(12), 0, dp(24));
        ScrollView sv = new ScrollView(this);
        sv.addView(log);
        root.addView(sv, new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1));
        setContentView(root);

        say("JIANT Probe 0.4 · " + Build.MANUFACTURER + " " + Build.MODEL + " · Android " + Build.VERSION.RELEASE
            + " (API " + Build.VERSION.SDK_INT + ")");
        if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED)
            requestPermissions(new String[] {Manifest.permission.RECORD_AUDIO}, 1);
    }

    // ------------------------------------------------------------------ UI helpers
    private int dp(int v) { return Math.round(v * getResources().getDisplayMetrics().density); }

    private Button button(String text, View.OnClickListener l) {
        Button bt = new Button(this);
        bt.setText(text);
        bt.setAllCaps(false);
        bt.setTextColor(FG);
        bt.setBackgroundColor(0xFF1A1820);
        bt.setOnClickListener(l);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
                                                                     LinearLayout.LayoutParams.WRAP_CONTENT);
        lp.setMargins(0, dp(3), dp(3), dp(3));
        bt.setLayoutParams(lp);
        return bt;
    }

    private void say(String s) {
        report.append(s).append('\n');
        ui.post(() -> log.setText(report.toString()));
    }

    private void run(Runnable r) {
        if (busy) { say("(esperá a que termine la prueba anterior)"); return; }
        if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            say("Falta el permiso de micrófono: aceptalo y volvé a tocar el botón.");
            requestPermissions(new String[] {Manifest.permission.RECORD_AUDIO}, 1);
            return;
        }
        busy = true;
        new Thread(() -> {
            try { r.run(); } catch (Throwable t) { say("ERROR: " + t); }
            busy = false;
        }).start();
    }

    private void copyReport() {
        ClipboardManager cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
        cm.setPrimaryClip(ClipData.newPlainText("JIANT Probe", report.toString()));
        say("(informe copiado)");
    }

    private void shareReport() {
        Intent i = new Intent(Intent.ACTION_SEND);
        i.setType("text/plain");
        i.putExtra(Intent.EXTRA_SUBJECT, "JIANT Probe " + Build.MODEL);
        i.putExtra(Intent.EXTRA_TEXT, report.toString());
        startActivity(Intent.createChooser(i, "Compartir informe"));
    }

    // ------------------------------------------------------------------ devices
    private static String typeName(int t) {
        switch (t) {
            case AudioDeviceInfo.TYPE_USB_DEVICE: return "USB_DEVICE";
            case AudioDeviceInfo.TYPE_USB_HEADSET: return "USB_HEADSET";
            case AudioDeviceInfo.TYPE_USB_ACCESSORY: return "USB_ACCESSORY";
            case AudioDeviceInfo.TYPE_BUILTIN_MIC: return "BUILTIN_MIC";
            case AudioDeviceInfo.TYPE_BUILTIN_SPEAKER: return "BUILTIN_SPEAKER";
            case AudioDeviceInfo.TYPE_BUILTIN_EARPIECE: return "EARPIECE";
            case AudioDeviceInfo.TYPE_WIRED_HEADSET: return "WIRED_HEADSET";
            case AudioDeviceInfo.TYPE_WIRED_HEADPHONES: return "WIRED_HEADPHONES";
            case AudioDeviceInfo.TYPE_BLUETOOTH_SCO: return "BT_SCO";
            case AudioDeviceInfo.TYPE_BLUETOOTH_A2DP: return "BT_A2DP";
            case AudioDeviceInfo.TYPE_TELEPHONY: return "TELEPHONY";
            default: return "type " + t;
        }
    }

    private static boolean isUsb(AudioDeviceInfo d) {
        int t = d.getType();
        return t == AudioDeviceInfo.TYPE_USB_DEVICE || t == AudioDeviceInfo.TYPE_USB_HEADSET
               || t == AudioDeviceInfo.TYPE_USB_ACCESSORY;
    }

    private static boolean isFm1(AudioDeviceInfo d) {
        String n = String.valueOf(d.getProductName()).toUpperCase(Locale.ROOT);
        return isUsb(d) && (n.contains("JIANT") || n.contains("FM") || n.contains("FELUCCA"));
    }

    private void describe(AudioDeviceInfo d) {
        say("  · " + d.getProductName() + "  [" + typeName(d.getType()) + ", id " + d.getId() + "]");
        say("      canales: " + Arrays.toString(d.getChannelCounts())
            + "  máscaras índice: " + hexes(d.getChannelIndexMasks()));
        say("      frecuencias: " + (d.getSampleRates().length == 0 ? "cualquiera" : Arrays.toString(d.getSampleRates()))
            + "  codificaciones: " + Arrays.toString(d.getEncodings()));
    }

    private static String hexes(int[] a) {
        StringBuilder s = new StringBuilder("[");
        for (int i = 0; i < a.length; i++) s.append(i == 0 ? "" : ", ").append("0x").append(Integer.toHexString(a[i]));
        return s.append(']').toString();
    }

    private void listDevices() {
        say("\n== 1 · DISPOSITIVOS");
        say("Entradas:");
        for (AudioDeviceInfo d : am.getDevices(AudioManager.GET_DEVICES_INPUTS)) describe(d);
        say("Salidas:");
        for (AudioDeviceInfo d : am.getDevices(AudioManager.GET_DEVICES_OUTPUTS)) describe(d);
        say("UNPROCESSED soportado: " + am.getProperty(AudioManager.PROPERTY_SUPPORT_AUDIO_SOURCE_UNPROCESSED));
        say("Frecuencia nativa: " + am.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE) + " Hz, buffer "
            + am.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER) + " frames");
        MidiManager mm = (MidiManager) getSystemService(Context.MIDI_SERVICE);
        if (mm == null) { say("MIDI: no disponible"); return; }
        MidiDeviceInfo[] md = mm.getDevices();
        say("MIDI: " + md.length + " dispositivo(s)");
        for (MidiDeviceInfo m : md) {
            Bundle p = m.getProperties();
            say("  · " + p.getString(MidiDeviceInfo.PROPERTY_NAME) + "  [" + (m.getType() == MidiDeviceInfo.TYPE_USB ? "USB" : "tipo " + m.getType())
                + ", entradas " + m.getInputPortCount() + ", salidas " + m.getOutputPortCount() + "]");
        }
    }

    private AudioDeviceInfo findUsbInput(boolean fm1) {
        AudioDeviceInfo any = null;
        for (AudioDeviceInfo d : am.getDevices(AudioManager.GET_DEVICES_INPUTS)) {
            if (!isUsb(d)) continue;
            if (isFm1(d) == fm1) return d;
            if (any == null) any = d;
        }
        return fm1 ? any : null;
    }

    // ------------------------------------------------------------------ 2 · eight channels
    /** opens an AudioRecord of n channels (index mask) on device d; null when Android refuses it */
    private AudioRecord open(AudioDeviceInfo d, int n, boolean indexMask) {
        int src = "true".equals(am.getProperty(AudioManager.PROPERTY_SUPPORT_AUDIO_SOURCE_UNPROCESSED))
                  ? MediaRecorder.AudioSource.UNPROCESSED : MediaRecorder.AudioSource.MIC;
        return open(d, n, indexMask, src);
    }

    private AudioRecord open(AudioDeviceInfo d, int n, boolean indexMask, int src) {
        AudioFormat.Builder fb = new AudioFormat.Builder().setEncoding(AudioFormat.ENCODING_PCM_16BIT).setSampleRate(RATE);
        if (indexMask) fb.setChannelIndexMask((1 << n) - 1);
        else fb.setChannelMask(n == 1 ? AudioFormat.CHANNEL_IN_MONO : AudioFormat.CHANNEL_IN_STEREO);
        try {
            AudioRecord r = new AudioRecord.Builder().setAudioSource(src).setAudioFormat(fb.build())
                                .setBufferSizeInBytes(RATE / 4 * n * 2).build();
            if (r.getState() != AudioRecord.STATE_INITIALIZED) { r.release(); return null; }
            if (d != null) r.setPreferredDevice(d);
            return r;
        } catch (Throwable t) {
            say("      (" + n + " canales" + (indexMask ? ", máscara índice" : "") + ": " + t.getMessage() + ")");
            return null;
        }
    }

    private void recordStems() {
        say("\n== 2 · GRABAR 10 s");
        AudioDeviceInfo d = findUsbInput(true);
        if (d == null) { say("No veo ninguna entrada USB. ¿El FM-1 está enchufado y en USB › STEMS?"); return; }
        say("Entrada: " + d.getProductName() + " (canales que declara: " + Arrays.toString(d.getChannelCounts()) + ")");
        AudioRecord r = null;
        int n = 0;
        for (int want : new int[] {8, 6, 4, 2}) {           /* the most channels Android will give */
            r = open(d, want, true);
            if (r != null) { n = want; break; }
        }
        if (r == null) { r = open(d, 2, false); n = 2; }
        if (r == null) { say("RESULTADO: Android no abrió la entrada USB."); return; }
        say("Pedí " + n + " canales; Android dio " + r.getChannelCount() + " a " + r.getSampleRate() + " Hz.");
        n = r.getChannelCount();
        int frames = RATE * 10;
        short[] all = new short[frames * n];
        short[] buf = new short[RATE / 20 * n];
        r.startRecording();
        AudioDeviceInfo routed = r.getRoutedDevice();
        say("Ruteado a: " + (routed == null ? "?" : routed.getProductName() + " [" + typeName(routed.getType()) + "]"));
        say("Grabando… tocá algo en el FM-1 (que suenen varias pistas).");
        int got = 0, reads = 0, shorts = 0;
        long t0 = System.nanoTime();
        while (got < all.length) {
            if (System.nanoTime() - t0 > 20_000_000_000L) { say("(se cortó a los 20 s: el FM-1 no mandaba audio)"); break; }
            int k = r.read(buf, 0, Math.min(buf.length, all.length - got));
            if (k < 0) { say("read() devolvió " + k); break; }
            if (k < buf.length && got + k < all.length) shorts++;
            System.arraycopy(buf, 0, all, got, k);
            got += k;
            reads++;
        }
        double secs = (System.nanoTime() - t0) / 1e9;
        r.stop();
        r.release();
        say(String.format(Locale.ROOT, "Leí %d frames en %.2f s (%d lecturas, %d cortas).", got / n, secs, reads, shorts));
        int live = levels(all, got, n, true);
        say("Canales con señal: " + live + " de " + n + ".");
        String name = saveWav(all, got, n);
        say(name != null ? "WAV guardado: Descargas/JIANT/" + name : "No pude guardar el WAV.");
        say(live >= 6 ? "RESULTADO: este teléfono pasa " + live + " canales con sonido por la API normal."
            : live > 0 ? "RESULTADO: Android abrió " + n + " canales pero solo " + live + " traen sonido (si sonaban más pistas,"
                         + " Android pasa solo los primeros). Probá 2b y 5."
            : "RESULTADO: no llegó sonido. ¿Sonaba el FM-1?");
    }

    /** per-channel peaks and rms of interleaved pcm (count samples, n channels); the channels that carry sound */
    private int levels(short[] all, int got, int n, boolean print) {
        int live = 0;
        for (int c = 0; c < n; c++) {
            int pk = 0;
            double e = 0;
            for (int i = c; i < got; i += n) {
                int v = Math.abs(all[i]);
                if (v > pk) pk = v;
                e += (double) all[i] * all[i];
            }
            double rms = Math.sqrt(e / Math.max(1, got / n));
            double db = pk > 0 ? 20 * Math.log10(pk / 32768.0) : -120;
            if (pk > 64) live++;
            if (print)
                say(String.format(Locale.ROOT, "  canal %d: pico %6.1f dBFS  rms %7.1f%s%s", c + 1, db, rms,
                                  pk > 64 ? "" : "  (silencio)", pk >= 32767 ? "  (¡satura!)" : ""));
        }
        return live;
    }

    // ------------------------------------------------------------------ 2b · the ways of opening the input
    private void scanOpens() {
        say("\n== 2b · ¿CUÁNTOS CANALES PASA ANDROID?");
        AudioDeviceInfo d = findUsbInput(true);
        if (d == null) { say("No veo ninguna entrada USB. ¿El FM-1 está enchufado y en USB › STEMS?"); return; }
        say("Dejá sonando las 6 pistas. Cada prueba graba 2 s.");
        say("Máscaras índice que declara: " + hexes(d.getChannelIndexMasks()) + "  canales: " + Arrays.toString(d.getChannelCounts()));
        int[] srcs = {MediaRecorder.AudioSource.UNPROCESSED, MediaRecorder.AudioSource.MIC, MediaRecorder.AudioSource.CAMCORDER,
                      MediaRecorder.AudioSource.VOICE_RECOGNITION};
        String[] names = {"UNPROCESSED", "MIC", "CAMCORDER", "VOICE_RECOG"};
        int best = 0;
        String bestHow = "";
        for (int si = 0; si < srcs.length; si++)
            for (int want : new int[] {8, 6}) {
                AudioRecord r = open(d, want, true, srcs[si]);
                if (r == null) { say(String.format(Locale.ROOT, "  %-11s %d ch: no abre", names[si], want)); continue; }
                int n = r.getChannelCount();
                short[] all = new short[RATE * 2 * n];
                r.startRecording();
                int got = 0;
                long t0 = System.nanoTime();
                while (got < all.length && System.nanoTime() - t0 < 5_000_000_000L) {
                    int k = r.read(all, got, Math.min(RATE / 20 * n, all.length - got));
                    if (k <= 0) break;
                    got += k;
                }
                r.stop();
                r.release();
                int live = levels(all, got, n, false);
                StringBuilder which = new StringBuilder();
                for (int c = 0; c < n; c++) {
                    int pk = 0;
                    for (int i = c; i < got; i += n) pk = Math.max(pk, Math.abs(all[i]));
                    which.append(pk > 64 ? (char) ('1' + c) : '·');
                }
                say(String.format(Locale.ROOT, "  %-11s %d ch: %d con sonido  [%s]", names[si], want, live, which));
                if (live > best) { best = live; bestHow = names[si] + " " + want + " ch"; }
            }
        say(best >= 6 ? "RESULTADO: " + bestHow + " pasa " + best + " canales: la app puede usar la API normal así."
                      : "RESULTADO: ninguna forma pasa más de " + best + " canales con sonido. Para los 8 hace falta el driver propio (5).");
    }

    private String saveWav(short[] pcm, int count, int ch) {
        String name = "probe_" + new SimpleDateFormat("yyyyMMdd_HHmmss", Locale.ROOT).format(new Date()) + "_" + ch + "ch.wav";
        try {
            ContentValues cv = new ContentValues();
            cv.put(MediaStore.MediaColumns.DISPLAY_NAME, name);
            cv.put(MediaStore.MediaColumns.MIME_TYPE, "audio/wav");
            cv.put(MediaStore.MediaColumns.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS + "/JIANT");
            Uri u = getContentResolver().insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, cv);
            if (u == null) return null;
            try (OutputStream o = getContentResolver().openOutputStream(u)) {
                int bytes = count * 2;
                byte[] h = new byte[44];
                wr(h, 0, 'R', 'I', 'F', 'F'); le32(h, 4, 36 + bytes); wr(h, 8, 'W', 'A', 'V', 'E');
                wr(h, 12, 'f', 'm', 't', ' '); le32(h, 16, 16); le16(h, 20, 1); le16(h, 22, ch);
                le32(h, 24, RATE); le32(h, 28, RATE * ch * 2); le16(h, 32, ch * 2); le16(h, 34, 16);
                wr(h, 36, 'd', 'a', 't', 'a'); le32(h, 40, bytes);
                o.write(h);
                byte[] b = new byte[count * 2];
                for (int i = 0; i < count; i++) { b[2 * i] = (byte) pcm[i]; b[2 * i + 1] = (byte) (pcm[i] >> 8); }
                o.write(b);
            }
            return name;
        } catch (Throwable t) {
            say("  (" + t.getMessage() + ")");
            return null;
        }
    }

    private static void wr(byte[] h, int o, char a, char b, char c, char d) { h[o] = (byte) a; h[o + 1] = (byte) b; h[o + 2] = (byte) c; h[o + 3] = (byte) d; }
    private static void le16(byte[] h, int o, int v) { h[o] = (byte) v; h[o + 1] = (byte) (v >> 8); }
    private static void le32(byte[] h, int o, int v) { le16(h, o, v); le16(h, o + 2, v >> 16); }

    // ------------------------------------------------------------------ 5 · our own driver
    private static final String USB_PERM = "fm.jiant.probe.USB";

    /** asks the user for the device (Android's dialog); true when granted */
    private boolean permit(UsbManager um, UsbDevice dev) {
        if (um.hasPermission(dev)) return true;
        CountDownLatch done = new CountDownLatch(1);
        BroadcastReceiver rx = new BroadcastReceiver() {
            @Override public void onReceive(Context c, Intent i) { done.countDown(); }
        };
        IntentFilter f = new IntentFilter(USB_PERM);
        if (Build.VERSION.SDK_INT >= 33) registerReceiver(rx, f, Context.RECEIVER_NOT_EXPORTED);
        else registerReceiver(rx, f);
        PendingIntent pi = PendingIntent.getBroadcast(this, 0, new Intent(USB_PERM).setPackage(getPackageName()),
                                                      PendingIntent.FLAG_MUTABLE);
        um.requestPermission(dev, pi);
        say("Aceptá el permiso USB que muestra Android…");
        try { done.await(60, TimeUnit.SECONDS); } catch (InterruptedException ignored) { }
        unregisterReceiver(rx);
        return um.hasPermission(dev);
    }

    private void directStems() {
        say("\n== 5 · DRIVER PROPIO (8 CANALES DIRECTO)");
        UsbManager um = (UsbManager) getSystemService(Context.USB_SERVICE);
        UsbDevice dev = null;
        UsbInterface on = null, off = null;
        UsbEndpoint ep = null;
        for (UsbDevice dv : um.getDeviceList().values()) {
            for (int i = 0; i < dv.getInterfaceCount(); i++) {
                UsbInterface it = dv.getInterface(i);
                if (it.getInterfaceClass() != UsbConstants.USB_CLASS_AUDIO || it.getInterfaceSubclass() != 2) continue;
                for (int e = 0; e < it.getEndpointCount(); e++) {
                    UsbEndpoint x = it.getEndpoint(e);
                    if (x.getType() == UsbConstants.USB_ENDPOINT_XFER_ISOC && x.getDirection() == UsbConstants.USB_DIR_IN
                        && on == null) { on = it; ep = x; dev = dv; }
                }
            }
            if (dev != null) {
                for (int i = 0; i < dv.getInterfaceCount(); i++) {
                    UsbInterface it = dv.getInterface(i);
                    if (it.getId() == on.getId() && it.getAlternateSetting() == 0) off = it;
                }
                break;
            }
        }
        if (dev == null) { say("No veo un dispositivo USB que grabe audio. ¿El FM-1 está enchufado y en USB › STEMS?"); return; }
        say(String.format(Locale.ROOT, "Dispositivo: %s (%04x:%04x), interfaz %d alt %d, EP 0x%02x, paquete máx %d bytes",
                          dev.getProductName(), dev.getVendorId(), dev.getProductId(), on.getId(), on.getAlternateSetting(),
                          ep.getAddress(), ep.getMaxPacketSize()));
        if (!permit(um, dev)) { say("RESULTADO: sin permiso USB no puedo leerlo."); return; }
        UsbDeviceConnection conn = um.openDevice(dev);
        if (conn == null) { say("RESULTADO: Android no me dejó abrir el dispositivo."); return; }
        int ch = 0, sub = 2;
        byte[] raw = conn.getRawDescriptors();           /* the format of our interface: type I, channels, bytes a sample */
        int curIf = -1, curAlt = -1;
        for (int i = 0; raw != null && i + 1 < raw.length && raw[i] > 0; i += raw[i] & 0xFF) {
            int len = raw[i] & 0xFF, type = raw[i + 1] & 0xFF;
            if (type == 4 && len >= 9) { curIf = raw[i + 2] & 0xFF; curAlt = raw[i + 3] & 0xFF; }
            else if (type == 0x24 && len >= 8 && raw[i + 2] == 2 && curIf == on.getId() && curAlt == on.getAlternateSetting()) {
                ch = raw[i + 4] & 0xFF;
                sub = raw[i + 5] & 0xFF;
            }
        }
        say("Formato: " + ch + " canales, " + (sub * 8) + " bit.");
        if (ch == 2) say("(2 canales: el FM-1 está en STEREO o IN+OUT. Para los stems: MENU › SYSTEM › USB › STEMS y reenchufá.)");
        if (ch == 0 || sub != 2) { say("RESULTADO: no entiendo el formato."); conn.close(); return; }
        if (!conn.claimInterface(on, true)) { say("RESULTADO: no pude tomar la interfaz de audio."); conn.close(); return; }
        try {
            if (!conn.setInterface(on)) { say("RESULTADO: el FM-1 no aceptó arrancar el audio (SET_INTERFACE)."); return; }
            conn.controlTransfer(0x22, 0x01, 0x0100, ep.getAddress(), new byte[] {0x44, (byte) 0xAC, 0x00}, 3, 500);
            int secs = 10;
            byte[] bytes = new byte[RATE * secs * ch * 2];
            int[] st = new int[6];
            say("Grabando " + secs + " s directo del FM-1…");
            long t0 = System.nanoTime();
            int got = isoRead(conn.getFileDescriptor(), ep.getAddress(), ep.getMaxPacketSize(), bytes, (secs + 5) * 1000, st);
            double el = (System.nanoTime() - t0) / 1e9;
            if (off != null) conn.setInterface(off);
            if (got <= 0) { say("RESULTADO: no llegó nada (error " + (-got) + ", paquetes " + st[0] + ")."); return; }
            int frames = got / (ch * 2);
            say(String.format(Locale.ROOT, "Leí %d frames en %.2f s (%.0f frames/s). Paquetes %d, vacíos %d, con error %d, el más grande %d bytes%s.",
                              frames, el, frames / el, st[0], st[1], st[2], st[4], st[3] != 0 ? ", errno " + st[3] : ""));
            short[] all = new short[frames * ch];
            for (int i = 0; i < all.length; i++) all[i] = (short) ((bytes[2 * i] & 0xFF) | (bytes[2 * i + 1] << 8));
            int live = levels(all, all.length, ch, true);
            say("Canales con señal: " + live + " de " + ch + ".");
            String name = saveWav(all, all.length, ch);
            say(name != null ? "WAV guardado: Descargas/JIANT/" + name : "No pude guardar el WAV.");
            boolean steady = Math.abs(frames / el - RATE) < RATE * 0.02 && st[2] == 0;
            say(live >= 6 && steady ? "RESULTADO: el driver propio lee los " + ch + " canales completos y estables. Es el camino para la app."
                : live >= 6 ? "RESULTADO: llegan los " + ch + " canales, pero el flujo no fue parejo (mirá frames/s y errores)."
                : "RESULTADO: llegaron " + live + " canales con sonido. ¿Sonaban las 6 pistas?");
        } finally {
            conn.releaseInterface(on);
            conn.close();
            say("(Si después el audio USB normal no anda, desenchufá y volvé a enchufar el FM-1.)");
        }
    }

    // ------------------------------------------------------------------ 6 · the stems and the phone's mic together
    private void stemsAndMic() {
        say("\n== 6 · FM-1 DIRECTO + MIC DEL CELU");
        AudioDeviceInfo mic = null;
        for (AudioDeviceInfo d : am.getDevices(AudioManager.GET_DEVICES_INPUTS))
            if (d.getType() == AudioDeviceInfo.TYPE_BUILTIN_MIC) { mic = d; break; }
        AudioRecord r = open(mic, 1, false);
        if (r == null) { say("RESULTADO: no pude abrir el mic del celu."); return; }
        say("Mic: " + (mic == null ? "el de Android por defecto" : mic.getProductName()) + ". Hablá o aplaudí cerca del celu mientras suena el FM-1.");
        short[] m = new short[RATE * 12];
        int[] got = {0};
        long[] first = {0};
        Thread t = new Thread(() -> {
            r.startRecording();
            while (got[0] < m.length && !Thread.currentThread().isInterrupted()) {
                int k = r.read(m, got[0], Math.min(RATE / 20, m.length - got[0]));
                if (k <= 0) break;
                if (got[0] == 0) first[0] = System.nanoTime();
                got[0] += k;
            }
            r.stop();
        });
        t.start();
        directStems();
        t.interrupt();
        try { t.join(2000); } catch (InterruptedException ignored) { }
        r.release();
        say("Mic del celu:");
        int live = levels(m, got[0], 1, true);
        say(String.format(Locale.ROOT, "  %.2f s de mic grabados a la vez.", got[0] / (double) RATE));
        String name = saveWav(m, got[0], 1);
        say(name != null ? "WAV del mic: Descargas/JIANT/" + name : "No pude guardar el WAV del mic.");
        say(live > 0 ? "RESULTADO: el mic del celu graba junto al FM-1: la app puede sumarlo como una pista más."
                     : "RESULTADO: el mic no trajo sonido mientras leía el FM-1.");
    }

    // ------------------------------------------------------------------ 7 · the mic through the FM-1 and back
    /** the streaming interface (alt 1) with an isochronous endpoint going dir, its alt 0, the endpoint; null if none */
    private static Object[] stream(UsbDevice dv, int dir) {
        for (int i = 0; i < dv.getInterfaceCount(); i++) {
            UsbInterface it = dv.getInterface(i);
            if (it.getInterfaceClass() != UsbConstants.USB_CLASS_AUDIO || it.getInterfaceSubclass() != 2) continue;
            for (int e = 0; e < it.getEndpointCount(); e++) {
                UsbEndpoint x = it.getEndpoint(e);
                if (x.getType() != UsbConstants.USB_ENDPOINT_XFER_ISOC || x.getDirection() != dir) continue;
                UsbInterface off = null;
                for (int j = 0; j < dv.getInterfaceCount(); j++)
                    if (dv.getInterface(j).getId() == it.getId() && dv.getInterface(j).getAlternateSetting() == 0) off = dv.getInterface(j);
                return new Object[] {it, off, x};
            }
        }
        return null;
    }

    /** the lag (frames, 0..max) at which b best follows a, and the peak's share of the correlation (0..1) */
    private static double[] lag(float[] a, float[] b, int max) {
        int d = 4, n = Math.min(a.length, b.length) / d - max / d - 1;
        if (n < 1000) return new double[] {-1, 0};
        float[] x = new float[n + max / d + 1], y = new float[n + max / d + 1];
        for (int i = 0; i < x.length; i++) {               /* 11 kHz, the slope: the onsets */
            float sa = 0, sb = 0;
            for (int k = 0; k < d; k++) { sa += a[i * d + k]; sb += b[i * d + k]; }
            x[i] = sa; y[i] = sb;
        }
        for (int i = x.length - 1; i > 0; i--) { x[i] -= x[i - 1]; y[i] -= y[i - 1]; }
        int best = 0;
        double bv = -1e30, sum = 0;
        double[] c = new double[max / d + 1];
        for (int l = 0; l <= max / d; l++) {
            double v = 0;
            for (int i = 1; i < n; i++) v += x[i] * y[i + l];
            c[l] = v;
            sum += Math.abs(v);
            if (v > bv) { bv = v; best = l; }
        }
        int fb = best * d;                                  /* then the full rate, +-d */
        double fv = -1e30;
        for (int l = Math.max(0, best * d - d); l <= best * d + d; l++) {
            double v = 0;
            for (int i = d; i + l < b.length && i < a.length - 1 && i < n * d; i++)
                v += (a[i] - a[i - 1]) * (b[i + l] - b[i + l - 1]);
            if (v > fv) { fv = v; fb = l; }
        }
        return new double[] {fb, sum > 0 ? bv / sum * (max / d + 1) / 8.0 : 0};
    }

    private void stemsIn() {
        say("\n== 7 · MIC AL FM-1 Y VUELTA (STEMS+IN)");
        say("Para medir la latencia: FM-1 en MENU › SYSTEM › USB › STEMS+IN (firmware 0.8.0), sin auriculares, el celu a"
            + " unos 20 cm del parlante del FM-1 y un tema con batería sonando. El mic oye al FM-1 y la app mide cuánto tarda en volver."
            + " Bajale el LEVEL a la última pista activa (o muteala) para que el mic no acople por el parlante.");
        UsbManager um = (UsbManager) getSystemService(Context.USB_SERVICE);
        UsbDevice dev = null;
        Object[] in = null, out = null;
        for (UsbDevice dv : um.getDeviceList().values()) {
            in = stream(dv, UsbConstants.USB_DIR_IN);
            out = stream(dv, UsbConstants.USB_DIR_OUT);
            if (in != null) { dev = dv; break; }
        }
        if (dev == null) { say("No veo el FM-1 grabando por USB."); return; }
        if (out == null) { say("RESULTADO: el FM-1 no tiene salida USB en este modo. Poné MENU › SYSTEM › USB › STEMS+IN y reenchufá."); return; }
        UsbInterface inOn = (UsbInterface) in[0], inOff = (UsbInterface) in[1], outOn = (UsbInterface) out[0], outOff = (UsbInterface) out[1];
        UsbEndpoint ep = (UsbEndpoint) in[2], epo = (UsbEndpoint) out[2];
        if (!permit(um, dev)) { say("RESULTADO: sin permiso USB no puedo."); return; }
        UsbDeviceConnection conn = um.openDevice(dev);
        if (conn == null) { say("RESULTADO: Android no me dejó abrir el dispositivo."); return; }
        int ch = ep.getMaxPacketSize() / (46 * 2);
        say("Entrada: " + ch + " canales (paquete " + ep.getMaxPacketSize() + " bytes); salida: EP 0x" + Integer.toHexString(epo.getAddress()));
        if (ch != 10) { say("RESULTADO: espero 10 canales (STEMS+IN). ¿El FM-1 tiene el firmware 0.8.0 y está en STEMS+IN?"); conn.close(); return; }
        AudioDeviceInfo micDev = null;
        for (AudioDeviceInfo d : am.getDevices(AudioManager.GET_DEVICES_INPUTS))
            if (d.getType() == AudioDeviceInfo.TYPE_BUILTIN_MIC) { micDev = d; break; }
        AudioRecord r = open(micDev, 1, false, MediaRecorder.AudioSource.UNPROCESSED);
        if (r == null) r = open(micDev, 1, false, MediaRecorder.AudioSource.MIC);
        if (r == null) { say("RESULTADO: no pude abrir el mic del celu."); conn.close(); return; }
        final AudioRecord mic = r;
        if (!conn.claimInterface(inOn, true) || !conn.claimInterface(outOn, true)) {
            say("RESULTADO: no pude tomar las interfaces de audio."); mic.release(); conn.close(); return;
        }
        java.util.concurrent.atomic.AtomicBoolean stop = new java.util.concurrent.atomic.AtomicBoolean();
        Thread t = new Thread(() -> {
            short[] b = new short[128];
            mic.startRecording();
            while (!stop.get()) {
                int k = mic.read(b, 0, b.length);
                if (k <= 0) break;
                pushMic(b, k);
            }
            mic.stop();
        });
        try {
            if (!conn.setInterface(inOn) || !conn.setInterface(outOn)) { say("RESULTADO: el FM-1 no aceptó arrancar el audio."); return; }
            int secs = 10;
            byte[] bytes = new byte[RATE * secs * ch * 2];
            int[] st = new int[9];
            t.start();
            say("Grabando " + secs + " s: el mic va al FM-1 (última pista activa) y vuelven los 10 canales…");
            long t0 = System.nanoTime();
            int got = duplex(conn.getFileDescriptor(), ep.getAddress(), ep.getMaxPacketSize(), epo.getAddress(), bytes, (secs + 5) * 1000, st);
            double el = (System.nanoTime() - t0) / 1e9;
            stop.set(true);
            try { t.join(1000); } catch (InterruptedException ignored) { }
            conn.setInterface(outOff);
            conn.setInterface(inOff);
            if (got <= 0) { say("RESULTADO: no llegó nada (error " + (-got) + ")."); return; }
            int frames = got / (ch * 2);
            say(String.format(Locale.ROOT, "Entrada: %d frames en %.2f s (%.0f/s), paquetes %d, vacíos %d, error %d.",
                              frames, el, frames / el, st[0], st[1], st[2]));
            say(String.format(Locale.ROOT, "Salida: %d frames enviados (%.0f/s), %d en silencio por falta de mic, %d de mic descartados%s.",
                              st[6], st[6] / el, st[7], st[8], st[3] != 0 ? ", errno " + st[3] : ""));
            short[] all = new short[frames * ch];
            for (int i = 0; i < all.length; i++) all[i] = (short) ((bytes[2 * i] & 0xFF) | (bytes[2 * i + 1] << 8));
            String[] names = {"T1", "T2", "T3", "T4", "T5", "T6", "FX L", "FX R", "MIC (mezclado)", "MIC (crudo)"};
            for (int c = 0; c < ch; c++) {
                int pk = 0;
                for (int i = c; i < all.length; i += ch) pk = Math.max(pk, Math.abs(all[i]));
                say(String.format(Locale.ROOT, "  %-15s pico %6.1f dBFS%s", names[c], pk > 0 ? 20 * Math.log10(pk / 32768.0) : -120,
                                  pk > 64 ? "" : "  (silencio)"));
            }
            float[] mix = new float[frames], raw = new float[frames];
            for (int i = 0; i < frames; i++) {
                float m = 0;
                for (int c = 0; c < 8; c++) m += all[i * ch + c];
                mix[i] = m;
                raw[i] = all[i * ch + 9];
            }
            double[] lg = lag(mix, raw, RATE / 5);
            int L = (int) lg[0];
            say(String.format(Locale.ROOT, "LATENCIA: el sonido del FM-1 vuelve por el mic %d frames después = %.1f ms (confianza %.1f; > 3 es clara).",
                              L, L * 1000.0 / RATE, lg[1]));
            String name = saveWav(all, all.length, ch);
            say(name != null ? "WAV 10 canales: Descargas/JIANT/" + name : "No pude guardar el WAV.");
            if (L > 0 && lg[1] > 3) {
                short[] al = all.clone();                   /* the two mic channels moved earlier by the latency */
                for (int i = 0; i < frames; i++)
                    for (int c = 8; c < 10; c++) al[i * ch + c] = i + L < frames ? all[(i + L) * ch + c] : 0;
                String n2 = saveWav(al, al.length, ch);
                say(n2 != null ? "WAV alineado (el mic corrido " + L + " frames): Descargas/JIANT/" + n2 : "");
            }
            say(st[7] < RATE / 10 && st[2] == 0 && Math.abs(frames / el - RATE) < RATE * 0.02
                ? "RESULTADO: ida y vuelta estable. Escuchá el WAV alineado: el mic tiene que caer justo con el tema."
                : "RESULTADO: anduvo con cortes (mirá silencios y errores). Mandame el informe.");
        } finally {
            stop.set(true);
            mic.release();
            conn.releaseInterface(outOn);
            conn.releaseInterface(inOn);
            conn.close();
            say("(Si después el audio USB normal no anda, desenchufá y volvé a enchufar el FM-1.)");
        }
    }

    // ------------------------------------------------------------------ 3 · two USB inputs at once
    private void twoInputs() {
        say("\n== 3 · FM-1 + MIC USB A LA VEZ");
        List<AudioDeviceInfo> usb = new ArrayList<>();
        for (AudioDeviceInfo d : am.getDevices(AudioManager.GET_DEVICES_INPUTS)) if (isUsb(d)) usb.add(d);
        say("Entradas USB que veo: " + usb.size());
        for (AudioDeviceInfo d : usb) say("  · " + d.getProductName() + " [" + typeName(d.getType()) + "]");
        if (usb.size() < 2) {
            say("RESULTADO: Android muestra " + usb.size() + " entrada(s) USB. Con el FM-1 y un mic USB en un hub deberían ser 2;"
                + " si ves 1, este teléfono abre un solo dispositivo de audio USB a la vez.");
            return;
        }
        AudioRecord a = open(usb.get(0), 2, false), b = open(usb.get(1), 1, false);
        if (a == null || b == null) { say("RESULTADO: no pude abrir las dos a la vez."); if (a != null) a.release(); if (b != null) b.release(); return; }
        a.startRecording();
        b.startRecording();
        short[] ba = new short[2048], bb = new short[1024];
        int pa = 0, pb = 0;
        long end = System.currentTimeMillis() + 3000;
        while (System.currentTimeMillis() < end) {
            int ka = a.read(ba, 0, ba.length, AudioRecord.READ_NON_BLOCKING), kb = b.read(bb, 0, bb.length, AudioRecord.READ_NON_BLOCKING);
            for (int i = 0; i < ka; i++) pa = Math.max(pa, Math.abs(ba[i]));
            for (int i = 0; i < kb; i++) pb = Math.max(pb, Math.abs(bb[i]));
            try { Thread.sleep(5); } catch (InterruptedException ignored) { }
        }
        AudioDeviceInfo ra = a.getRoutedDevice(), rb = b.getRoutedDevice();
        a.stop(); b.stop(); a.release(); b.release();
        say("  " + usb.get(0).getProductName() + ": pico " + pa + ", ruteado a " + (ra == null ? "?" : ra.getProductName()));
        say("  " + usb.get(1).getProductName() + ": pico " + pb + ", ruteado a " + (rb == null ? "?" : rb.getProductName()));
        boolean both = ra != null && rb != null && ra.getId() != rb.getId() && pa > 64 && pb > 64;
        say(both ? "RESULTADO: las dos entradas USB funcionan a la vez."
                 : "RESULTADO: no llegó señal de las dos por separado (o Android las mandó al mismo dispositivo).");
    }

    // ------------------------------------------------------------------ 4 · the phone's mic into the FM-1
    private void sendMic() {
        say("\n== 4 · ENVIAR EL MIC AL FM-1");
        AudioDeviceInfo out = null, mic = null;
        for (AudioDeviceInfo d : am.getDevices(AudioManager.GET_DEVICES_OUTPUTS)) if (isUsb(d)) { out = d; break; }
        for (AudioDeviceInfo d : am.getDevices(AudioManager.GET_DEVICES_INPUTS)) if (d.getType() == AudioDeviceInfo.TYPE_BUILTIN_MIC) { mic = d; break; }
        if (out == null) { say("No veo una salida USB. Poné el FM-1 en MENU › SYSTEM › USB › IN+OUT y volvé a probar."); return; }
        say("Salida: " + out.getProductName() + "  ·  micrófono: " + (mic == null ? "el de siempre" : mic.getProductName()));
        AudioRecord r;
        try {
            r = new AudioRecord.Builder().setAudioSource(MediaRecorder.AudioSource.VOICE_RECOGNITION)
                    .setAudioFormat(new AudioFormat.Builder().setEncoding(AudioFormat.ENCODING_PCM_16BIT).setSampleRate(RATE)
                                        .setChannelMask(AudioFormat.CHANNEL_IN_MONO).build())
                    .setBufferSizeInBytes(RATE / 10 * 2).build();
        } catch (Throwable t) { say("No pude abrir el micrófono: " + t.getMessage()); return; }
        if (mic != null) r.setPreferredDevice(mic);
        AudioTrack tr = new AudioTrack.Builder()
                .setAudioFormat(new AudioFormat.Builder().setEncoding(AudioFormat.ENCODING_PCM_16BIT).setSampleRate(RATE)
                                    .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO).build())
                .setBufferSizeInBytes(RATE / 10 * 4).setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY).build();
        tr.setPreferredDevice(out);
        r.startRecording();
        tr.play();
        say("Hablá al teléfono: tu voz tendría que salir por el FM-1 (pista T6) durante 5 s.");
        short[] in = new short[256], st = new short[512];
        int pk = 0;
        long end = System.currentTimeMillis() + 5000;
        while (System.currentTimeMillis() < end) {
            int k = r.read(in, 0, in.length);
            if (k <= 0) continue;
            for (int i = 0; i < k; i++) { st[2 * i] = st[2 * i + 1] = in[i]; pk = Math.max(pk, Math.abs(in[i])); }
            tr.write(st, 0, 2 * k);
        }
        AudioDeviceInfo routed = tr.getRoutedDevice();
        r.stop(); r.release(); tr.stop(); tr.release();
        say("Pico del mic: " + pk + "  ·  la salida fue a: " + (routed == null ? "?" : routed.getProductName() + " [" + typeName(routed.getType()) + "]"));
        say(routed != null && isUsb(routed) ? "RESULTADO: el audio salió por USB. Decime si lo escuchaste en el FM-1."
                                            : "RESULTADO: Android no mandó el audio al FM-1.");
    }
}

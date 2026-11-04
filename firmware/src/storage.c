/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Persistent storage on the SPI NOR.
 *
 * Every object has an A/B sector pair. A save goes to the copy that is not
 * the current one: erase the sector, program the payload pages (from offset
 * 256), then the 32-byte header at offset 0 LAST. The header is the commit
 * record (magic, seq, length, payload CRC, header CRC); on load the valid
 * copy with the newest seq wins (including wrap), so a write torn at any point leaves the
 * previous copy in charge.
 *
 * Flash access goes through three hooks (also used by the host test):
 *   st_read(off, dst, n)   st_erase(off)   st_prog(off, src, n)
 */
#define ST_MAGIC 0x554C4546u                   /* "FELU" */
#define ST_SECTOR 4096u
#define ST_PAYLOAD_OFF 256u
#define ST_PAYLOAD_MAX (ST_SECTOR - ST_PAYLOAD_OFF)

/* flash map (FL_DATA 0x97000..0xDFFFF, FL_GLOB 0xFC000.., FL_AUTO 0xE5000..0xE6FFF, FL_VBANK 0xE7000): settings 0xFC000, projects
 * 0x97000..0x9EFFF, 0xA0000..0xDBFFF free (Felucca's user sample slots: JIANT's songs, docs/TONIC-SONG-PLAN.md 4d),
 * user preset banks 0xDC000..0xDFFFF (upreset.c),
 * the user presets' FM6 patches (up_fm6.c, since 1.0.3): copy A 0x9F000, copy B 0xFE000; the autosave (project.c,
 * 1.2): A 0xE5000, B 0xE6000, above the OTA staging (0xE0000..0xE4FFF), in the margin the stock firmware's own update
 * stages into (only ever while the stock firmware runs: a later Felucca finds no valid copy and starts as without one).
 * Those two sectors held the FM6 patch bank of 1.0..1.0.2 (OBJ_FM6BANK, retired): both objects use the same pair,
 * told apart by the commit record's type. The bank is only read, once, to move its patches into the user presets
 * (up_fm6.c upf_boot); the first write of the new object goes to the sector that does not hold the bank's newest
 * copy (st_save_to), so a power cut never loses both.
 * The FM6 voice bank (JIANT 0.6.11, from Felucca 1.4.1, fm6_vbank.c): 0xE7000, the sector above the autosave, with a
 * commit record of the same kind (type OBJ_FM6VB, not an st_load object); a new bank is first staged in the
 * autosave's older copy (fm6_vbank.c). */
enum { OBJ_SETTINGS, OBJ_PROJECT0, OBJ_UPRESET0 = OBJ_PROJECT0 + 4, OBJ_FM6BANK = OBJ_UPRESET0 + 2, OBJ_UPFM6,
       OBJ_AUTOSAVE, OBJ_SONGIDX, OBJ_SONG1, OBJ_COUNT = OBJ_SONG1 + 7 * 4 };
/* JIANT's songs (song_chain.c): song 1's four sections are the four project slots (OBJ_PROJECT0..3, 0x97000..), songs
 * 2..8 four objects each (OBJ_SONG1 + (song - 1) * 4 + section) at 0xA0000..0xD7FFF, the song index (the songs' rows,
 * the current song) at 0xD8000 / 0xD9000; 0xDA000..0xDBFFF free */
#define OBJ_FM6VB 64u                          /* the voice bank's commit records (fm6_vbank.c; >= OBJ_COUNT, 39 with
                                                * the songs: st_load never takes one, no object takes its sector.
                                                * Felucca's 32 is a song object here) */
_Static_assert(OBJ_FM6VB >= OBJ_COUNT, "the voice bank's record type is no object's");

typedef struct {
    uint32_t magic;
    uint16_t type, slot;
    uint32_t seq, len, crc, rsv[2];
    uint32_t hcrc;
} st_hdr_t;
_Static_assert(sizeof(st_hdr_t) == 32u, "storage commit record layout");

static int st_read(uint32_t off, void *dst, uint32_t n);
static int st_erase(uint32_t off);
static int st_prog(uint32_t off, const void *src, uint32_t n);

static uint32_t st_crc32_add(uint32_t c, const void *p, uint32_t n)   /* zlib CRC-32, 4 bits per step; running:
                                                                         * from 0xFFFFFFFF, the CRC is ~c */
{
    static const uint32_t T[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu};
    const uint8_t *b = p;
    while (n--) {
        c ^= *b++;
        c = (c >> 4) ^ T[c & 15u];
        c = (c >> 4) ^ T[c & 15u];
    }
    return c;
}
static uint32_t st_crc32(const void *p, uint32_t n) { return ~st_crc32_add(0xFFFFFFFFu, p, n); }

static uint32_t st_sector(uint32_t obj, uint32_t copy)  /* flash offset of copy A (0) / B (1) */
{
    if (obj == OBJ_SETTINGS)
        return 0xFC000u + copy * ST_SECTOR;
    if (obj == OBJ_FM6BANK || obj == OBJ_UPFM6)          /* (the same pair: see the flash map) */
        return copy ? 0xFE000u : 0x9F000u;
    if (obj == OBJ_AUTOSAVE)
        return 0xE5000u + copy * ST_SECTOR;
    if (obj == OBJ_SONGIDX)
        return 0xD8000u + copy * ST_SECTOR;
    if (obj >= OBJ_SONG1)
        return 0xA0000u + (obj - OBJ_SONG1) * 2u * ST_SECTOR + copy * ST_SECTOR;
    if (obj >= OBJ_UPRESET0)
        return 0xDC000u + (obj - OBJ_UPRESET0) * 2u * ST_SECTOR + copy * ST_SECTOR;
    return 0x97000u + (obj - OBJ_PROJECT0) * 2u * ST_SECTOR + copy * ST_SECTOR;
}

static uint8_t st_buf[ST_PAYLOAD_MAX] __attribute__((aligned(4), section(".lowbuf")));   /* (0.9.0: app.ld LOWBUF, was
                                                   * .bss: main loop only, zeroed at boot as .bss) */

static int st_head(uint32_t obj, uint32_t copy, st_hdr_t *h)   /* commit record valid: 0 */
{
    if (obj >= OBJ_COUNT || copy > 1u)
        return -1;
    if (st_read(st_sector(obj, copy), h, sizeof *h))
        return -1;
    if (h->magic != ST_MAGIC || h->type != obj || h->slot != copy || h->len > ST_PAYLOAD_MAX ||
        h->hcrc != st_crc32(h, sizeof *h - 4u))
        return -1;
    return 0;
}

static int st_body(uint32_t obj, uint32_t copy, const st_hdr_t *h)   /* payload -> st_buf, CRC ok: 0 */
{
    if (st_read(st_sector(obj, copy) + ST_PAYLOAD_OFF, st_buf, h->len) || st_crc32(st_buf, h->len) != h->crc)
        return -1;
    return 0;
}

/* the current copy: the newest valid sequence (A on a tie), -1 when
 * neither is valid. Headers first, so only the winner's payload is read (it
 * is left in st_buf); *h gets its header. */
static int st_current(uint32_t obj, st_hdr_t *h)
{
    st_hdr_t a, b;
    int va = st_head(obj, 0, &a) == 0, vb = st_head(obj, 1, &b) == 0;
    if (vb && (!va || (b.seq != a.seq && b.seq - a.seq < 0x80000000u))) {
        if (st_body(obj, 1, &b) == 0) {
            *h = b;
            return 1;
        }
        vb = 0;
    }
    if (va && st_body(obj, 0, &a) == 0) {
        *h = a;
        return 0;
    }
    if (vb && st_body(obj, 1, &b) == 0) {
        *h = b;
        return 1;
    }
    return -1;
}

/* load the whole object into dst; returns its length, or -1 if it does not fit */
static int st_load(uint32_t obj, void *dst, uint32_t max)
{
    uint32_t i;
    st_hdr_t h;
    if (obj >= OBJ_COUNT || st_current(obj, &h) < 0 || h.len > max)
        return -1;
    for (i = 0; i < h.len; i++)
        ((uint8_t *)dst)[i] = st_buf[i];
    return (int)h.len;
}

/* save into copy `to` (0 A, 1 B; -1: the one that is not the current copy, as st_save) */
static int st_save_to(uint32_t obj, const void *src, uint32_t len, int to)
{
    uint32_t seq, base, off;
    int cur, rc;
    st_hdr_t h;
    if (obj >= OBJ_COUNT || len > ST_PAYLOAD_MAX || to > 1)
        return -1;
    cur = st_current(obj, &h);
    seq = cur < 0 ? 0u : h.seq;
    if (to < 0)
        to = cur == 0 ? 1 : 0;                        /* write the other copy */
    base = st_sector(obj, (uint32_t)to);
    for (off = 0; off < len; off++)
        st_buf[off] = ((const uint8_t *)src)[off];    /* the driver wants RAM sources */
    if ((rc = st_erase(base)) != 0)
        return rc;
    for (off = 0; off < len; off += 256u) {
        uint32_t n = len - off > 256u ? 256u : len - off;
        if ((rc = st_prog(base + ST_PAYLOAD_OFF + off, st_buf + off, n)) != 0)
            return rc;
    }
    h.magic = ST_MAGIC;
    h.type = (uint16_t)obj;
    h.slot = (uint16_t)to;
    h.seq = seq + 1u;
    h.len = len;
    h.crc = st_crc32(st_buf, len);
    h.rsv[0] = h.rsv[1] = 0xFFFFFFFFu;
    h.hcrc = st_crc32(&h, sizeof h - 4u);
    if ((rc = st_prog(base, &h, sizeof h)) != 0)       /* the commit record, last */
        return rc;
    {   /* read back: a write-protected or failing part must not report SAVED */
        st_hdr_t chk;
        uint32_t c = (uint32_t)to;
        if (st_head(obj, c, &chk) || memcmp(&chk, &h, sizeof h) || st_body(obj, c, &chk))
            return -7;
    }
    return 0;
}

static int st_save(uint32_t obj, const void *src, uint32_t len) { return st_save_to(obj, src, len, -1); }

/* -------- optional extension stored in the first page (bytes 32..255) --------

   Layout of the 256-byte first page:
     0..31   commit header (st_hdr_t)
     32..35  extension magic     (0x4558544E = "EXTN")
     36..37  extension version   (uint16_t, =1)
     38..39  extension data len  (uint16_t)
     40..251 extension data      (max 212 bytes)
     252..255 CRC-32 of extension data

   Header reserved fields mirror extension state:
     rsv[0] = extension CRC     (0xFFFFFFFF if no extension)
     rsv[1] = extension version (0xFFFFFFFF if no extension)
   Old objects have both rsv fields == 0xFFFFFFFF and no extension area.
*/
#define EXT_MAGIC 0x4558544Eu
#define EXT_VERSION 1u
#define EXT_OFF 32u
#define EXT_DATA_OFF 40u
#define EXT_CRC_OFF 252u
#define EXT_MAX_DATA (EXT_CRC_OFF - EXT_DATA_OFF)  /* 212 bytes */
#define EXT_PAGE_SIZE (EXT_CRC_OFF - EXT_OFF + 4u) /* 224 bytes: header page tail */

/* Return 1 when copy `copy` of object `obj` is fully valid: header CRC,
 * payload CRC, and (when reserved fields indicate one) extension metadata,
 * version, length and CRC. 0 otherwise. *h gets the commit header. */
static int st_copy_valid(uint32_t obj, uint32_t copy, st_hdr_t *h)
{
    uint8_t ext_page[EXT_PAGE_SIZE];
    uint32_t magic, crc_stored, crc_calc;
    uint16_t version, ext_len;
    int has_ext, no_ext;

    if (st_head(obj, copy, h) != 0)
        return 0;
    if (st_body(obj, copy, h) != 0)
        return 0;

    has_ext = (h->rsv[0] != 0xFFFFFFFFu && h->rsv[1] != 0xFFFFFFFFu);
    no_ext = (h->rsv[0] == 0xFFFFFFFFu && h->rsv[1] == 0xFFFFFFFFu);
    if (!has_ext && !no_ext)
        return 0;   /* inconsistent reserved fields: not a trustworthy copy */

    if (has_ext) {
        if (st_read(st_sector(obj, copy) + EXT_OFF, ext_page, sizeof ext_page))
            return 0;
        memcpy(&magic, ext_page, 4);
        memcpy(&version, ext_page + 4, 2);
        memcpy(&ext_len, ext_page + 6, 2);
        memcpy(&crc_stored, ext_page + (EXT_CRC_OFF - EXT_OFF), 4);
        if (magic != EXT_MAGIC || version != EXT_VERSION || ext_len > EXT_MAX_DATA)
            return 0;
        crc_calc = st_crc32(ext_page + (EXT_DATA_OFF - EXT_OFF), ext_len);
        if (crc_stored != crc_calc || crc_stored != h->rsv[0])
            return 0;
    }

    return 1;
}

/* Save payload plus optional extension data.
 * Builds the complete 256-byte first page in RAM, writes payload pages
 * first, then programs the header+extension page last. Returns 0 on success. */
int st_save_ext(uint32_t obj, const void *src, uint32_t len,
                const void *ext, uint32_t ext_len)
{
    uint32_t seq, base, off;
    int cur, rc;
    st_hdr_t h, h_a, h_b;
    uint8_t page[256];
    uint32_t crc;
    int to_copy, va, vb;
    uint32_t ext_magic;
    uint16_t ext_version;

    if (obj >= OBJ_COUNT || len > ST_PAYLOAD_MAX || ext_len > EXT_MAX_DATA)
        return -1;
    if ((ext == 0) != (ext_len == 0u))
        return -1;

    va = st_copy_valid(obj, 0, &h_a);
    vb = st_copy_valid(obj, 1, &h_b);
    if (va && vb)
        cur = (h_b.seq != h_a.seq && h_b.seq - h_a.seq < 0x80000000u) ? 1 : 0;
    else if (va)
        cur = 0;
    else if (vb)
        cur = 1;
    else
        cur = -1;

    seq = cur < 0 ? 0u : (cur == 0 ? h_a.seq : h_b.seq);
    to_copy = cur < 0 ? 0 : (cur == 0 ? 1 : 0);
    base = st_sector(obj, (uint32_t)to_copy);

    /* Stage the payload. */
    for (off = 0; off < len; off++)
        st_buf[off] = ((const uint8_t *)src)[off];

    if ((rc = st_erase(base)) != 0)
        return rc;

    /* Program payload pages first. */
    for (off = 0; off < len; off += 256u) {
        uint32_t n = len - off > 256u ? 256u : len - off;
        if ((rc = st_prog(base + ST_PAYLOAD_OFF + off, st_buf + off, n)) != 0)
            return rc;
    }

    /* Build the complete first page: header + extension. */
    memset(page, 0xFF, sizeof page);
    h.magic = ST_MAGIC;
    h.type = (uint16_t)obj;
    h.slot = (uint16_t)to_copy;
    h.seq = seq + 1u;
    h.len = len;
    h.crc = st_crc32(st_buf, len);
    h.rsv[0] = h.rsv[1] = 0xFFFFFFFFu;
    memcpy(page, &h, sizeof h);

    if (ext && ext_len) {
        ext_magic = EXT_MAGIC;
        ext_version = (uint16_t)EXT_VERSION;
        memcpy(page + EXT_OFF, &ext_magic, 4);        /* byte 32: magic */
        memcpy(page + EXT_OFF + 4, &ext_version, 2);  /* byte 36: version */
        memcpy(page + EXT_OFF + 6, &ext_len, 2);      /* byte 38: length */
        memcpy(page + EXT_DATA_OFF, ext, ext_len);    /* byte 40: data */
        crc = st_crc32(page + EXT_DATA_OFF, ext_len);
        memcpy(page + EXT_CRC_OFF, &crc, 4);          /* byte 252: CRC */
        h.rsv[0] = crc;
        h.rsv[1] = EXT_VERSION;
    }
    h.hcrc = st_crc32(&h, sizeof h - 4u);
    memcpy(page, &h, sizeof h);   /* re-copy header with final hcrc and rsv */

    /* Program the first page (header + extension) in one operation. */
    if ((rc = st_prog(base, page, sizeof page)) != 0)
        return rc;

    /* Read-back verify header, payload, extension metadata/length/CRC/data. */
    {
        st_hdr_t chk;
        uint8_t ext_page[EXT_PAGE_SIZE];
        uint32_t c = (uint32_t)to_copy;
        if (st_head(obj, c, &chk) || memcmp(&chk, &h, sizeof h) || st_body(obj, c, &chk))
            return -7;
        if (ext && ext_len) {
            if (st_read(base + EXT_OFF, ext_page, sizeof ext_page))
                return -7;
            uint32_t flash_magic, flash_crc;
            uint16_t flash_ver, flash_len;
            memcpy(&flash_magic, ext_page, 4);
            memcpy(&flash_ver, ext_page + 4, 2);
            memcpy(&flash_len, ext_page + 6, 2);
            memcpy(&flash_crc, ext_page + (EXT_CRC_OFF - EXT_OFF), 4);
            if (flash_magic != EXT_MAGIC || flash_ver != EXT_VERSION ||
                flash_len != ext_len || flash_crc != crc ||
                memcmp(ext_page + (EXT_DATA_OFF - EXT_OFF), ext, ext_len) != 0)
                return -7;
        }
    }
    return 0;
}

/* Load payload plus optional extension from the same A/B copy.
 * Evaluates both copies and picks the newest one with a valid header,
 * valid payload, and valid extension (if present). Falls back to the
 * older copy if the newer one's extension is corrupt or missing.
 *
 * Returns payload length (>=0), or -1 if no valid object, -2 if the
 * extension exists but ext_max is too small.
 * *ext_len_out is set to the extension length (0 if none).
 */
int st_load_ext(uint32_t obj, void *dst, uint32_t max,
                void *ext_dst, uint32_t ext_max, uint32_t *ext_len_out)
{
    uint32_t i;
    st_hdr_t h_a, h_b, *best;
    int va, vb, best_copy = -1;
    uint32_t ext_len = 0;

    if (obj >= OBJ_COUNT) return -1;
    if (ext_len_out) *ext_len_out = 0;

    va = st_copy_valid(obj, 0, &h_a);
    vb = st_copy_valid(obj, 1, &h_b);

    if (va && vb)
        best_copy = (h_b.seq != h_a.seq && h_b.seq - h_a.seq < 0x80000000u) ? 1 : 0;
    else if (va)
        best_copy = 0;
    else if (vb)
        best_copy = 1;

    if (best_copy < 0) return -1;

    best = best_copy == 0 ? &h_a : &h_b;

    if (best->len > max) return -1;

    /* Determine extension length from the winning copy. */
    if (best->rsv[0] != 0xFFFFFFFFu && best->rsv[1] != 0xFFFFFFFFu) {
        uint8_t ext_page[EXT_PAGE_SIZE];
        if (st_read(st_sector(obj, (uint32_t)best_copy) + EXT_OFF, ext_page, sizeof ext_page))
            return -1;
        memcpy(&ext_len, ext_page + 6, 2);
    }

    /* Re-read the payload for the winning copy into st_buf. */
    st_body(obj, (uint32_t)best_copy, best);

    for (i = 0; i < best->len; i++)
        ((uint8_t *)dst)[i] = st_buf[i];

    if (ext_len_out) *ext_len_out = ext_len;

    if (ext_len > 0) {
        if (!ext_dst || ext_max < ext_len)
            return -2;
        {
            uint8_t ext_page[EXT_PAGE_SIZE];
            if (st_read(st_sector(obj, (uint32_t)best_copy) + EXT_OFF, ext_page, sizeof ext_page))
                return -1;
            memcpy(ext_dst, ext_page + (EXT_DATA_OFF - EXT_OFF), ext_len);
        }
    }

    return (int)best->len;
}

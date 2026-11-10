/* FRAGMAP.C - shows where the files on a FAT12/FAT16 drive are split.
 *
 * Read-only: the boot sector, the FAT and the folders are read with INT 25h;
 * nothing is ever written to the disk.
 *
 *   FRAGMAP [d:] [/L]
 *     d:  drive to show (default: the current drive)
 *     /L  print a report of the split files instead of the map
 *
 * The map: every cell stands for a few clusters. Green = file in one piece,
 * red = file that is split, cyan = folder, yellow = the selected file,
 * dots = free. Up/Down picks the split files (most pieces first) and shows
 * where their pieces lie; L lists them.
 *
 * OpenWatcom, 8086 (-0), small model. Screen through BIOS INT 10h, keys through
 * INT 16h (MDA, Hercules, CGA, EGA, VGA). MS-DOS 3.0+, FAT12 and FAT16.
 * ERRORLEVEL: 0 done, 1 drive could not be read or is not FAT12/16, 2 bad options.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <i86.h>

#define VERSION "1.0"

#pragma pack(1)
struct dpkt { unsigned long sector; unsigned count; void __far *buf; };
typedef struct {                  /* one split file or folder, 96 bytes = 6 paragraphs */
    unsigned start, pieces, clusters;
    unsigned long size;
    unsigned char isdir;
    char path[85];
} FENT;
#pragma pack()

extern int __cdecl abs_read_pkt(int drive, struct dpkt *pkt);
extern int __cdecl abs_read_old(int drive, unsigned sector, unsigned count, void __far *buf);

/* cluster classes */
enum { C_FREE, C_FILE, C_SPLIT, C_DIR, C_BAD, C_LOST, C_NUM };
#define C_SEL 0x80

#define PATH_MAX_LEN 80
#define MAX_DEPTH    32

static int drv;                   /* 0 = A: */
static char dl;                   /* drive letter */
static int old_form;              /* 1: use the DOS 2/3 INT 25h form */
static struct dpkt pk;
static unsigned bps, spc, nres, nfats, nroot, spf, root_secs;
static unsigned long tot, fat_lba, root_lba, data_lba;
static unsigned nclus, last_clus; /* data clusters are 2..last_clus */
static int fat12;
static unsigned fat_seg, cls_seg, lst_seg, lst_max, lst_n;
static unsigned long lst_over;
static unsigned *order;
static unsigned char secbuf[512];
static char path[PATH_MAX_LEN + 16];

static unsigned long n_files, n_dirs, n_split_files, n_split_dirs, n_extra;
static unsigned long c_used, c_free, c_bad, c_lost, c_cross, n_broken, n_readerr, n_deep;

#define VALID(c) ((c) >= 2 && (c) <= last_clus)
#define ENT(i)   ((FENT __far *)MK_FP(lst_seg + (i) * 6, 0))

static unsigned fat_get(unsigned c)
{
    return *(unsigned __far *)MK_FP(fat_seg + (c >> 3), (c & 7) << 1);
}

static unsigned char __far *cls_at(unsigned c)
{
    return (unsigned char __far *)MK_FP(cls_seg + (c >> 4), c & 15);
}

static char *commas(unsigned long v, char *buf)
{
    char t[16];
    int n = 0, j = 0, i;
    do {
        if (n && n % 3 == 0) t[j++] = ',';
        t[j++] = (char)('0' + (int)(v % 10));
        v /= 10;
        n++;
    } while (v);
    for (i = 0; i < j; i++) buf[i] = t[j - 1 - i];
    buf[j] = 0;
    return buf;
}

/* ---------------------------------------------------------------- disk */

static int rd(unsigned long sec, unsigned cnt, void __far *buf)
{
    if (!old_form) {
        pk.sector = sec;
        pk.count = cnt;
        pk.buf = buf;
        return abs_read_pkt(drv, &pk);
    }
    if (sec + cnt > 0xFFFFUL) return -1;
    return abs_read_old(drv, (unsigned)sec, cnt, buf);
}

static int fail(const char *msg)
{
    printf("FRAGMAP: %c: %s\n", dl, msg);
    return 1;
}

static int open_drive(void)
{
    unsigned char *b = secbuf;
    unsigned long data;
    union REGS r;

    r.x.ax = 0x4409;                      /* network drive? */
    r.h.bl = (unsigned char)(drv + 1);
    intdos(&r, &r);
    if (r.x.cflag) return fail("is not a valid drive");
    if (r.x.dx & 0x1000) return fail("is a network drive - FRAGMAP needs a local FAT drive");

    old_form = 0;
    if (rd(0, 1, secbuf)) {
        old_form = 1;
        if (rd(0, 1, secbuf)) return fail("cannot read the boot sector");
    }
    bps = *(unsigned *)(b + 11);
    spc = b[13];
    nres = *(unsigned *)(b + 14);
    nfats = b[16];
    nroot = *(unsigned *)(b + 17);
    tot = *(unsigned *)(b + 19);
    if (!tot) tot = *(unsigned long *)(b + 32);
    spf = *(unsigned *)(b + 22);

    if (bps != 512) return fail("sector size is not 512 bytes - not supported");
    if (!spf) return fail("is FAT32 - FRAGMAP shows FAT12 and FAT16 drives");
    if (!spc || (spc & (spc - 1)) || !nfats || !nres || !tot)
        return fail("has no valid FAT boot sector");

    root_secs = (unsigned)(((unsigned long)nroot * 32 + 511) / 512);
    fat_lba = nres;
    root_lba = fat_lba + (unsigned long)nfats * spf;
    data_lba = root_lba + root_secs;
    if (data_lba >= tot) return fail("has no valid FAT boot sector");
    data = (tot - data_lba) / spc;
    if (data > 65524UL) return fail("has too many clusters for FAT16");
    nclus = (unsigned)data;
    last_clus = nclus + 1;
    fat12 = nclus < 4085;
    if (!fat12 && (unsigned long)(last_clus + 1) * 2 > (unsigned long)spf * 512)
        return fail("FAT is smaller than the drive - boot sector looks wrong");
    if (fat12 && (unsigned long)(last_clus + 1) * 3 / 2 + 1 > (unsigned long)spf * 512)
        return fail("FAT is smaller than the drive - boot sector looks wrong");
    return 0;
}

static unsigned paras(unsigned long bytes)
{
    return (unsigned)((bytes + 15) >> 4);
}

static int load_fat(void)
{
    unsigned raw, s, n, c;
    unsigned long need = (unsigned long)(last_clus + 1) * 2;

    if (_dos_allocmem(paras(need), &fat_seg) ||
        _dos_allocmem(paras((unsigned long)last_clus + 1), &cls_seg))
        return fail("not enough memory for the FAT");
    if (fat12) {
        if (_dos_allocmem(spf * 32, &raw)) return fail("not enough memory for the FAT");
    } else {
        _dos_freemem(fat_seg);            /* FAT16: read straight into place */
        if (_dos_allocmem(spf * 32, &fat_seg)) return fail("not enough memory for the FAT");
        raw = fat_seg;
    }
    for (s = 0; s < spf; s += n) {
        n = spf - s;
        if (n > 32) n = 32;
        if (rd(fat_lba + s, n, MK_FP(raw + s * 32, 0))) return fail("cannot read the FAT");
    }
    if (fat12) {
        for (c = 0; c <= last_clus; c++) {
            unsigned off = c + (c >> 1);
            unsigned char __far *p = (unsigned char __far *)MK_FP(raw + (off >> 4), off & 15);
            unsigned v = p[0] | ((unsigned)p[1] << 8);
            v = (c & 1) ? v >> 4 : v & 0xFFF;
            if (v >= 0xFF8) v = 0xFFFF;
            else if (v == 0xFF7) v = 0xFFF7;
            *(unsigned __far *)MK_FP(fat_seg + (c >> 3), (c & 7) << 1) = v;
        }
        _dos_freemem(raw);
    }
    for (c = 0; c <= last_clus; c++) *cls_at(c) = C_FREE;

    lst_max = 2000;                       /* split-file list: as much as fits */
    while (lst_max && _dos_allocmem(lst_max * 6, &lst_seg)) lst_max /= 2;
    return 0;
}

/* ---------------------------------------------------------------- scan */

/* clusters in the chain from c; pieces = runs of consecutive clusters */
static unsigned chain_len(unsigned c, unsigned *pieces, int *broken)
{
    unsigned n = 0, nx;
    *pieces = 0;
    *broken = 0;
    if (!VALID(c)) { *broken = 1; return 0; }
    *pieces = 1;
    for (;;) {
        n++;
        if (n > nclus) { *broken = 1; break; }   /* chain runs in a circle */
        nx = fat_get(c);
        if (nx >= 0xFFF8) break;
        if (!VALID(nx)) { *broken = 1; break; }  /* free, bad or out of range */
        if (nx != c + 1) (*pieces)++;
        c = nx;
    }
    return n;
}

static void chain_mark(unsigned c, unsigned n, unsigned char k)
{
    while (n-- && VALID(c)) {
        unsigned char __far *p = cls_at(c);
        if (*p != C_FREE) c_cross++;
        else *p = k;
        c = fat_get(c);
    }
}

static void do_chain(unsigned start, unsigned long size, int isdir)
{
    unsigned pieces, n;
    int broken;

    if (!start) return;                   /* empty file */
    n = chain_len(start, &pieces, &broken);
    if (broken) n_broken++;
    if (!n) return;
    chain_mark(start, n, (unsigned char)(isdir ? C_DIR : (pieces > 1 ? C_SPLIT : C_FILE)));
    if (pieces < 2) return;
    if (isdir) n_split_dirs++; else n_split_files++;
    n_extra += pieces - 1;
    if (lst_n < lst_max) {
        FENT __far *e = ENT(lst_n);
        e->start = start;
        e->pieces = pieces;
        e->clusters = n;
        e->size = size;
        e->isdir = (unsigned char)isdir;
        _fstrcpy(e->path, path);
        lst_n++;
    } else lst_over++;
}

static int add_name(int plen, const unsigned char *e)
{
    int i, n = plen;
    path[n++] = '\\';
    for (i = 0; i < 8 && e[i] != ' '; i++)
        path[n++] = (char)((i == 0 && e[0] == 0x05) ? 0xE5 : e[i]);
    if (e[8] != ' ') {
        path[n++] = '.';
        for (i = 8; i < 11 && e[i] != ' '; i++) path[n++] = (char)e[i];
    }
    path[n] = 0;
    return n;
}

static void walk_dir(unsigned clus, int plen, int depth);

/* one sector of a folder; returns 1 at the end of the folder */
static int scan_sector(unsigned long sec, int plen, int depth)
{
    int i, n;
    unsigned char *e;

    if (rd(sec, 1, secbuf)) { n_readerr++; return 1; }
    for (i = 0; i < 16; i++) {
        e = secbuf + i * 32;
        if (e[0] == 0) return 1;
        if (e[0] == 0xE5 || e[0] == '.') continue;
        if (e[11] == 0x0F || (e[11] & 0x08)) continue;   /* long name part, volume label */
        if (plen + 13 > PATH_MAX_LEN) { n_deep++; continue; }
        n = add_name(plen, e);
        if (e[11] & 0x10) {
            unsigned start = *(unsigned *)(e + 26);
            n_dirs++;
            do_chain(start, 0, 1);
            if (VALID(start) && depth < MAX_DEPTH) {
                walk_dir(start, n, depth + 1);
                if (rd(sec, 1, secbuf)) { n_readerr++; return 1; }   /* buffer was reused */
            } else if (depth >= MAX_DEPTH) n_deep++;
        } else {
            n_files++;
            do_chain(*(unsigned *)(e + 26), *(unsigned long *)(e + 28), 0);
        }
        path[plen] = 0;
    }
    return 0;
}

static void walk_dir(unsigned clus, int plen, int depth)
{
    unsigned steps = 0, s;
    unsigned long sec;

    while (VALID(clus) && steps++ <= nclus) {
        sec = data_lba + (unsigned long)(clus - 2) * spc;
        for (s = 0; s < spc; s++)
            if (scan_sector(sec + s, plen, depth)) return;
        clus = fat_get(clus);
    }
}

static void scan(void)
{
    unsigned s, c;
    path[0] = 0;
    for (s = 0; s < root_secs; s++)
        if (scan_sector(root_lba + s, 0, 0)) break;
    for (c = 2; c <= last_clus; c++) {
        unsigned char __far *p = cls_at(c);
        if (*p == C_FREE) {
            unsigned v = fat_get(c);
            if (!v) c_free++;
            else if (v == 0xFFF7) { *p = C_BAD; c_bad++; }
            else { *p = C_LOST; c_lost++; }
        } else c_used++;
        if (c == 0xFFFF) break;
    }
}

static int ent_cmp(unsigned a, unsigned b)
{
    FENT __far *x = ENT(a);
    FENT __far *y = ENT(b);
    if (x->pieces != y->pieces) return x->pieces > y->pieces ? -1 : 1;
    return _fstrcmp(x->path, y->path);
}

static void sort_list(void)
{
    unsigned i, j, gap, t;
    order = (unsigned *)malloc((lst_n ? lst_n : 1) * sizeof(unsigned));
    if (!order) { lst_n = 0; order = &lst_n; return; }
    for (i = 0; i < lst_n; i++) order[i] = i;
    for (gap = lst_n / 2; gap; gap /= 2)
        for (i = gap; i < lst_n; i++)
            for (j = i; j >= gap && ent_cmp(order[j - gap], order[j]) > 0; j -= gap) {
                t = order[j]; order[j] = order[j - gap]; order[j - gap] = t;
            }
}

/* ---------------------------------------------------------------- report */

static char *ent_path(FENT __far *e, char *buf)
{
    buf[0] = dl;
    buf[1] = ':';
    _fstrcpy(buf + 2, e->path);
    if (e->isdir) strcat(buf, "\\");
    return buf;
}

static unsigned long kb(unsigned long clusters)
{
    return clusters * spc / 2;
}

static void report(void)
{
    char a[16], b[16], c[16], p[100];
    unsigned i;
    FENT __far *e;

    printf("FRAGMAP %s - %c: FAT%d, %s clusters of %u bytes\n", VERSION, dl,
           fat12 ? 12 : 16, commas(nclus, a), spc * 512);
    printf("%s files, %s folders. Split: %s files, %lu folder%s (%s extra pieces)\n",
           commas(n_files, a), commas(n_dirs, b), commas(n_split_files, c), n_split_dirs,
           n_split_dirs == 1 ? "" : "s", commas(n_extra, p));
    printf("Used %s KB, free %s KB", commas(kb(c_used), a), commas(kb(c_free), b));
    if (c_bad) printf(", bad %lu clusters", c_bad);
    if (c_lost) printf(", lost %lu clusters", c_lost);
    if (c_cross) printf(", cross-linked %lu clusters", c_cross);
    if (n_broken) printf(", %lu broken chains", n_broken);
    if (n_readerr) printf(", %lu read errors", n_readerr);
    if (n_deep) printf(", %lu entries too deep", n_deep);
    printf("\n");
    if (!lst_n) {
        printf("\nNo split files.\n");
        return;
    }
    printf("\nPieces  Clusters          Bytes  Path\n");
    for (i = 0; i < lst_n; i++) {
        e = ENT(order[i]);
        printf("%6u  %8u  %13s  %s\n", e->pieces, e->clusters,
               e->isdir ? "<DIR>" : commas(e->size, a), ent_path(e, p));
    }
    if (lst_over) printf("... and %lu more (list full)\n", lst_over);
}

/* ---------------------------------------------------------------- screen */

#define MAP_TOP  1
#define MAP_ROWS 18
#define CELLS    (MAP_ROWS * 80)

static unsigned char vmode, orig_mode, mono, rows = 25;
static unsigned cur_shape, cpc;
static int sel = -1;

/* attributes and characters: colour / mono */
static unsigned char a_title, a_text, a_hi, a_key, a_bar, a_cls[C_NUM], a_sel;
static unsigned char ch_cls[C_NUM], ch_sel;

static void vid(union REGS *r) { int86(0x10, r, r); }

static void at(int r, int c)
{
    union REGS g;
    g.h.ah = 2; g.h.bh = 0; g.h.dh = (unsigned char)r; g.h.dl = (unsigned char)c;
    vid(&g);
}

static void put(int r, int c, unsigned char ch, unsigned char a, unsigned n)
{
    union REGS g;
    at(r, c);
    g.h.ah = 9; g.h.al = ch; g.h.bh = 0; g.h.bl = a; g.x.cx = n;
    vid(&g);
}

/* text at r,c, padded with blanks to w columns (stops at column 80) */
static int text(int r, int c, unsigned char a, const char *s, int w)
{
    while (*s && w > 0 && c < 80) { put(r, c++, (unsigned char)*s++, a, 1); w--; }
    if (w > 0 && c < 80) put(r, c, ' ', a, (unsigned)(c + w > 80 ? 80 - c : w));
    return c;
}

static void cls(void)
{
    union REGS g;
    g.x.ax = 0x0600; g.h.bh = 0x07; g.x.cx = 0;
    g.h.dh = (unsigned char)(rows - 1); g.h.dl = 79;
    vid(&g);
}

static void vid_init(void)
{
    union REGS g;
    unsigned char __far *bda_rows = (unsigned char __far *)MK_FP(0x40, 0x84);
    int i;

    g.h.ah = 0x0F; vid(&g);
    orig_mode = vmode = (unsigned char)(g.h.al & 0x7F);
    if (vmode != 2 && vmode != 3 && vmode != 7) {
        g.x.ax = 3; vid(&g);
        vmode = 3;
    }
    if (*bda_rows >= 24 && *bda_rows < 60) rows = (unsigned char)(*bda_rows + 1);
    mono = vmode == 7;
    g.h.ah = 3; g.h.bh = 0; vid(&g);
    cur_shape = g.x.cx;
    g.h.ah = 1; g.x.cx = 0x2000; vid(&g);           /* hide the cursor */

    for (i = 0; i < C_NUM; i++) { a_cls[i] = 0x07; ch_cls[i] = 0xDB; }
    ch_cls[C_FREE] = 0xFA;
    ch_cls[C_BAD] = 'B';
    if (mono) {
        a_title = 0x70; a_text = 0x07; a_hi = 0x0F; a_key = 0x70; a_bar = 0x70;
        ch_cls[C_SPLIT] = 0xB1; a_cls[C_SPLIT] = 0x0F;
        ch_cls[C_DIR] = 0xB0;
        ch_cls[C_LOST] = '?'; a_cls[C_LOST] = 0x0F;
        a_cls[C_BAD] = 0x0F;
        ch_sel = 0x04; a_sel = 0x70;
    } else {
        a_title = 0x1F; a_text = 0x07; a_hi = 0x0F; a_key = 0x30; a_bar = 0x70;
        a_cls[C_FREE] = 0x08;
        a_cls[C_FILE] = 0x0A;
        a_cls[C_SPLIT] = 0x0C;
        a_cls[C_DIR] = 0x0B;
        a_cls[C_BAD] = 0x4E;
        a_cls[C_LOST] = 0x0D;
        ch_sel = 0xDB; a_sel = 0x0E;
    }
    cpc = (unsigned)(((unsigned long)nclus + CELLS - 1) / CELLS);
    if (!cpc) cpc = 1;
}

static void vid_done(void)
{
    union REGS g;
    if (vmode != orig_mode) { g.h.ah = 0; g.h.al = orig_mode; vid(&g); }
    cls();
    g.h.ah = 1; g.x.cx = cur_shape; vid(&g);
    at(0, 0);
}

static void cell_look(unsigned idx, unsigned char *ch, unsigned char *a)
{
    unsigned long f = 2 + (unsigned long)idx * cpc, l = f + cpc, c;
    unsigned cnt[C_NUM], used = 0, span, k;
    int s = 0, i;

    if (f > last_clus) { *ch = ' '; *a = 0x07; return; }
    if (l > (unsigned long)last_clus + 1) l = (unsigned long)last_clus + 1;
    span = (unsigned)(l - f);
    for (i = 0; i < C_NUM; i++) cnt[i] = 0;
    for (c = f; c < l; c++) {
        k = *cls_at((unsigned)c);
        if (k & C_SEL) s = 1;
        k &= 0x7F;
        if (k < C_NUM) cnt[k]++;
        if (k != C_FREE) used++;
    }
    if (s) { *ch = ch_sel; *a = a_sel; return; }
    if (cnt[C_SPLIT]) k = C_SPLIT;
    else if (cnt[C_LOST]) k = C_LOST;
    else if (cnt[C_BAD]) k = C_BAD;
    else if (cnt[C_DIR] || cnt[C_FILE]) k = cnt[C_DIR] > cnt[C_FILE] ? C_DIR : C_FILE;
    else k = C_FREE;
    *a = a_cls[k];
    *ch = ch_cls[k];
    if (!mono && k != C_FREE && k != C_BAD)       /* colour: shade shows how full */
        *ch = used == span ? 0xDB : (used * 2 >= span ? 0xB2 : 0xB1);
}

static void draw_map(void)
{
    unsigned char ch[80], a[80];
    int r, c, n;
    for (r = 0; r < MAP_ROWS; r++) {
        for (c = 0; c < 80; c++) cell_look(r * 80 + c, &ch[c], &a[c]);
        for (c = 0; c < 80; c += n) {
            for (n = 1; c + n < 80 && ch[c + n] == ch[c] && a[c + n] == a[c]; n++)
                ;
            put(MAP_TOP + r, c, ch[c], a[c], n);
        }
    }
}

static void sel_mark(int on)
{
    FENT __far *e;
    unsigned c, n;
    if (sel < 0) return;
    e = ENT(order[sel]);
    for (c = e->start, n = e->clusters; n-- && VALID(c); c = fat_get(c)) {
        if (on) *cls_at(c) |= C_SEL;
        else *cls_at(c) &= ~C_SEL;
    }
}

static void extents(FENT __far *e, char *out, int w)
{
    unsigned c = e->start, n = e->clusters, first = c, len = 0;
    char t[24];
    strcpy(out, "Clusters");
    while (n-- && VALID(c)) {
        unsigned nx = fat_get(c);
        if (!n || !VALID(nx) || nx != c + 1) {
            if (first == c) sprintf(t, " %u", first);
            else sprintf(t, " %u-%u", first, c);
            len = strlen(out);
            if (len + strlen(t) > (unsigned)w - 4) { strcat(out, " ..."); return; }
            strcat(out, t);
            first = nx;
        }
        c = nx;
    }
}

static void legend_item(int *c, unsigned char ch, unsigned char a, const char *s)
{
    put(19, *c, ch, a, 1);
    *c = text(19, *c + 1, a_text, s, (int)strlen(s)) + 2;
}

static void draw_status(void)
{
    char l[100], a[16], b[16], c2[16], d[16];
    int c = 1;
    unsigned long pct10;

    put(19, 0, ' ', a_text, 80);
    legend_item(&c, mono ? 0xDB : 0xDB, a_cls[C_FILE], " file");
    legend_item(&c, ch_cls[C_SPLIT], a_cls[C_SPLIT], " split file");
    legend_item(&c, ch_cls[C_DIR], a_cls[C_DIR], " folder");
    legend_item(&c, ch_sel, a_sel, " selected");
    legend_item(&c, ch_cls[C_FREE], a_cls[C_FREE], " free");
    if (c_lost || c_bad) legend_item(&c, ch_cls[C_LOST], a_cls[C_LOST], " lost/bad");

    pct10 = n_files ? (n_split_files * 1000 + n_files / 2) / n_files : 0;
    sprintf(l, " Files %s   Folders %s   Split: %s files (%lu.%lu%%), %lu folder%s   Extra pieces %s",
            commas(n_files, a), commas(n_dirs, b), commas(n_split_files, c2),
            pct10 / 10, pct10 % 10, n_split_dirs, n_split_dirs == 1 ? "" : "s", commas(n_extra, d));
    text(20, 0, a_hi, l, 80);

    sprintf(l, " Used %s KB   Free %s KB", commas(kb(c_used), a), commas(kb(c_free), b));
    if (c_bad || c_lost || c_cross || n_broken || n_readerr) {
        sprintf(l + strlen(l), "   Bad %lu  Lost %lu  Cross %lu  Broken %lu", c_bad, c_lost, c_cross,
                n_broken + n_readerr);
    }
    text(21, 0, a_text, l, 80);

    if (sel < 0) {
        if (lst_n) sprintf(l, " %u split file%s - press \x19 to see where the pieces are (most pieces first)",
                           lst_n, lst_n > 1 ? "s" : "");
        else sprintf(l, " No split files on %c: - every file is in one piece.", dl);
        text(22, 0, a_text, l, 80);
        text(23, 0, a_text, "", 80);
    } else {
        FENT __far *e = ENT(order[sel]);
        char p[100];
        sprintf(l, " %d/%u  %s  ", sel + 1, lst_n, ent_path(e, p));
        c = text(22, 0, a_hi, l, (int)strlen(l));
        if (e->isdir) sprintf(l, "folder, %u pieces", e->pieces);
        else sprintf(l, "%s bytes, %u pieces", commas(e->size, a), e->pieces);
        text(22, c, a_text, l, 80 - c);
        l[0] = ' ';
        extents(e, l + 1, 78);
        text(23, 0, a_text, l, 80);
    }
    text(24, 0, a_key, lst_n ? " \x18\x19 select  PgUp PgDn  Home End  L list  Esc quit"
                             : " Esc quit", 80);
}

static void draw_title(const char *what)
{
    char l[100], a[16];
    sprintf(l, " FRAGMAP %s   %c:  FAT%d  %s clusters of %u KB   %s", VERSION, dl,
            fat12 ? 12 : 16, commas(nclus, a), spc / 2, what);
    text(0, 0, a_title, l, 80);
}

static unsigned key(void)
{
    union REGS g;
    g.h.ah = 0;
    int86(0x16, &g, &g);
    return g.x.ax;
}

#define K_UP   0x4800
#define K_DOWN 0x5000
#define K_PGUP 0x4900
#define K_PGDN 0x5100
#define K_HOME 0x4700
#define K_END  0x4F00

/* moves *pos by the key; returns 1 when the key was a move key */
static int move(unsigned k, int *pos, int lo, int page)
{
    int hi = (int)lst_n - 1, p = *pos;
    switch (k & 0xFF00) {
    case K_UP:   p--; break;
    case K_DOWN: p++; break;
    case K_PGUP: p -= page; break;
    case K_PGDN: p += page; break;
    case K_HOME: p = lo; break;
    case K_END:  p = hi; break;
    default: return 0;
    }
    if (p > hi) p = hi;
    if (p < lo) p = lo;
    *pos = p;
    return 1;
}

/* the list screen; returns the chosen entry or -2 for Esc */
static int list_screen(int cur)
{
    char l[100], a[16], p[100];
    int top = 0, i, r, page = 22;
    unsigned k;
    FENT __far *e;

    if (cur < 0) cur = 0;
    sprintf(l, "split: %u", lst_n);
    draw_title(l);
    text(1, 0, a_hi, " Pieces  Clusters          Bytes  Path", 80);
    text(24, 0, a_key, " \x18\x19 PgUp PgDn move   Enter show on the map   Esc back", 80);
    for (;;) {
        if (cur < top) top = cur;
        if (cur >= top + page) top = cur - page + 1;
        for (r = 0; r < page; r++) {
            i = top + r;
            if (i >= (int)lst_n) { text(2 + r, 0, a_text, "", 80); continue; }
            e = ENT(order[i]);
            sprintf(l, " %6u  %8u  %13s  %s", e->pieces, e->clusters,
                    e->isdir ? "<DIR>" : commas(e->size, a), ent_path(e, p));
            text(2 + r, 0, i == cur ? a_bar : a_text, l, 80);
        }
        k = key();
        if ((k & 0xFF) == 27) return -2;
        if ((k & 0xFF) == 13) return cur;
        move(k, &cur, 0, page);
    }
}

static void map_screen(void)
{
    unsigned k;
    int s, redraw = 1;
    char l[40];

    for (;;) {
        if (redraw) {
            sprintf(l, "1 cell = %u cluster%s", cpc, cpc > 1 ? "s" : "");
            draw_title(l);
            draw_map();
            draw_status();
            redraw = 0;
        }
        k = key();
        if ((k & 0xFF) == 27 || (k & 0xFF) == 'q' || (k & 0xFF) == 'Q') return;
        if (!lst_n) continue;
        s = sel;
        if ((k & 0xFF) == 'l' || (k & 0xFF) == 'L' || (k & 0xFF) == 13) {
            int r = list_screen(sel);
            if (r >= 0) s = r;
            sel_mark(0);
            sel = s;
            sel_mark(1);
            redraw = 1;
            continue;
        }
        if (!move(k, &s, sel < 0 && (k & 0xFF00) == K_UP ? -1 : 0, 10)) continue;
        if (s == sel) continue;
        sel_mark(0);
        sel = s;
        sel_mark(1);
        draw_map();
        draw_status();
    }
}

/* ---------------------------------------------------------------- main */

static void usage(void)
{
    printf("FRAGMAP %s - shows where the files on a FAT12/FAT16 drive are split.\n"
           "Read-only: nothing is written to the disk.\n\n"
           "FRAGMAP [d:] [/L]\n"
           "  d:  drive to show (default: the current drive)\n"
           "  /L  print the split files as a list instead of the map\n\n"
           "Map: green = file in one piece, red = split file, cyan = folder,\n"
           "yellow = selected file, dots = free. Up/Down selects the split files.\n",
           VERSION);
}

int main(int argc, char **argv)
{
    int i, list = 0;
    unsigned d;

    _dos_getdrive(&d);
    drv = (int)d - 1;
    for (i = 1; i < argc; i++) {
        char *a = argv[i];
        if ((a[0] == '/' || a[0] == '-') && (a[1] == 'l' || a[1] == 'L') && !a[2]) list = 1;
        else if ((a[0] == '/' || a[0] == '-') && a[1] == '?') { usage(); return 0; }
        else if (a[0] && a[1] == ':' && !a[2] &&
                 ((a[0] >= 'A' && a[0] <= 'Z') || (a[0] >= 'a' && a[0] <= 'z')))
            drv = (a[0] & 0xDF) - 'A';
        else { usage(); return 2; }
    }
    dl = (char)('A' + drv);

    if (!list) printf("FRAGMAP: reading %c: ...\n", dl);
    if (open_drive() || load_fat()) return 1;
    scan();
    sort_list();
    if (list) {
        report();
        return 0;
    }
    vid_init();
    map_screen();
    vid_done();
    printf("FRAGMAP: %c: %lu of %lu files split, %lu extra pieces.\n", dl, n_split_files,
           n_files, n_extra);
    return 0;
}

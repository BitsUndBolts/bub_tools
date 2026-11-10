# FRAGMAP

Shows where the files on a FAT12/FAT16 drive are split, as a colour map of the whole disk.
It only looks: there is no defragmenter, and nothing is ever written to the disk.

- Platform: MS-DOS 3.0 or later, 8086 or later, MDA/Hercules/CGA/EGA/VGA (80×25 text)
- Drives: FAT12 and FAT16 (floppies, hard disks, CF cards); FAT32 and network drives are refused
- Binary: [`bin/FRAGMAP.EXE`](bin/FRAGMAP.EXE) (about 20 KB), source: [`FRAGMAP.C`](FRAGMAP.C) + [`ABSREAD.ASM`](ABSREAD.ASM)

## Usage

```
FRAGMAP [d:]        the map (default: the current drive)
FRAGMAP [d:] /L     the split files as a text list (works with > FILE.TXT)
FRAGMAP /?          help
```

The map: each cell stands for a number of clusters (shown in the title bar).

| Colour | Meaning |
|--------|---------|
| green | file in one piece |
| red | file that is split |
| cyan | folder |
| yellow | the selected file |
| magenta | lost clusters (used in the FAT, but no file owns them); `B` = bad clusters |
| grey dots | free |

A full block means every cluster in the cell is used; ▓ and ▒ mean the cell is partly free.
A cell is red as soon as one cluster in it belongs to a split file.

Keys: **↑/↓** step through the split files, most pieces first. The selected file turns
yellow, and the bottom rows show its path, size, piece count and cluster ranges.
**PgUp/PgDn/Home/End** jump, **L** or **Enter** opens a full-screen list (Enter there shows
the file on the map), **Esc** quits.

The status rows also count lost clusters, bad clusters, cross-linked clusters and broken
chains, so FRAGMAP shows CHKDSK-style trouble too (it reports it but never fixes anything).

## How it works

1. Reads the boot sector with INT 25h (the DOS 4+ packet form, needed above 32 MB; falls
   back to the old form on DOS 3) and works out the FAT layout from the BPB.
2. Reads the whole FAT into memory (up to 128 KB for FAT16, allocated from DOS).
3. Walks every folder from the root, follows each file's cluster chain and counts the
   places where the next cluster is not the one right after the current one (= pieces).
4. Draws the map with BIOS INT 10h only, so it works on any display adapter.

Memory: about 20 KB program, plus 3 bytes per cluster and 96 bytes per split file
(up to 2000 listed). A 1 GB FAT16 disk needs about 200 KB in total.

## Exit codes

| Code | Meaning |
|------|---------|
| 0 | ok |
| 1 | drive not readable, not FAT12/16, or not enough memory |
| 2 | bad options |

## Build

[OpenWatcom](https://github.com/open-watcom/open-watcom-v2) and [NASM](https://www.nasm.us/):

```
nasm -f obj -o absread.obj ABSREAD.ASM
wcl -zq -cc -0 -ms -os -bt=dos -wx FRAGMAP.C absread.obj -fe=bin/FRAGMAP.EXE
```

(`-cc` makes sure `FRAGMAP.C` is compiled as C, not C++, on case-sensitive systems.)

## Testing

[`FATREF.PY`](FATREF.PY) is an independent Python reader that lists the split files of a
FAT12/16 disk image: `python FATREF.PY disk.img [partition offset in bytes]`.
FRAGMAP's `/L` output was compared with it in DOSBox-X on a 64 MB FAT16 hard disk image
(65,217 clusters, deliberately split files and a split folder) and a 1.44 MB FAT12 floppy:
both lists matched. On real hardware it ran on an ASUS P2B with MS-DOS 6.22 and a 1 GB
SanDisk CF card (62,477 clusters of 16 KB, 595 files): the whole scan took 0.4 s.

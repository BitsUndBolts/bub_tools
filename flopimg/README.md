# FLOPIMG

Raw, sector-by-sector floppy imaging for MS-DOS. The image includes the boot sector, so a
bootable disk written back to a floppy stays bootable.

- Platform: MS-DOS, 8086 or later, single `.COM` file (about 4 KB)
- Disks: any floppy whose geometry is described by its boot sector, up to 2 heads,
  36 sectors per track and 85 cylinders (360 KB to 2.88 MB). Tested layout: 1.44 MB.
- Binary: [`bin/FLOPIMG.COM`](bin/FLOPIMG.COM), source: [`FLOPIMG.ASM`](FLOPIMG.ASM)

## Usage

```
FLOPIMG <image> /read      floppy -> image file
FLOPIMG <image> /write     image -> floppy (erases the whole disk)
FLOPIMG <image> /verify    compare floppy with image, read only
FLOPIMG <image> /repair    compare, then write only the sectors that differ
FLOPIMG /?                 help screen
```

`/B` uses drive B: instead of A: (default).

Examples:

```
FLOPIMG MATS.IMG /read
FLOPIMG MATS.IMG /verify
FLOPIMG MATS.IMG /repair
FLOPIMG MATS.IMG /write /B
```

## What each function does

- **/read** copies the whole disk to the image file. Sectors that cannot be read are
  counted and reported (exit code 1). Asks before overwriting an existing image file.
- **/write** writes the image to the disk. Each track is read back and compared after
  writing; a mismatch stops the run with an error.
- **/verify** compares disk and image and lists up to the first 8 differing sectors. It
  never writes to the disk.
- **/repair** first compares read-only, reports how many sectors differ, and after
  confirmation writes only those sectors and reads each one back.

`/write` and `/repair` change the disk. They show a warning and wait for **Y** or **N**.
**N** aborts immediately, other keys are ignored.

The image size must match the geometry in its boot sector, otherwise it is refused.

## Exit codes

| Code | Meaning |
|------|---------|
| 0 | ok |
| 1 | differences found (`/verify`) or unreadable sectors (`/read`) |
| 2 | error (file not found, invalid image, disk error, write protected) |
| 3 | aborted by the user |

## Build

With [NASM](https://www.nasm.us/), flat binary:

```
nasm -f bin FLOPIMG.ASM -o bin/FLOPIMG.COM
```

## Testing

The logic was tested against an emulated floppy (BIOS INT 13h and DOS INT 21h simulated):
read, verify, write, repair, N-abort, write-protected disk, unreadable sectors and invalid
images. `/verify` was also run on a real 1.44 MB disk. `/write` and `/repair` have not been
run against real hardware yet.

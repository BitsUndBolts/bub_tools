# V2GUARD

Keeps the 3dfx Voodoo2 driver quiet when no Voodoo2 is in the PC — so the driver can stay
installed while the card is out.

With the Voodoo2 driver installed (e.g. 3dfx 3.02.02) but the card taken out, Windows shows
`_GlideInitEnvironment: glide2x.dll expected Voodoo^2, none detected` at every start, and
Display Properties has a **3dfx V2-1000** tab that freezes the PC for a while. The usual cure
is to uninstall the driver and install it again whenever the card goes back in. V2GUARD
makes that unnecessary.

- Platform: Windows 95 / 98 / ME
- Binary: [`bin/V2GUARD.EXE`](bin/V2GUARD.EXE) (30 KB) with [`bin/SETUP.BAT`](bin/SETUP.BAT),
  [`bin/REMOVE.BAT`](bin/REMOVE.BAT) and [`bin/README.TXT`](bin/README.TXT) — copy the whole
  `bin` folder to the PC. Source: [`V2GUARD.C`](V2GUARD.C)

## Usage

1. Copy `bin/` to the PC (e.g. `C:\V2GUARD`).
2. In Windows, run `SETUP.BAT` from that folder (double-click, or in an MS-DOS Prompt).
3. Restart Windows.

`REMOVE.BAT` undoes it: the driver gets its start entry and its tab back, exactly as it
installed them, and V2GUARD is removed. Both take `/Q` for no message box.

```
V2GUARD              what runs at every Windows start (silent)
V2GUARD /INSTALL     take over the driver's entries and run at every start
V2GUARD /UNINSTALL   give the driver its entries back, remove V2GUARD from the start
V2GUARD /STATUS      show the driver's entries, the tab, and whether a Voodoo2 is there
/Q                   no message box (result in V2GUARD.LOG and the exit code)
/ID=PCI\VEN_xxxx&DEV_xxxx   test only: treat this device as "the Voodoo2"
```

What it did at the last start is in `V2GUARD.LOG` next to the program (`%windir%`).

## How it works

The driver hooks into Windows in two places, both in the registry:

| Hook | Registry | Effect without the card |
|------|----------|-------------------------|
| start command | `HKLM\Software\Microsoft\Windows\CurrentVersion\Run` `"Voodoo2"` = `rundll32.exe 3dfxv2ps.dll,UpdateRegSettings` | the Glide error at every start |
| Display Properties tab | `HKLM\...\Controls Folder\Display\shellex\PropertySheetHandlers\Voodoo2` = `{00BD7141-…}` (`3dfxV2ps.dll`) | the tab that hangs |

`V2GUARD /INSTALL` moves the driver's `Run` entry into `HKLM\Software\Bits und Bolts\V2Guard`
(with the tab's CLSID) and puts itself into `Run` as `"V2Guard"`. At every start it then looks
for `PCI\VEN_121A&DEV_0002` among the devices Windows found **at this start**
(`HKEY_DYN_DATA\Config Manager\Enum`) — not under `HKLM\Enum`, which keeps the card's entry
after it is taken out.

- **Voodoo2 there:** the tab entry is put back (if it was taken away) and the driver's own
  command runs — the driver works exactly as installed.
- **No Voodoo2:** the tab entry is taken away and the command does not run.

The driver's files are never changed. When the driver is installed again later, it writes its
`Run` entry again; V2GUARD takes it over at the next start.

## Exit codes

| Code | Meaning |
|------|---------|
| 0 | ok |
| 1 | nothing to do (no Voodoo2 driver entries known) |
| 2 | registry error (start entry not written, device list not readable) |
| 3 | bad options |

## Build

[OpenWatcom](https://github.com/open-watcom/open-watcom-v2) 2.0:

```
wcl386 -cc -bt=nt -l=nt_win -zq -s -os -fe=bin/V2GUARD.EXE V2GUARD.C
```

It imports only KERNEL32, USER32 and ADVAPI32 functions that Windows 95/98 has.

## Testing

Tested on an ASUS P2B with Windows 98 SE, the 3dfx 3.02.02 Voodoo2 driver installed and the
card taken out (2026-11-12, through WINAI):

- `SETUP.BAT`, then a restart: no Glide error, Display Properties without the 3dfx tab
  (7 tabs instead of 8), `V2GUARD.LOG` says the command was not run.
- The "card there" path with `/ID=` set to the S3 Trio64V2 in the same PC: the tab entry
  came back and the driver's command ran (showing its Glide error, as there was no real card);
  a normal run took the tab away again.
- `REMOVE.BAT`: the driver's `Run` entry and tab entry were back exactly as before,
  `V2GUARD.EXE`, the log and the `Run` entry gone; `SETUP.BAT` again took everything over.

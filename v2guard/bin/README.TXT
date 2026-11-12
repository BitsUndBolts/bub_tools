V2GUARD 1.0 - keeps the 3dfx Voodoo2 driver quiet when no Voodoo2 is in the PC
Windows 95 / 98 / ME                                         Bits und Bolts, 2026

THE PROBLEM
With the Voodoo2 driver installed (e.g. 3dfx 3.02.02) but the card taken out:
  - every Windows start shows "_GlideInitEnvironment: glide2x.dll expected
    Voodoo^2, none detected"
  - Display Properties has a "3dfx V2-1000" tab that freezes the PC for a while
The usual cure is to uninstall the driver - and install it again with the card.

WHAT V2GUARD DOES
It takes the driver's start entry over and runs at every Windows start itself:
  - Voodoo2 in the PC:  the driver works exactly as before (its start command
                        runs, the Display Properties tab is there).
  - no Voodoo2:         no start command (no error message), no tab.
Put the card back in and restart: everything is back at once. The driver's
files are never changed. A driver installed again later is taken over again
at the next start.

INSTALL
  1. Copy this folder to the PC (e.g. C:\V2GUARD).
  2. In Windows, run SETUP.BAT from that folder (double-click, or MS-DOS Prompt).
  3. Restart Windows.
SETUP.BAT copies V2GUARD.EXE to the Windows folder and runs V2GUARD /INSTALL.

UNDO
  Run REMOVE.BAT: the driver gets its start entry and its tab back, exactly as
  it installed them, and V2GUARD is removed.

MORE
  V2GUARD /STATUS   shows what it knows: driver entries, tab, card found or not
  %windir%\V2GUARD.LOG   what it did at the last start
  How it tells: it looks for PCI\VEN_121A&DEV_0002 among the devices Windows
  found at this start (HKEY_DYN_DATA\Config Manager\Enum) - not in the old
  device list under HKLM\Enum, which keeps the card after it is taken out.
  Registry: HKLM\Software\Microsoft\Windows\CurrentVersion\Run "V2Guard"
  (the driver's "Voodoo2" entry is kept in HKLM\Software\Bits und Bolts\V2Guard),
  ...\Controls Folder\Display\shellex\PropertySheetHandlers\Voodoo2 (the tab).

BUILD
  Source: V2GUARD.C - github.com/BitsUndBolts/bub_tools, folder v2guard
  OpenWatcom 2.0:  wcl386 -cc -bt=nt -l=nt_win -zq -s -os -fe=V2GUARD.EXE V2GUARD.C

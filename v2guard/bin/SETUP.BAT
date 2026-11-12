@ECHO OFF
REM SETUP.BAT - installs V2GUARD (Bits und Bolts)
REM Keeps the 3dfx Voodoo2 driver quiet when no Voodoo2 is in the PC:
REM no Glide error at Windows start, no Voodoo2 tab in Display Properties.
REM Run it in Windows (MS-DOS Prompt or double-click), from the folder it is in.
REM SETUP /Q = no message box (the result is in %windir%\V2GUARD.LOG)
IF "%windir%"=="" GOTO NOWIN
IF NOT EXIST V2GUARD.EXE GOTO NOEXE
ECHO Copying V2GUARD.EXE to %windir% ...
COPY V2GUARD.EXE %windir%\V2GUARD.EXE > NUL
IF NOT EXIST %windir%\V2GUARD.EXE GOTO NOCOPY
START /W %windir%\V2GUARD.EXE /INSTALL %1
ECHO.
ECHO Done. V2GUARD now runs at every Windows start.
ECHO What it did: %windir%\V2GUARD.LOG    To undo it: REMOVE.BAT
GOTO END
:NOWIN
ECHO SETUP.BAT must run in Windows 95/98/ME (MS-DOS Prompt), not in plain DOS.
GOTO END
:NOEXE
ECHO V2GUARD.EXE is not here. Run SETUP.BAT from the folder that holds V2GUARD.EXE.
GOTO END
:NOCOPY
ECHO Could not copy V2GUARD.EXE to %windir%.
:END

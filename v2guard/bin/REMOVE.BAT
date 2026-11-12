@ECHO OFF
REM REMOVE.BAT - removes V2GUARD (Bits und Bolts)
REM Gives the Voodoo2 driver its start entry and Display Properties tab back,
REM exactly as the driver installed them, and removes V2GUARD.
REM REMOVE /Q = no message box
IF "%windir%"=="" GOTO NOWIN
IF NOT EXIST %windir%\V2GUARD.EXE GOTO NOTIN
START /W %windir%\V2GUARD.EXE /UNINSTALL %1
DEL %windir%\V2GUARD.EXE
IF EXIST %windir%\V2GUARD.LOG DEL %windir%\V2GUARD.LOG
ECHO V2GUARD is removed. The Voodoo2 driver is as it was installed.
GOTO END
:NOWIN
ECHO REMOVE.BAT must run in Windows 95/98/ME (MS-DOS Prompt), not in plain DOS.
GOTO END
:NOTIN
ECHO V2GUARD is not installed (%windir%\V2GUARD.EXE not found).
:END

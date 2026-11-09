; FLOPIMG.COM 1.0 - raw floppy image tool for DOS
;   FLOPIMG image /read | /write | /verify | /repair  [/B]
org 100h
%macro jcxzn 1
  jcxz %%y
  jmp short %%n
%%y:
  jmp %1
%%n:
%endmacro
  cld
  mov ax,cs
  add ax,1000h
  add ax,0FFFh
  and ax,0F000h
  mov [bseg],ax
  add ax,1000h
  mov [bseg2],ax
  mov si,81h
  xor ch,ch
  mov cl,[80h]
ploop:
  jcxzn pdn
  lodsb
  dec cx
  cmp al,' '
  je ploop
  cmp al,9
  je ploop
  cmp al,13
  je pdn
  cmp al,'/'
  je psw
  cmp al,'-'
  je psw
  cmp byte [havefn],0
  jne ploop
  mov byte [havefn],1
  mov di,fname
  stosb
fl:
  jcxz fend
  lodsb
  dec cx
  cmp al,' '
  jbe fend
  stosb
  jmp fl
fend:
  mov byte [di],0
  jmp ploop
psw:
  jcxzn pdn
  lodsb
  dec cx
  cmp al,'?'
  je sH
  and al,0DFh
  cmp al,'H'
  je sH
  cmp al,'V'
  je sV
  cmp al,'W'
  je sW
  cmp al,'B'
  je sB
  cmp al,'R'
  jne skiptok
  mov byte [mode],'R'
  cmp cx,2
  jb skiptok
  mov al,[si]
  and al,0DFh
  cmp al,'E'
  jne skiptok
  mov al,[si+1]
  and al,0DFh
  cmp al,'P'
  jne skiptok
  mov byte [mode],'P'
  jmp skiptok
sH:
  mov byte [mode],'H'
  jmp skiptok
sV:
  mov byte [mode],'V'
  jmp skiptok
sW:
  mov byte [mode],'W'
  jmp skiptok
sB:
  mov byte [drv],1
skiptok:
  jcxzn pdn
  lodsb
  dec cx
  cmp al,' '
  je ploop
  cmp al,13
  je pdn
  jmp skiptok
pdn:
  jmp pdone
pdone:
  mov al,[drv]
  add al,'A'
  mov [mdl1],al
  mov [mdl2],al
  mov [mdl3],al
  mov [mdl4],al
  mov al,[mode]
  cmp al,'H'
  je help
  cmp byte [havefn],0
  je help
  cmp al,0
  je help
  cmp al,'R'
  je doread
  cmp al,'W'
  je dowrite
  cmp al,'V'
  je doverify
  jmp dorepair
help:
  mov dx,mhelp
  mov ah,9
  int 21h
  mov ax,4C00h
  int 21h

; ================= /READ =================
doread:
  mov ax,3D00h
  mov dx,fname
  int 21h
  jc rnew
  mov bx,ax
  mov ah,3Eh
  int 21h
  mov dx,mexist1
  call prstr
  call prfname
  mov dx,mexist2
  call confirm
rnew:
  mov es,[bseg]
  xor bx,bx
  mov ax,0201h
  mov cx,1
  xor dh,dh
  call dio
  jnc gotboot
  mov dx,mboot
  jmp failerr
gotboot:
  call getgeom
  mov ah,3Ch
  xor cx,cx
  mov dx,fname
  int 21h
  jnc created
  mov dx,mcre
  jmp fail
created:
  mov [fh],ax
  call setwant
  mov dx,mread
  call prstr
  mov word [bad],0
  mov word [cyl],0
rc:
  mov word [hd],0
rh:
  mov es,[bseg]
  call readtrack
  mov ax,[trkbad]
  add [bad],ax
  mov bx,[fh]
  mov cx,[want]
  xor dx,dx
  push ds
  mov ds,[bseg]
  mov ah,40h
  int 21h
  pop ds
  jc wfail
  cmp ax,[want]
  jne wfail
  inc word [hd]
  mov ax,[hd]
  cmp ax,[heads]
  jb rh
  call dot
  inc word [cyl]
  mov ax,[cyl]
  cmp ax,[cyls]
  jb rc
  mov ah,3Eh
  mov bx,[fh]
  int 21h
  mov dx,mdone
  call prstr
  mov ax,[bad]
  call pnum
  mov dx,mend2
  call prstr
  mov ax,4C00h
  cmp word [bad],0
  je rex
  mov ax,4C01h
rex:
  int 21h
wfail:
  mov dx,mwr
  jmp fail

; ================= /VERIFY =================
doverify:
  call openimg
  mov dx,mverify
  call prstr
  mov byte [fixflag],0
  call scan
  mov dx,mvok
  cmp word [diffs],0
  je vout
  mov dx,mvdiff
vout:
  call prstr
  cmp word [diffs],0
  je vex
  mov ax,[diffs]
  call pnum
  mov dx,mend3
  call prstr
  mov ax,4C01h
  int 21h
vex:
  mov ax,4C00h
  int 21h

; ================= /REPAIR =================
dorepair:
  call openimg
  mov dx,mscan
  call prstr
  mov byte [fixflag],0
  call scan
  cmp word [diffs],0
  jne rp1
  mov dx,mnothing
  call prstr
  mov ax,4C00h
  int 21h
rp1:
  mov dx,mrp1
  call prstr
  mov ax,[diffs]
  call pnum
  mov dx,mrp2
  call prstr
  call prfname
  mov dx,mrp3
  call confirm
  call seek0
  mov dx,mfix
  call prstr
  mov byte [fixflag],1
  call scan
  mov dx,mrpdone1
  call prstr
  mov ax,[fixed]
  call pnum
  mov dx,mrpdone2
  call prstr
  mov ax,4C00h
  int 21h

; ================= /WRITE =================
dowrite:
  call openimg
  mov dx,mwarn1
  call prstr
  call prfname
  mov dx,mwarn2
  call confirm
  mov dx,mwrite
  call prstr
  mov word [cyl],0
wc:
  mov word [hd],0
wh:
  call getfiletrack
  mov es,[bseg]
  xor bx,bx
  mov ax,[spt]
  mov ah,3
  mov ch,[cyl]
  mov cl,1
  mov dh,[hd]
  call dio
  jnc wv
  mov dx,mwerr
  jmp failerr
wv:
  mov es,[bseg2]
  call readtrack
  mov word [sc],1
wcs:
  call cmpsec
  jc vfatal
  inc word [sc]
  mov ax,[sc]
  cmp ax,[spt]
  jbe wcs
  inc word [hd]
  mov ax,[hd]
  cmp ax,[heads]
  jb wh
  call dot
  inc word [cyl]
  mov ax,[cyl]
  cmp ax,[cyls]
  jb wc
  mov ah,3Eh
  mov bx,[fh]
  int 21h
  mov dx,mwdone
  call prstr
  mov ax,4C00h
  int 21h

; ---------------- shared flow ----------------
vfatal:
  mov dx,mvmis
  jmp fail
badimg:
  mov dx,mbad
fail:
  call prstr
  mov ax,4C02h
  int 21h
failerr:
  call prstr
  mov dx,mcode
  call prstr
  mov al,[lasterr]
  xor ah,ah
  call pnum
  mov dx,mend
  call prstr
  mov ax,4C02h
  int 21h
fixfail:
  cmp byte [stage],3
  je vfatal
  mov dx,mwerr
  cmp byte [stage],1
  je failerr
  mov dx,mverr
  jmp failerr

; scan whole image against floppy; [fixflag]=1: write differing sectors
scan:
  mov word [diffs],0
  mov word [fixed],0
  mov word [cyl],0
sc_c:
  mov word [hd],0
sc_h:
  call getfiletrack
  mov es,[bseg2]
  call readtrack
  mov word [sc],1
sc_s:
  call cmpsec
  jnc sc_n
  inc word [diffs]
  cmp word [diffs],8
  ja sc_np
  call prdiff
sc_np:
  cmp byte [fixflag],0
  je sc_n
  call fixsec
  jnc sc_fx
  jmp fixfail
sc_fx:
  inc word [fixed]
sc_n:
  inc word [sc]
  mov ax,[sc]
  cmp ax,[spt]
  jbe sc_s
  inc word [hd]
  mov ax,[hd]
  cmp ax,[heads]
  jb sc_h
  call dot
  inc word [cyl]
  mov ax,[cyl]
  cmp ax,[cyls]
  jb sc_c
  ret

; open image, read geometry from its boot sector, check size, seek to start
openimg:
  mov ax,3D00h
  mov dx,fname
  int 21h
  jnc oi1
  mov dx,mnofile
  jmp fail
oi1:
  mov [fh],ax
  mov bx,ax
  mov cx,512
  xor dx,dx
  push ds
  mov ds,[bseg]
  mov ah,3Fh
  int 21h
  pop ds
  jc badimg
  cmp ax,512
  jne badimg
  call getgeom
  mov ax,4202h
  mov bx,[fh]
  xor cx,cx
  xor dx,dx
  int 21h
  jc badimg
  test ax,511
  jnz badimg
  mov cl,9
  shr ax,cl
  mov cl,7
  shl dx,cl
  or ax,dx
  mov [fsec],ax
  mov ax,[spt]
  mul word [heads]
  mul word [cyls]
  cmp ax,[fsec]
  jne badimg
  call setwant
seek0:
  mov ax,4200h
  mov bx,[fh]
  xor cx,cx
  xor dx,dx
  int 21h
  ret

setwant:
  mov ax,[spt]
  mov cl,9
  shl ax,cl
  mov [want],ax
  ret

getfiletrack:
  mov bx,[fh]
  mov cx,[want]
  xor dx,dx
  push ds
  mov ds,[bseg]
  mov ah,3Fh
  int 21h
  pop ds
  jc badimg
  cmp ax,[want]
  jne badimg
  ret

; read track [cyl],[hd] into ES:0; secbad[] marks unreadable sectors, [trkbad]=count
readtrack:
  push es
  push ds
  pop es
  mov di,secbad
  mov cx,18
  xor ax,ax
  rep stosw
  pop es
  mov word [trkbad],0
  xor bx,bx
  mov ax,[spt]
  mov ah,2
  mov ch,[cyl]
  mov cl,1
  mov dh,[hd]
  call dio
  jnc rtok
  mov word [sc],1
rts:
  mov ax,[sc]
  dec ax
  mov cl,9
  shl ax,cl
  mov bx,ax
  mov ax,0201h
  mov ch,[cyl]
  mov cl,[sc]
  mov dh,[hd]
  call dio
  jnc rtn
  mov bx,[sc]
  mov byte [bx+secbad-1],1
  inc word [trkbad]
  mov ax,[sc]
  dec ax
  mov cl,9
  shl ax,cl
  mov di,ax
  mov cx,256
  xor ax,ax
  rep stosw
rtn:
  inc word [sc]
  mov ax,[sc]
  cmp ax,[spt]
  jbe rts
rtok:
  ret

; compare sector [sc]: file buffer (bseg) vs floppy buffer (bseg2); CF=1 if different/unreadable
cmpsec:
  mov bx,[sc]
  cmp byte [bx+secbad-1],0
  jne csd
  mov ax,[sc]
  dec ax
  mov cl,9
  shl ax,cl
  mov si,ax
  mov di,ax
  mov es,[bseg2]
  mov ax,[bseg]
  push ds
  mov ds,ax
  mov cx,256
  repe cmpsw
  pop ds
  jne csd
  clc
  ret
csd:
  stc
  ret

; write sector [sc] from file buffer, read back, compare; CF=1 on failure ([stage] says where)
fixsec:
  mov ax,[sc]
  dec ax
  mov cl,9
  shl ax,cl
  mov bx,ax
  mov es,[bseg]
  mov ax,0301h
  mov ch,[cyl]
  mov cl,[sc]
  mov dh,[hd]
  call dio
  jnc fx1
  mov byte [stage],1
  stc
  ret
fx1:
  mov ax,[sc]
  dec ax
  mov cl,9
  shl ax,cl
  mov bx,ax
  mov es,[bseg2]
  mov ax,0201h
  mov ch,[cyl]
  mov cl,[sc]
  mov dh,[hd]
  call dio
  jnc fx2
  mov byte [stage],2
  stc
  ret
fx2:
  mov bx,[sc]
  mov byte [bx+secbad-1],0
  call cmpsec
  jnc fx3
  mov byte [stage],3
  stc
  ret
fx3:
  clc
  ret

; BIOS disk call with retries: AX=function/count, ES:BX buffer, CX/DH set; CF=1 on failure
dio:
  mov [rdax],ax
  mov byte [tries],5
dio1:
  mov ax,[rdax]
  mov dl,[drv]
  int 13h
  jnc dook
  mov [lasterr],ah
  xor ax,ax
  mov dl,[drv]
  int 13h
  dec byte [tries]
  jnz dio1
  stc
  ret
dook:
  clc
  ret

getgeom:
  mov es,[bseg]
  mov ax,[es:18h]
  mov [spt],ax
  mov ax,[es:1Ah]
  mov [heads],ax
  mov ax,[es:13h]
  test ax,ax
  jnz hv
  mov ax,[es:20h]
hv:
  mov [total],ax
  cmp word [spt],1
  jb defs
  cmp word [spt],36
  ja defs
  cmp word [heads],1
  jb defs
  cmp word [heads],2
  ja defs
  cmp word [total],0
  je defs
  jmp geom
defs:
  mov word [spt],18
  mov word [heads],2
  mov word [total],2880
geom:
  mov ax,[spt]
  mul word [heads]
  mov bx,ax
  mov ax,[total]
  xor dx,dx
  div bx
  mov [cyls],ax
  cmp ax,1
  jb defc
  cmp ax,85
  jbe okc
defc:
  mov word [cyls],80
okc:
  ret

; Y/N question: DX = text. N aborts at once, only Y continues.
confirm:
  call prstr
cf1:
  mov ah,8
  int 21h
  and al,0DFh
  cmp al,'N'
  je cfno
  cmp al,'Y'
  jne cf1
  mov dl,'Y'
  mov ah,2
  int 21h
  mov dx,mend2
  jmp prstr
cfno:
  mov dl,'N'
  mov ah,2
  int 21h
  mov dx,mabort
  call prstr
  mov ax,4C03h
  int 21h

prdiff:
  mov dx,mdc
  call prstr
  mov ax,[cyl]
  call pnum
  mov dx,mdh
  call prstr
  mov ax,[hd]
  call pnum
  mov dx,mds
  call prstr
  mov ax,[sc]
  call pnum
  mov dx,mend2
  jmp prstr

prstr:
  mov ah,9
  int 21h
  ret

prfname:
  mov si,fname
pf1:
  lodsb
  test al,al
  jz pf2
  mov dl,al
  mov ah,2
  int 21h
  jmp pf1
pf2:
  ret

dot:
  mov dl,'.'
  mov ah,2
  int 21h
  ret

pnum:
  mov bx,10
  xor cx,cx
pn1:
  xor dx,dx
  div bx
  push dx
  inc cx
  test ax,ax
  jnz pn1
pn2:
  pop dx
  add dl,'0'
  mov ah,2
  int 21h
  loop pn2
  ret

mhelp  db 'FLOPIMG 1.0 - raw floppy image tool (boot sector included)',13,10
       db 13,10
       db 'Usage: FLOPIMG <image> <function> [/B]',13,10
       db 13,10
       db '  /read     copy the floppy into <image> (exact sector-by-sector copy)',13,10
       db '  /write    write <image> to the floppy; ERASES the whole disk',13,10
       db '  /verify   compare floppy with <image>; read only, never writes',13,10
       db '  /repair   compare, then write ONLY the sectors that differ',13,10
       db '  /B        use drive B: (default is drive A:)',13,10
       db '  /? /help  this screen',13,10
       db 13,10
       db '/write and /repair show a warning and wait for Y or N. N aborts at once.',13,10
       db 'Everything written is read back and compared. Unreadable sectors on',13,10
       db '/read are reported. Image size is taken from the disk geometry.',13,10
       db 13,10
       db 'Examples: FLOPIMG MATS.IMG /read     FLOPIMG MATS.IMG /verify',13,10
       db '          FLOPIMG MATS.IMG /write    FLOPIMG MATS.IMG /repair /B',13,10
       db 'Exit codes: 0 ok, 1 differences/unreadable, 2 error, 3 aborted',13,10,'$'
mboot  db 'ERROR: cannot read boot sector of drive $'
mcre   db 'ERROR: cannot create output file',13,10,'$'
mwr    db 'ERROR: write to image file failed (disk full?)',13,10,'$'
mnofile db 'ERROR: image file not found',13,10,'$'
mbad   db 'ERROR: not a valid floppy image (size or boot sector geometry)',13,10,'$'
mwerr  db 'ERROR: cannot write to floppy - write protected?$'
mverr  db 'ERROR: cannot read back from floppy$'
mvmis  db 13,10,'ERROR: verify mismatch - floppy does NOT match the image!',13,10,'$'
mexist1 db 'File $'
mexist2 db ' exists and will be overwritten. Continue (Y/N)? $'
mwarn1 db 13,10,'*** WARNING ***',13,10
       db 'Everything on the disk in drive '
mdl2   db 'A: will be ERASED and replaced with',13,10,'$'
mwarn2 db 13,10,'Write the image to the disk? (Y/N) $'
mrp1   db 13,10,'$'
mrp2   db ' sector(s) on drive '
mdl4   db 'A: differ. They will be OVERWRITTEN with data from',13,10,'$'
mrp3   db 13,10,'Repair these sectors? (Y/N) $'
mscan  db 'Comparing (read only)...',13,10,'$'
mfix   db 'Writing differing sectors...',13,10,'$'
mverify db 'Verifying (read only, nothing is written)...',13,10,'$'
mread  db 'Reading drive '
mdl1   db 'A: ',13,10,'$'
mwrite db 'Writing and verifying...',13,10,'$'
mdone  db 13,10,'DONE, unreadable sectors: $'
mwdone db 13,10,'DONE - floppy written and verified OK.',13,10,'$'
mvok   db 13,10,'VERIFY OK - floppy is identical to the image.',13,10,'$'
mvdiff db 13,10,'VERIFY FAILED - floppy differs from the image. Differing sectors: $'
mnothing db 13,10,'Nothing to repair - floppy already matches the image.',13,10,'$'
mrpdone1 db 13,10,'REPAIR DONE - $'
mrpdone2 db ' sector(s) written and verified OK.',13,10,'$'
mdc    db 13,10,'  differs: cylinder $'
mdh    db ' head $'
mds    db ' sector $'
mcode  db ' (BIOS error $'
mend   db ')',13,10,'$'
mend2  db 13,10,'$'
mend3  db 13,10,'$'
mabort db 13,10,'Aborted - nothing was changed.',13,10,'$'
mdl3   db 'A'
havefn db 0
yes    db 0
drv    db 0
mode   db 0
fixflag db 0
stage  db 0
tries  db 0
lasterr db 0
rdax   dw 0
bseg   dw 0
bseg2  dw 0
spt    dw 0
heads  dw 0
total  dw 0
cyls   dw 0
cyl    dw 0
hd     dw 0
sc     dw 0
bad    dw 0
fh     dw 0
want   dw 0
fsec   dw 0
diffs  dw 0
fixed  dw 0
trkbad dw 0
secbad times 40 db 0
fname  db 0
 times 80 db 0

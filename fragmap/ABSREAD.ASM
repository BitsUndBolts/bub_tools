; ABSREAD.ASM - INT 25h absolute disk read for FRAGMAP (NASM, 8086, OpenWatcom small model)
;
;   int __cdecl abs_read_pkt(int drive, struct dpkt *pkt);
;       DOS 4+ form (CX=FFFFh, DS:BX -> packet), needed for partitions over 32 MB
;   int __cdecl abs_read_old(int drive, unsigned sector, unsigned count, void __far *buf);
;       DOS 2/3 form (DX = sector, CX = count, DS:BX -> buffer), partitions up to 32 MB
;
; drive: 0 = A:. Returns 0 when the read worked, else the DOS/BIOS error code (never 0).
; INT 25h only reads; nothing on the disk is changed. It leaves the caller's flags
; on the stack and may destroy every register except the segment registers.

        cpu     8086

segment _TEXT public class=CODE use16 align=1

        global  _abs_read_pkt
        global  _abs_read_old

_abs_read_pkt:
        push    bp
        mov     bp, sp
        push    bx
        push    si
        push    di
        push    ds
        mov     al, [bp+4]              ; drive
        mov     bx, [bp+6]              ; packet (in DGROUP = DS)
        mov     cx, 0FFFFh
        int     25h
        jmp     short finish

_abs_read_old:
        push    bp
        mov     bp, sp
        push    bx
        push    si
        push    di
        push    ds
        mov     al, [bp+4]              ; drive
        mov     dx, [bp+6]              ; first sector
        mov     cx, [bp+8]              ; count
        mov     bx, [bp+10]             ; buffer offset
        mov     ds, [bp+12]             ; buffer segment
        int     25h

finish:
        pop     dx                      ; the flags INT 25h left (POP keeps CF)
        jc      failed
        xor     ax, ax
        jmp     short done
failed:
        or      ax, ax
        jnz     done
        mov     ax, 0FFFFh
done:
        pop     ds
        pop     di
        pop     si
        pop     bx
        pop     bp
        ret

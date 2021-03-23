.section __TEXT,__stacks
.align 14
_cpu5_stack: .space 0x4000, 0x0
_cpu5_exception_stack: .space 0x4000, 0x0

    .section __TEXT,__cpu5debug
    .align 14
_cpu5_debug: .space 0x4000, 0x0

.section __TEXT,__text
.align 12
.global _cpu5_iorvbar

_cpu5_iorvbar:
    /* Unlock this core for debugging */
    msr oslar_el1, xzr
    msr DAIFSet, #0xf

    /* Original ROM VBAR, will I need to make a new one later? */
    mov x0, #0x100000000
    movk x0, #0x800
    msr vbar_el1, x0

    msr DAIFClr, #0x4
    isb sy

    mrs x0, cpacr_el1
    orr x0, x0, #0x300000
    msr cpacr_el1, x0


    /* ff04: device memory is Device-nGnRE */
    /* ff00: device memory is Device-nGnRnE */
    mov x0, #0xff04
    ; mov x0, #0xff00
    ; mov x0, #0xffff
    msr mair_el1, x0


    mov x0, #0x100000000
    movk x0, #0x659c, lsl #16
    movk x0, #0xa51c
    msr tcr_el1, x0

    msr SPSel, #1
    adrp x0, _cpu5_exception_stack@PAGE
    add x0, x0, _cpu5_exception_stack@PAGEOFF
    add x0, x0, #0x4000
    /* PA --> VA slide */
    ; mov x2, #0x700000000
    ; add x0, x0, x2
    mov sp, x0

    msr SPSel, #0
    adrp x0, _cpu5_stack@PAGE
    add x0, x0, _cpu5_stack@PAGEOFF
    ; add x0, x0, #0x4000
    ; add x0, x0, x2
    mov sp, x0

    /* Original ROM TTBR0 */
    mov x0, #0x180000000
    movk x0, #0xc000
    msr ttbr0_el1, x0

    dsb ish
    isb sy
    tlbi vmalle1
    isb sy

    /* Enable caches, instruction cache, SP alignment checking, 
     * and MMU, but don't enable WXN since we live on rwx memory.
     * sctlr_el1 from reset doesn't have WXN enabled */
    mrs x0, sctlr_el1
    mov x1, #0x100d

    /* This x1 does not enable caches, only SP alignment checking and MMU */
    ; mov x1, #0x9
    ; mov x1, #0x1

    orr x0, x0, x1
    msr sctlr_el1, x0

    dsb ish
    isb sy
    tlbi vmalle1
    isb sy

    /* Page tables have been set up, rebase ourselves to the rwx view
     * of AOP SRAM cpu0 uses */
    ; adr x30, Lrebase
    ; add x30, x30, x2
    ; adrp x16, _cpu5_debug@PAGE
    ; add x16, x16, _cpu5_debug@PAGEOFF
    ; mov x7, x30
    ; mov x0, x30
    ; mov x1, #0x40
    ; bl dcache_clean_PoU
    ; bl icache_invalidate_PoU
    ; mov x30, x7
    ; str x30, [x16]
    ; dmb sy
    ; ; ; adr x0, .
    ; ; dc cvau, x30
    ; ; dsb ish
    ; ldr x0, [x30]
    ; dmb sy
    ; str x0, [x16, #0x8]

    ;934E00000
    ; mov x0, #0x900000000
    ; ; movk x0, #0x34e0, lsl #16
    ; movk x0, #0x34e0, lsl #16
    ; movk x0, #0x10d4
    ; str x0, [x16]

    ; at s1e1r, x0
    ; isb sy
    ; mrs x1, par_el1

    ; ldr w1, [x0]
    ; ; dsb sy
    ; str w1, [x16, #0x8]
    ; str x30, [x16, #0x10]

    ; adr x0, _cpu5_init_done
    ; mov w1, #1
    ; str w1, [x0]
    ; b .
    ; ret

Lrebase:
    ; adrp x16, _cpu5_debug@PAGE
    ; add x16, x16, _cpu5_debug@PAGEOFF
    ; mov w0, #1
    ; str w0, [x16, #0x18]

    ; msr DAIFClr, #0xf

    ; adr x0, _cpu5_init_done
    ; str x0, [x16, #0x20]
    ; adrp x0, _debugger_tick@PAGE
    ; add x0, x0, _debugger_tick@PAGEOFF
    ; str x0, [x16, #0x28]
    ; ldr w0, [x0]
    ; str w0, [x16, #0x30]
    ; ; b .
    adr x0, _cpu5_init_done
    mov w1, #0x1
    str w1, [x0]
    ; b .

    adrp x0, _cpu5_debug@PAGE
    add x0, x0, _cpu5_debug@PAGEOFF
    b _debugger_tick

.global _cpu5_init_done
_cpu5_init_done: .dword 0x0

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

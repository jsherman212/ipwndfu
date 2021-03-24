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

    mov x0, #0xff04
    msr mair_el1, x0

    mov x0, #0x100000000
    movk x0, #0x659c, lsl #16
    movk x0, #0xa51c
    msr tcr_el1, x0

    msr SPSel, #1
    adrp x0, _cpu5_exception_stack@PAGE
    add x0, x0, _cpu5_exception_stack@PAGEOFF
    add x0, x0, #0x4000
    mov sp, x0

    msr SPSel, #0
    adrp x0, _cpu5_stack@PAGE
    add x0, x0, _cpu5_stack@PAGEOFF
    add x0, x0, #0x4000
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
    orr x0, x0, x1
    msr sctlr_el1, x0

    dsb ish
    isb sy
    tlbi vmalle1
    isb sy

    msr DAIFClr, #0xf

    adr x0, _cpu5_init_done
    mov w1, #0x1
    str w1, [x0]

    b _debugger_tick

.global _cpu5_init_done
_cpu5_init_done: .dword 0x0

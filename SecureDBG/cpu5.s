#define rvbar_el1 s3_0_c12_c0_1

.section __TEXT,__pagetables
.align 14
.space 0x4000, 0x0

.section __TEXT,__stacks
.align 14
_cpu5_exception_stack: .space 0x2000, 0x0
_cpu5_stack: .space 0x2000, 0x0

.section __TEXT,__text
.align 12
.global _cpu5_iorvbar

_cpu5_iorvbar:
    msr DAIFSet, #0xf

    /* Will be exception vector in SecureROM */
    ; adr x0, _ExceptionVectorsBase
    mov x0, #0x100000000
    orr x0, x0, #0x800
    msr vbar_el1, x0

    /* Is this correct? Also cannot be done in qemu, only SecureROM */
    ; adr x0, _cpu5_iorvbar
    ; msr rvbar_el1, x0

    mov x0, #0xff04
    msr mair_el1, x0
    ; mov x0, #0x1659ca51c
    mov x0, #0x100000000
    movk x0, #0xa51c
    movk x0, #0x659c, lsl #16
    msr tcr_el1, x0

    /* Set ttbr0_el1 to SecureROM's if we're not in qemu. Otherwise,
     * point it to the mock ttbase I set up */
    ; adr x0, ttroot

    mov x0, #0x180000000
    orr x0, x0, #0xc000

    ; movk x1, #0x6a5
    ; str x0, [x0]
    ; str x1, [x0, #0x400]
    ; str x1, [x0, #0x8]
    ; str x1, [x0, #0x80]
    msr ttbr0_el1, x0

    mov x0, #0x300000
    msr cpacr_el1, x0

    ; mov x0, #-1
    ; mov w1, #0x4141
    ; str w1, [x0]
    ; mov x0, #0x100000000
    ; orr x0, x0, #0x10000

    adrp x0, _cpu5_exception_stack@PAGE
    add x0, x0, _cpu5_exception_stack@PAGEOFF
    mov sp, x0
    msr SPSel, #0
    adrp x0, _cpu5_stack@PAGE
    add x0, x0, _cpu5_stack@PAGEOFF
    mov sp, x0

    /* Is this needed inside SecureROM? */
    mrs x0, s3_1_c15_c2_1
    orr x0, x0, #(1 << 6)
    msr s3_1_c15_c2_1, x0

    msr DAIFClr, #0xf

    isb sy
    dsb sy
    tlbi vmalle1
    dsb sy
    isb sy

    mrs x0, sctlr_el1
    /* Enable caches, instruction cache, SP alignment checking, 
     * and MMU, but don't enable WXN since we live on rwx memory */
    mov x1, #0x100d
    /* This x1 has no MMU enable bit */
    /* Enabling MMU inside qemu without setting TTBR0_EL1 screws
     * it up really badly */
    ; mov x1, #0x100c
    orr x0, x0, x1
    msr sctlr_el1, x0
    dsb sy
    isb
    b _debugger_tick

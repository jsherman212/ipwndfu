#define L2_SRAM_CONFIG s3_3_c15_c7_0

#define AP_SRAM_PART1_T8015 0x180000000
#define AP_SRAM_PART1_SZ_T8015 0x1c000
#define AP_SRAM_PART2_T8015 0x1801e4000
#define AP_SRAM_PART2_SZ_T8015 0x1c000

/* 
    !!!! IMPORTANT !!!! 
    DO NOT WRITE TO THE STACK IN THIS FILE
    STACKS LIVE IN AP SRAM AS WELL, THAT WILL CAPTURE OLD STATE
    DO NOT DO ANY LOADS/STORES (BESIDES MEMCPY8)

    1. copy AP SRAM to AOP SRAM
    2. map 4 more MB from L2 cache
    3. copy AP SRAM back from AOP SRAM

    We disable MMU while doing this since page tables live
    on AP SRAM and AP SRAM is wiped in the process
*/

.section __TEXT,__ap_sram
.align 14
aop_ap_sram: .space 0x38000, 0x0

.section __TEXT,__text
.align 2
.global _sram_expand
_sram_expand:
    mrs x16, DAIF
    msr DAIFSet, #0xf
    isb sy

    mov x8, x30

    mov x17, #0x7908
    movk x17, #0x1, lsl #32

    bl _mmu_disable
    mov x9, x0
    /* X9 old sctlr */

    adrp x13, aop_ap_sram@PAGE
    mov x14, AP_SRAM_PART1_SZ_T8015

    mov x0, x13
    mov x1, AP_SRAM_PART1_T8015
    mov x2, x14
    bl _memcpy8

    dsb sy
    isb sy

    add x0, x13, x14
    mov x1, #0x4000
    movk x1, #0x801e, lsl #16
    movk x1, #0x1, lsl #32
    mov x2, AP_SRAM_PART2_SZ_T8015
    bl _memcpy8

    dsb sy
    isb sy

    mrs x15, L2_SRAM_CONFIG
    and x15, x15, #0xfffffffffffffff0
    msr L2_SRAM_CONFIG, x15
1:
    mrs x15, L2_SRAM_CONFIG
    tbnz x15, #63, 1b

    mrs x15, L2_SRAM_CONFIG
    and x15, x15, #0xfffffffffffffff0
    orr x15, x15, #0x3
    msr L2_SRAM_CONFIG, x15
2:
    mrs x15, L2_SRAM_CONFIG
    tbz x15, #63, 2b

    dsb sy
    isb sy

    mov x0, AP_SRAM_PART1_T8015
    mov x1, x13
    mov x2, x14
    bl _memcpy8

    dsb sy
    isb sy

    mov x0, #0x4000
    movk x0, #0x801e, lsl #16
    movk x0, #0x1, lsl #32
    add x1, x13, x14
    mov x2, AP_SRAM_PART2_SZ_T8015
    bl _memcpy8

    dsb sy
    isb sy

    mov x0, x9
    bl _mmu_enable
    dsb sy
    isb sy
    tlbi vmalle1
    isb sy
    msr DAIF, x16
    mov x30, x8
    ret

.global _relocate_and_jump
_relocate_and_jump:
    mov x8, x30         /* x8 is something like 0x234E00xxx */
    mov x9, x0          /* x9 is 0x180200000 */
    mov x10, x1         /* x10 is address of first insn of earlypongo_entryp */
    sub x2, x2, x1
    bl _memcpy8
    dsb sy
    ic iallu
    isb sy
    sub x8, x8, x10
    add x30, x8, x9

    ; mov x30, x8
    ret

/* Returns old SCTLR */
_mmu_disable:
    mrs x0, sctlr_el1
    msr sctlr_el1, xzr
    isb sy
    ret

/* First parameter is old SCTLR value */
_mmu_enable:
    msr sctlr_el1, x0
    isb sy
    ret

_memcpy8:
    /* End of src */
    add x3, x1, x2

Lmemcpy8_loop:
    ldrb w4, [x1], #0x1
    strb w4, [x0], #0x1
    cmp x3, x1
    b.ne Lmemcpy8_loop
    ret

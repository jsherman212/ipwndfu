    /* This file contains the hook for T8015's boot trampoline.
    The idea is to swap out the first argument (which points to iBoot's
    entrypoint) with Pongo's entrypoint and save iBoot's entrypoint in the
    unused second argument. Pongo will be in charge of dispatching to iBoot
    when the user requests it */

.align 2
.global _boot_tramp_hook

_boot_tramp_hook:
    /* We overwrote the first instruction to get here */
    msr DAIFSet, #0xf
    ; mov x8, x30
    ; add x8, x8, #0x4
    
    /* Save iBoot entrypoint in second arg */
    mov x1, x0

    /* Place Pongo's entrypoint in first arg */
    ldr x0, =section$start$__TEXT$__pongoOS

    ; ret x8
    ret
